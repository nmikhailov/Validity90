/*
 * AuthenTec AES2501 driver for libfprint
 * Copyright (C) 2007 Cyrille Bagard
 *
 * Based on code from http://home.gna.org/aes2501, relicensed with permission
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef __AES2501_H
#define __AES2501_H

enum aes2501_regs {
	AES2501_REG_CTRL1 = 0x80,
	AES2501_REG_CTRL2 = 0x81,
	AES2501_REG_EXCITCTRL = 0x82, /* excitation control */
	AES2501_REG_DETCTRL = 0x83, /* detect control */
	AES2501_REG_COLSCAN = 0x88, /* column scan rate register */
	AES2501_REG_MEASDRV = 0x89, /* measure drive */
	AES2501_REG_MEASFREQ = 0x8a, /* measure frequency */
	AES2501_REG_DEMODPHASE1 = 0x8d,
	AES2501_REG_DEMODPHASE2 = 0x8c,
	AES2501_REG_CHANGAIN = 0x8e, /* channel gain */
	AES2501_REG_ADREFHI = 0x91, /* A/D reference high */
	AES2501_REG_ADREFLO = 0x92, /* A/D reference low */
	AES2501_REG_STRTROW = 0x93, /* start row */
	AES2501_REG_ENDROW = 0x94, /* end row */
	AES2501_REG_STRTCOL = 0x95, /* start column */
	AES2501_REG_ENDCOL = 0x96, /* end column */
	AES2501_REG_DATFMT = 0x97, /* data format */
	AES2501_REG_IMAGCTRL = 0x98, /* image data */
	AES2501_REG_STAT = 0x9a,
	AES2501_REG_CHWORD1 = 0x9b, /* challenge word 1 */
	AES2501_REG_CHWORD2 = 0x9c,
	AES2501_REG_CHWORD3 = 0x9d,
	AES2501_REG_CHWORD4 = 0x9e,
	AES2501_REG_CHWORD5 = 0x9f,
	AES2501_REG_TREG1 = 0xa1, /* test register 1 */
	AES2501_REG_AUTOCALOFFSET = 0xa8,
	AES2501_REG_TREGC = 0xac,
	AES2501_REG_TREGD = 0xad,
	AES2501_REG_LPONT = 0xb4, /* low power oscillator on time */
};

#define FIRST_AES2501_REG	AES2501_REG_CTRL1
#define LAST_AES2501_REG	AES2501_REG_CHWORD5

#define AES2501_CTRL1_MASTER_RESET	(1<<0)
#define AES2501_CTRL1_SCAN_RESET	(1<<1) /* stop + restart scan sequencer */
/* 1 = continuously updated, 0 = updated prior to starting a scan */
#define AES2501_CTRL1_REG_UPDATE	(1<<2)

/* 1 = continuous scans, 0 = single scans */
#define AES2501_CTRL2_CONTINUOUS	0x01 
#define AES2501_CTRL2_READ_REGS		0x02 /* dump registers */
#define AES2501_CTRL2_SET_ONE_SHOT	0x04
#define AES2501_CTRL2_CLR_ONE_SHOT	0x08
#define AES2501_CTRL2_READ_ID		0x10

enum aes2501_detection_rate {
	/* rate of detection cycles: */
	AES2501_DETCTRL_DRATE_CONTINUOUS 	= 0x00, /* continuously */
	AES2501_DETCTRL_DRATE_16_MS 		= 0x01, /* every 16.62ms */
	AES2501_DETCTRL_DRATE_31_MS			= 0x02, /* every 31.24ms */
	AES2501_DETCTRL_DRATE_62_MS			= 0x03, /* every 62.50ms */
	AES2501_DETCTRL_DRATE_125_MS 		= 0x04, /* every 125.0ms */
	AES2501_DETCTRL_DRATE_250_MS 		= 0x05, /* every 250.0ms */
	AES2501_DETCTRL_DRATE_500_MS 		= 0x06, /* every 500.0ms */
	AES2501_DETCTRL_DRATE_1_S 			= 0x07, /* every 1s */
};

enum aes2501_settling_delay {
	AES2501_DETCTRL_SDELAY_31_MS	= 0x00,	/* 31.25ms */
	AES2501_DETCTRL_SSDELAY_62_MS	= 0x10,	/* 62.5ms */
	AES2501_DETCTRL_SSDELAY_125_MS	= 0x20,	/* 125ms */
	AES2501_DETCTRL_SSDELAY_250_MS	= 0x30	/* 250ms */
};

enum aes2501_col_scan_rate {
    AES2501_COLSCAN_SRATE_32_US		= 0x00,	/* 32us */
    AES2501_COLSCAN_SRATE_64_US		= 0x01,	/* 64us */
    AES2501_COLSCAN_SRATE_128_US	= 0x02,	/* 128us */
    AES2501_COLSCAN_SRATE_256_US	= 0x03,	/* 256us */
    AES2501_COLSCAN_SRATE_512_US	= 0x04,	/* 512us */
    AES2501_COLSCAN_SRATE_1024_US	= 0x05,	/* 1024us */
    AES2501_COLSCAN_SRATE_2048_US	= 0x06,	/* 2048us */

};

enum aes2501_mesure_drive {
	AES2501_MEASDRV_MDRIVE_0_325	= 0x00,	/* 0.325 Vpp */
	AES2501_MEASDRV_MDRIVE_0_65		= 0x01,	/* 0.65 Vpp */
	AES2501_MEASDRV_MDRIVE_1_3		= 0x02,	/* 1.3 Vpp */
	AES2501_MEASDRV_MDRIVE_2_6		= 0x03	/* 2.6 Vpp */

};

/* Select (1=square | 0=sine) wave drive during measure */
#define AES2501_MEASDRV_SQUARE		0x20
/* 0 = use mesure drive setting, 1 = when sine wave is selected */
#define AES2501_MEASDRV_MEASURE_SQUARE	0x10

enum aes2501_measure_freq {
	AES2501_MEASFREQ_125K	= 0x01,	/* 125 kHz */
	AES2501_MEASFREQ_250K	= 0x02,	/* 250 kHz */
	AES2501_MEASFREQ_500K	= 0x03,	/* 500 kHz */
	AES2501_MEASFREQ_1M		= 0x04,	/* 1 MHz */
	AES2501_MEASFREQ_2M		= 0x05	/* 2 MHz */
};

#define DEMODPHASE_NONE		0x00
#define DEMODPHASE_180_00	0x40	/* 180 degrees */
#define DEMODPHASE_2_81		0x01	/* 2.8125 degrees */

#define AES2501_REG_DEMODPHASE1 0x8d
#define DEMODPHASE_1_40		0x40	/* 1.40625 degrees */
#define DEMODPHASE_0_02		0x01	/* 0.02197256 degrees */

enum aes2501_sensor_gain1 {
	AES2501_CHANGAIN_STAGE1_2X	= 0x00,	/* 2x */
	AES2501_CHANGAIN_STAGE1_4X	= 0x01,	/* 4x */
	AES2501_CHANGAIN_STAGE1_8X	= 0x02,	/* 8x */
	AES2501_CHANGAIN_STAGE1_16X	= 0x03	/* 16x */
};

enum aes2501_sensor_gain2 {
	AES2501_CHANGAIN_STAGE2_2X	= 0x00,	/* 2x */
	AES2501_CHANGAIN_STAGE2_4X	= 0x10,	/* 4x */
	AES2501_CHANGAIN_STAGE2_8X	= 0x20,	/* 8x */
	AES2501_CHANGAIN_STAGE2_16X	= 0x30	/* 16x */
};

#define AES2501_DATFMT_EIGHT	0x40	/* 1 = 8-bit data, 0 = 4-bit data */
#define AES2501_DATFMT_LOW_RES	0x20
#define AES2501_DATFMT_BIN_IMG	0x10

/* don't send image or authentication messages when imaging */
#define AES2501_IMAGCTRL_IMG_DATA_DISABLE	0x01
/* send histogram when imaging */
#define AES2501_IMAGCTRL_HISTO_DATA_ENABLE	0x02
/* send histogram at end of each row rather than each scan */
#define AES2501_IMAGCTRL_HISTO_EACH_ROW		0x04
/* send full image array rather than 64x64 center */
#define AES2501_IMAGCTRL_HISTO_FULL_ARRAY	0x08
/* return registers before data (rather than after) */
#define AES2501_IMAGCTRL_REG_FIRST		0x10
/* return test registers with register dump */
#define AES2501_IMAGCTRL_TST_REG_ENABLE		0x20

#define AES2501_CHWORD1_IS_FINGER	0x01 /* If set, finger is present */

/* Enable the reading of the register in TREGD */
#define AES2501_TREGC_ENABLE	0x01

#define AES2501_LPONT_MIN_VALUE 0x00	/* 0 ms */
#define AES2501_LPONT_MAX_VALUE 0x1f	/* About 16 ms */

#define AES2501_ADREFHI_MIN_VALUE 0x28
#define AES2501_ADREFHI_MAX_VALUE 0x58

#define AES2501_SUM_HIGH_THRESH 1000
#define AES2501_SUM_LOW_THRESH 700

#endif	/* __AES2501_H */
