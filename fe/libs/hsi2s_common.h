/*
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
Copyright (c) 2020, The Linux Foundation. All rights reserved.
SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef _HSI2S_COMMON_H_
#define _HSI2S_COMMON_H_

/* IOCTL commands */

/* Configures I2S/PCM and DMA registers for normal data transfer on the interface */
#define LPAIF_NORMAL_MODE _IOWR('i', 0, int)
/* Configures I2S/PCM and DMA registers for internal loopback on the interface */
#define LPAIF_INTERNAL_LOOPBACK _IOWR('i', 1, int)
/* Configures I2S/PCM and DMA registers for external loopback on the interface */
#define LPAIF_EXTERNAL_LOOPBACK _IOWR('i', 2, int)
/* Configures the interface as either master or slave */
#define LPAIF_MUXMODE _IOWR('i', 3, int)
/* Configures I2S/PCM and DMA registers for speaker operation on the interface */
#define LPAIF_SPEAKER _IOWR('i', 4, int)
/* Configures I2S/PCM and DMA registers for mic operation on the interface */
#define LPAIF_MIC _IOWR('i', 5, int)
/* Used in master-slave external loopback to set slave field of master interface data structure */
#define LPAIF_SET_SLAVE _IOWR('i', 6, int)
/* Enables read DMA channel and speaker */
#define LPAIF_INIT_TX _IOWR('i', 7, int)
/* Disables read DMA channel and speaker */
#define LPAIF_DEINIT_TX _IOWR('i', 8, int)
/* Configures the master clock on the interface */
#define LPAIF_SET_CLOCK _IOWR('i', 9, int)
/* Resets the I2S/PCM and DMA registers */
#define LPAIF_RESET _IOWR('i', 10, int)
/* Configures LPAIF to be in I2S/PCM mode */
#define LPAIF_MODE _IOWR('i', 11, int)
/* Configures I2S parameters on the interface */
#define I2S_CONFIG_PARAMS _IOWR('i', 12, int)
/* Configures PCM parameters on the interface */
#define PCM_CONFIG_PARAMS _IOWR('i', 13, int)
/* Configures TDM parameters on the interface */
#define TDM_CONFIG_PARAMS _IOWR('i', 14, int)
/* Sets PCM lane configuration */
#define PCM_CONFIG_LANE  _IOWR('i', 15, int)
/* Inverts the bit clock on the interface */
#define LPAIF_INVERT_BIT_CLOCK _IOWR('i', 16, int)
/* Sets DAB MRC configuration */
#define CONFIGURE_DAB_MRC _IOWR('i', 17, int)

#endif
