/*
 * Elan driver for libfprint
 *
 * Copyright (C) 2017 Igor Filatov <ia.filatov@gmail.com>
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

#ifndef __ELAN_H
#define __ELAN_H

#include <string.h>
#include <libusb.h>

/* number of pixels to discard on left and right (along raw image height)
 * because they have different intensity from the rest of the frame */
#define ELAN_FRAME_MARGIN 12

/* min and max frames in a capture */
#define ELAN_MIN_FRAMES 7
#define ELAN_MAX_FRAMES 30

/* number of frames to drop at the end of capture because frames captured
 * while the finger is being lifted can be bad */
#define ELAN_SKIP_LAST_FRAMES 1

#define ELAN_CMD_LEN 0x2
#define ELAN_EP_CMD_OUT (0x1 | LIBUSB_ENDPOINT_OUT)
#define ELAN_EP_CMD_IN (0x3 | LIBUSB_ENDPOINT_IN)
#define ELAN_EP_IMG_IN (0x2 | LIBUSB_ENDPOINT_IN)

/* usual command timeout and timeout for when we need to check if the finger is
 * still on the device */
#define ELAN_CMD_TIMEOUT 10000
#define ELAN_FINGER_TIMEOUT 200

struct elan_cmd {
	unsigned char cmd[ELAN_CMD_LEN];
	int response_len;
	int response_in;
};

static const struct elan_cmd get_sensor_dim_cmds[] = {
	{
	 .cmd = {0x00, 0x0c},
	 .response_len = 0x4,
	 .response_in = ELAN_EP_CMD_IN,
	 },
};

static const size_t get_sensor_dim_cmds_len =
array_n_elements(get_sensor_dim_cmds);

static const struct elan_cmd init_start_cmds[] = {
	{
	 .cmd = {0x40, 0x19},
	 .response_len = 0x2,
	 .response_in = ELAN_EP_CMD_IN,
	 },
	{
	 .cmd = {0x40, 0x2a},
	 .response_len = 0x2,
	 .response_in = ELAN_EP_CMD_IN,
	 },
};

static const size_t init_start_cmds_len = array_n_elements(init_start_cmds);

static const struct elan_cmd read_cmds[] = {
	/* raw frame sizes are calculated from image dimesions reported by the
	 * device */
	{
	 .cmd = {0x00, 0x09},
	 .response_len = -1,
	 .response_in = ELAN_EP_IMG_IN,
	 },
};

const size_t read_cmds_len = array_n_elements(read_cmds);

/* issued after data reads during init and calibration */
static const struct elan_cmd init_end_cmds[] = {
	{
	 .cmd = {0x40, 0x24},
	 .response_len = 0x2,
	 .response_in = ELAN_EP_CMD_IN,
	 },
};

static const size_t init_end_cmds_len = array_n_elements(init_end_cmds);

/* same command 2 times
 * original driver may observe return value to determine how many times it
 * should be repeated */
static const struct elan_cmd calibrate_start_cmds[] = {
	{
	 .cmd = {0x40, 0x23},
	 .response_len = 0x1,
	 .response_in = ELAN_EP_CMD_IN,
	 },
	{
	 .cmd = {0x40, 0x23},
	 .response_len = 0x1,
	 .response_in = ELAN_EP_CMD_IN,
	 },
};

static const size_t calibrate_start_cmds_len =
array_n_elements(calibrate_start_cmds);

/* issued after data reads during init and calibration */
static const struct elan_cmd calibrate_end_cmds[] = {
	{
	 .cmd = {0x40, 0x24},
	 .response_len = 0x2,
	 .response_in = ELAN_EP_CMD_IN,
	 },
};

static const size_t calibrate_end_cmds_len =
array_n_elements(calibrate_end_cmds);

static const struct elan_cmd capture_start_cmds[] = {
	/* led on */
	{
	 .cmd = {0x40, 0x31},
	 .response_len = 0x0,
	 .response_in = ELAN_EP_CMD_IN,
	 },
};

static size_t capture_start_cmds_len = array_n_elements(capture_start_cmds);

static const struct elan_cmd capture_wait_finger_cmds[] = {
	/* wait for finger
	 * subsequent read will not complete until finger is placed on the reader */
	{
	 .cmd = {0x40, 0x3f},
	 .response_len = 0x1,
	 .response_in = ELAN_EP_CMD_IN,
	 },
};

static size_t capture_wait_finger_cmds_len =
array_n_elements(capture_wait_finger_cmds);

static const struct elan_cmd deactivate_cmds[] = {
	/* led off */
	{
	 .cmd = {0x00, 0x0b},
	 .response_len = 0x0,
	 .response_in = ELAN_EP_CMD_IN,
	 },
};

static const size_t deactivate_cmds_len = array_n_elements(deactivate_cmds);

static void elan_cmd_cb(struct libusb_transfer *transfer);
static void elan_cmd_read(struct fpi_ssm *ssm);
static void elan_run_next_cmd(struct fpi_ssm *ssm);

static void elan_capture(struct fp_img_dev *dev);

#endif
