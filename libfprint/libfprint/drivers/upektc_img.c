/*
 * UPEK TouchChip driver for libfprint
 * Copyright (C) 2013 Vasily Khoruzhick <anarsoul@gmail.com>
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

#define FP_COMPONENT "upektc_img"

#include <errno.h>
#include <string.h>

#include <libusb.h>

#include <aeslib.h>
#include <fp_internal.h>

#include "upektc_img.h"
#include "driver_ids.h"

static void start_capture(struct fp_img_dev *dev);
static void start_deactivation(struct fp_img_dev *dev);

#define EP_IN			(1 | LIBUSB_ENDPOINT_IN)
#define EP_OUT			(2 | LIBUSB_ENDPOINT_OUT)
#define CTRL_TIMEOUT		4000
#define BULK_TIMEOUT		4000

#define IMAGE_WIDTH		144
#define IMAGE_HEIGHT		384
#define IMAGE_SIZE		(IMAGE_WIDTH * IMAGE_HEIGHT)

#define MAX_CMD_SIZE		64
#define MAX_RESPONSE_SIZE	2052
#define SHORT_RESPONSE_SIZE	64

struct upektc_img_dev {
	unsigned char cmd[MAX_CMD_SIZE];
	unsigned char response[MAX_RESPONSE_SIZE];
	unsigned char image_bits[IMAGE_SIZE * 2];
	unsigned char seq;
	size_t image_size;
	size_t response_rest;
	gboolean deactivating;
};

/****** HELPERS ******/

static const uint16_t crc_table[256] = {
	0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
	0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
	0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
	0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
	0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
	0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
	0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
	0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
	0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
	0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
	0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
	0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
	0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
	0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
	0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
	0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
	0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
	0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
	0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
	0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
	0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
	0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
	0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
	0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
	0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
	0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
	0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
	0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
	0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
	0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
	0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
	0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
};

static uint16_t udf_crc(unsigned char *buffer, size_t size)
{
	uint16_t crc = 0;
	while (size--)
	crc = (uint16_t) ((crc << 8) ^
			crc_table[((crc >> 8) & 0x00ff) ^ *buffer++]);
	return crc;
}

static void upektc_img_cmd_fix_seq(unsigned char *cmd_buf, unsigned char seq)
{
	uint8_t byte;

	byte = cmd_buf[5];
	byte &= 0x0f;
	byte |= (seq << 4);
	cmd_buf[5] = byte;
}

static void upektc_img_cmd_update_crc(unsigned char *cmd_buf, size_t size)
{
	/* CRC does not cover Ciao prefix (4 bytes) and CRC location (2 bytes) */
	uint16_t crc = udf_crc(cmd_buf + 4, size - 6);

	cmd_buf[size - 2] = (crc & 0x00ff);
	cmd_buf[size - 1] = (crc & 0xff00) >> 8;
}

static void upektc_img_submit_req(struct fpi_ssm *ssm,
	const unsigned char *buf, size_t buf_size, unsigned char seq,
	libusb_transfer_cb_fn cb)
{
	struct fp_img_dev *dev = ssm->priv;
	struct upektc_img_dev *upekdev = dev->priv;
	struct libusb_transfer *transfer = libusb_alloc_transfer(0);
	int r;

	BUG_ON(buf_size > MAX_CMD_SIZE);

	if (!transfer) {
		fpi_ssm_mark_aborted(ssm, -ENOMEM);
		return;
	}

	transfer->flags |= LIBUSB_TRANSFER_FREE_TRANSFER;

	memcpy(upekdev->cmd, buf, buf_size);
	upektc_img_cmd_fix_seq(upekdev->cmd, seq);
	upektc_img_cmd_update_crc(upekdev->cmd, buf_size);

	libusb_fill_bulk_transfer(transfer, dev->udev, EP_OUT, upekdev->cmd, buf_size,
		cb, ssm, BULK_TIMEOUT);

	r = libusb_submit_transfer(transfer);
	if (r < 0) {
		libusb_free_transfer(transfer);
		fpi_ssm_mark_aborted(ssm, r);
	}
}

static void upektc_img_read_data(struct fpi_ssm *ssm, size_t buf_size, size_t buf_offset, libusb_transfer_cb_fn cb)
{
	struct libusb_transfer *transfer = libusb_alloc_transfer(0);
	struct fp_img_dev *dev = ssm->priv;
	struct upektc_img_dev *upekdev = dev->priv;
	int r;

	if (!transfer) {
		fpi_ssm_mark_aborted(ssm, -ENOMEM);
		return;
	}

	BUG_ON(buf_size > MAX_RESPONSE_SIZE);

	transfer->flags |= LIBUSB_TRANSFER_FREE_TRANSFER;

	libusb_fill_bulk_transfer(transfer, dev->udev, EP_IN, upekdev->response + buf_offset, buf_size,
		cb, ssm, BULK_TIMEOUT);

	r = libusb_submit_transfer(transfer);
	if (r < 0) {
		libusb_free_transfer(transfer);
		fpi_ssm_mark_aborted(ssm, r);
	}
}

/****** CAPTURE ******/

enum capture_states {
	CAPTURE_INIT_CAPTURE,
	CAPTURE_READ_DATA,
	CAPTURE_READ_DATA_TERM,
	CAPTURE_ACK_00_28,
	CAPTURE_ACK_08,
	CAPTURE_ACK_FRAME,
	CAPTURE_ACK_00_28_TERM,
	CAPTURE_NUM_STATES,
};

static void capture_reqs_cb(struct libusb_transfer *transfer)
{
	struct fpi_ssm *ssm = transfer->user_data;

	if ((transfer->status != LIBUSB_TRANSFER_COMPLETED) ||
		(transfer->length != transfer->actual_length)) {
		fpi_ssm_mark_aborted(ssm, -EIO);
		return;
	}
	switch (ssm->cur_state) {
	case CAPTURE_ACK_00_28_TERM:
		fpi_ssm_jump_to_state(ssm, CAPTURE_READ_DATA_TERM);
		break;
	default:
		fpi_ssm_jump_to_state(ssm, CAPTURE_READ_DATA);
		break;
	}
}

static int upektc_img_process_image_frame(unsigned char *image_buf, unsigned char *cmd_res)
{
	int offset = 8;
	int len = ((cmd_res[5] & 0x0f) << 8) | (cmd_res[6]);

	len -= 1;
	if (cmd_res[7] == 0x2c) {
		len -= 10;
		offset += 10;
	}
	if (cmd_res[7] == 0x20) {
		len -= 4;
	}
	memcpy(image_buf, cmd_res + offset, len);

	return len;
}

static void capture_read_data_cb(struct libusb_transfer *transfer)
{
	struct fpi_ssm *ssm = transfer->user_data;
	struct fp_img_dev *dev = ssm->priv;
	struct upektc_img_dev *upekdev = dev->priv;
	unsigned char *data = upekdev->response;
	struct fp_img *img;
	size_t response_size;

	if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
		fp_dbg("request is not completed, %d", transfer->status);
		fpi_ssm_mark_aborted(ssm, -EIO);
		return;
	}

	if (upekdev->deactivating) {
		fp_dbg("Deactivate requested\n");
		fpi_ssm_mark_completed(ssm);
		return;
	}

	fp_dbg("request completed, len: %.4x", transfer->actual_length);
	if (transfer->actual_length == 0) {
		fpi_ssm_jump_to_state(ssm, ssm->cur_state);
		return;
	}

	if (ssm->cur_state == CAPTURE_READ_DATA_TERM) {
		fp_dbg("Terminating SSM\n");
		fpi_ssm_mark_completed(ssm);
		return;
	}

	if (!upekdev->response_rest) {
		response_size = ((data[5] & 0x0f) << 8) + data[6];
		response_size += 9; /* 7 bytes for header, 2 for CRC */
		if (response_size > transfer->actual_length) {
			fp_dbg("response_size is %d, actual_length is %d\n",
				response_size, transfer->actual_length);
			fp_dbg("Waiting for rest of transfer");
			BUG_ON(upekdev->response_rest);
			upekdev->response_rest = response_size - transfer->actual_length;
			fpi_ssm_jump_to_state(ssm, CAPTURE_READ_DATA);
			return;
		}
	}
	upekdev->response_rest = 0;

	switch (data[4]) {
	case 0x00:
		switch (data[7]) {
			/* No finger */
			case 0x28:
				fp_dbg("18th byte is %.2x\n", data[18]);
				switch (data[18]) {
				case 0x0c:
					/* no finger */
					fpi_ssm_jump_to_state(ssm, CAPTURE_ACK_00_28);
					break;
				case 0x00:
					/* finger is present! */
					fpi_ssm_jump_to_state(ssm, CAPTURE_ACK_00_28);
					break;
				case 0x1e:
					/* short scan */
					fp_err("short scan, aborting\n");
					fpi_imgdev_abort_scan(dev, FP_VERIFY_RETRY_TOO_SHORT);
					fpi_imgdev_report_finger_status(dev, FALSE);
					fpi_ssm_jump_to_state(ssm, CAPTURE_ACK_00_28_TERM);
					break;
				case 0x1d:
					/* too much horisontal movement */
					fp_err("too much horisontal movement, aborting\n");
					fpi_imgdev_abort_scan(dev, FP_VERIFY_RETRY_CENTER_FINGER);
					fpi_imgdev_report_finger_status(dev, FALSE);
					fpi_ssm_jump_to_state(ssm, CAPTURE_ACK_00_28_TERM);
					break;
				default:
					/* some error happened, cancel scan */
					fp_err("something bad happened, stop scan\n");
					fpi_imgdev_abort_scan(dev, FP_VERIFY_RETRY);
					fpi_imgdev_report_finger_status(dev, FALSE);
					fpi_ssm_jump_to_state(ssm, CAPTURE_ACK_00_28_TERM);
					break;
				}
				break;
			/* Image frame with additional info */
			case 0x2c:
				fpi_imgdev_report_finger_status(dev, TRUE);
			/* Plain image frame */
			case 0x24:
				upekdev->image_size +=
					upektc_img_process_image_frame(upekdev->image_bits + upekdev->image_size,
						data);
				fpi_ssm_jump_to_state(ssm, CAPTURE_ACK_FRAME);
				break;
			/* Last image frame */
			case 0x20:
				upekdev->image_size +=
					upektc_img_process_image_frame(upekdev->image_bits + upekdev->image_size,
						data);
				BUG_ON(upekdev->image_size != IMAGE_SIZE);
				fp_dbg("Image size is %d\n", upekdev->image_size);
				img = fpi_img_new(IMAGE_SIZE);
				img->flags = FP_IMG_PARTIAL;
				memcpy(img->data, upekdev->image_bits, IMAGE_SIZE);
				fpi_imgdev_image_captured(dev, img);
				fpi_imgdev_report_finger_status(dev, FALSE);
				fpi_ssm_mark_completed(ssm);
				break;
			default:
				fp_err("Uknown response!\n");
				fpi_ssm_mark_aborted(ssm, -EIO);
				break;
		}
		break;
	case 0x08:
		fpi_ssm_jump_to_state(ssm, CAPTURE_ACK_08);
		break;
	default:
		fp_err("Not handled response!\n");
		fpi_ssm_mark_aborted(ssm, -EIO);
	}
}

static void capture_run_state(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct upektc_img_dev *upekdev = dev->priv;

	switch (ssm->cur_state) {
	case CAPTURE_INIT_CAPTURE:
		upektc_img_submit_req(ssm, upek2020_init_capture, sizeof(upek2020_init_capture),
			upekdev->seq, capture_reqs_cb);
			upekdev->seq++;
		break;
	case CAPTURE_READ_DATA:
	case CAPTURE_READ_DATA_TERM:
		if (!upekdev->response_rest)
			upektc_img_read_data(ssm, SHORT_RESPONSE_SIZE, 0, capture_read_data_cb);
		else
			upektc_img_read_data(ssm, MAX_RESPONSE_SIZE - SHORT_RESPONSE_SIZE,
			SHORT_RESPONSE_SIZE, capture_read_data_cb);
		break;
	case CAPTURE_ACK_00_28:
	case CAPTURE_ACK_00_28_TERM:
		upektc_img_submit_req(ssm, upek2020_ack_00_28, sizeof(upek2020_ack_00_28),
			upekdev->seq, capture_reqs_cb);
			upekdev->seq++;
		break;
	case CAPTURE_ACK_08:
		upektc_img_submit_req(ssm, upek2020_ack_08, sizeof(upek2020_ack_08),
			0, capture_reqs_cb);
		break;
	case CAPTURE_ACK_FRAME:
		upektc_img_submit_req(ssm, upek2020_ack_frame, sizeof(upek2020_ack_frame),
			upekdev->seq, capture_reqs_cb);
			upekdev->seq++;
		break;
	};
}

static void capture_sm_complete(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct upektc_img_dev *upekdev = dev->priv;
	int err = ssm->error;

	fp_dbg("Capture completed, %d", err);
	fpi_ssm_free(ssm);

	if (upekdev->deactivating)
		start_deactivation(dev);
	else if (err)
		fpi_imgdev_session_error(dev, err);
	else
		start_capture(dev);
}

static void start_capture(struct fp_img_dev *dev)
{
	struct upektc_img_dev *upekdev = dev->priv;
	struct fpi_ssm *ssm;

	upekdev->image_size = 0;

	ssm = fpi_ssm_new(dev->dev, capture_run_state, CAPTURE_NUM_STATES);
	ssm->priv = dev;
	fpi_ssm_start(ssm, capture_sm_complete);
}

/****** INITIALIZATION/DEINITIALIZATION ******/

enum deactivate_states {
	DEACTIVATE_DEINIT,
	DEACTIVATE_READ_DEINIT_DATA,
	DEACTIVATE_NUM_STATES,
};

static void deactivate_reqs_cb(struct libusb_transfer *transfer)
{
	struct fpi_ssm *ssm = transfer->user_data;

	if ((transfer->status == LIBUSB_TRANSFER_COMPLETED) &&
		(transfer->length == transfer->actual_length)) {
		fpi_ssm_jump_to_state(ssm, CAPTURE_READ_DATA);
	} else {
		fpi_ssm_mark_aborted(ssm, -EIO);
	}
}

/* TODO: process response properly */
static void deactivate_read_data_cb(struct libusb_transfer *transfer)
{
	struct fpi_ssm *ssm = transfer->user_data;

	if (transfer->status == LIBUSB_TRANSFER_COMPLETED) {
		fpi_ssm_mark_completed(ssm);
	} else {
		fpi_ssm_mark_aborted(ssm, -EIO);
	}
}

static void deactivate_run_state(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct upektc_img_dev *upekdev = dev->priv;

	switch (ssm->cur_state) {
	case DEACTIVATE_DEINIT:
		upektc_img_submit_req(ssm, upek2020_deinit, sizeof(upek2020_deinit),
			upekdev->seq, deactivate_reqs_cb);
		upekdev->seq++;
		break;
	case DEACTIVATE_READ_DEINIT_DATA:
		upektc_img_read_data(ssm, SHORT_RESPONSE_SIZE, 0, deactivate_read_data_cb);
		break;
	};
}

static void deactivate_sm_complete(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct upektc_img_dev *upekdev = dev->priv;
	int err = ssm->error;

	fp_dbg("Deactivate completed");
	fpi_ssm_free(ssm);

	if (err) {
		fpi_imgdev_session_error(dev, err);
		return;
	}

	upekdev->deactivating = FALSE;
	fpi_imgdev_deactivate_complete(dev);
}

static void start_deactivation(struct fp_img_dev *dev)
{
	struct upektc_img_dev *upekdev = dev->priv;
	struct fpi_ssm *ssm;

	upekdev->image_size = 0;

	ssm = fpi_ssm_new(dev->dev, deactivate_run_state, DEACTIVATE_NUM_STATES);
	ssm->priv = dev;
	fpi_ssm_start(ssm, deactivate_sm_complete);
}

enum activate_states {
	ACTIVATE_CONTROL_REQ_1,
	ACTIVATE_READ_CTRL_RESP_1,
	ACTIVATE_INIT_1,
	ACTIVATE_READ_INIT_1_RESP,
	ACTIVATE_INIT_2,
	ACTIVATE_READ_INIT_2_RESP,
	ACTIVATE_CONTROL_REQ_2,
	ACTIVATE_READ_CTRL_RESP_2,
	ACTIVATE_INIT_3,
	ACTIVATE_READ_INIT_3_RESP,
	ACTIVATE_INIT_4,
	ACTIVATE_READ_INIT_4_RESP,
	ACTIVATE_NUM_STATES,
};

static void init_reqs_ctrl_cb(struct libusb_transfer *transfer)
{
	struct fpi_ssm *ssm = transfer->user_data;

	if (transfer->status == LIBUSB_TRANSFER_COMPLETED) {
		fpi_ssm_next_state(ssm);
	} else {
		fpi_ssm_mark_aborted(ssm, -EIO);
	}
}

static void init_reqs_cb(struct libusb_transfer *transfer)
{
	struct fpi_ssm *ssm = transfer->user_data;

	if ((transfer->status == LIBUSB_TRANSFER_COMPLETED) &&
		(transfer->length == transfer->actual_length)) {
		fpi_ssm_next_state(ssm);
	} else {
		fpi_ssm_mark_aborted(ssm, -EIO);
	}
}

/* TODO: process response properly */
static void init_read_data_cb(struct libusb_transfer *transfer)
{
	struct fpi_ssm *ssm = transfer->user_data;

	if (transfer->status == LIBUSB_TRANSFER_COMPLETED) {
		fpi_ssm_next_state(ssm);
	} else {
		fpi_ssm_mark_aborted(ssm, -EIO);
	}
}

static void activate_run_state(struct fpi_ssm *ssm)
{
	struct libusb_transfer *transfer;
	struct fp_img_dev *dev = ssm->priv;
	struct upektc_img_dev *upekdev = dev->priv;
	int r;

	switch (ssm->cur_state) {
	case ACTIVATE_CONTROL_REQ_1:
	case ACTIVATE_CONTROL_REQ_2:
	{
		unsigned char *data;

		transfer = libusb_alloc_transfer(0);
		if (!transfer) {
			fpi_ssm_mark_aborted(ssm, -ENOMEM);
			break;
		}
		transfer->flags |= LIBUSB_TRANSFER_FREE_BUFFER |
			LIBUSB_TRANSFER_FREE_TRANSFER;

		data = g_malloc0(LIBUSB_CONTROL_SETUP_SIZE + 1);
		libusb_fill_control_setup(data,
			LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE, 0x0c, 0x100, 0x0400, 1);
		libusb_fill_control_transfer(transfer, ssm->dev->udev, data,
			init_reqs_ctrl_cb, ssm, CTRL_TIMEOUT);
		r = libusb_submit_transfer(transfer);
		if (r < 0) {
			g_free(data);
			libusb_free_transfer(transfer);
			fpi_ssm_mark_aborted(ssm, r);
		}
	}
	break;
	case ACTIVATE_INIT_1:
		upektc_img_submit_req(ssm, upek2020_init_1, sizeof(upek2020_init_1),
			0, init_reqs_cb);
	break;
	case ACTIVATE_INIT_2:
		upektc_img_submit_req(ssm, upek2020_init_2, sizeof(upek2020_init_2),
			0, init_reqs_cb);
	break;
	case ACTIVATE_INIT_3:
		upektc_img_submit_req(ssm, upek2020_init_3, sizeof(upek2020_init_3),
			0, init_reqs_cb);
	break;
	case ACTIVATE_INIT_4:
		upektc_img_submit_req(ssm, upek2020_init_4, sizeof(upek2020_init_4),
			upekdev->seq, init_reqs_cb);
		/* Seq should be updated after 4th init */
		upekdev->seq++;
	break;
	case ACTIVATE_READ_CTRL_RESP_1:
	case ACTIVATE_READ_CTRL_RESP_2:
	case ACTIVATE_READ_INIT_1_RESP:
	case ACTIVATE_READ_INIT_2_RESP:
	case ACTIVATE_READ_INIT_3_RESP:
	case ACTIVATE_READ_INIT_4_RESP:
		upektc_img_read_data(ssm, SHORT_RESPONSE_SIZE, 0, init_read_data_cb);
	break;
	}
}

static void activate_sm_complete(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	int err = ssm->error;

	fpi_ssm_free(ssm);
	fp_dbg("%s status %d", __func__, err);
	fpi_imgdev_activate_complete(dev, err);

	if (!err)
		start_capture(dev);
}

static int dev_activate(struct fp_img_dev *dev, enum fp_imgdev_state state)
{
	struct upektc_img_dev *upekdev = dev->priv;
	struct fpi_ssm *ssm = fpi_ssm_new(dev->dev, activate_run_state,
		ACTIVATE_NUM_STATES);
	ssm->priv = dev;
	upekdev->seq = 0;
	fpi_ssm_start(ssm, activate_sm_complete);
	return 0;
}

static void dev_deactivate(struct fp_img_dev *dev)
{
	struct upektc_img_dev *upekdev = dev->priv;

	upekdev->deactivating = TRUE;
}

static int dev_init(struct fp_img_dev *dev, unsigned long driver_data)
{
	/* TODO check that device has endpoints we're using */
	int r;

	r = libusb_claim_interface(dev->udev, 0);
	if (r < 0) {
		fp_err("could not claim interface 0: %s", libusb_error_name(r));
		return r;
	}

	dev->priv = g_malloc0(sizeof(struct upektc_img_dev));
	fpi_imgdev_open_complete(dev, 0);
	return 0;
}

static void dev_deinit(struct fp_img_dev *dev)
{
	g_free(dev->priv);
	libusb_release_interface(dev->udev, 0);
	fpi_imgdev_close_complete(dev);
}

static int discover(struct libusb_device_descriptor *dsc, uint32_t *devtype)
{
	if (dsc->idProduct == 0x2020 && dsc->bcdDevice == 1)
		return 1;
#ifndef ENABLE_UPEKE2
	if (dsc->idProduct == 0x2016 && dsc->bcdDevice == 2)
		return 1;
#endif

	return 0;
}

static const struct usb_id id_table[] = {
#ifndef ENABLE_UPEKE2
	{ .vendor = 0x147e, .product = 0x2016 },
#endif
	{ .vendor = 0x147e, .product = 0x2020 },
	{ 0, 0, 0, },
};

struct fp_img_driver upektc_img_driver = {
	.driver = {
		.id = UPEKTC_IMG_ID,
		.name = FP_COMPONENT,
		.full_name = "Upek TouchChip Fingerprint Coprocessor",
		.id_table = id_table,
		.scan_type = FP_SCAN_TYPE_SWIPE,
		.discover = discover,
	},
	.flags = 0,
	.img_height = IMAGE_HEIGHT,
	.img_width = IMAGE_WIDTH,
	.bz3_threshold = 20,

	.open = dev_init,
	.close = dev_deinit,
	.activate = dev_activate,
	.deactivate = dev_deactivate,
};
