/*
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#include "hsi2s_umd_protocol.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <time.h>


#include "shm_ops.h"

#include <poll.h>
#include <sys/epoll.h>
#include "hsi2s_common.h"

struct priv_ops {
	void* (*init)(void);
	void (*exit)(void *priv);
	int (*apptoumd)(struct client_cmd *cmd);
	void (*app_write_to_umd)(int interface, unsigned char* data, int data_len, int sync);
	void (*app_read_from_umd)(int interface, Funconsume consume, void *arg);
};

struct my_priv {
	int handle;
	int fd_core;
	struct priv_ops ops;
} gpriv;

#define MAX_INTF_CNT 3


/* use for shm */
struct GvmShareBufInfo {
	struct myBufInfo* info;
	unsigned char *txbuf;
	unsigned char *rxbuf;
	unsigned char *ctrl;
};

#define BUF_SIZE (4 *1024 *1024)
struct GvmShareBufInfo gvmInfo[MAX_INTF_CNT];

/* use for shm */
#define HSI2S_CORE_PATH "/dev/hsi2s_reginfo"
static int hs_fds[MAX_INTF_CNT];


static void* shm_channel_init(void);
static void shm_channel_exit(void* priv);
static int shm_apptoumd(struct client_cmd *cmd);
static void shm_app_write_to_umd(int interface, unsigned char* data, int data_len, int sync);
static void shm_app_read_from_umd(int interface, Funconsume consume, void *arg);

static void* drv_channel_init(void);
static void drv_channel_exit(void* priv);
static int drv_apptoumd(struct client_cmd *cmd);
static void drv_app_write_to_umd(int interface, unsigned char* data, int data_len, int sync);
static void drv_app_read_from_umd(int interface, Funconsume consume, void *arg);

struct priv_ops shm_ops = {
	.init = shm_channel_init,
	.exit = shm_channel_exit,
	.apptoumd = shm_apptoumd,
	.app_write_to_umd = shm_app_write_to_umd,
	.app_read_from_umd = shm_app_read_from_umd,
};
struct priv_ops drv_ops = {
	.init = drv_channel_init,
	.exit = drv_channel_exit,
	.apptoumd = drv_apptoumd,
	.app_write_to_umd = drv_app_write_to_umd,
	.app_read_from_umd = drv_app_read_from_umd,
};


static void* shm_channel_init(void)
{
	int handle = shm_setup_channel();
	if (handle < 0) {
		return NULL;
	}

	gpriv.handle = handle;
	return &gpriv;
}
static void shm_channel_exit(void* priv)
{
	struct my_priv *p = priv;

	shm_shutdown_channel(p->handle);
	p->handle = -1;
}

static int setup_data_channel(int interface)
{
	int ret;
	printf("%s: interface %d\n", __func__, interface);

	if (gvmInfo[interface].ctrl) {
		printf("%s: interface %d data channel already setup\n", __func__, interface);
		return -1;
	}
	gvmInfo[interface].info = create_shm(interface, 2 * BUF_SIZE + (1 *1024 *1024));
	if (NULL == gvmInfo[interface].info) {
		printf("GVM: create_shm failed\n");
		return -1;
	}

	gvmInfo[interface].txbuf = gvmInfo[interface].info->buf;
	gvmInfo[interface].rxbuf = gvmInfo[interface].txbuf + BUF_SIZE;
	gvmInfo[interface].ctrl = gvmInfo[interface].rxbuf + BUF_SIZE;

	return 0;
}
static void shutdown_data_channel(int interface)
{
	printf("%s: interface %d\n", __func__, interface);

	if (!gvmInfo[interface].ctrl) {
		printf("%s: interface %d data channel already shutdown\n", __func__, interface);
		return;
	}
	destroy_shm(gvmInfo[interface].info);
	gvmInfo[interface].ctrl = NULL;
}

static int shm_apptoumd(struct client_cmd *cmd)
{
	int ret = 0;

	switch (cmd->type) {
		case UMD_LPAIF_INTERNAL_LOOPBACK:
		case UMD_LPAIF_EXTERNAL_LOOPBACK:
		case UMD_LPAIF_NORMAL_MODE:
		case UMD_LPAIF_SPEAKER:
		case UMD_LPAIF_MIC:
			setup_data_channel(cmd->interface);
			break;
	}

	printf("Sending message %x\n", cmd->type);
	cmd->result = 0;
	ret = shm_notify_peer(cmd, sizeof(*cmd));
	if (ret) {
		printf("GVM: send client_cmd ret=%d\n", ret);
		return ret;
	}

	ret = cmd->result;
	printf("%s: Type 0x%x, result  %d\n", __func__, cmd->type, ret);

	switch (cmd->type) {
		case UMD_LPAIF_RESET:
		case UMD_LPAIF_DEINIT_TX:
			shutdown_data_channel(cmd->interface);
			break;
	}
	return ret;
}
static void shm_app_write_to_umd(int interface, unsigned char* data, int data_len, int sync)
{
	int count = data_len / BUF_SIZE;
	int ret;
	unsigned char* txbuf = gvmInfo[interface].txbuf;
	if (!txbuf) {
		printf("interface %d: %s: data channel not setup\n", interface, __func__);
		return;
	}

	struct client_cmd cmd = {
		.interface = interface,
		.type = UMD_TEST,
	};
	for (int i=0; i< count; i++) {
		memcpy(txbuf, data + i *BUF_SIZE, BUF_SIZE);
		printf("%s: send %.2x %.2x %.2x %.2x\n", __func__, txbuf[0], txbuf[1], txbuf[2], txbuf[3]);
		memcpy(&cmd.param, txbuf, sizeof(int));
		cmd.result = 0;
		ret = shm_notify_peer(&cmd, sizeof(cmd));
	}

	if (data_len > BUF_SIZE * count) {
		memcpy(txbuf, data + count *BUF_SIZE, data_len - BUF_SIZE * count);
		printf("%s: send %.2x %.2x %.2x %.2x\n", __func__, txbuf[0], txbuf[1], txbuf[2], txbuf[3]);
		memcpy(&cmd.param, txbuf, sizeof(int));
		cmd.result = 0;
		ret = shm_notify_peer(&cmd, sizeof(cmd));
	}
	return;
}

static void shm_app_read_from_umd(int interface, Funconsume consume, void *arg)
{
	unsigned char* rxbuf = gvmInfo[interface].rxbuf;
	if (!rxbuf) {
		printf("interface %d: %s: data channel not setup\n", interface, __func__);
		return;
	}
	volatile unsigned int* ctrl = (unsigned int*)gvmInfo[interface].ctrl;
	ctrl[0] = 0;
	unsigned int last = 0;
	struct timespec ts = { .tv_sec = 0, .tv_nsec = 100000000 }; // 100 ms

	while(1) {
		if (last == ctrl[0]) {
			//sleep(1);
			nanosleep(&ts, NULL);
			if (last == ctrl[0]) {
				continue;
			}
			printf("last = %d, ctrl[0]= %d, ctrl[1]= %d\n", last, ctrl[0], ctrl[1]);
		}
		last = ctrl[0];
		int data_len = ctrl[1];
		if (data_len == 0) continue;
		//printf("last = %d, data_len=%d\n", last, data_len);
		int exit = consume(arg, rxbuf, data_len, time(NULL));
		ctrl[1] = 0;
		if(exit) {
			printf("interface %d: consume want exit!\n", interface);
			break;
		}
	}
	return;
}

static void* drv_channel_init(void)
{
	int fd_core = open(HSI2S_CORE_PATH, O_RDWR);
	if(fd_core < 0) {
		printf("Cannot open core device file /dev/hsi2s_reginfo\n");
		return NULL;
	}
	gpriv.fd_core = fd_core;
	return &gpriv;
}

static void drv_channel_exit(void* priv)
{
	struct my_priv *p = priv;

	for(int i=0; i<MAX_INTF_CNT; i++) {
		int fd = hs_fds[i];
		if(fd > 0) {
			close(fd);
		}
		hs_fds[i] = 0;
	}
	int fd_core = p->fd_core;
	close(fd_core);
}

static int cmd2kernel(int fd, struct client_cmd *cmd)
{
	switch(cmd->type) {
		case UMD_LPAIF_NORMAL_MODE:
			return ioctl(fd, LPAIF_NORMAL_MODE);
		case UMD_LPAIF_INTERNAL_LOOPBACK:
			return ioctl(fd, LPAIF_INTERNAL_LOOPBACK);
		case UMD_LPAIF_EXTERNAL_LOOPBACK:
			return ioctl(fd, LPAIF_EXTERNAL_LOOPBACK);
		case UMD_LPAIF_MUXMODE:
			return ioctl(fd, LPAIF_MUXMODE, cmd->param);
		case UMD_LPAIF_SPEAKER:
			return ioctl(fd, LPAIF_SPEAKER);
		case UMD_LPAIF_MIC:
			return ioctl(fd, LPAIF_MIC);
		case UMD_LPAIF_SET_SLAVE:
			return ioctl(fd, LPAIF_SET_SLAVE, cmd->param);
		case UMD_LPAIF_INIT_TX:
			return -1;
		case UMD_LPAIF_DEINIT_TX:
			return ioctl(fd, LPAIF_DEINIT_TX);
		case UMD_LPAIF_SET_CLOCK:
			return ioctl(fd, LPAIF_SET_CLOCK, cmd->param);
		case UMD_LPAIF_RESET:
			return ioctl(fd, LPAIF_RESET);
		case UMD_LPAIF_MODE:
			return ioctl(fd, LPAIF_MODE, cmd->param);
		case UMD_I2S_CONFIG_PARAMS:
			return ioctl(fd, I2S_CONFIG_PARAMS, &cmd->i2s_params);
		case UMD_PCM_CONFIG_PARAMS:
			return ioctl(fd, PCM_CONFIG_PARAMS, &cmd->pcm_params);
		case UMD_TDM_CONFIG_PARAMS:
			return ioctl(fd, TDM_CONFIG_PARAMS, &cmd->tdm_params);
		case UMD_PCM_CONFIG_LANE:
			return ioctl(fd, PCM_CONFIG_LANE, cmd->param);
		case UMD_LPAIF_INVERT_BIT_CLOCK:
			return ioctl(fd, LPAIF_INVERT_BIT_CLOCK);
		case UMD_CONFIGURE_DAB_MRC:
			return ioctl(fd, CONFIGURE_DAB_MRC);
	}
	return -1;
}

static int drv_apptoumd(struct client_cmd *cmd)
{
	int ret = 0;

	int fd = hs_fds[cmd->interface];
	if (fd <= 0) {
		char name[16] = "/dev/hs0_i2s";
		name[7] += cmd->interface & 0x03;
		printf("open path %s\n", name);

		fd = open(name, O_RDWR);
		if (fd < 0) {
			printf("Cannot open device file %s\n", name);
			return -1;
		}
		hs_fds[cmd->interface] = fd;
	}
	printf("Sending message %x\n", cmd->type);
	ret = cmd2kernel(fd, cmd);
	printf("%s: Type 0x%x, result  %d\n", __func__, cmd->type, ret);
	return ret;
}

static void drv_app_write_to_umd(int interface, unsigned char* data, int data_len, int sync)
{
	int fd = hs_fds[interface];
	write(fd, data, data_len);
	return;
}

static void drv_app_read_from_umd(int interface, Funconsume consume, void *arg)
{
#define PG_SIZE 4096
#define SHM_SIZE (PG_SIZE * 5)
#define BYTES_PER_WORD 4
#define READ_LENGTH_MB 2
#define SHM_WRDMA_BASE 0
#define SHM_WRDMA_CURRENT 1
	int fd_core = gpriv.fd_core;
	void *shm = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd_core, 0);
	if (shm == MAP_FAILED) {
		printf("mmap failed for core\n");
		return;
	}
	int fd = hs_fds[interface];

	/* Map the device write DMA buffer */
	struct pollfd pfd;
	pfd.fd = fd;
	pfd.events = EPOLLIN | EPOLLRDNORM;
	printf("Mapping userspace memory with kernel memory for device\n");
	long read_length_bytes = READ_LENGTH_MB * 1024 * 1024;
	void *mmap_ptr = mmap(NULL, read_length_bytes * 2, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (mmap_ptr == MAP_FAILED) {
		printf("mmap failed for device\n");
		return;
	}
	void *mmap_end = (char *)mmap_ptr + (read_length_bytes * 2);
	int minor = interface;

	unsigned int base_addr_phy = *((uint32_t *)(shm) + ((minor * PG_SIZE) / BYTES_PER_WORD) + SHM_WRDMA_BASE);
	unsigned char *prev_addr = mmap_ptr;

	while(1) {
		int ret = poll(&pfd, 1, -1);
		if (ret < 0) {
			printf("Poll failed\n");
		} else if (pfd.revents & EPOLLIN) {
			unsigned int curr_addr_phy = *((uint32_t *)(shm) + ((minor * PG_SIZE) / BYTES_PER_WORD) + SHM_WRDMA_CURRENT);
			unsigned char *curr_addr = (unsigned char *)mmap_ptr + (curr_addr_phy - base_addr_phy);

			int exit = 0;
			if(prev_addr < curr_addr) {
				int read_len = (char *)curr_addr - (char *)prev_addr;
				exit = consume(arg, prev_addr, read_len, time(NULL));
				if(exit) {
					printf("interface %d: consume want exit!\n", interface);
					break;
				}
			} else {
				int read_len = ((char *)mmap_end - (char *)prev_addr);
				exit = consume(arg, prev_addr, read_len, time(NULL));
				if(exit) {
					printf("interface %d: consume want exit!\n", interface);
					break;
				}
				read_len = ((char *)curr_addr - (char *)mmap_ptr);
				exit = consume(arg, mmap_ptr, read_len, time(NULL));
				if(exit) {
					printf("interface %d: consume want exit!\n", interface);
					break;
				}
			}
			prev_addr = curr_addr;
		}
	}
	munmap(mmap_ptr, read_length_bytes * 2);
	munmap(shm, SHM_SIZE);

	return;
}


void* setup_cmd_channel(void)
{
	/* check /dev/hs0_i2s exist or not */
	if (access(HSI2S_CORE_PATH, F_OK) < 0) {
		printf("file %s not exists\n", HSI2S_CORE_PATH);
		gpriv.ops = shm_ops;
	} else {
		gpriv.ops = drv_ops;
	}
	return gpriv.ops.init();
}
void shutdown_cmd_channel(void *priv)
{
	gpriv.ops.exit(priv);
	memset(&gpriv, 0, sizeof(gpriv));
}
int apptoumd(struct client_cmd *cmd)
{
	return gpriv.ops.apptoumd(cmd);
}
void app_write_to_umd(int interface, unsigned char* data, int data_len, int sync)
{
	return gpriv.ops.app_write_to_umd(interface, data, data_len, sync);
}
void app_read_from_umd(int interface, Funconsume consume, void *arg)
{
	return gpriv.ops.app_read_from_umd(interface, consume, arg);
}
