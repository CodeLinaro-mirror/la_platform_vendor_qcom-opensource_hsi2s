/*
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#ifndef __HSI2S_UMD_PROTOCOl_H__
#define __HSI2S_UMD_PROTOCOl_H__
/* protocal between client and server */

typedef unsigned int u32;
typedef unsigned char u8;

/* I2S parameters */
struct i2s_params {
	u32 bit_clk;
	u32 buffer_ms;
	u32 bit_depth;
	u32 spkr_channel_count;
	u32 mic_channel_count;
	u8 en_long_rate;
	u32 long_rate;
};

/* PCM parameters */
struct pcm_params {
	u32 bit_clk;
	u32 buffer_ms;
	u8 rate;
	u8 sync_src;
	u8 aux_mode;
	u8 rpcm_width;
	u8 tpcm_width;
};

/* TDM parameters */
struct tdm_params {
	u8 sync_delay;
	u32 tpcm_width;
	u32 rpcm_width;
	u32 rate;
	u8 en_diff_sample_width;
	u32 tpcm_sample_width;
	u32 rpcm_sample_width;
};

struct log_params {
	u32 bits;

	u32 level;
	u32 intfs;
	u32 modules;
};

struct client_cmd {
	int interface;
	/*  */
	int type;  
	union {
		int param;
		struct i2s_params i2s_params;
		struct pcm_params pcm_params;
		struct tdm_params tdm_params;
		struct log_params log_params;
		int result;
	};
};

enum HSI2S_UMD_CMD_TYPE {
	UMD_TEST = 0,
	UMD_CONNECT,
	UMD_LPAIF_NORMAL_MODE,
	UMD_LPAIF_INTERNAL_LOOPBACK,
	UMD_LPAIF_EXTERNAL_LOOPBACK,
	UMD_LPAIF_MUXMODE,
	UMD_LPAIF_SPEAKER,
	UMD_LPAIF_MIC,
	UMD_LPAIF_SET_SLAVE,
	UMD_LPAIF_INIT_TX,
	UMD_LPAIF_DEINIT_TX,
	UMD_LPAIF_SET_CLOCK,
	UMD_LPAIF_RESET,
	UMD_LPAIF_MODE,
	UMD_I2S_CONFIG_PARAMS,
	UMD_PCM_CONFIG_PARAMS,
	UMD_TDM_CONFIG_PARAMS,
	UMD_PCM_CONFIG_LANE,
	UMD_LPAIF_INVERT_BIT_CLOCK,
	UMD_CONFIGURE_DAB_MRC,
	UMD_DISCONNECT,
};

/* app side using to send cmd */
int apptoumd(struct client_cmd *cmd);

/* umd side using to handle cmd */
typedef int (*Cmdhandler)(void *arg, struct client_cmd *cmd);
void* create_cmd_channel(Cmdhandler handler, void *arg);
void destroy_cmd_channel(void *priv);

typedef int (*Funconsume)(void *arg, unsigned char* data, int len, long long ts);
typedef int (*Funproduce)(void *arg, unsigned char* data, int len, long long ts);

/* app side using to tx/rx data */
void app_write_to_umd(int interface, unsigned char* data, int data_len, int sync);
void app_read_from_umd(int interface, Funconsume consume, void *arg);

/* umd side using to tx/rx data */
void umd_read_from_app(int interface, Funconsume consume, void *arg);
void umd_write_to_app(int interface, Funproduce produce, void *arg);

/* umd side using to clean up */
void umd_wait_tx_complete(int interface);
void umd_exit_tx_loop(int interface);
void umd_exit_rx_loop(int interface);

#endif
