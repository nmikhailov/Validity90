/*
 * AuthenTec AES1660/AES2660 common definitions
 * Copyright (c) 2012 Vasily Khoruzhick <anarsoul@gmail.com>
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

#ifndef __AESX660_H
#define __AESX660_H

#define AESX660_HEADER_SIZE 3
#define AESX660_RESPONSE_TYPE_OFFSET 0x00
#define AESX660_RESPONSE_SIZE_LSB_OFFSET 0x01
#define AESX660_RESPONSE_SIZE_MSB_OFFSET 0x02

#define AESX660_CALIBRATE_RESPONSE 0x06
#define AESX660_FINGER_DET_RESPONSE 0x40
#define AESX660_FINGER_PRESENT_OFFSET 0x03
#define AESX660_FINGER_PRESENT 0x01

#define AESX660_IMAGE_OK_OFFSET 0x03
#define AESX660_IMAGE_OK 0x0d
#define AESX660_LAST_FRAME_OFFSET 0x04
#define AESX660_LAST_FRAME_BIT 0x01

#define AESX660_FRAME_DELTA_X_OFFSET 16
#define AESX660_FRAME_DELTA_Y_OFFSET 17

#define AESX660_IMAGE_OFFSET 43
#define AESX660_BULK_TRANSFER_SIZE 4096

#define AESX660_FRAME_HEIGHT 8

struct aesX660_dev {
	GSList *strips;
	size_t strips_len;
	gboolean deactivating;
	struct aesX660_cmd *init_seq;
	size_t init_seq_len;
	unsigned int init_cmd_idx;
	unsigned int init_seq_idx;
	struct libusb_transfer *fd_data_transfer;
	unsigned char *buffer;
	size_t buffer_size;
	size_t buffer_max;

	/* Device-specific stuff */
	struct aesX660_cmd *init_seqs[2];
	size_t init_seqs_len[2];
	unsigned char *start_imaging_cmd;
	size_t start_imaging_cmd_len;
	struct fpi_frame_asmbl_ctx *assembling_ctx;
	uint16_t extra_img_flags;
};

struct aesX660_cmd {
	const unsigned char *cmd;
	size_t len;
};

/* 0x77 cmd seems to control LED, this sequence
 * makes LED blink
 */
static const unsigned char led_blink_cmd[] = {
0x77, 0x18, 0x00,
0x00, 0x3f, 0x00, 0xff, 0x00,
0x01, 0x01, 0x00, 0x00, 0x00, 0xf3, 0x01, 0x00,
0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0xf3,
0x01, 0x00, 0x7f
};

/* This sequence makes LED light solid
 */
static const unsigned char led_solid_cmd[] = {
0x77, 0x18, 0x00, 0x00, 0x3f, 0x00, 0xff, 0x00,
0x01, 0x01, 0x00, 0x00, 0x00, 0xe7, 0x03, 0x00,
0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x7f
};

static const unsigned char wait_for_finger_cmd[] = {
0x20,
0x40, 0x04, 0x00, 0x02, 0x1e, 0x00, 0x32
};

/* 0x40 cmd response
 *
static const unsigned char pkt1371[] = {
0x40, 0x01, 0x00, 0x01
};
*/

static const unsigned char set_idle_cmd[] = {
	0x0d, /* Reset or "set idle"? */
};

static const unsigned char read_id_cmd[] = {
	0x44, 0x02, 0x00, 0x08, 0x00, /* Max transfer size is 8 */
	0x07, /* Read ID? */
};

static const unsigned char calibrate_cmd[] = {
	0x44, 0x02, 0x00, 0x04, 0x00,
	0x06,
};

int aesX660_dev_activate(struct fp_img_dev *dev, enum fp_imgdev_state state);
void aesX660_dev_deactivate(struct fp_img_dev *dev);

#endif
