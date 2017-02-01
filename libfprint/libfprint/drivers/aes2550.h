/*
 * AuthenTec AES2550/AES2810 driver for libfprint
 * Copyright (C) 2012 Vasily Khoruzhick
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

#ifndef __AES2550_H
#define __AES2550_H

/* Registers bits */

#define AES2550_REG80_MASTER_RESET		(1 << 0)
#define AES2550_REG80_FORCE_FINGER_PRESENT	(1 << 1)
#define AES2550_REG80_LPO_START			(1 << 2)
#define AES2550_REG80_HGC_ENABLE		(1 << 3)
#define AES2550_REG80_SENSOR_MODE_OFS		(4)
#define AES2550_REG80_AUTO_RESTART_FD		(1 << 6)
#define AES2550_REG80_EXT_REG_ENABLE		(1 << 7)

#define AES2550_REG81_CONT_SCAN			(1 << 0)
#define AES2550_REG81_READ_REG			(1 << 1)
#define AES2550_REG81_NSHOT			(1 << 2)
#define AES2550_REG81_RUN_FD			(1 << 3)
#define AES2550_REG81_READ_ID			(1 << 4)
#define AES2550_REG81_RUN_CAL			(1 << 5)
#define AES2550_REG81_RUN_TIMER			(1 << 6)
#define AES2550_REG81_RUN_BIST			(1 << 7)

#define AES2550_REG83_FINGER_PRESENT		(1 << 7)

#define AES2550_REG85_FLUSH_PER_FRAME		(1 << 7)

#define AES2550_REG8F_EDATA_DISABLE		(1 << 1)
#define AES2550_REG8F_AUTH_DISABLE		(1 << 2)
#define AES2550_REG8F_EHISTO_DISABLE		(1 << 3)
#define AES2550_REG8F_HISTO64			(1 << 4)
#define AES2550_REG8F_SINGLE_REG_ENABLE		(1 << 6)

#define AES2550_REG95_COL_SCANNED_OFS		(0)
#define AES2550_REG95_EPIX_AVG_OFS		(4)

#define AES2550_REGA8_DIG_BIT_DATA_OFS		(0)
#define AES2550_REGA8_DIG_BIT_EN		(1 << 4)
#define AES2550_REGA8_FIXED_BIT_DATA		(1 << 5)
#define AES2550_REGA8_INVERT_BIT_DATA		(1 << 6)

#define AES2550_REGAD_LPFD_AVG_OFS		(0)
#define AES2550_REGAD_DETECT_FGROFF		(1 << 4)
#define AES2550_REGAD_ADVRANGE_2V		(1 << 6)

#define AES2550_REGB1_ATE_CONT_IMAGE		(1 << 1)
#define AES2550_REGB1_ANALOG_RESET		(1 << 2)
#define AES2550_REGB1_ANALOG_PD			(1 << 3)
#define AES2550_REGB1_TEST_EMBD_WORD		(1 << 4)
#define AES2550_REGB1_ORIG_EMBD_WORD		(1 << 5)
#define AES2550_REGB1_RESET_UHSM		(1 << 6)
#define AES2550_REGB1_RESET_SENSOR		(1 << 7)

#define AES2550_REGBD_LPO_IN_15_8_OFS		(0)
#define AES2550_REGBE_LPO_IN_7_0_OFS		(0)

#define AES2550_REGBF_RSR_LEVEL_DISABLED	(0 << 0)
#define AES2550_REGBF_RSR_LEVEL_LEADING_RSR	(1 << 0)
#define AES2550_REGBF_RSR_LEVEL_SIMPLE_RSR	(2 << 0)
#define AES2550_REGBF_RSR_LEVEL_SUPER_RSR	(3 << 0)
#define AES2550_REGBF_RSR_DIR_DOWN_MOTION	(0 << 2)
#define AES2550_REGBF_RSR_DIR_UP_MOTION		(1 << 2)
#define AES2550_REGBF_RSR_DIR_UPDOWN_MOTION	(2 << 2)
#define AES2550_REGBF_NOISE_FLOOR_MODE		(1 << 4)
#define AES2550_REGBF_QUADRATURE_MODE		(1 << 5)

#define AES2550_REGCF_INTERFERENCE_CHK_EN	(1 << 0)
#define AES2550_REGCF_INTERFERENCE_AVG_EN	(1 << 1)
#define AES2550_REGCF_INTERFERENCE_AVG_OFFS	(4)

#define AES2550_REGDC_BP_NUM_REF_SWEEP_OFS	(0)
#define AES2550_REGDC_DEBUG_CTRL2_OFS		(3)

#define AES2550_REGDD_DEBUG_CTRL1_OFS		(0)

/* Commands */

enum aes2550_cmds {
	AES2550_CMD_SET_IDLE_MODE = 0x00,
	AES2550_CMD_RUN_FD = 0x01,
	AES2550_CMD_GET_ENROLL_IMG = 0x02,
	AES2550_CMD_CALIBRATE = 0x06,
	AES2550_CMD_READ_CALIBRATION_DATA = 0x10,
	AES2550_CMD_HEARTBEAT = 0x70,
};

/* Messages */

#define AES2550_STRIP_SIZE		(0x31e + 3)
#define AES2550_HEARTBEAT_SIZE		(4 + 3)
#define AES2550_EDATA_MAGIC		0xe0
#define AES2550_HEARTBEAT_MAGIC		0xdb

#define AES2550_EP_IN_BUF_SIZE		8192

#endif
