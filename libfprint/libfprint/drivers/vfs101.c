/*
 * Validity VFS101 driver for libfprint
 * Copyright (C) 2011 Sergio Cerlesi <sergio.cerlesi@gmail.com>
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

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define FP_COMPONENT "vfs101"

#include <fp_internal.h>

#include "driver_ids.h"

/* Input-Output usb endpoint */
#define EP_IN(n)	(n | LIBUSB_ENDPOINT_IN)
#define EP_OUT(n)	(n | LIBUSB_ENDPOINT_OUT)

/* Usb bulk timeout */
#define BULK_TIMEOUT		100

/* The device send back the image into block of 16 frames of 292 bytes */
#define VFS_FRAME_SIZE		292
#define VFS_BLOCK_SIZE		16 * VFS_FRAME_SIZE

/* Buffer height */
#define VFS_BUFFER_HEIGHT	5000

/* Buffer size */
#define VFS_BUFFER_SIZE		(VFS_BUFFER_HEIGHT * VFS_FRAME_SIZE)

/* Image width */
#define VFS_IMG_WIDTH		200

/* Maximum image height */
#define VFS_IMG_MAX_HEIGHT	1023

/* Minimum image height */
#define VFS_IMG_MIN_HEIGHT	200

/* Scan level thresold */
#define VFS_IMG_SLT_BEGIN		768
#define VFS_IMG_SLT_END			64
#define VFS_IMG_SLT_LINES		4

/* Minimum image level */
#define VFS_IMG_MIN_IMAGE_LEVEL	144

/* Best image contrast */
#define VFS_IMG_BEST_CONRAST	128

/* Device parameters address */
#define VFS_PAR_000E			0x000e
#define VFS_PAR_0011			0x0011
#define VFS_PAR_THRESHOLD		0x0057
#define VFS_PAR_STATE_3			0x005e
#define VFS_PAR_STATE_5			0x005f
#define VFS_PAR_INFO_RATE		0x0062
#define VFS_PAR_0076			0x0076
#define VFS_PAR_INFO_CONTRAST	0x0077
#define VFS_PAR_0078			0x0078

/* Device regiones address */
#define VFS_REG_IMG_EXPOSURE	0xff500e
#define VFS_REG_IMG_CONTRAST	0xff5038

/* Device settings */
#define VFS_VAL_000E			0x0001
#define VFS_VAL_0011			0x0008
#define VFS_VAL_THRESHOLD		0x0096
#define VFS_VAL_STATE_3			0x0064
#define VFS_VAL_STATE_5			0x00c8
#define VFS_VAL_INFO_RATE		0x0001
#define VFS_VAL_0076			0x0012
#define VFS_VAL_0078			0x2230
#define VFS_VAL_IMG_EXPOSURE	0x21c0

/* Structure for Validity device */
struct vfs101_dev
{
	/* Action state */
	int active;

	/* Sequential number */
	unsigned int seqnum;

	/* Usb transfer */
	struct libusb_transfer *transfer;

	/* Buffer for input/output */
	unsigned char buffer[VFS_BUFFER_SIZE];

	/* Length of data to send or received */
	unsigned int length;

	/* Ignore usb error */
	int ignore_error;

	/* Timeout */
	struct fpi_timeout *timeout;

	/* Loop counter */
	int counter;

	/* Number of enroll stage */
	int enroll_stage;

	/* Image contrast */
	int contrast;

	/* Best contrast */
	int best_contrast;

	/* Best contrast level */
	int best_clevel;

	/* Bottom line of image */
	int bottom;

	/* Image height */
	int height;
};

/* Return byte at specified position */
static inline unsigned char byte(int position, int value)
{
	return (value >> (position * 8)) & 0xff;
}

/* Return sequential number */
static inline unsigned short get_seqnum(int h, int l)
{
	return (h<<8) | l;
}

/* Check sequential number */
static inline int check_seqnum(struct vfs101_dev *vdev)
{
	if ((byte(0, vdev->seqnum) == vdev->buffer[0]) &&
		(byte(1, vdev->seqnum) == vdev->buffer[1]))
		return 0;
	else
		return 1;
}

/* Internal result codes */
enum
{
	RESULT_RETRY,
	RESULT_RETRY_SHORT,
	RESULT_RETRY_REMOVE,
	RESULT_COUNT,
};

/* Enroll result codes */
static int result_codes[2][RESULT_COUNT] =
{
	{
		FP_ENROLL_RETRY,
		FP_ENROLL_RETRY_TOO_SHORT,
		FP_ENROLL_RETRY_REMOVE_FINGER,
	},
	{
		FP_VERIFY_RETRY,
		FP_VERIFY_RETRY_TOO_SHORT,
		FP_VERIFY_RETRY_REMOVE_FINGER,
	},
};

/* Return result code based on current action */
static int result_code(struct fp_img_dev *dev, int result)
{
	/* Check result value */
	if (result < 0 || result >= RESULT_COUNT)
		return result;

	/* Return result code */
	if (dev->action == IMG_ACTION_ENROLL)
		return result_codes[0][result];
	else
		return result_codes[1][result];
};

/* Dump buffer for debug */
#define dump_buffer(buf) \
	fp_dbg("%02x %02x %02x %02x %02x %02x %02x %02x", \
		buf[6], buf[7], buf[8], buf[9], buf[10], buf[11], buf[12], buf[13] \
	)

/* Callback of asynchronous send */
static void async_send_cb(struct libusb_transfer *transfer)
{
	struct fpi_ssm *ssm = transfer->user_data;
	struct fp_img_dev *dev = ssm->priv;
	struct vfs101_dev *vdev = dev->priv;

	/* Cleanup transfer */
	vdev->transfer = NULL;

	/* Skip error check if ignore_error is set */
	if (!vdev->ignore_error)
	{
		if (transfer->status != LIBUSB_TRANSFER_COMPLETED)
		{
			/* Transfer not completed, return IO error */
			fp_err("transfer not completed, status = %d", transfer->status);
			fpi_imgdev_session_error(dev, -EIO);
			fpi_ssm_mark_aborted(ssm, -EIO);
			goto out;
		}

		if (transfer->length != transfer->actual_length)
		{
			/* Data sended mismatch with expected, return protocol error */
			fp_err("length mismatch, got %d, expected %d",
				transfer->actual_length, transfer->length);
			fpi_imgdev_session_error(dev, -EIO);
			fpi_ssm_mark_aborted(ssm, -EIO);
			goto out;
		}
	}
	else
		/* Reset ignore_error flag */
		vdev->ignore_error = FALSE;

	/* Dump buffer for debug */
	dump_buffer(vdev->buffer);

	fpi_ssm_next_state(ssm);

out:
	libusb_free_transfer(transfer);
}

/* Submit asynchronous send */
static void async_send(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct vfs101_dev *vdev = dev->priv;
	int r;

	/* Allocation of transfer */
	vdev->transfer = libusb_alloc_transfer(0);
	if (!vdev->transfer)
	{
		/* Allocation transfer failed, return no memory error */
		fp_err("allocation of usb transfer failed");
		fpi_imgdev_session_error(dev, -ENOMEM);
		fpi_ssm_mark_aborted(ssm, -ENOMEM);
		return;
	}

	/* Put sequential number into the buffer */
	vdev->seqnum++;
	vdev->buffer[0] = byte(0, vdev->seqnum);
	vdev->buffer[1] = byte(1, vdev->seqnum);

	/* Prepare bulk transfer */
	libusb_fill_bulk_transfer(vdev->transfer, dev->udev, EP_OUT(1), vdev->buffer, vdev->length, async_send_cb, ssm, BULK_TIMEOUT);

	/* Submit transfer */
	r = libusb_submit_transfer(vdev->transfer);
	if (r != 0)
	{
		/* Submission of transfer failed, return IO error */
		libusb_free_transfer(vdev->transfer);
		fp_err("submit of usb transfer failed");
		fpi_imgdev_session_error(dev, -EIO);
		fpi_ssm_mark_aborted(ssm, -EIO);
		return;
	}
}

/* Callback of asynchronous recv */
static void async_recv_cb(struct libusb_transfer *transfer)
{
	struct fpi_ssm *ssm = transfer->user_data;
	struct fp_img_dev *dev = ssm->priv;
	struct vfs101_dev *vdev = dev->priv;

	/* Cleanup transfer */
	vdev->transfer = NULL;

	/* Skip error check if ignore_error is set */
	if (!vdev->ignore_error)
	{
		if (transfer->status != LIBUSB_TRANSFER_COMPLETED)
		{
			/* Transfer not completed, return IO error */
			fp_err("transfer not completed, status = %d", transfer->status);
			fpi_imgdev_session_error(dev, -EIO);
			fpi_ssm_mark_aborted(ssm, -EIO);
			goto out;
		}

		if (check_seqnum(vdev))
		{
			/* Sequential number received mismatch, return protocol error */
			fp_err("seqnum mismatch, got %04x, expected %04x",
				get_seqnum(vdev->buffer[1], vdev->buffer[0]), vdev->seqnum);
			fpi_imgdev_session_error(dev, -EIO);
			fpi_ssm_mark_aborted(ssm, -EIO);
			goto out;
		}
	}
	else
		/* Reset ignore_error flag */
		vdev->ignore_error = FALSE;

	/* Dump buffer for debug */
	dump_buffer(vdev->buffer);

	/* Set length of received data */
	vdev->length = transfer->actual_length;

	fpi_ssm_next_state(ssm);

out:
	libusb_free_transfer(transfer);
}

/* Submit asynchronous recv */
static void async_recv(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct vfs101_dev *vdev = dev->priv;
	int r;

	/* Allocation of transfer */
	vdev->transfer = libusb_alloc_transfer(0);
	if (!vdev->transfer)
	{
		/* Allocation transfer failed, return no memory error */
		fp_err("allocation of usb transfer failed");
		fpi_imgdev_session_error(dev, -ENOMEM);
		fpi_ssm_mark_aborted(ssm, -ENOMEM);
		return;
	}

	/* Prepare bulk transfer */
	libusb_fill_bulk_transfer(vdev->transfer, dev->udev, EP_IN(1), vdev->buffer, 0x0f, async_recv_cb, ssm, BULK_TIMEOUT);

	/* Submit transfer */
	r = libusb_submit_transfer(vdev->transfer);
	if (r != 0)
	{
		/* Submission of transfer failed, free transfer and return IO error */
		libusb_free_transfer(vdev->transfer);
		fp_err("submit of usb transfer failed");
		fpi_imgdev_session_error(dev, -EIO);
		fpi_ssm_mark_aborted(ssm, -EIO);
		return;
	}
}

static void async_load(struct fpi_ssm *ssm);

/* Callback of asynchronous load */
static void async_load_cb(struct libusb_transfer *transfer)
{
	struct fpi_ssm *ssm = transfer->user_data;
	struct fp_img_dev *dev = ssm->priv;
	struct vfs101_dev *vdev = dev->priv;

	/* Cleanup transfer */
	vdev->transfer = NULL;

	/* Skip error check if ignore_error is set */
	if (!vdev->ignore_error)
	{
		if (transfer->status != LIBUSB_TRANSFER_COMPLETED)
		{
			/* Transfer not completed */
			fp_err("transfer not completed, status = %d, length = %d", transfer->status, vdev->length);
			fpi_imgdev_session_error(dev, -EIO);
			fpi_ssm_mark_aborted(ssm, -EIO);
			goto out;
		}

		if (transfer->actual_length % VFS_FRAME_SIZE)
		{
			/* Received incomplete frame, return protocol error */
			fp_err("received incomplete frame");
			fpi_imgdev_session_error(dev, -EIO);
			fpi_ssm_mark_aborted(ssm, -EIO);
			goto out;
		}
	}

	/* Increase image length */
	vdev->length += transfer->actual_length;

	if (transfer->actual_length == VFS_BLOCK_SIZE)
	{
		if ((VFS_BUFFER_SIZE - vdev->length) < VFS_BLOCK_SIZE)
		{
			/* Buffer full, image too large, return no memory error */
			fp_err("buffer full, image too large");
			fpi_imgdev_session_error(dev, -ENOMEM);
			fpi_ssm_mark_aborted(ssm, -ENOMEM);
			goto out;
		}
		else
			/* Image load not completed, submit another asynchronous load */
			async_load(ssm);
	}
	else
	{
		/* Reset ignore_error flag */
		if (vdev->ignore_error)
			vdev->ignore_error = FALSE;

		/* Image load completed, go to next state */
		vdev->height = vdev->length / VFS_FRAME_SIZE;
		fp_dbg("image loaded, height = %d", vdev->height);
		fpi_ssm_next_state(ssm);
	}

out:
	libusb_free_transfer(transfer);
}

/* Submit asynchronous load */
static void async_load(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct vfs101_dev *vdev = dev->priv;
	unsigned char *buffer;
	int r;

	/* Allocation of transfer */
	vdev->transfer = libusb_alloc_transfer(0);
	if (!vdev->transfer)
	{
		/* Allocation transfer failed, return no memory error */
		fp_err("allocation of usb transfer failed");
		fpi_imgdev_session_error(dev, -ENOMEM);
		fpi_ssm_mark_aborted(ssm, -ENOMEM);
		return;
	}

	/* Append new data into the buffer */
	buffer = vdev->buffer + vdev->length;

	/* Prepare bulk transfer */
	libusb_fill_bulk_transfer(vdev->transfer, dev->udev, EP_IN(2), buffer, VFS_BLOCK_SIZE, async_load_cb, ssm, BULK_TIMEOUT);

	/* Submit transfer */
	r = libusb_submit_transfer(vdev->transfer);
	if (r != 0)
	{
		/* Submission of transfer failed, return IO error */
		libusb_free_transfer(vdev->transfer);
		fp_err("submit of usb transfer failed");
		fpi_imgdev_session_error(dev, -EIO);
		fpi_ssm_mark_aborted(ssm, -EIO);
		return;
	}
}

/* Callback of asynchronous sleep */
static void async_sleep_cb(void *data)
{
	struct fpi_ssm *ssm = data;
	struct fp_img_dev *dev = ssm->priv;
	struct vfs101_dev *vdev = dev->priv;

	/* Cleanup timeout */
	vdev->timeout = NULL;

	fpi_ssm_next_state(ssm);
}

/* Submit asynchronous sleep */
static void async_sleep(unsigned int msec, struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct vfs101_dev *vdev = dev->priv;

	/* Add timeout */
	vdev->timeout = fpi_timeout_add(msec, async_sleep_cb, ssm);

	if (vdev->timeout == NULL)
	{
		/* Failed to add timeout */
		fp_err("failed to add timeout");
		fpi_imgdev_session_error(dev, -ETIME);
		fpi_ssm_mark_aborted(ssm, -ETIME);
	}
}

/* Swap ssm states */
enum
{
	M_SWAP_SEND,
	M_SWAP_RECV,
	M_SWAP_NUM_STATES,
};

/* Exec swap sequential state machine */
static void m_swap_state(struct fpi_ssm *ssm)
{
	switch (ssm->cur_state)
	{
	case M_SWAP_SEND:
		/* Send data */
		async_send(ssm);
		break;

	case M_SWAP_RECV:
		/* Recv response */
		async_recv(ssm);
		break;
	}
}

/* Start swap sequential state machine */
static void m_swap(struct fpi_ssm *ssm, unsigned char *data, size_t length)
{
	struct fp_img_dev *dev = ssm->priv;
	struct vfs101_dev *vdev = dev->priv;
	struct fpi_ssm *subsm;

	/* Prepare data for sending */
	memcpy(vdev->buffer, data, length);
	memset(vdev->buffer + length, 0, 16 - length);
	vdev->length = length;

	/* Start swap ssm */
	subsm = fpi_ssm_new(dev->dev, m_swap_state, M_SWAP_NUM_STATES);
	subsm->priv = dev;
	fpi_ssm_start_subsm(ssm, subsm);
}

/* Retrieve fingerprint image */
static void vfs_get_print(struct fpi_ssm *ssm, unsigned int param, int type)
{
	unsigned char data[2][0x0e] = {
		{	0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00,
			0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01	},
		{	0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00,
			0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x01	}
	};

	fp_dbg("param = %04x, type = %d", param, type);

	/* Prepare data for sending */
	data[type][6] = byte(0, param);
	data[type][7] = byte(1, param);

	/* Run swap sequential state machine */
	m_swap(ssm, data[type], 0x0e);
}

/* Set a parameter value on the device */
static void vfs_set_param(struct fpi_ssm *ssm, unsigned int param, unsigned int value)
{
	unsigned char data[0x0a] = { 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00 };

	fp_dbg("param = %04x, value = %04x", param, value);

	/* Prepare data for sending */
	data[6] = byte(0, param);
	data[7] = byte(1, param);
	data[8] = byte(0, value);
	data[9] = byte(1, value);

	/* Run swap sequential state machine */
	m_swap(ssm, data, 0x0a);
}

/* Abort previous print */
static void vfs_abort_print(struct fpi_ssm *ssm)
{
	unsigned char data[0x06] = { 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00 };

	fp_dbg("");

	/* Run swap sequential state machine */
	m_swap (ssm, data, 0x06);
}

/* Poke a value on a region */
static void vfs_poke(struct fpi_ssm *ssm, unsigned int addr, unsigned int value, unsigned int size)
{
	unsigned char data[0x0f] = { 0x00, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

	fp_dbg("addr = %04x, value = %04x", addr, value);

	/* Prepare data for sending */
	data[6] = byte(0, addr);
	data[7] = byte(1, addr);
	data[8] = byte(2, addr);
	data[9] = byte(3, addr);
	data[10] = byte(0, value);
	data[11] = byte(1, value);
	data[12] = byte(2, value);
	data[13] = byte(3, value);
	data[14] = byte(0, size);

	/* Run swap sequential state machine */
	m_swap(ssm, data, 0x0f);
}

/* Get current finger state */
static void vfs_get_finger_state(struct fpi_ssm *ssm)
{
	unsigned char data[0x06] = { 0x00, 0x00, 0x00, 0x00, 0x16, 0x00 };

	fp_dbg("");

	/* Run swap sequential state machine */
	m_swap (ssm, data, 0x06);
}

/* Load raw image from reader */
static void vfs_img_load(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct vfs101_dev *vdev = dev->priv;

	fp_dbg("");

	/* Reset buffer length */
	vdev->length = 0;

	/* Reset image properties */
	vdev->bottom = 0;
	vdev->height = -1;

	/* Asynchronous load */
	async_load(ssm);
}

/* Check if action is completed */
static int action_completed(struct fp_img_dev *dev)
{
	struct vfs101_dev *vdev = dev->priv;

	if ((dev->action == IMG_ACTION_ENROLL) &&
		(vdev->enroll_stage < dev->dev->nr_enroll_stages))
		/* Enroll not completed, return false */
		return FALSE;

	else if (vdev->enroll_stage < 1)
		/* Action not completed, return false */
		return FALSE;

	/* Action completed, return true */
	return TRUE;
}

#define offset(x, y)	((x) + ((y) * VFS_FRAME_SIZE))

/* Screen image to remove noise and find bottom line and height od image */
static void img_screen(struct vfs101_dev *vdev)
{
	int y, x, count, top;
	long int level;
	int last_line = vdev->height - 1;

	fp_dbg("image height before screen = %d", vdev->height);

	count = 0;

	/* Image returned from sensor can contain many empty lines,
	 * for remove these lines compare byte 282-283 (scan level information)
	 * with two differents threshold, one for the begin of finger image and
	 * one for the end. To increase stability of the code use a counter
	 * of lines that satisfy the threshold.
	 */
	for (y = last_line, top = last_line; y >= 0; y--)
	{
		/* Take image scan level */
		level = vdev->buffer[offset(283, y)] * 256 +
			vdev->buffer[offset(282, y)];

		fp_dbg("line = %d, scan level = %ld", y, level);

		if (level >= VFS_IMG_SLT_BEGIN && top == last_line)
		{
			/* Begin threshold satisfied */
			if (count < VFS_IMG_SLT_LINES)
				/* Increase count */
				count++;
			else
			{
				/* Found top fingerprint line */
				top = y + VFS_IMG_SLT_LINES;
				count = 0;
			}
		}
		else if ((level < VFS_IMG_SLT_END || level >= 65535) &&
			top != last_line)
		{
			/* End threshold satisfied */
			if (count < VFS_IMG_SLT_LINES)
				/* Increase count */
				count++;
			else
			{
				/* Found bottom fingerprint line */
				vdev->bottom = y + VFS_IMG_SLT_LINES + 1;
				break;
			}
		}
		else
			/* Not threshold satisfied, reset count */
			count = 0;
	}

	vdev->height = top - vdev->bottom + 1;

	/* Checkk max height */
	if (vdev->height > VFS_IMG_MAX_HEIGHT)
		vdev->height = VFS_IMG_MAX_HEIGHT;

	fp_dbg("image height after screen = %d", vdev->height);

	/* Scan image and remove noise */
	for (y = vdev->bottom; y <= top; y++)
		for (x = 6; x < VFS_IMG_WIDTH + 6; x++)
			if (vdev->buffer[offset(x, y)] > VFS_IMG_MIN_IMAGE_LEVEL)
				vdev->buffer[offset(x, y)] = 255;
};

/* Copy image from reader buffer and put it into image data */
static void img_copy(struct vfs101_dev *vdev, struct fp_img *img)
{
	unsigned int line;
	unsigned char *img_buffer = img->data;
	unsigned char *vdev_buffer = vdev->buffer + (vdev->bottom * VFS_FRAME_SIZE) + 6;

	for (line = 0; line < img->height; line++)
	{
		/* Copy image line from reader buffer to image data */
		memcpy(img_buffer, vdev_buffer, VFS_IMG_WIDTH);

		/* Next line of reader buffer */
		vdev_buffer = vdev_buffer + VFS_FRAME_SIZE;

		/* Next line of image buffer */
		img_buffer = img_buffer + VFS_IMG_WIDTH;
	}
}

/* Extract fingerpint image from raw data */
static void img_extract(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct vfs101_dev *vdev = dev->priv;
	struct fp_img *img;

	/* Screen image to remove noise and find top and bottom line */
	img_screen(vdev);

	/* Check image height */
	if (vdev->height < VFS_IMG_MIN_HEIGHT)
	{
		/* Image too short */
		vdev->height = 0;
		return;
	}

	/* Fingerprint is present, load image from reader */
	fpi_imgdev_report_finger_status(dev, TRUE);

	/* Create new image */
	img = fpi_img_new(vdev->height * VFS_IMG_WIDTH);
	img->width = VFS_IMG_WIDTH;
	img->height = vdev->height;
	img->flags = FP_IMG_V_FLIPPED;

	/* Copy data into image */
	img_copy(vdev, img);

	/* Notify image captured */
	fpi_imgdev_image_captured(dev, img);

	/* Check captured result */
	if (dev->action_result >= 0 &&
		dev->action_result != FP_ENROLL_RETRY &&
		dev->action_result != FP_VERIFY_RETRY)
	{
		/* Image captured, increase enroll stage */
		vdev->enroll_stage++;

		/* Check if action is completed */
		if (!action_completed(dev))
			dev->action_result = FP_ENROLL_PASS;
	}
	else
	{
		/* Image capture failed */
		if (dev->action == IMG_ACTION_ENROLL)
			/* Return retry */
			dev->action_result = result_code(dev, RESULT_RETRY);
		else
		{
			/* Return no match */
			vdev->enroll_stage++;
			dev->action_result = FP_VERIFY_NO_MATCH;
		}
	}

	/* Fingerprint is removed from reader */
	fpi_imgdev_report_finger_status(dev, FALSE);
};

/* Finger states */
enum
{
	VFS_FINGER_EMPTY,
	VFS_FINGER_PRESENT,
	VFS_FINGER_UNKNOWN,
};

/* Return finger state */
static inline int vfs_finger_state(struct vfs101_dev *vdev)
{
	/* Check finger state */
	switch (vdev->buffer[0x0a])
	{
	case 0x00:
	case 0x01:
		/* Finger is empty */
		return VFS_FINGER_EMPTY;
		break;

	case 0x02:
	case 0x03:
	case 0x04:
	case 0x05:
	case 0x06:
		/* Finger is present */
		return VFS_FINGER_PRESENT;
		break;

	default:
		return VFS_FINGER_UNKNOWN;
	}
};

/* Check contrast of image */
static void vfs_check_contrast(struct vfs101_dev *vdev)
{
	int y;
	long int count = 0;

	/* Check difference from byte 4 to byte 5 for verify contrast of image */
	for (y = 0; y < vdev->height; y++)
		count = count + vdev->buffer[offset(5, y)] - vdev->buffer[offset(4, y)];
	count = count / vdev->height;

	if (count < 16)
	{
		/* Contrast not valid, retry */
		vdev->contrast++;
		return;
	}

	fp_dbg("contrast = %d, level = %ld", vdev->contrast, count);

	if (abs(count - VFS_IMG_BEST_CONRAST) < abs(vdev->best_clevel - VFS_IMG_BEST_CONRAST))
	{
		/* Better contrast found, use it */
		vdev->best_contrast = vdev->contrast;
		vdev->best_clevel = count;
	}
}

/* Loop ssm states */
enum
{
	/* Step 0 - Scan finger */
	M_LOOP_0_GET_PRINT,
	M_LOOP_0_SLEEP,
	M_LOOP_0_GET_STATE,
	M_LOOP_0_LOAD_IMAGE,
	M_LOOP_0_EXTRACT_IMAGE,
	M_LOOP_0_CHECK_ACTION,

	/* Step 1 - Scan failed */
	M_LOOP_1_GET_STATE,
	M_LOOP_1_CHECK_STATE,
	M_LOOP_1_GET_PRINT,
	M_LOOP_1_LOAD_IMAGE,
	M_LOOP_1_LOOP,
	M_LOOP_1_SLEEP,

	/* Step 2 - Abort print */
	M_LOOP_2_ABORT_PRINT,
	M_LOOP_2_LOAD_IMAGE,

	/* Step 3 - Wait aborting */
	M_LOOP_3_GET_PRINT,
	M_LOOP_3_LOAD_IMAGE,
	M_LOOP_3_CHECK_IMAGE,
	M_LOOP_3_LOOP,

	/* Number of states */
	M_LOOP_NUM_STATES,
};

/* Exec loop sequential state machine */
static void m_loop_state(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct vfs101_dev *vdev = dev->priv;

	/* Check action state */
	if (!vdev->active)
	{
		/* Action not active, mark sequential state machine completed */
		fpi_ssm_mark_completed(ssm);
		return;
	}

	switch (ssm->cur_state)
	{
	case M_LOOP_0_GET_PRINT:
		/* Send get print command to the reader */
		vfs_get_print(ssm, VFS_BUFFER_HEIGHT, 1);
		break;

	case M_LOOP_0_SLEEP:
		/* Wait fingerprint scanning */
		async_sleep(50, ssm);
		break;

	case M_LOOP_0_GET_STATE:
		/* Get finger state */
		vfs_get_finger_state(ssm);
		break;

	case M_LOOP_0_LOAD_IMAGE:
		/* Check finger state */
		switch (vfs_finger_state(vdev))
		{
		case VFS_FINGER_EMPTY:
			/* Finger isn't present, loop */
			fpi_ssm_jump_to_state(ssm, M_LOOP_0_SLEEP);
			break;

		case VFS_FINGER_PRESENT:
			/* Load image from reader */
			vdev->ignore_error = TRUE;
			vfs_img_load(ssm);
			break;

		default:
			/* Unknown state */
			fp_err("unknown device state 0x%02x", vdev->buffer[0x0a]);
			fpi_imgdev_session_error(dev, -EIO);
			fpi_ssm_mark_aborted(ssm, -EIO);
			break;
		}
		break;

	case M_LOOP_0_EXTRACT_IMAGE:
		/* Check if image is loaded */
		if (vdev->height > 0)
			/* Fingerprint is loaded, extract image from raw data */
			img_extract(ssm);

		/* Wait handling image */
		async_sleep(10, ssm);
		break;

	case M_LOOP_0_CHECK_ACTION:
		/* Check if action is completed */
		if (action_completed(dev))
			/* Action completed */
			fpi_ssm_mark_completed(ssm);
		else
			/* Action not completed */
			if (vdev->height > 0)
				/* Continue loop */
				fpi_ssm_jump_to_state(ssm, M_LOOP_2_ABORT_PRINT);
			else
				/* Error found */
				fpi_ssm_next_state(ssm);
		break;

	case M_LOOP_1_GET_STATE:
		/* Get finger state */
		vfs_get_finger_state(ssm);
		break;

	case M_LOOP_1_CHECK_STATE:
		/* Check finger state */
		if (vfs_finger_state(vdev) == VFS_FINGER_PRESENT)
		{
			if (vdev->counter < 20)
			{
				if (vdev->counter == 1)
				{
					/* The user should remove their finger from the scanner */
					fp_warn("finger present after scan, remove it");
					fpi_imgdev_session_error(dev, result_code(dev, RESULT_RETRY_REMOVE));
				}

				/* Wait removing finger */
				vdev->counter++;
				async_sleep(250, ssm);
			}
			else
			{
				/* reach max loop counter, return protocol error */
				fp_err("finger not removed from the scanner");
				fpi_imgdev_session_error(dev, -EIO);
				fpi_ssm_mark_aborted(ssm, -EIO);
			}
		}
		else
		{
			/* Finger not present */
			if (vdev->counter == 0)
			{
				/* Check image height */
				if (vdev->height == 0)
				{
					/* Return retry to  short */
					fp_warn("image too short, retry");
					fpi_imgdev_session_error(dev, result_code(dev, RESULT_RETRY_SHORT));
				}
				else
				{
					/* Return retry result */
					fp_warn("load image failed, retry");
					fpi_imgdev_session_error(dev, result_code(dev, RESULT_RETRY));
				}
			}

			/* Continue */
			vdev->counter = 0;
			fpi_ssm_jump_to_state(ssm, M_LOOP_1_SLEEP);
		}
		break;

	case M_LOOP_1_GET_PRINT:
		/* Send get print command to the reader */
		vfs_get_print(ssm, VFS_BUFFER_HEIGHT, 1);
		break;

	case M_LOOP_1_LOAD_IMAGE:
		/* Load image */
		vdev->ignore_error = TRUE;
		vfs_img_load(ssm);
		break;

	case M_LOOP_1_LOOP:
		/* Loop */
		fpi_ssm_jump_to_state(ssm, M_LOOP_1_GET_STATE);
		break;

	case M_LOOP_1_SLEEP:
		/* Wait fingerprint scanning */
		async_sleep(10, ssm);
		break;

	case M_LOOP_2_ABORT_PRINT:
		/* Abort print command */
		vfs_abort_print(ssm);
		break;

	case M_LOOP_2_LOAD_IMAGE:
		/* Load abort image */
		vdev->ignore_error = TRUE;
		vfs_img_load(ssm);
		break;

	case M_LOOP_3_GET_PRINT:
		/* Get empty image */
		vfs_get_print(ssm, 0x000a, 0);
		break;

	case M_LOOP_3_LOAD_IMAGE:
		/* Load abort image */
		vdev->ignore_error = TRUE;
		vfs_img_load(ssm);
		break;

	case M_LOOP_3_CHECK_IMAGE:
		if (vdev->height == 10)
		{
			/* Image load correctly, jump to step 0 */
			vdev->counter = 0;
			fpi_ssm_jump_to_state(ssm, M_LOOP_0_GET_PRINT);
		}
		else if (vdev->counter < 10)
		{
			/* Wait aborting */
			vdev->counter++;
			async_sleep(100, ssm);
		}
		else
		{
			/* reach max loop counter, return protocol error */
			fp_err("waiting abort reach max loop counter");
			fpi_imgdev_session_error(dev, -EIO);
			fpi_ssm_mark_aborted(ssm, -EIO);
		}
		break;

	case M_LOOP_3_LOOP:
		/* Loop */
		fpi_ssm_jump_to_state(ssm, M_LOOP_3_GET_PRINT);
		break;
	}
}

/* Complete loop sequential state machine */
static void m_loop_complete(struct fpi_ssm *ssm)
{
	/* Free sequential state machine */
	fpi_ssm_free(ssm);
}

/* Init ssm states */
enum
{
	/* Step 0 - Cleanup device buffer */
	M_INIT_0_RECV_DIRTY,
	M_INIT_0_ABORT_PRINT,
	M_INIT_0_LOAD_IMAGE,

	/* Step 1 - Wait aborting */
	M_INIT_1_GET_PRINT,
	M_INIT_1_LOAD_IMAGE,
	M_INIT_1_CHECK_IMAGE,
	M_INIT_1_LOOP,

	/* Step 2 - Handle unexpected finger presence */
	M_INIT_2_GET_STATE,
	M_INIT_2_CHECK_STATE,
	M_INIT_2_GET_PRINT,
	M_INIT_2_LOAD_IMAGE,
	M_INIT_2_LOOP,

	/* Step 3 - Set parameters */
	M_INIT_3_SET_000E,
	M_INIT_3_SET_0011,
	M_INIT_3_SET_0076,
	M_INIT_3_SET_0078,
	M_INIT_3_SET_THRESHOLD,
	M_INIT_3_SET_STATE3_COUNT,
	M_INIT_3_SET_STATE5_COUNT,
	M_INIT_3_SET_INFO_CONTRAST,
	M_INIT_3_SET_INFO_RATE,

	/* Step 4 - Autocalibrate contrast */
	M_INIT_4_SET_EXPOSURE,
	M_INIT_4_SET_CONTRAST,
	M_INIT_4_GET_PRINT,
	M_INIT_4_LOAD_IMAGE,
	M_INIT_4_CHECK_CONTRAST,

	/* Step 5 - Set info line parameters */
	M_INIT_5_SET_EXPOSURE,
	M_INIT_5_SET_CONTRAST,
	M_INIT_5_SET_INFO_CONTRAST,
	M_INIT_5_SET_INFO_RATE,

	/* Number of states */
	M_INIT_NUM_STATES,
};

/* Exec init sequential state machine */
static void m_init_state(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct vfs101_dev *vdev = dev->priv;

	/* Check action state */
	if (!vdev->active)
	{
		/* Action not active, mark sequential state machine completed */
		fpi_ssm_mark_completed(ssm);
		return;
	}

	switch (ssm->cur_state)
	{
	case M_INIT_0_RECV_DIRTY:
		/* Recv eventualy dirty data */
		vdev->ignore_error = TRUE;
		async_recv(ssm);
		break;

	case M_INIT_0_ABORT_PRINT:
		/* Abort print command */
		vfs_abort_print(ssm);
		break;

	case M_INIT_0_LOAD_IMAGE:
		/* Load abort image */
		vdev->ignore_error = TRUE;
		vfs_img_load(ssm);
		break;

	case M_INIT_1_GET_PRINT:
		/* Get empty image */
		vfs_get_print(ssm, 0x000a, 0);
		break;

	case M_INIT_1_LOAD_IMAGE:
		/* Load abort image */
		vdev->ignore_error = TRUE;
		vfs_img_load(ssm);
		break;

	case M_INIT_1_CHECK_IMAGE:
		if (vdev->height == 10)
		{
			/* Image load correctly, jump to step 2 */
			vdev->counter = 0;
			fpi_ssm_jump_to_state(ssm, M_INIT_2_GET_STATE);
		}
		else if (vdev->counter < 10)
		{
			/* Wait aborting */
			vdev->counter++;
			async_sleep(100, ssm);
		}
		else
		{
			/* reach max loop counter, return protocol error */
			fp_err("waiting abort reach max loop counter");
			fpi_imgdev_session_error(dev, -EIO);
			fpi_ssm_mark_aborted(ssm, -EIO);
		}
		break;

	case M_INIT_1_LOOP:
		/* Loop */
		fpi_ssm_jump_to_state(ssm, M_INIT_1_GET_PRINT);
		break;

	case M_INIT_2_GET_STATE:
		/* Get finger state */
		vfs_get_finger_state(ssm);
		break;

	case M_INIT_2_CHECK_STATE:
		/* Check finger state */
		if (vfs_finger_state(vdev) == VFS_FINGER_PRESENT)
		{
			if (vdev->counter < 20)
			{
				if (vdev->counter == 2)
				{
					/* The user should remove their finger from the scanner */
					fp_warn("unexpected finger find, remove finger from the scanner");
					fpi_imgdev_session_error(dev, result_code(dev, RESULT_RETRY_REMOVE));
				}

				/* Wait removing finger */
				vdev->counter++;
				async_sleep(250, ssm);
			}
			else
			{
				/* reach max loop counter, return protocol error */
				fp_err("finger not removed from the scanner");
				fpi_imgdev_session_error(dev, -EIO);
				fpi_ssm_mark_aborted(ssm, -EIO);
			}
		}
		else
		{
			/* Finger not present */
			if (vdev->counter == 0)
				/* Continue */
				fpi_ssm_jump_to_state(ssm, M_INIT_3_SET_000E);
			else
			{
				/* Finger removed, jump to abort */
				vdev->counter = 0;
				fpi_ssm_jump_to_state(ssm, M_INIT_0_ABORT_PRINT);
			}
		}
		break;

	case M_INIT_2_GET_PRINT:
		/* Send get print command to the reader */
		vfs_get_print(ssm, VFS_BUFFER_HEIGHT, 1);
		break;

	case M_INIT_2_LOAD_IMAGE:
		/* Load unexpected image */
		vdev->ignore_error = TRUE;
		vfs_img_load(ssm);
		break;

	case M_INIT_2_LOOP:
		/* Loop */
		fpi_ssm_jump_to_state(ssm, M_INIT_2_GET_STATE);
		break;

	case M_INIT_3_SET_000E:
		/* Set param 0x000e, required for take image */
		vfs_set_param(ssm, VFS_PAR_000E, VFS_VAL_000E);
		break;

	case M_INIT_3_SET_0011:
		/* Set param 0x0011, required for take image */
		vfs_set_param(ssm, VFS_PAR_0011, VFS_VAL_0011);
		break;

	case M_INIT_3_SET_0076:
		/* Set param 0x0076, required for use info line */
		vfs_set_param(ssm, VFS_PAR_0076, VFS_VAL_0076);
		break;

	case M_INIT_3_SET_0078:
		/* Set param 0x0078, required for use info line */
		vfs_set_param(ssm, VFS_PAR_0078, VFS_VAL_0078);
		break;

	case M_INIT_3_SET_THRESHOLD:
		/* Set threshold */
		vfs_set_param(ssm, VFS_PAR_THRESHOLD, VFS_VAL_THRESHOLD);
		break;

	case M_INIT_3_SET_STATE3_COUNT:
		/* Set state 3 count */
		vfs_set_param(ssm, VFS_PAR_STATE_3, VFS_VAL_STATE_3);
		break;

	case M_INIT_3_SET_STATE5_COUNT:
		/* Set state 5 count */
		vfs_set_param(ssm, VFS_PAR_STATE_5, VFS_VAL_STATE_5);
		break;

	case M_INIT_3_SET_INFO_CONTRAST:
		/* Set info line contrast */
		vfs_set_param(ssm, VFS_PAR_INFO_CONTRAST, 10);
		break;

	case M_INIT_3_SET_INFO_RATE:
		/* Set info line rate */
		vfs_set_param(ssm, VFS_PAR_INFO_RATE, 32);
		break;

	case M_INIT_4_SET_EXPOSURE:
		/* Set exposure level of reader */
		vfs_poke(ssm, VFS_REG_IMG_EXPOSURE, 0x4000, 0x02);
		vdev->counter = 1;
		break;

	case M_INIT_4_SET_CONTRAST:
		/* Set contrast level of reader */
		vfs_poke(ssm, VFS_REG_IMG_CONTRAST, vdev->contrast, 0x01);
		break;

	case M_INIT_4_GET_PRINT:
		/* Get empty image */
		vfs_get_print(ssm, 0x000a, 0);
		break;

	case M_INIT_4_LOAD_IMAGE:
		/* Load empty image */
		vfs_img_load(ssm);
		break;

	case M_INIT_4_CHECK_CONTRAST:
		/* Check contrast */
		vfs_check_contrast(vdev);

		if (vdev->contrast <= 6 || vdev->counter >= 12)
		{
			/* End contrast scan, continue */
			vdev->contrast = vdev->best_contrast;
			vdev->counter = 0;
			fp_dbg("use contrast value = %d", vdev->contrast);
			fpi_ssm_next_state(ssm);
		}
		else
		{
			/* Continue contrast scan, loop */
			vdev->contrast--;
			vdev->counter++;
			fpi_ssm_jump_to_state(ssm, M_INIT_4_SET_CONTRAST);
		}
		break;

	case M_INIT_5_SET_EXPOSURE:
		/* Set exposure level of reader */
		vfs_poke(ssm, VFS_REG_IMG_EXPOSURE, VFS_VAL_IMG_EXPOSURE, 0x02);
		break;

	case M_INIT_5_SET_CONTRAST:
		/* Set contrast level of reader */
		vfs_poke(ssm, VFS_REG_IMG_CONTRAST, vdev->contrast, 0x01);
		break;

	case M_INIT_5_SET_INFO_CONTRAST:
		/* Set info line contrast */
		vfs_set_param(ssm, VFS_PAR_INFO_CONTRAST, vdev->contrast);
		break;

	case M_INIT_5_SET_INFO_RATE:
		/* Set info line rate */
		vfs_set_param(ssm, VFS_PAR_INFO_RATE, VFS_VAL_INFO_RATE);
		break;
	}
}

/* Complete init sequential state machine */
static void m_init_complete(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct vfs101_dev *vdev = dev->priv;
	struct fpi_ssm *ssm_loop;

	if (!ssm->error && vdev->active)
	{
		/* Notify activate complete */
		fpi_imgdev_activate_complete(dev, 0);

		/* Start loop ssm */
		ssm_loop = fpi_ssm_new(dev->dev, m_loop_state, M_LOOP_NUM_STATES);
		ssm_loop->priv = dev;
		fpi_ssm_start(ssm_loop, m_loop_complete);
	}

	/* Free sequential state machine */
	fpi_ssm_free(ssm);
}

/* Activate device */
static int dev_activate(struct fp_img_dev *dev, enum fp_imgdev_state state)
{
	struct vfs101_dev *vdev = dev->priv;
	struct fpi_ssm *ssm;

	/* Check if already active */
	if (vdev->active)
	{
		fp_err("device already activated");
		fpi_imgdev_session_error(dev, -EBUSY);
		return 1;
	}

	/* Set active state */
	vdev->active = TRUE;

	/* Set contrast */
	vdev->contrast = 15;
	vdev->best_clevel = -1;

	/* Reset loop counter and enroll stage */
	vdev->counter = 0;
	vdev->enroll_stage = 0;

	/* Start init ssm */
	ssm = fpi_ssm_new(dev->dev, m_init_state, M_INIT_NUM_STATES);
	ssm->priv = dev;
	fpi_ssm_start(ssm, m_init_complete);

	return 0;
}

/* Deactivate device */
static void dev_deactivate(struct fp_img_dev *dev)
{
	struct vfs101_dev *vdev = dev->priv;

	/* Reset active state */
	vdev->active = FALSE;

	/* Handle eventualy existing events */
	while (vdev->transfer || vdev->timeout)
		fp_handle_events();

	/* Notify deactivate complete */
	fpi_imgdev_deactivate_complete(dev);
}

/* Open device */
static int dev_open(struct fp_img_dev *dev, unsigned long driver_data)
{
	struct vfs101_dev *vdev = NULL;
	int r;

	/* Claim usb interface */
	r = libusb_claim_interface(dev->udev, 0);
	if (r < 0)
	{
		/* Interface not claimed, return error */
		fp_err("could not claim interface 0: %s", libusb_error_name(r));
		return r;
	}

	/* Initialize private structure */
	vdev = g_malloc0(sizeof(struct vfs101_dev));
	vdev->seqnum = -1;
	dev->priv = vdev;

	/* Notify open complete */
	fpi_imgdev_open_complete(dev, 0);

	return 0;
}

/* Close device */
static void dev_close(struct fp_img_dev *dev)
{
	/* Release private structure */
	g_free(dev->priv);

	/* Release usb interface */
	libusb_release_interface(dev->udev, 0);

	/* Notify close complete */
	fpi_imgdev_close_complete(dev);
}

/* Usb id table of device */
static const struct usb_id id_table[] =
{
	{ .vendor = 0x138a, .product = 0x0001 },
	{ 0, 0, 0, },
};

/* Device driver definition */
struct fp_img_driver vfs101_driver =
{
	/* Driver specification */
	.driver =
	{
		.id = VFS101_ID,
		.name = FP_COMPONENT,
		.full_name = "Validity VFS101",
		.id_table = id_table,
		.scan_type = FP_SCAN_TYPE_SWIPE,
	},

	/* Image specification */
	.flags = 0,
	.img_width = VFS_IMG_WIDTH,
	.img_height = -1,
	.bz3_threshold = 24,

	/* Routine specification */
	.open = dev_open,
	.close = dev_close,
	.activate = dev_activate,
	.deactivate = dev_deactivate,
};
