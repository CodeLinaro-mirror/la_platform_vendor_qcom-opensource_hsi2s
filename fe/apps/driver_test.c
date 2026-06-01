/*
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
SPDX-License-Identifier: BSD-3-Clause-Clear
*/

/*
 * Test app for hs-i2s driver
 */

#define _GNU_SOURCE

/* Headers */
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
//#include "hsi2s_common.h"
#include "hsi2s_umd_protocol.h"

/* Macros */
#define BYTES_PER_WORD 4
#define READ_LENGTH_MB 2
#define READ_LENGTH_WORDS (READ_LENGTH_MB * 1024 * 1024) / BYTES_PER_WORD
#define READ_LIMIT 2048000 * 8 * 60 /* 60s of ramp data */
#define SRC_DIGITAL_PLL 0x500
#define BILLION 1000000000L
#define DAB_TUNER_COUNT 3
#define PG_SIZE 4096
#define SHM_SIZE (PG_SIZE * 5)
#define SHM_WRDMA_BASE 0
#define SHM_WRDMA_CURRENT 1
#define BBIQ_ALLOWED_ERROR  2
#define BBIQ_DEBUG_ERROR_MOD  1

/* Operation mode of the test utility */
enum operation_mode {
	NORMAL_RX,
	NORMAL_TX,
	INTERNAL_LB,
	EXTERNAL_LB_MASTER,
	EXTERNAL_LB_MASTER_SLAVE,
	SET_MUXMODE,
	CONFIG_M_CLK,
	CONFIG_I2S_PARAMS,
	CONFIG_PCM_PARAMS,
	CONFIG_TDM_PARAMS,
	CONFIG_LPAIF_MODE,
	CONFIG_PCM_LANE,
	TOGGLE_BIT_CLK,
	CONFIG_DAB_MRC
};


/* Global variables */
unsigned long long read_limit;
long mmap_len;
uint8_t minor_num;
long ch0_error_cnt;
long ch1_error_cnt;
long total_samples;
int test_previous;
int is_ramp;
int is_tx_active;

/* Prints the usage information */
static void help(void)
{
	printf("OPERATIONAL MODES:\n\n 0 - Normal Rx\n 1 - Normal Tx*\n 2 - Internal loopback\n 3 - External loopback on master*\n"
	       " 4 - External loopback on master-slave*\n 5 - Set master/slave mode*\n 6 - Configure master clock*\n"
	       " 7 - Configure I2S params\n 8 - Configure PCM params\n 9 - Configure TDM params\n"
	       " 10 - Configure LPAIF mode\n 11 - Set PCM lane configuration\n 12 - Configure bit clock*\n"
		   " 13 - Configure DAB MRC mode**\n");
	printf("* Supported only on SA8155/SA8195\n");
	printf("** Supported only on SA6155\n\n");
	printf("USAGE:\n\n");
	printf("NORMAL Rx:\n");
	printf("hsi2s_test --op_mode=0 --dev=<> --output=<> --bit_clock_hz=<> --data_buffer_ms=<> [--dma_buffer_length=<>] [--set_cpu_affinity] [--ramp]\n\n");
	printf("NORMAL Tx:\n");
	printf("hsi2s_test --op_mode=1 --dev=<> --input=<>\n\n");
	printf("INTERNAL LOOPBACK:\n");
	printf("hsi2s_test --op_mode=2 --dev=<> --output=<> --input=<> [--dma_buffer_length=<>]\n\n");
	printf("EXTERNAL LOOPBACK ON MASTER:\n");
	printf("hsi2s_test --op_mode=3 --dev=<> --output=<> --input=<> [--dma_buffer_length=<>]\n\n");
	printf("EXTERNAL LOOPBACK BETWEEN MASTER AND SLAVE INTERFACES:\n");
	printf("hsi2s_test --op_mode=4 --dev=<> --s_dev=<> --output=<> --input=<> [--dma_buffer_length=<>]\n\n");
	printf("SET MASTER/SLAVE MODE:\n");
	printf("hsi2s_test --op_mode=5 --dev=<> --master/slave\n\n");
	printf("CONFIGURE MASTER CLOCK:\n");
	printf("hsi2s_test --op_mode=6 --dev=<> --clk_src=<> --divide_by=<>\n\n");
	printf("CONFIGURE I2S PARAMETERS:\n");
	printf("hsi2s_test --op_mode=7 --dev=<> --bit_clock_hz=<> --data_buffer_ms=<> --bit_depth=<> --spkr_ch=<> --mic_ch=<>  [--long_rate=<>]\n\n");
	printf("CONFIGURE PCM PARAMETERS:\n");
	printf("hsi2s_test --op_mode=8 --dev=<> --bit_clock_hz=<> --data_buffer_ms=<> --pcm_rate=<> --pcm_sync_int/pcm_sync_ext --pcm_sync_short/pcm_sync_long --rpcm_width=<> --tpcm_width=<>\n\n");
	printf("CONFIGURE TDM PARAMETERS:\n");
	printf("hsi2s_test --op_mode=9 --dev=<device file> --tdm_sync_delay=<> --tdm_tpcm_width=<> --tdm_rpcm_width=<> --tdm_rate=<> [--tdm_tpcm_sample_width=<> --tdm_rpcm_sample_width=<>]\n\n");
	printf("SET I2S/PCM MODE:\n");
	printf("hsi2s_test --op_mode=10 --dev=<> --i2s/pcm\n\n");
	printf("SET PCM LANE CONFIGURATION:\n");
	printf("hsi2s_test --op_mode=11 --dev=<> --lane_config=<>\n\n");
	printf("TOGGLE BIT CLOCK:\n");
	printf("hsi2s_test --op_mode=12 --dev=<>\n\n");
	printf("CONFIGURE DAB MRC MODE:\n");
	printf("hsi2s_test --op_mode=13 --output_a=<> --output_b=<> --bit_clock_hz=<> --data_buffer_ms=<> --target_type=<> [--dma_buffer_length=<>] [--set_cpu_affinity]\n\n");
	printf("OPTIONS:\n\n");
	printf("--dev \n\t Device file : /dev/hs0_i2s | /dev/hs1_i2s | /dev/hs2_i2s\n");
	printf("--s_dev \n\t Slave device file used in master-slave loopback: /dev/hs0_i2s | /dev/hs1_i2s | /dev/hs2_i2s\n");
	printf("--output \n\t Path to the output file, to store the data read from the HS-I2S interface\n");
	printf("--output_a \n\t Path to the output file, to store the data read from tuner A\n");
	printf("--output_b \n\t Path to the output file, to store the data read from tuner B\n");
	printf("--input \n\t Path to the input file, to fetch data written to the HS-I2S interface\n");
	printf("--dma_buffer_length \n\t DMA buffer length in MB (4MB by default) (Optional)\n");
	printf("--master \n\t HS-I2S master\n");
	printf("--slave \n\t HS-I2S slave\n");
	printf("--clk_src \n\t Clock source : 0 -> CXO(19.2 MHz) 1 -> DIGITAL PLL(122.88 MHz)\n");
	printf("--divide_by \n\t 0 -> Bypass, 1 -> Div-1, 2 -> Div-1.5, 3 -> Div-2, 4 -> Div-2.5, ..... 31 -> Div-16\n");
	printf("--bit_clock_hz \n\t Bit clock freqeuncy in Hertz\n");
	printf("--data_buffer_ms \n\t Periodic length of data buffer in milli seconds\n");
	printf("--bit_depth \n\t Bit depth in I2S mode : 16 | 24 | 25 | 32\n");
	printf("--spkr_ch \n\t Speaker channel count in I2S mode  : 1 | 2 | 4\n");
	printf("--mic_ch \n\t Mic channel count in I2S mode : 1 | 2 |4\n");
	printf("--long_rate \n\t New WS rate when long rate is enabled, allows WS rate to be larger than bit depth\n");
	printf("--pcm_rate \n\t Frame size : 0 -> 8 bits, 1 -> 16 bits, 2 -> 32 bits, 3 -> 64 bits, 4 -> 128 bits, 5 -> 256 bits\n");
	printf("--pcm_sync_ext \n\t External frame sync\n");
	printf("--pcm_sync_int \n\t Internal frame sync\n");
	printf("--pcm_sync_short \n\t Short frame sync (Pulse)\n");
	printf("--pcm_sync_long \n\t Long frame sync (50 percent duty cycle)\n");
	printf("--rpcm_width \n\t PCM receive slot size : 0 -> 8 bits, 1 -> 16 bits\n");
	printf("--tpcm_width \n\t PCM transmit slot size : 0 -> 8 bits, 1 -> 16 bits\n");
	printf("--tdm_sync_delay \n\t Data delay in bit cycles wrt start of frame sync : 0 -> 2 CYCLE DELAY, 1 -> 1 CYCLE DELAY, 2 -> 0 CYCLE DELAY\n");
	printf("--tdm_tpcm_width \n\t TDM transmit slot size in bits (maximum 32)\n");
	printf("--tdm_rpcm_width \n\t TDM receive slot size in bits (maximum 32)\n");
	printf("--tdm_rate \n\t TDM frame size in bits (maximum 512)\n");
	printf("--tdm_tpcm_sample_width \n\t TDM TPCM sample width in bits (maximum 32) (Optional)\n");
	printf("--tdm_rpcm_sample_width \n\t TDM RPCM sample width in bits (maximum 32) (Optional)\n");
	printf("--i2s \n\t LPAIF in HS-I2S mode\n");
	printf("--pcm \n\t LPAIF in HS-PCM mode\n");
	printf("--lane_config \n\t Data lane direction in PCM mode : 0 -> SINGLE LANE, 1 -> MULTI LANE RX, 2 -> MULTI LANE TX\n");
	printf("--set_cpu_affinity \n\t Set CPU affinity to one of the available high cores\n");
	printf("--target_type \n\t 0 -> 6155 1-> 8155/8195\n");
	printf("--ramp \n\t Trigger ramp analysis on output\n");
	printf("--normal_read \n\t Use normal read thread. By default, fast read thread is selected\n\n");
}

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

/* Function to calculate periodic interrupt length */
uint32_t get_periodic_length(uint32_t bit_clk, uint32_t interval)
{
	/*
	 * Formula to calculate
	 * Bit clock -> 'm' Hz
	 * Bits per sec  = m
	 * Bits per msec = m * (10^(-3))
	 * Bytes per msec = (m * (10^(-3))) / 8 = m / 8000
	 * Bytes per 'k' msec = k * (m / 8000)
	 */
	return (((unsigned long long)interval * bit_clk) / 8000);
}


/* Signal handler */
void signal_handler(int sig_id)
{
	int ret = 0;
	if (sig_id == SIGINT) {
		if (is_tx_active) {
			int interface = minor_num;
			printf("Disabling Tx on read DMA channel\n");
			ret = umd_cmd(interface, UMD_LPAIF_DEINIT_TX);
			if (ret < 0) {
				printf("Failed to stop Tx on hsi2s device\n");
			}
			is_tx_active = 0;
		}
		exit(0);
	}
}

extern void* setup_cmd_channel(void);
extern void shutdown_cmd_channel(void*);

int main(int argc, char **argv)
{
	FILE *fd_read_ip = NULL;
	long read_length_bytes = 0;
	long read_length_words = 0;
	enum operation_mode mode = 0;
	struct i2s_params *i_params = NULL;
	struct pcm_params *p_params = NULL;
	struct tdm_params *t_params = NULL;
	long wav_samples = 0;
	long no_words = 0;
	long w_len;
	int32_t *wav_data = NULL;
	int32_t temp_data;
	int i;
	int slave = 0;
	uint32_t reg_val;
	pthread_t tid;
	uint8_t mux_mode = 0;
	uint8_t clk_source = 0;
	uint32_t divide_by = 0;
	uint32_t bit_clk = 0;
	uint32_t buffer_ms = 10;
	uint32_t bit_depth = 0;
	uint8_t spkr_channel_count = 0;
	uint8_t mic_channel_count = 0;
	uint32_t long_rate = 0;
	uint8_t pcm_rate = 0;
	uint8_t pcm_sync_src = 1;
	uint8_t pcm_aux_mode = 0;
	uint8_t pcm_tpcm_width = 0;
	uint8_t pcm_rpcm_width = 0;
	uint8_t tdm_sync_delay = 1;
	uint32_t tdm_tpcm_width = 0;
	uint32_t tdm_rpcm_width = 0;
	uint32_t tdm_rate = 0;
	uint32_t tdm_tpcm_sample_width = 0;
	uint32_t tdm_rpcm_sample_width = 0;
	uint8_t lpaif_mode = 0;
	uint8_t lane_config = 0;
	uint8_t set_affinity = 0;
	int ret = 0;
	int opt;
	const char *short_opt = ":a:b:c:d:e:f:g:h:ijk:l:m:n:o:p:qrstu:v:w:x:y:z:A:B:CDE:FGH:I:J:KLM";
	cpu_set_t cpuset;
	uint8_t target_type = 0;
	char *hs_dev[DAB_TUNER_COUNT] = {"/dev/hs0_i2s","/dev/hs1_i2s"};
	pthread_t tid_dab[DAB_TUNER_COUNT];
	int use_normal_read = 0;
	struct sigaction sa;

	struct option   long_opt[] =
	{
		{"op_mode", required_argument, NULL, 'a'},
		{"dev", required_argument, NULL, 'b'},
		{"s_dev", required_argument, NULL, 'c'},
		{"output", required_argument, NULL, 'd'},
		{"input", required_argument, NULL, 'e'},
		{"bit_clock_hz", required_argument, NULL, 'f'},
		{"data_buffer_ms", required_argument, NULL, 'g'},
		{"dma_buffer_length", required_argument, NULL, 'h'},
		{"master", no_argument, NULL, 'i'},
		{"slave", no_argument, NULL, 'j'},
		{"clk_src", required_argument, NULL, 'k'},
		{"divide_by", required_argument, NULL, 'l'},
		{"bit_depth", required_argument, NULL, 'm'},
		{"spkr_ch", required_argument, NULL, 'n'},
		{"mic_ch", required_argument, NULL, 'o'},
		{"long_rate", required_argument, NULL, 'p'},
		{"pcm_rate", required_argument, NULL, 'q'},
		{"pcm_sync_int", no_argument, NULL, 'r'},
		{"pcm_sync_ext", no_argument, NULL, 's'},
		{"pcm_sync_long", no_argument, NULL, 't'},
		{"pcm_sync_short", no_argument, NULL, 'u'},
		{"tpcm_width", required_argument, NULL, 'v'},
		{"rpcm_width", required_argument, NULL, 'w'},
		{"tdm_sync_delay", required_argument, NULL, 'x'},
		{"tdm_tpcm_width", required_argument, NULL, 'y'},
		{"tdm_rpcm_width", required_argument, NULL, 'z'},
		{"tdm_rate", required_argument, NULL, 'A'},
		{"tdm_tpcm_sample_width", required_argument, NULL, 'B'},
		{"tdm_rpcm_sample_width", required_argument, NULL, 'C'},
		{"i2s", no_argument, NULL, 'D'},
		{"pcm", no_argument, NULL, 'E'},
		{"lane_config", required_argument, NULL, 'F'},
		{"help", no_argument, NULL, 'G'},
		{"set_cpu_affinity", no_argument, NULL, 'H'},
		{"target_type", required_argument, NULL, 'I'},
		{"output_a", required_argument, NULL, 'J'},
		{"output_b", required_argument, NULL, 'K'},
		{"ramp", no_argument, NULL, 'L'},
		{"normal_read", no_argument, NULL, 'M'},
		{NULL, 0, NULL, 0}
	};

	/* Initializing to default macros and use the macros if no size specified by user */
	read_length_bytes = READ_LENGTH_MB * 1024 * 1024;
	read_length_words = READ_LENGTH_WORDS;
	mmap_len = read_length_bytes;

	/* Set the signal handler */
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = &signal_handler;
	sigaction(SIGINT, &sa, NULL);


	char *inputname = NULL;
	char *outputname = NULL;
	char *outputname_a = NULL;
	char *outputname_b = NULL;
	void *priv = NULL;
	while((opt = getopt_long(argc, argv, short_opt, long_opt, NULL)) != -1)
	{
		switch(opt)
		{
			case 'a':
				/* Operational mode */
				mode = atoi(optarg);
				if (mode > CONFIG_DAB_MRC) {
					printf("Undefined mode\n");
					help();
					ret = -1;
					goto exit_app;
				}
				break;
			case 'b':
				/* Device file */
				minor_num = optarg[7] - '0';
				break;
			case 'c':
				/* Slave device file */
				slave = optarg[7] - '0';
				break;
			case 'd':
				/* Output file */
				outputname = optarg;
				break;
			case 'e':
				/* Input file */
				inputname = optarg;
				if (access(inputname, F_OK) < 0) {
					printf("file %s not exists\n", inputname);
					help();
					ret = -1;
					goto exit_app;
				}
				break;
			case 'f':
				/* Bit clock rate */
				bit_clk = atoi(optarg);
				break;
			case 'g':
				/* Interrupt interval */
				buffer_ms = atoi(optarg);
				break;
			case 'h':
				/* DMA buffer length */
				read_length_bytes = (atoi(optarg) * 1024 * 1024) / 2;
				read_length_words = read_length_bytes/BYTES_PER_WORD;
				mmap_len = read_length_bytes;
				break;
			case 'i':
				/* Master mode */
				mux_mode = 0;
				break;
			case 'j':
				/* Slave mode */
				mux_mode = 1;
				break;
			case 'k':
				/* Clock source */
				clk_source = atoi(optarg);
				if (clk_source > 1) {
					printf("Undefined source\n");
					help();
					ret = -1;
					goto exit_app;
				}
				break;
			case 'l':
				/* Clock division factor */
				divide_by = atoi(optarg);
				if (divide_by > 31) {
					printf("Undefined division\n");
					help();
					ret = -1;
					goto exit_app;
				}
				break;
			case 'm':
				/* Bit depth */
				bit_depth = atoi(optarg);
				break;
			case 'n':
				/* Speaker channel count */
				spkr_channel_count = atoi(optarg);
				break;
			case 'o':
				/* Mic channel count */
				mic_channel_count = atoi(optarg);
				break;
			case 'p':
				/* Long rate */
				long_rate = atoi(optarg);
				break;
			case 'q':
				/* PCM frame size */
				pcm_rate = atoi(optarg);
				break;
			case 'r':
				/* PCM internal frame sync */
				pcm_sync_src = 1;
				break;
			case 's':
				/* PCM external frame sync */
				pcm_sync_src = 0;
				break;
			case 't':
				/* PCM long frame sync */
				pcm_aux_mode = 1;
				break;
			case 'u':
				/* PCM short frame sync */
				pcm_aux_mode = 0;
				break;
			case 'v':
				/* PCM Tx slot size */
				pcm_tpcm_width = atoi(optarg);
				break;
			case 'w':
				/* PCM Rx slot size */
				pcm_rpcm_width = atoi(optarg);
				break;
			case 'x':
				/* TDM sync delay */
				tdm_sync_delay = atoi(optarg);
				break;
			case 'y':
				/* TDM Tx slot size */
				tdm_tpcm_width = atoi(optarg);
				break;
			case 'z':
				/* TDM Rx slot size */
				tdm_rpcm_width = atoi(optarg);
				break;
			case 'A':
				/* TDM frame size */
				tdm_rate = atoi(optarg);
				break;
			case 'B':
				/* TDM Tx sample width */
				tdm_tpcm_sample_width = atoi(optarg);
				break;
			case 'C':
				/* TDM Rx sample width */
				tdm_rpcm_sample_width = atoi(optarg);
				break;
			case 'D':
				/* HS-I2S mode */
				lpaif_mode = 0;
				break;
			case 'E':
				/* HS-PCM mode */
				lpaif_mode = 1;
				break;
			case 'F':
				/* PCM data lane configuration */
				lane_config = atoi(optarg);
				break;
			case 'G':
				/* Print usage */
				help();
				goto exit_app;
			case 'H':
				/* Set CPU affinity */
				set_affinity = 1;
				break;
			case 'I':
				/* Check target type */
				target_type = atoi(optarg);
				break;
			case 'J':
				/* Output file a */
				outputname_a = optarg;
				break;
			case 'K':
				/* Output file b */
				outputname_b = optarg;
				break;
			case 'L':
				/* Trigger ramp analysis */
				is_ramp = 1;
				break;
			case 'M':
				/* Use normal read */
				use_normal_read = 1;
				break;
			case ':':
				/* Value missing for option */
				printf("Option needs a value. Check --help for usage.\n");
				goto exit_app;
			case '?':
				/* Unknown option */
				printf("Unknown option entered. Check --help for usage.\n");
				break;
			default:
				break;
		}
	}

	/* optind is for the extra arguments which are not parsed */
	for(; optind < argc; optind++) {
		printf("Extra arguments: %s\n", argv[optind]);
	}

	priv = setup_cmd_channel();
	switch (mode)
	{
		case NORMAL_RX:
		{
			int interface = minor_num;
			/* Operation mode : Normal mode data reception */
			if (argc < 6) {
				help();
				ret = -1;
				break;
			}
			/* Set CPU affinity to one of the available high cores */
			if (set_affinity) {
				printf("Setting CPU affinity \n");
				CPU_ZERO(&cpuset);
				for (i = 6; i < 8; i++) {
					CPU_SET(i, &cpuset);
					ret = sched_setaffinity(getpid(), sizeof(cpu_set_t), &cpuset);
					if (ret) {
						printf("Cannot set CPU affinity on CPU[%d]\n", i);
						CPU_ZERO(&cpuset);
						continue;
					}
					printf("CPU affinity set on CPU[%d]\n", i);
					break;
				}
			}
			printf("Setting normal mode \n");
			ret = umd_cmd(interface, UMD_LPAIF_RESET);
			if (ret < 0) {
				printf("Failed to reset the hsi2s device\n");
				break;
			}
			ret = umd_cmd(interface, UMD_LPAIF_NORMAL_MODE);
			if (ret < 0) {
				printf("Failed to configure normal operation on the hsi2s device\n");
				break;
			}

			struct thread_recv_params params = {
				.interface = interface,
				.read_limit = READ_LIMIT,
				.name = outputname,
			};
			pthread_t tid;
			int ret = pthread_create(&tid, NULL, thread_recv, &params);

			printf("Joining threads\n");
			pthread_join(tid,NULL);
			printf("Threads joined \n");
			umd_cmd(interface, UMD_LPAIF_RESET);
			break;
		}
		case NORMAL_TX:
		{
			int interface = minor_num;
			/* Operation mode : Normal Tx */
			if (argc < 4) {
				help();
				ret = -1;
				break;
			}
			printf("Setting Tx on master\n");
			ret = umd_cmd(interface, UMD_LPAIF_RESET);
			if (ret < 0) {
				printf("Failed to reset the hsi2s device\n");
				break;
			}
			ret = umd_cmd_param(interface, UMD_LPAIF_MUXMODE, 0);
			if (ret < 0) {
				printf("Failed to set master mode\n");
				break;
			}
			ret = umd_cmd(interface, UMD_LPAIF_SPEAKER);
			if (ret < 0) {
				printf("Failed to configure speaker\n");
				break;
			}
			is_tx_active = 1;

			printf("Reading i/p file...\n");
			/* Check for valid inputs */
			fd_read_ip = fopen(inputname, "r");
			if (!fd_read_ip) {
				printf("Unable to open input file, exiting...\n");
				ret = -1;
				break;
			}
			printf("Calculating i/p file size...\n");
			wav_samples = get_size(fd_read_ip);
			printf("Input file size in bytes: %ld\n", wav_samples);
			wav_data = (int32_t *) malloc(wav_samples);
			if (!wav_data) {
				printf("Failed to allocate transmit data buffer\n");
				fclose(fd_read_ip);
				fd_read_ip = NULL;
				ret = -ENOMEM;
				break;
			}

			/* Copy data from wav file into memory */
			int count = fread(wav_data, 1, wav_samples, fd_read_ip);
			fclose(fd_read_ip);
			fd_read_ip = NULL;

			app_write_to_umd(interface, wav_data, count, 1);
			free(wav_data);
			wav_data = NULL;

			/* Disable transmission */
			printf("Disabling Tx on read DMA channel\n");
			ret = umd_cmd(interface, UMD_LPAIF_DEINIT_TX);
			if (ret < 0) {
				printf("Failed to stop Tx on hsi2s device\n");
				break;
			}
			umd_cmd(interface, UMD_LPAIF_RESET);
			is_tx_active = 0;
			break;
		}
		case INTERNAL_LB:
		{
			int interface = minor_num;
			/* Operation mode : Internal loopback */
			if (argc < 5) {
				help();
				ret = -1;
				break;
			}
			printf("Setting internal loopback operation \n");
			ret = umd_cmd(interface, UMD_LPAIF_RESET);
			if (ret < 0) {
				printf("Failed to reset the hsi2s device\n");
				break;
			}
			ret = umd_cmd(interface, UMD_LPAIF_INTERNAL_LOOPBACK);
			if (ret < 0) {
				printf("Failed to trigger internal loopback\n");
				break;
			}

			is_tx_active = 1;

			printf("Reading i/p file...\n");
			fd_read_ip = fopen(inputname, "r");
			if (!fd_read_ip) {
				printf("Unable to open input file, exiting...\n");
				ret = -1;
				break;
			}
			printf("Calculating i/p file size...\n");
			wav_samples = get_size(fd_read_ip);
			printf("Input file size in bytes: %ld\n", wav_samples);
			wav_data = (int32_t *) malloc(wav_samples);
			if (!wav_data) {
				printf("Failed to allocate transmit data buffer\n");
				fclose(fd_read_ip);
				fd_read_ip = NULL;
				ret = -ENOMEM;
				break;
			}

			/* Copy data from wav file into memory */
			int count = fread(wav_data, 1, wav_samples, fd_read_ip);
			fclose(fd_read_ip);
			fd_read_ip = NULL;

			struct thread_recv_params params = {
				.interface = interface,
				.read_limit = count,
				.name = outputname,
			};
			pthread_t tid;
			int ret = pthread_create(&tid, NULL, thread_recv, &params);

			app_write_to_umd(interface, wav_data, count, 1);

			free(wav_data);
			wav_data = NULL;

			printf("Joining threads\n");
			pthread_join(tid,NULL);
			printf("Threads joined \n");

			/* Disable transmission */
			printf("Disabling Tx on read DMA channel\n");
			ret = umd_cmd(interface, UMD_LPAIF_DEINIT_TX);
			if (ret < 0) {
				printf("Failed to stop Tx on hsi2s device\n");
				break;
			}
			umd_cmd(interface, UMD_LPAIF_RESET);
			is_tx_active = 0;
			break;
		}
		case EXTERNAL_LB_MASTER:
		{
			int interface = minor_num;
			/* Operation mode : External loopback on master interface */
			if (argc < 5) {
				help();
				ret = -1;
				break;
			}
			printf("Setting external loopback on master \n");
			ret = umd_cmd(interface, UMD_LPAIF_RESET);
			if (ret < 0) {
				printf("Failed to reset the hsi2s device\n");
				break;
			}
			ret = umd_cmd(interface, UMD_LPAIF_EXTERNAL_LOOPBACK);
			if (ret < 0) {
				printf("Failed to trigger external loopback\n");
				break;
			}
			is_tx_active = 1;

			printf("Reading i/p file...\n");
			fd_read_ip = fopen(inputname, "r");
			if (!fd_read_ip) {
				printf("Unable to open input file, exiting...\n");
				ret = -1;
				break;
			}
			printf("Calculating i/p file size...\n");
			wav_samples = get_size(fd_read_ip);
			printf("Input file size in bytes: %ld\n", wav_samples);
			wav_data = (int32_t *) malloc(wav_samples);
			if (!wav_data) {
				printf("Failed to allocate transmit data buffer\n");
				fclose(fd_read_ip);
				fd_read_ip = NULL;
				ret = -ENOMEM;
				break;
			}

			/* Copy data from wav file into memory */
			int count = fread(wav_data, 1, wav_samples, fd_read_ip);
			fclose(fd_read_ip);
			fd_read_ip = NULL;

			struct thread_recv_params params = {
				.interface = interface,
				.read_limit = count,
				.name = outputname,
			};
			pthread_t tid;
			int ret = pthread_create(&tid, NULL, thread_recv, &params);

			app_write_to_umd(interface, wav_data, count, 1);

			free(wav_data);
			wav_data = NULL;

			printf("Joining threads\n");
			pthread_join(tid,NULL);
			printf("Threads joined \n");

			/* Disable transmission */
			printf("Disabling Tx on read DMA channel\n");
			ret = umd_cmd(interface, UMD_LPAIF_DEINIT_TX);
			if (ret < 0) {
				printf("Failed to stop Tx on hsi2s device\n");
				break;
			}
			umd_cmd(interface, UMD_LPAIF_RESET);

			is_tx_active = 0;
			break;
		}
		case EXTERNAL_LB_MASTER_SLAVE:
		{
			int interface_master = minor_num;
			int interface_slave = slave;
			/* Operation mode : External loopback between master and slave interfaces */
			if (argc < 6) {
				help();
				ret = -1;
				break;
			}
			printf("Slave node is hs%d_i2s\n",slave);
			printf("Setting external loopback on master/slave \n");
			ret = umd_cmd(interface_master, UMD_LPAIF_RESET);
			if (ret < 0) {
				printf("Failed to reset the hsi2s master\n");
				break;
			}
			ret = umd_cmd(interface_slave, UMD_LPAIF_RESET);
			if (ret < 0) {
				printf("Failed to reset the hsi2s slave\n");
				break;
			}
			ret = umd_cmd_param(interface_master, UMD_LPAIF_MUXMODE, 0);
			if (ret < 0) {
				printf("Failed to set master mode\n");
				break;
			}
			ret = umd_cmd_param(interface_slave, UMD_LPAIF_MUXMODE, 1);
			if (ret < 0) {
				printf("Failed to set slave mode\n");
				break;
			}
			ret = umd_cmd_param(interface_master, UMD_LPAIF_SET_SLAVE, interface_slave);
			if (ret < 0) {
				printf("Failed to set slave for the master\n");
				break;
			}
			ret = umd_cmd(interface_slave, UMD_LPAIF_MIC);
			if (ret < 0) {
				printf("Failed to configure mic\n");
				break;
			}
			ret = umd_cmd(interface_master, UMD_LPAIF_SPEAKER); 
			if (ret < 0) {
				printf("Failed to configure speaker\n");
				break;
			}
			is_tx_active = 1;

			printf("Reading i/p file...\n");
			fd_read_ip = fopen(inputname, "r");
			if (!fd_read_ip) {
				printf("Unable to open input file, exiting...\n");
				ret = -1;
				break;
			}
			printf("Calculating i/p file size...\n");
			wav_samples = get_size(fd_read_ip);
			printf("Input file size in bytes: %ld\n", wav_samples);
			wav_data = (int32_t *) malloc(wav_samples);
			if (!wav_data) {
				printf("Failed to allocate transmit data buffer\n");
				fclose(fd_read_ip);
				fd_read_ip = NULL;
				ret = -ENOMEM;
				break;
			}

			/* Copy data from wav file into memory */
			int count = fread(wav_data, 1, wav_samples, fd_read_ip);
			fclose(fd_read_ip);
			fd_read_ip = NULL;

			struct thread_recv_params params = {
				.interface = interface_slave,
				.read_limit = count,
				.name = outputname,
			};
			pthread_t tid;
			int ret = pthread_create(&tid, NULL, thread_recv, &params);

			app_write_to_umd(interface_master, wav_data, count, 1);

			free(wav_data);
			wav_data = NULL;

			printf("Joining threads\n");
			pthread_join(tid,NULL);
			printf("Threads joined \n");

			/* Disable transmission */
			printf("Disabling Tx on read DMA channel\n");
			ret = umd_cmd(interface_master, UMD_LPAIF_DEINIT_TX); 
			if (ret < 0) {
				printf("Failed to stop Tx on hsi2s device\n");
				break;
			}
			umd_cmd(interface_master, UMD_LPAIF_RESET);
			umd_cmd(interface_slave, UMD_LPAIF_RESET);
			is_tx_active = 0;
			break;
		}
		case SET_MUXMODE:
		{
			int interface = minor_num;
			/* Operation mode : Set I2S interface as master/slave */
			if (argc < 4) {
				help();
				ret = -1;
				break;
			}

			printf("Setting master/slave mode...\n");
			ret = umd_cmd_param(interface, UMD_LPAIF_MUXMODE, mux_mode);
			if (ret < 0) {
				printf("Failed to set master/slave configuration on target\n");
			}
			break;
		}
		case CONFIG_M_CLK:
		{
			int interface = minor_num;
			/* Operation mode : Configure master clock */
			if (argc < 5) {
				help();
				ret = -1;
				break;
			}
			if (!clk_source) {
				printf("Clock source is CXO\n");
				reg_val = divide_by;
				ret = umd_cmd_param(interface, UMD_LPAIF_SET_CLOCK, reg_val);
				if (ret < 0) {
					printf("Failed to set master clock on target\n");
					break;
				}
			} else {
				printf("Clock source is Digital PLL\n");
				reg_val = SRC_DIGITAL_PLL | divide_by;
				ret = umd_cmd_param(interface, UMD_LPAIF_SET_CLOCK, reg_val);
				if (ret < 0) {
					printf("Failed to set master clock on target\n");
					break;
				}
			}
			printf("Successfully configured master clock\n\n");
			break;
		}
		case CONFIG_I2S_PARAMS:
		{
			int interface = minor_num;
			/* Operation mode : Configure I2S parameters */
			if (argc < 8) {
				help();
				ret = -1;
				break;
			}
			printf("Configuring I2S parameters...\n");
			i_params = (struct i2s_params *) malloc(sizeof(struct i2s_params));
			if (!i_params) {
				printf("Failed to allocate I2S param structure\n");
				ret = -ENOMEM;
				break;
			}
			i_params->bit_clk = bit_clk;
			i_params->buffer_ms = buffer_ms;
			i_params->bit_depth = bit_depth;
			i_params->spkr_channel_count = spkr_channel_count;
			i_params->mic_channel_count = mic_channel_count;
			if (long_rate) {
				i_params->en_long_rate = 1;
				i_params->long_rate = long_rate;
			}
			ret = umd_cmd_i2s(interface, i_params);
			if (ret < 0) {
				printf("Failed to configure I2S parameters on target\n");
			}
			free(i_params);
			break;
		}
		case CONFIG_PCM_PARAMS:
		{
			int interface = minor_num;
			/* Operation mode : Configure PCM parameters */
			printf("Configuring PCM parameters...\n");
			if (argc < 10) {
				help();
				ret = -1;
				break;
			}
			p_params = (struct pcm_params *) malloc(sizeof(struct pcm_params));
			if (!p_params) {
				printf("Failed to allocate PCM param structure\n");
				ret = -ENOMEM;
				break;
			}
			p_params->bit_clk = bit_clk;
			p_params->buffer_ms = buffer_ms;
			p_params->rate = pcm_rate;
			p_params->sync_src = pcm_sync_src;
			p_params->aux_mode = pcm_aux_mode;
			p_params->rpcm_width = pcm_rpcm_width;
			p_params->tpcm_width = pcm_tpcm_width;
			ret = umd_cmd_pcm(interface, p_params);
			if (ret < 0) {
				printf("Failed to configure PCM parameters on target\n");
			}
			free(p_params);
			break;
		}
		case CONFIG_TDM_PARAMS:
		{
			int interface = minor_num;
			/* Operation mode : Configure TDM parameters */
			printf("Configuring TDM parameters...\n");
			if (argc < 7) {
				help();
				ret = -1;
				break;
			}
			t_params = (struct tdm_params *) malloc(sizeof(struct tdm_params));
			if (!t_params) {
				printf("Failed to allocate TDM param structure\n");
				ret = -ENOMEM;
				break;
			}
			t_params->sync_delay = tdm_sync_delay;
			t_params->tpcm_width = tdm_tpcm_width;
			t_params->rpcm_width = tdm_rpcm_width;
			t_params->rate = tdm_rate;
			if (tdm_tpcm_sample_width || tdm_rpcm_sample_width) {
				t_params->en_diff_sample_width = 1;
				t_params->tpcm_sample_width = tdm_tpcm_sample_width;
				t_params->rpcm_sample_width = tdm_rpcm_sample_width;
			}
			ret = umd_cmd_tdm(interface, t_params);
			if (ret < 0) {
				printf("Failed to configure TDM parameters on target\n");
			}
			free(t_params);
			break;
		}
		case CONFIG_LPAIF_MODE:
		{
			int interface = minor_num;
			/* Operation mode : Set LPAIF interface in I2S/PCM mode */
			if (argc < 4) {
				help();
				ret = -1;
				break;
			}
			printf("Setting LPAIF mode...\n");
			ret = umd_cmd_param(interface, UMD_LPAIF_MODE, lpaif_mode);
			if (ret < 0) {
				printf("Failed to set I2S/PCM configuration on target\n");
			}
			break;
		}
		case CONFIG_PCM_LANE:
		{
			int interface = minor_num;
			/* Operation mode : Set PCM lane configuration */
			if (argc < 4) {
				help();
				ret = -1;
				break;
			}
			printf("Setting PCM lane configuration...\n");
			ret = umd_cmd_param(interface, UMD_PCM_CONFIG_LANE, lane_config);
			if (ret < 0) {
				printf("Failed to set PCM lane configuration on target\n");
			}
			break;
		}
		case TOGGLE_BIT_CLK:
		{
			int interface = minor_num;
			/* Operation mode : Toggle bit clock */
			if (argc < 3) {
				help();
				ret = -1;
				break;
			}
			printf("Toggling bit clock direction...\n");
			ret = umd_cmd(interface, UMD_LPAIF_INVERT_BIT_CLOCK);
			if (ret < 0) {
				printf("Failed to toggle bit clock on target\n");
			}
			break;
		}
		case CONFIG_DAB_MRC:
		{
			int interface = minor_num;
			if (argc < 7) {
				help();
				ret = -1;
				break;
			}
			if (target_type) {
				printf("DAB MRC support is not available on SA8155/SA8195 targets\n");
				ret = -1;
			} else {
				/* Set CPU affinity to one of the available high cores */
				if (set_affinity) {
					printf("Setting CPU affinity \n");
					CPU_ZERO(&cpuset);
					for (i = 6; i < 8; i++) {
						CPU_SET(i, &cpuset);
						ret = sched_setaffinity(getpid(), sizeof(cpu_set_t), &cpuset);
						if (ret) {
							printf("Cannot set CPU affinity on CPU[%d]\n", i);
							CPU_ZERO(&cpuset);
							continue;
						}
						printf("CPU affinity set on CPU[%d]\n", i);
						break;
					}
				}
				int interface0 = 0;
				int interface1 = 0;

				/* Set the read limit and periodic length */
				read_limit = READ_LIMIT;
				mmap_len = get_periodic_length(bit_clk, buffer_ms);
				printf("Periodic length set to %ld bytes\n", mmap_len);

				/* Reset the HS-I2S instances */
				umd_cmd(interface0, UMD_LPAIF_RESET);
				umd_cmd(interface1, UMD_LPAIF_RESET);

				/* Set DAB MRC configuration */
				printf("Setting DAB MRC configuration...\n");
				ret = umd_cmd(interface0, UMD_CONFIGURE_DAB_MRC);
				if (ret < 0) {
					printf("Failed to set DAB MRC configuration on target\n");
					ret = -1;
					goto exit_app;
				}

				struct thread_recv_params params0 = {
					.interface = interface0,
					.read_limit = READ_LIMIT,
					.name = outputname_a,
				};
				pthread_t tid0;
				ret = pthread_create(&tid0, NULL, thread_recv, &params0);

				struct thread_recv_params params1 = {
					.interface = interface1,
					.read_limit = READ_LIMIT,
					.name = outputname_b,
				};
				pthread_t tid1;
				ret = pthread_create(&tid1, NULL, thread_recv, &params1);

				/* Wait for the threads to join */
				printf("Joining threads\n");
				pthread_join(tid0, NULL);
				pthread_join(tid1, NULL);
				printf("Threads joined \n");
				umd_cmd(interface0, UMD_LPAIF_RESET);
				umd_cmd(interface1, UMD_LPAIF_RESET);
			}
			break;
		}
		default:
			printf("Undefined operation mode\n");
			ret = -1;
			break;
	};

exit_app:
	printf("Clean-up in progress...\n");
	if (wav_data) {
		free(wav_data);
		wav_data = NULL;
	}
	if (fd_read_ip) {
		fclose(fd_read_ip);
		fd_read_ip = NULL;
	}
	if (priv) {
		shutdown_cmd_channel(priv);
	}
	printf("Exiting...\n");

	return ret;
}
