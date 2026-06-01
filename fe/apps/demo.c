/*
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>
#include <pthread.h>
#include <sys/mman.h>
#include <string.h>
#include <poll.h>
#include <sys/epoll.h>
#include <time.h>
#include <errno.h>
#include <getopt.h>
#include <sched.h>
#include <signal.h>
#include "hsi2s_umd_protocol.h"

/* Returns the size of input file in bytes */
long get_size(FILE *fp)
{
        long n;

        /* Find end of file */
        fseek(fp, 0L, SEEK_END);

        /* Get current position */
        n = ftell(fp);

        /* Seek back */
        fseek(fp, 0L, SEEK_SET);

        /* Return the file size*/
        return n;
}

static int umd_cmd(int interface, enum HSI2S_UMD_CMD_TYPE cmd_type)
{
	struct client_cmd cmd = {0};

	cmd.interface = interface;
	cmd.type = cmd_type;

	return apptoumd(&cmd);
}

static int umd_cmd_param(int interface, enum HSI2S_UMD_CMD_TYPE cmd_type, int param)
{
	struct client_cmd cmd = {0};

	cmd.interface = interface;
	cmd.type = cmd_type;
	cmd.param = param;

	return apptoumd(&cmd);
}

static int umd_cmd_i2s(int interface, struct i2s_params *i2s)
{
	struct client_cmd cmd = {0};

	cmd.interface = interface;
	cmd.type = UMD_I2S_CONFIG_PARAMS;
	cmd.i2s_params = *i2s;

	return apptoumd(&cmd);
}
static int umd_cmd_pcm(int interface, struct pcm_params *pcm)
{
	struct client_cmd cmd = {0};

	cmd.interface = interface;
	cmd.type = UMD_PCM_CONFIG_PARAMS;
	cmd.pcm_params = *pcm;

	return apptoumd(&cmd);
}

static int umd_cmd_tdm(int interface, struct tdm_params *tdm)
{
	struct client_cmd cmd = {0};

	cmd.interface = interface;
	cmd.type = UMD_TDM_CONFIG_PARAMS;
	cmd.tdm_params = *tdm;

	return apptoumd(&cmd);
}

/* Thread params */
struct thread_recv_params {
	int interface;
	unsigned long long read_limit;
	int real_read;
	time_t start_ts;

	/* save output data */
	FILE *fp;
	char *name;
};

static int consume(void *arg, unsigned char *buf, int len, long long ts)
{
        struct thread_recv_params *params = (struct thread_recv_params *)arg;
        time_t rcvd_ts = (time_t)ts;

        int interface = params->interface;
        unsigned long long read_limit = params->read_limit;
	time_t start_ts = params->start_ts;
	int real_read = params->real_read;

        printf("%s: len(%d) %.2x %.2x %.2x %.2x\n", __func__, len, buf[0], buf[1], buf[2], buf[3]);
        if( rcvd_ts < start_ts) {
                printf("dropped previous data(%lu < %lu )\n", rcvd_ts, start_ts);
                return 0;
        }

        if (real_read + len > read_limit) {
                len = read_limit - real_read;
        }

	/* save output data */
	FILE *fp = params->fp;
        fwrite(buf, len, 1, fp);

        real_read += len;
	params->real_read = real_read;

        printf("%s: %d/%lld\n", __func__, real_read, read_limit);
        if(real_read >= read_limit) {
                printf("%s exit\n", __func__);
                return 1;
        }
        return 0;
}

static void *thread_recv(void *arg)
{
        struct thread_recv_params *params = (struct thread_recv_params *)arg;

        int interface = params->interface;
	char *name = params->name;
        if (!name) name = "/data/output.wav";
        params->fp = fopen(name, "w");
	params->real_read = 0;
        params->start_ts = time(NULL);
        printf("%s(%d): start_ts = %lu\n", __func__, __LINE__, params->start_ts);
        app_read_from_umd(interface, consume, arg);
        printf("%s(%d): stop_ts = %lu\n", __func__, __LINE__, time(NULL));
	fclose(params->fp);
        return NULL;
}

int i2s_internal_loop(int interface)
{
	struct i2s_params i2s_params = {
                .bit_clk = 19200000,
                .buffer_ms = 10,
                .bit_depth = 32,
                .spkr_channel_count = 4,
                .mic_channel_count = 4,
                .en_long_rate = 0,
                .long_rate = 0,
        };

	/* 0: HS-I2S mode, 1: HS-PCM mode */
	umd_cmd_param(interface, UMD_LPAIF_MODE, 0);

	umd_cmd_param(interface, UMD_LPAIF_SET_CLOCK, 0x0);

	umd_cmd_i2s(interface, &i2s_params);

	umd_cmd(interface, UMD_LPAIF_RESET);

	/* 0: master, 1: slave */
	umd_cmd_param(interface, UMD_LPAIF_MUXMODE, 0);
	umd_cmd(interface, UMD_LPAIF_INTERNAL_LOOPBACK);

	FILE * fp = fopen("/data/yesterday.wav", "r");
        int buf_size = get_size(fp);
        unsigned char *data = malloc(buf_size);

        //fread(data, 1, 44, fp);
	int count = fread(data, 1, buf_size, fp);
        fclose(fp);


	struct thread_recv_params params = {
		.interface = interface,
		.read_limit = count,
	};
	pthread_t tid;
	int ret = pthread_create(&tid, NULL, thread_recv, &params);

	app_write_to_umd(interface, data, count, 1);

        free(data);

	pthread_join(tid, NULL);
	umd_cmd(interface, UMD_LPAIF_DEINIT_TX);

        printf("%s sent %d\n", __func__, count);

	return 0;
}

int i2s_external_loop(int interface)
{
	struct i2s_params i2s_params = {
                .bit_clk = 19200000,
                .buffer_ms = 10,
                .bit_depth = 32,
                .spkr_channel_count = 4,
                .mic_channel_count = 4,
                .en_long_rate = 0,
                .long_rate = 0,
        };

	/* 0: HS-I2S mode, 1: HS-PCM mode */
	umd_cmd_param(interface, UMD_LPAIF_MODE, 0);

	umd_cmd_param(interface, UMD_LPAIF_SET_CLOCK, 0x0);

	umd_cmd_i2s(interface, &i2s_params);

	umd_cmd(interface, UMD_LPAIF_RESET);

	/* 0: master, 1: slave */
	umd_cmd_param(interface, UMD_LPAIF_MUXMODE, 0);
	umd_cmd(interface, UMD_LPAIF_EXTERNAL_LOOPBACK);

	FILE * fp = fopen("/data/yesterday.wav", "r");
        int buf_size = get_size(fp);
        char *data = malloc(buf_size);

        //fread(data, 1, 44, fp);
	int count = fread(data, 1, buf_size, fp);
        fclose(fp);

	struct thread_recv_params params = {
		.interface = interface,
		.read_limit = count,
	};
	pthread_t tid;
	int ret = pthread_create(&tid, NULL, thread_recv, &params);

	app_write_to_umd(interface, data, count, 1);

        free(data);

	pthread_join(tid, NULL);
	umd_cmd(interface, UMD_LPAIF_DEINIT_TX);

        printf("%s sent %d\n", __func__, count);

	return 0;
}

int i2s_master_slave_loop(int interface_master, int interface_slave)
{
	struct i2s_params i2s_params = {
                .bit_clk = 24567000,
                .buffer_ms = 10,
                .bit_depth = 32,
                .spkr_channel_count = 4,
                .mic_channel_count = 4,
                .en_long_rate = 0,
                .long_rate = 0,
        };

	/* 0: HS-I2S mode, 1: HS-PCM mode */
	umd_cmd_param(interface_master, UMD_LPAIF_MODE, 0);
	umd_cmd_param(interface_slave, UMD_LPAIF_MODE, 0);

	umd_cmd_param(interface_master, UMD_LPAIF_SET_CLOCK, 0x500 | 0x09);

	umd_cmd_i2s(interface_master, &i2s_params);
	umd_cmd_i2s(interface_slave, &i2s_params);

	umd_cmd(interface_master, UMD_LPAIF_RESET);
	umd_cmd(interface_slave, UMD_LPAIF_RESET);

	/* set master mode: 0 master, 1 slave */
	umd_cmd_param(interface_master, UMD_LPAIF_MUXMODE, 0);
	umd_cmd_param(interface_slave, UMD_LPAIF_MUXMODE, 1);

	umd_cmd_param(interface_master, UMD_LPAIF_SET_SLAVE, interface_slave);
	umd_cmd(interface_slave, UMD_LPAIF_MIC);
	umd_cmd(interface_master, UMD_LPAIF_SPEAKER);

	FILE * fp = fopen("/data/yesterday.wav", "r");
        int buf_size = get_size(fp);
        char *data = malloc(buf_size);

        //fread(data, 1, 44, fp);
	int count = fread(data, 1, buf_size, fp);
        fclose(fp);

	/* tx from master interface, rx from slave interface */
	struct thread_recv_params params = {
		.interface = interface_slave,
		.read_limit = count,
	};
	pthread_t tid;
	int ret = pthread_create(&tid, NULL, thread_recv, &params);

	app_write_to_umd(interface_master, data, count, 1);
        free(data);

	pthread_join(tid, NULL);
	umd_cmd(interface_master, UMD_LPAIF_DEINIT_TX);
	umd_cmd(interface_master, UMD_LPAIF_RESET);
	umd_cmd(interface_slave, UMD_LPAIF_RESET);

        printf("%s sent %d\n", __func__, count);
	return 0;
}

int i2s_rx(int interface)
{
	struct i2s_params i2s_params = {
                .bit_clk = 24576000,
                .buffer_ms = 10,
                .bit_depth = 32,
                .spkr_channel_count = 4,
                .mic_channel_count = 4,
                .en_long_rate = 0,
                .long_rate = 0,
        };
	umd_cmd_param(interface, UMD_LPAIF_MODE, 0);
	umd_cmd_i2s(interface, &i2s_params);
	umd_cmd(interface, UMD_LPAIF_RESET);
	umd_cmd_param(interface, UMD_LPAIF_MUXMODE, 1);
	umd_cmd(interface, UMD_LPAIF_NORMAL_MODE);

	FILE * fp = fopen("/data/yesterday.wav", "r");
        int buf_size = get_size(fp);
        fclose(fp);

	struct thread_recv_params params = {
		.interface = interface,
		.read_limit = buf_size,
	};
	pthread_t tid;
	int ret = pthread_create(&tid, NULL, thread_recv, &params);
	pthread_join(tid, NULL);
	umd_cmd(interface, UMD_LPAIF_RESET);

	return 0;
}

int i2s_tx(int interface)
{
	struct i2s_params i2s_params = {
                .bit_clk = 24576000,
                .buffer_ms = 10,
                .bit_depth = 32,
                .spkr_channel_count = 4,
                .mic_channel_count = 4,
                .en_long_rate = 0,
                .long_rate = 0,
        };

	umd_cmd_param(interface, UMD_LPAIF_MODE, 0);
	umd_cmd_param(interface, UMD_LPAIF_SET_CLOCK, 0x500 | 0x09);
	umd_cmd_i2s(interface, &i2s_params);
	umd_cmd(interface, UMD_LPAIF_RESET);
	umd_cmd_param(interface, UMD_LPAIF_MUXMODE, 0);
	umd_cmd(interface, UMD_LPAIF_SPEAKER);

	FILE * fp = fopen("/data/yesterday.wav", "r");
        int buf_size = get_size(fp);
        char *data = malloc(buf_size);

        //fread(data, 1, 44, fp);
	int count = fread(data, 1, buf_size, fp);
        fclose(fp);

	app_write_to_umd(interface, data, count, 1);
        free(data);

	umd_cmd(interface, UMD_LPAIF_DEINIT_TX);
	umd_cmd(interface, UMD_LPAIF_RESET);

	return 0;
}

int pcm_internal_loop(int interface)
{
	struct pcm_params pcm_params = {
		.bit_clk = 19200000,
		.buffer_ms = 10,
		.rate = 2,
		.sync_src = 1, //PCM internal frame sync
		.aux_mode = 0, //PCM short frame sync
		.rpcm_width = 1,
		.tpcm_width = 1,
	};

	struct tdm_params tdm_params = {
		.sync_delay = 1,
		.tpcm_width = 16,
		.rpcm_width = 16,
		.rate = 16,
		.en_diff_sample_width = 0,
	};

	/* 0: HS-I2S mode, 1: HS-PCM mode */
	umd_cmd_param(interface, UMD_LPAIF_MODE, 1);

	umd_cmd_param(interface, UMD_LPAIF_SET_CLOCK, 0x0);

	umd_cmd_pcm(interface, &pcm_params);
	umd_cmd_tdm(interface, &tdm_params);

	umd_cmd(interface, UMD_LPAIF_RESET);

	/* 0: master, 1: slave */
	umd_cmd_param(interface, UMD_LPAIF_MUXMODE, 0);

	/* Data lane direction in PCM mode : 0 -> SINGLE LANE, 1 -> MULTI LANE RX, 2 -> MULTI LANE TX */
	umd_cmd_param(interface, UMD_PCM_CONFIG_LANE, 0);

	umd_cmd(interface, UMD_LPAIF_INTERNAL_LOOPBACK);

	FILE * fp = fopen("/data/yesterday.wav", "r");
        int buf_size = get_size(fp);
        unsigned char *data = malloc(buf_size);

        //fread(data, 1, 44, fp);
	int count = fread(data, 1, buf_size, fp);
        fclose(fp);


	struct thread_recv_params params = {
		.interface = interface,
		.read_limit = count,
	};
	pthread_t tid;
	int ret = pthread_create(&tid, NULL, thread_recv, &params);

	app_write_to_umd(interface, data, count, 1);

        free(data);

	pthread_join(tid, NULL);
	umd_cmd(interface, UMD_LPAIF_DEINIT_TX);

        printf("%s sent %d\n", __func__, count);

	return 0;
}

int pcm_external_loop(int interface)
{
	struct pcm_params pcm_params = {
		.bit_clk = 19200000,
		.buffer_ms = 10,
		.rate = 2,
		.sync_src = 1, //PCM internal frame sync
		.aux_mode = 0, //PCM short frame sync
		.rpcm_width = 1,
		.tpcm_width = 1,
	};

	struct tdm_params tdm_params = {
		.sync_delay = 1,
		.tpcm_width = 16,
		.rpcm_width = 16,
		.rate = 16,
		.en_diff_sample_width = 0,
	};

	/* 0: HS-I2S mode, 1: HS-PCM mode */
	umd_cmd_param(interface, UMD_LPAIF_MODE, 1);

	umd_cmd_param(interface, UMD_LPAIF_SET_CLOCK, 0x0);

	umd_cmd_pcm(interface, &pcm_params);
	umd_cmd_tdm(interface, &tdm_params);

	umd_cmd(interface, UMD_LPAIF_RESET);

	/* 0: master, 1: slave */
	umd_cmd_param(interface, UMD_LPAIF_MUXMODE, 0);

	/* Data lane direction in PCM mode : 0 -> SINGLE LANE, 1 -> MULTI LANE RX, 2 -> MULTI LANE TX */
	umd_cmd_param(interface, UMD_PCM_CONFIG_LANE, 0);

	umd_cmd(interface, UMD_LPAIF_EXTERNAL_LOOPBACK);

	FILE * fp = fopen("/data/yesterday.wav", "r");
        int buf_size = get_size(fp);
        unsigned char *data = malloc(buf_size);

        //fread(data, 1, 44, fp);
	int count = fread(data, 1, buf_size, fp);
        fclose(fp);


	struct thread_recv_params params = {
		.interface = interface,
		.read_limit = count,
	};
	pthread_t tid;
	int ret = pthread_create(&tid, NULL, thread_recv, &params);

	app_write_to_umd(interface, data, count, 1);

        free(data);

	pthread_join(tid, NULL);
	umd_cmd(interface, UMD_LPAIF_DEINIT_TX);

        printf("%s sent %d\n", __func__, count);

	return 0;
}

int pcm_master_slave_loop(int interface_master, int interface_slave)
{
        printf("%s not support!\n", __func__);
	return 0;
}

int pcm_rx(int interface)
{
	struct pcm_params pcm_params = {
		.bit_clk = 12288000,
		.buffer_ms = 10,
		.rate = 2,
		.sync_src = 0, //PCM external frame sync
		.aux_mode = 0, //PCM short frame sync
		.rpcm_width = 1,
		.tpcm_width = 1,
	};

	struct tdm_params tdm_params = {
		.sync_delay = 1,
		.tpcm_width = 32,
		.rpcm_width = 32,
		.rate = 64,
		.en_diff_sample_width = 0,
	};
	/* 0: HS-I2S mode, 1: HS-PCM mode */
	umd_cmd_param(interface, UMD_LPAIF_MODE, 1);

	umd_cmd_pcm(interface, &pcm_params);
	umd_cmd_tdm(interface, &tdm_params);

	umd_cmd(interface, UMD_LPAIF_RESET);

	/* 0: master, 1: slave */
	umd_cmd_param(interface, UMD_LPAIF_MUXMODE, 1);

	/* Data lane direction in PCM mode : 0 -> SINGLE LANE, 1 -> MULTI LANE RX, 2 -> MULTI LANE TX */
	umd_cmd_param(interface, UMD_PCM_CONFIG_LANE, 1);

	umd_cmd(interface, UMD_LPAIF_NORMAL_MODE);

	FILE * fp = fopen("/data/yesterday.wav", "r");
        int buf_size = get_size(fp);
        fclose(fp);

	struct thread_recv_params params = {
		.interface = interface,
		.read_limit = buf_size,
	};
	pthread_t tid;
	int ret = pthread_create(&tid, NULL, thread_recv, &params);
	pthread_join(tid, NULL);
	umd_cmd(interface, UMD_LPAIF_RESET);

	return 0;
}

int pcm_tx(int interface)
{
        printf("%s not support!\n", __func__);
	return 0;
}

extern void* setup_cmd_channel(void);
extern void shutdown_cmd_channel(void*);
int main(int argc, char *argv[])
{
	int index = 2;
	printf("0 - I2S: Normal Rx\n"
			"1 - I2S: Normal Tx\n"
			"2 - I2S: Internal loopback\n"
			"3 - I2S: External loopback\n"
			"4 - I2S: External master-slave loopback\n"
			"10 - PCM: Normal Rx\n"
			"11 - PCM: Normal Tx\n"
			"12 - PCM: Internal loopback\n"
			"13 - PCM: External loopback\n"
			"14 - PCM: External master-slave loopback\n");

	if (argc >= 2) {
		index = atoi(argv[1]);
		printf("index = %d\n", index);
	}

	void *priv = setup_cmd_channel();
	switch (index) {
		case 0: i2s_rx(0);
			break;
		case 1: i2s_tx(1);
			break;
		case 2: i2s_internal_loop(0);
			break;
		case 3: i2s_external_loop(0);
			break;
		case 4: i2s_master_slave_loop(0, 1);
			break;
		case 10: pcm_rx(0);
			break;
		case 11: pcm_tx(0);
			break;
		case 12: pcm_internal_loop(0);
			break;
		case 13: pcm_external_loop(0);
			break;
		case 14: pcm_master_slave_loop(0, 1);
			break;
		default:
			printf("index = %d, not support yet!\n", index);
	}
	shutdown_cmd_channel(priv);

	return 0;
}
