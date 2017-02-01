/*
 * UPEK TouchStrip Sensor-Only driver for libfprint
 * Copyright (C) 2008 Daniel Drake <dsd@gentoo.org>
 *
 * TCS4C (USB ID 147e:1000) support:
 * Copyright (C) 2010 Hugo Grostabussiat <dw23.devel@gmail.com>
 *
 * TCRD5B (USB ID 147e:1001) support:
 * Copyright (C) 2014 Vasily Khoruzhick <anarsoul@gmail.com>
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

#define FP_COMPONENT "upeksonly"

#include <errno.h>
#include <string.h>

#include <glib.h>
#include <libusb.h>

#include <fp_internal.h>

#include <assembling.h>

#include "upeksonly.h"
#include "driver_ids.h"

#define CTRL_TIMEOUT	1000
#define NUM_BULK_TRANSFERS 24
#define MAX_ROWS 2048
#define MIN_ROWS 64

#define BLANK_THRESHOLD 250
#define FINGER_PRESENT_THRESHOLD 32
#define FINGER_REMOVED_THRESHOLD 100
#define DIFF_THRESHOLD 13

enum {
	UPEKSONLY_2016,
	UPEKSONLY_1000,
	UPEKSONLY_1001,
};

struct img_transfer_data {
	int idx;
	struct fp_img_dev *dev;
	gboolean flying;
	gboolean cancelling;
};

enum sonly_kill_transfers_action {
	NOT_KILLING = 0,

	/* abort a SSM with an error code */
	ABORT_SSM,

	/* report an image session error */
	IMG_SESSION_ERROR,

	/* iterate a SSM to the next state */
	ITERATE_SSM,

	/* call a callback */
	EXEC_CALLBACK,
};

enum sonly_fs {
	AWAIT_FINGER,
	FINGER_DETECTED,
	FINGER_REMOVED,
};

struct sonly_dev {
	gboolean capturing;
	gboolean deactivating;
	uint8_t read_reg_result;

	int dev_model;
	int img_width;

	struct fpi_ssm *loopsm;
	struct libusb_transfer *img_transfer[NUM_BULK_TRANSFERS];
	struct img_transfer_data *img_transfer_data;
	int num_flying;

	GSList *rows;
	size_t num_rows;
	unsigned char *rowbuf;
	int rowbuf_offset;

	int wraparounds;
	int num_blank;
	int num_nonblank;
	enum sonly_fs finger_state;
	int last_seqnum;

	enum sonly_kill_transfers_action killing_transfers;
	int kill_status_code;
	union {
		struct fpi_ssm *kill_ssm;
		void (*kill_cb)(struct fp_img_dev *dev);
	};
};


/* Calculade squared standand deviation of sum of two lines */
static int upeksonly_get_deviation2(struct fpi_line_asmbl_ctx *ctx,
			  GSList *line1, GSList *line2)
{
	unsigned char *buf1 = line1->data, *buf2 = line2->data;
	int res = 0, mean = 0, i;
	for (i = 0; i < ctx->line_width; i+= 2)
		mean += (int)buf1[i + 1] + (int)buf2[i];

	mean /= (ctx->line_width / 2);

	for (i = 0; i < ctx->line_width; i+= 2) {
		int dev = (int)buf1[i + 1] + (int)buf2[i] - mean;
		res += dev*dev;
	}

	return res / (ctx->line_width / 2);
}


static unsigned char upeksonly_get_pixel(struct fpi_line_asmbl_ctx *ctx,
				   GSList *row,
				   unsigned x)
{
	unsigned char *buf;
	unsigned offset;

	/* The scans from this device are rolled right by two colums */
	if (x < ctx->line_width - 2)
		offset = x + 2;
	else if ((x > ctx->line_width - 2) && (x < ctx->line_width))
		offset = x - (ctx->line_width - 2);
	else
		return 0;
	/* Each 2nd pixel is shifted 2 pixels down */
	if ((!(x & 1)) && g_slist_next(row) && g_slist_next(g_slist_next(row)))
		buf = g_slist_next(g_slist_next(row))->data;
	else
		buf = row->data;

	return buf[offset];
}

static struct fpi_line_asmbl_ctx assembling_ctx = {
	.max_height = 1024,
	.resolution = 8,
	.median_filter_size = 25,
	.max_search_offset = 30,
	.get_deviation = upeksonly_get_deviation2,
	.get_pixel = upeksonly_get_pixel,
};

/***** IMAGE PROCESSING *****/

static void free_img_transfers(struct sonly_dev *sdev)
{
	int i;
	for (i = 0; i < NUM_BULK_TRANSFERS; i++) {
		struct libusb_transfer *transfer = sdev->img_transfer[i];
		if (!transfer)
			continue;

		g_free(transfer->buffer);
		libusb_free_transfer(transfer);
	}
	g_free(sdev->img_transfer_data);
}

static void last_transfer_killed(struct fp_img_dev *dev)
{
	struct sonly_dev *sdev = dev->priv;
	switch (sdev->killing_transfers) {
	case ABORT_SSM:
		fp_dbg("abort ssm error %d", sdev->kill_status_code);
		fpi_ssm_mark_aborted(sdev->kill_ssm, sdev->kill_status_code);
		return;
	case ITERATE_SSM:
		fp_dbg("iterate ssm");
		fpi_ssm_next_state(sdev->kill_ssm);
		return;
	case IMG_SESSION_ERROR:
		fp_dbg("session error %d", sdev->kill_status_code);
		fpi_imgdev_session_error(dev, sdev->kill_status_code);
		return;
	default:
		return;
	}
}

static void cancel_img_transfers(struct fp_img_dev *dev)
{
	struct sonly_dev *sdev = dev->priv;
	int i;

	if (sdev->num_flying == 0) {
		last_transfer_killed(dev);
		return;
	}

	for (i = 0; i < NUM_BULK_TRANSFERS; i++) {
		struct img_transfer_data *idata = &sdev->img_transfer_data[i];
		if (!idata->flying || idata->cancelling)
			continue;
		fp_dbg("cancelling transfer %d", i);
		int r = libusb_cancel_transfer(sdev->img_transfer[i]);
		if (r < 0)
			fp_dbg("cancel failed error %d", r);
		idata->cancelling = TRUE;
	}
}

static gboolean is_capturing(struct sonly_dev *sdev)
{
	return sdev->num_rows < MAX_ROWS && (sdev->finger_state != FINGER_REMOVED);
}

static void handoff_img(struct fp_img_dev *dev)
{
	struct sonly_dev *sdev = dev->priv;
	struct fp_img *img;

	GSList *elem = sdev->rows;

	if (!elem) {
		fp_err("no rows?");
		return;
	}

	sdev->rows = g_slist_reverse(sdev->rows);

	fp_dbg("%d rows", sdev->num_rows);
	img = fpi_assemble_lines(&assembling_ctx, sdev->rows, sdev->num_rows);

	g_slist_free_full(sdev->rows, g_free);
	sdev->rows = NULL;

	fpi_imgdev_image_captured(dev, img);
	fpi_imgdev_report_finger_status(dev, FALSE);

	sdev->killing_transfers = ITERATE_SSM;
	sdev->kill_ssm = sdev->loopsm;
	cancel_img_transfers(dev);
}

static void row_complete(struct fp_img_dev *dev)
{
	struct sonly_dev *sdev = dev->priv;
	sdev->rowbuf_offset = -1;

	if (sdev->num_rows > 0) {
		unsigned char *lastrow = sdev->rows->data;
		int std_sq_dev, mean_sq_diff;

		std_sq_dev = fpi_std_sq_dev(sdev->rowbuf, sdev->img_width);
		mean_sq_diff = fpi_mean_sq_diff_norm(lastrow, sdev->rowbuf, sdev->img_width);

		switch (sdev->finger_state) {
		case AWAIT_FINGER:
			if (sdev->deactivating) {
				sdev->killing_transfers = ITERATE_SSM;
				sdev->kill_ssm = sdev->loopsm;
				cancel_img_transfers(dev);
			}
			fp_dbg("std_sq_dev: %d", std_sq_dev);
			if (std_sq_dev > BLANK_THRESHOLD) {
				sdev->num_nonblank++;
			} else {
				sdev->num_nonblank = 0;
			}

			if (sdev->num_nonblank > FINGER_PRESENT_THRESHOLD) {
				sdev->finger_state = FINGER_DETECTED;
				fpi_imgdev_report_finger_status(dev, TRUE);
			} else {
				return;
			}
			break;
		case FINGER_DETECTED:
		case FINGER_REMOVED:
		default:
			break;
		}

		if (std_sq_dev > BLANK_THRESHOLD) {
			sdev->num_blank = 0;
		} else {
			sdev->num_blank++;
			/* Don't consider the scan complete unless theres at least
			 * MIN_ROWS recorded or very long blank read occurred.
			 *
			 * Typical problem spot: one brief touch before starting the
			 * actual scan. Happens most commonly if scan is started
			 * from before the first joint resulting in a gap after the inital touch.
			 */
			if (sdev->num_blank > FINGER_REMOVED_THRESHOLD) {
				sdev->finger_state = FINGER_REMOVED;
				fp_dbg("detected finger removal. Blank rows: %d, Full rows: %d", sdev->num_blank, sdev->num_rows);
				handoff_img(dev);
				return;
			}
		}
		fp_dbg("mean_sq_diff: %d, std_sq_dev: %d", mean_sq_diff, std_sq_dev);
		fp_dbg("num_blank: %d", sdev->num_blank);
		if (mean_sq_diff < DIFF_THRESHOLD) {
			return;
		}
	}

	switch (sdev->finger_state) {
	case AWAIT_FINGER:
		if (!sdev->num_rows) {
			sdev->rows = g_slist_prepend(sdev->rows, sdev->rowbuf);
			sdev->num_rows++;
		} else {
			return;
		}
		break;
	case FINGER_DETECTED:
	case FINGER_REMOVED:
		sdev->rows = g_slist_prepend(sdev->rows, sdev->rowbuf);
		sdev->num_rows++;
		break;
	}
	sdev->rowbuf = NULL;

	if (sdev->num_rows >= MAX_ROWS) {
		fp_dbg("row limit met");
		handoff_img(dev);
	}
}

/* add data to row buffer */
static void add_to_rowbuf(struct fp_img_dev *dev, unsigned char *data, int size)
{
	struct sonly_dev *sdev = dev->priv;

	memcpy(sdev->rowbuf + sdev->rowbuf_offset, data, size);
	sdev->rowbuf_offset += size;
	if (sdev->rowbuf_offset >= sdev->img_width)
		row_complete(dev);

}

static void start_new_row(struct sonly_dev *sdev, unsigned char *data, int size)
{
	if (!sdev->rowbuf)
		sdev->rowbuf = g_malloc(sdev->img_width);
	memcpy(sdev->rowbuf, data, size);
	sdev->rowbuf_offset = size;
}

/* returns number of bytes left to be copied into rowbuf (capped to 62)
 * or -1 if we aren't capturing anything */
static int rowbuf_remaining(struct sonly_dev *sdev)
{
	int r;

	if (sdev->rowbuf_offset == -1)
		return -1;

	r = sdev->img_width - sdev->rowbuf_offset;
	if (r > 62)
		r = 62;
	return r;
}

static void handle_packet(struct fp_img_dev *dev, unsigned char *data)
{
	struct sonly_dev *sdev = dev->priv;
	uint16_t seqnum = data[0] << 8 | data[1];
	int abs_base_addr;
	int for_rowbuf;
	int next_row_addr;
	int diff;
	unsigned char dummy_data[62];

	/* Init dummy data to something neutral */
	memset (dummy_data, 204, 62);

	data += 2; /* skip sequence number */
	if (seqnum != sdev->last_seqnum + 1) {
		if (seqnum != 0 && sdev->last_seqnum != 16383) {
			int missing_data = seqnum - sdev->last_seqnum;
			int i;
			fp_warn("lost %d packets of data between %d and %d", missing_data, sdev->last_seqnum, seqnum );

			/* Minimize distortions for readers that lose a lot of packets */
			for (i =1; i < missing_data; i++) {
				abs_base_addr = (sdev->last_seqnum + 1) * 62;

				/* If possible take the replacement data from last row */
				if (sdev->num_rows > 1) {
					int row_left = sdev->img_width - sdev->rowbuf_offset;
					unsigned char *last_row = g_slist_nth_data (sdev->rows, 0);

					if (row_left >= 62) {
						memcpy(dummy_data, last_row + sdev->rowbuf_offset, 62);
					} else {
						memcpy(dummy_data, last_row + sdev->rowbuf_offset, row_left);
						memcpy(dummy_data + row_left, last_row , 62 - row_left);
					}
				}

				fp_warn("adding dummy input for %d, i=%d", sdev->last_seqnum + i, i);
				for_rowbuf = rowbuf_remaining(sdev);
				if (for_rowbuf != -1) {
					add_to_rowbuf(dev, dummy_data, for_rowbuf);
					/* row boundary */
					if (for_rowbuf < 62) {
						start_new_row(sdev, dummy_data + for_rowbuf, 62 - for_rowbuf);
					}
				} else if (abs_base_addr % sdev->img_width == 0) {
					start_new_row(sdev, dummy_data, 62);
				} else {
					/* does the data in the packet reside on a row boundary?
					 * if so capture it */
					next_row_addr = ((abs_base_addr / sdev->img_width) + 1) * sdev->img_width;
					diff = next_row_addr - abs_base_addr;
					if (diff < 62)
						start_new_row(sdev, dummy_data + diff, 62 - diff);
				}
				sdev->last_seqnum = sdev->last_seqnum + 1;
			}
		}
	}
	if (seqnum <= sdev->last_seqnum) {
		fp_dbg("detected wraparound");
		sdev->wraparounds++;
	}

	sdev->last_seqnum = seqnum;
	seqnum += sdev->wraparounds * 16384;
	abs_base_addr = seqnum * 62;

	/* are we already capturing a row? if so append the data to the
	 * row buffer */
	for_rowbuf = rowbuf_remaining(sdev);
	if (for_rowbuf != -1) {
		add_to_rowbuf(dev, data, for_rowbuf);
		/*row boundary*/
		if (for_rowbuf < 62) {
			start_new_row(sdev, data + for_rowbuf, 62 - for_rowbuf);
		}
		return;
	}

	/* does the packet START on a boundary? if so we want it in full */
	if (abs_base_addr % sdev->img_width == 0) {
		start_new_row(sdev, data, 62);
		return;
	}

	/* does the data in the packet reside on a row boundary?
	 * if so capture it */
	next_row_addr = ((abs_base_addr / sdev->img_width) + 1) * sdev->img_width;
	diff = next_row_addr - abs_base_addr;
	if (diff < 62)
		start_new_row(sdev, data + diff, 62 - diff);
}

static void img_data_cb(struct libusb_transfer *transfer)
{
	struct img_transfer_data *idata = transfer->user_data;
	struct fp_img_dev *dev = idata->dev;
	struct sonly_dev *sdev = dev->priv;
	int i;

	idata->flying = FALSE;
	idata->cancelling = FALSE;
	sdev->num_flying--;

	if (sdev->killing_transfers) {
		if (sdev->num_flying == 0)
			last_transfer_killed(dev);

		/* don't care about error or success if we're terminating */
		return;
	}

	if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
		fp_warn("bad status %d, terminating session", transfer->status);
		sdev->killing_transfers = IMG_SESSION_ERROR;
		sdev->kill_status_code = transfer->status;
		cancel_img_transfers(dev);
	}

	/* there are 64 packets in the transfer buffer
	 * each packet is 64 bytes in length
	 * the first 2 bytes are a sequence number
	 * then there are 62 bytes for image data
	 */
	for (i = 0; i < 4096; i += 64) {
		if (!is_capturing(sdev))
			return;
		handle_packet(dev, transfer->buffer + i);
	}

	if (is_capturing(sdev)) {
		int r = libusb_submit_transfer(transfer);
		if (r < 0) {
			fp_warn("failed resubmit, error %d", r);
			sdev->killing_transfers = IMG_SESSION_ERROR;
			sdev->kill_status_code = r;
			cancel_img_transfers(dev);
			return;
		}
		sdev->num_flying++;
		idata->flying = TRUE;
	}
}

/***** STATE MACHINE HELPERS *****/

struct write_regs_data {
	struct fpi_ssm *ssm;
	struct libusb_transfer *transfer;
	const struct sonly_regwrite *regs;
	size_t num_regs;
	size_t regs_written;
};

static void write_regs_finished(struct write_regs_data *wrdata, int result)
{
	g_free(wrdata->transfer->buffer);
	libusb_free_transfer(wrdata->transfer);
	if (result == 0)
		fpi_ssm_next_state(wrdata->ssm);
	else
		fpi_ssm_mark_aborted(wrdata->ssm, result);
	g_free(wrdata);
}


static void write_regs_iterate(struct write_regs_data *wrdata)
{
	struct libusb_control_setup *setup;
	const struct sonly_regwrite *regwrite;
	int r;

	if (wrdata->regs_written >= wrdata->num_regs) {
		write_regs_finished(wrdata, 0);
		return;
	}

	regwrite = &wrdata->regs[wrdata->regs_written];

	fp_dbg("set %02x=%02x", regwrite->reg, regwrite->value);
	setup = libusb_control_transfer_get_setup(wrdata->transfer);
	setup->wIndex = regwrite->reg;
	wrdata->transfer->buffer[LIBUSB_CONTROL_SETUP_SIZE] = regwrite->value;

	r = libusb_submit_transfer(wrdata->transfer);
	if (r < 0)
		write_regs_finished(wrdata, r);
}

static void write_regs_cb(struct libusb_transfer *transfer)
{
	struct write_regs_data *wrdata = transfer->user_data;
	if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
		write_regs_finished(wrdata, transfer->status);
		return;
	}

	wrdata->regs_written++;
	write_regs_iterate(wrdata);
}

static void sm_write_regs(struct fpi_ssm *ssm,
	const struct sonly_regwrite *regs, size_t num_regs)
{
	struct write_regs_data *wrdata = g_malloc(sizeof(*wrdata));
	unsigned char *data;

	wrdata->transfer = libusb_alloc_transfer(0);
	if (!wrdata->transfer) {
		g_free(wrdata);
		fpi_ssm_mark_aborted(ssm, -ENOMEM);
		return;
	}

	data = g_malloc(LIBUSB_CONTROL_SETUP_SIZE + 1);
	libusb_fill_control_setup(data, 0x40, 0x0c, 0, 0, 1);
	libusb_fill_control_transfer(wrdata->transfer, ssm->dev->udev, data,
		write_regs_cb, wrdata, CTRL_TIMEOUT);
	wrdata->transfer->flags = LIBUSB_TRANSFER_SHORT_NOT_OK;

	wrdata->ssm = ssm;
	wrdata->regs = regs;
	wrdata->num_regs = num_regs;
	wrdata->regs_written = 0;
	write_regs_iterate(wrdata);
}

static void sm_write_reg_cb(struct libusb_transfer *transfer)
{
	struct fpi_ssm *ssm = transfer->user_data;
	g_free(transfer->buffer);
	if (transfer->status != LIBUSB_TRANSFER_COMPLETED)
		fpi_ssm_mark_aborted(ssm, -EIO);
	else
		fpi_ssm_next_state(ssm);

}

static void sm_write_reg(struct fpi_ssm *ssm, uint8_t reg, uint8_t value)
{
	struct fp_img_dev *dev = ssm->priv;
	struct libusb_transfer *transfer = libusb_alloc_transfer(0);
	unsigned char *data;
	int r;

	if (!transfer) {
		fpi_ssm_mark_aborted(ssm, -ENOMEM);
		return;
	}

	fp_dbg("set %02x=%02x", reg, value);
	data = g_malloc(LIBUSB_CONTROL_SETUP_SIZE + 1);
	libusb_fill_control_setup(data, 0x40, 0x0c, 0, reg, 1);
	libusb_fill_control_transfer(transfer, dev->udev, data, sm_write_reg_cb,
		ssm, CTRL_TIMEOUT);

	data[LIBUSB_CONTROL_SETUP_SIZE] = value;
	transfer->flags = LIBUSB_TRANSFER_SHORT_NOT_OK |
		LIBUSB_TRANSFER_FREE_TRANSFER;

	r = libusb_submit_transfer(transfer);
	if (r < 0) {
		g_free(data);
		libusb_free_transfer(transfer);
		fpi_ssm_mark_aborted(ssm, r);
	}
}

static void sm_read_reg_cb(struct libusb_transfer *transfer)
{
	struct fpi_ssm *ssm = transfer->user_data;
	struct fp_img_dev *dev = ssm->priv;
	struct sonly_dev *sdev = dev->priv;

	if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
		fpi_ssm_mark_aborted(ssm, -EIO);
	} else {
		sdev->read_reg_result = libusb_control_transfer_get_data(transfer)[0];
		fp_dbg("read reg result = %02x", sdev->read_reg_result);
		fpi_ssm_next_state(ssm);
	}

	g_free(transfer->buffer);
}

static void sm_read_reg(struct fpi_ssm *ssm, uint8_t reg)
{
	struct fp_img_dev *dev = ssm->priv;
	struct libusb_transfer *transfer = libusb_alloc_transfer(0);
	unsigned char *data;
	int r;

	if (!transfer) {
		fpi_ssm_mark_aborted(ssm, -ENOMEM);
		return;
	}

	fp_dbg("read reg %02x", reg);
	data = g_malloc(LIBUSB_CONTROL_SETUP_SIZE + 8);
	libusb_fill_control_setup(data, 0xc0, 0x0c, 0, reg, 8);
	libusb_fill_control_transfer(transfer, dev->udev, data, sm_read_reg_cb,
		ssm, CTRL_TIMEOUT);
	transfer->flags = LIBUSB_TRANSFER_SHORT_NOT_OK |
		LIBUSB_TRANSFER_FREE_TRANSFER;

	r = libusb_submit_transfer(transfer);
	if (r < 0) {
		g_free(data);
		libusb_free_transfer(transfer);
		fpi_ssm_mark_aborted(ssm, r);
	}
}

static void sm_await_intr_cb(struct libusb_transfer *transfer)
{
	struct fpi_ssm *ssm = transfer->user_data;
	struct fp_img_dev *dev = ssm->priv;
	struct sonly_dev *sdev = dev->priv;

	if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
		g_free(transfer->buffer);
		fpi_ssm_mark_aborted(ssm, transfer->status);
		return;
	}

	fp_dbg("interrupt received: %02x %02x %02x %02x",
		transfer->buffer[0], transfer->buffer[1],
		transfer->buffer[2], transfer->buffer[3]);
	g_free(transfer->buffer);

	sdev->finger_state = FINGER_DETECTED;
	fpi_imgdev_report_finger_status(dev, TRUE);
	fpi_ssm_next_state(ssm);
}

static void sm_await_intr(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct libusb_transfer *transfer = libusb_alloc_transfer(0);
	unsigned char *data;
	int r;

	if (!transfer) {
		fpi_ssm_mark_aborted(ssm, -ENOMEM);
		return;
	}

	fp_dbg("");
	data = g_malloc(4);
	libusb_fill_interrupt_transfer(transfer, dev->udev, 0x83, data, 4,
		sm_await_intr_cb, ssm, 0);
	transfer->flags = LIBUSB_TRANSFER_SHORT_NOT_OK |
		LIBUSB_TRANSFER_FREE_TRANSFER;

	r = libusb_submit_transfer(transfer);
	if (r < 0) {
		libusb_free_transfer(transfer);
		g_free(data);
		fpi_ssm_mark_aborted(ssm, r);
	}
}

/***** AWAIT FINGER *****/

enum awfsm_2016_states {
	AWFSM_2016_WRITEV_1,
	AWFSM_2016_READ_01,
	AWFSM_2016_WRITE_01,
	AWFSM_2016_WRITEV_2,
	AWFSM_2016_READ_13,
	AWFSM_2016_WRITE_13,
	AWFSM_2016_WRITEV_3,
	AWFSM_2016_READ_07,
	AWFSM_2016_WRITE_07,
	AWFSM_2016_WRITEV_4,
	AWFSM_2016_NUM_STATES,
};

enum awfsm_1000_states {
	AWFSM_1000_WRITEV_1,
	AWFSM_1000_WRITEV_2,
	AWFSM_1000_NUM_STATES,
};

static void awfsm_2016_run_state(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct sonly_dev *sdev = dev->priv;

	switch (ssm->cur_state) {
	case AWFSM_2016_WRITEV_1:
		sm_write_regs(ssm, awfsm_2016_writev_1, G_N_ELEMENTS(awfsm_2016_writev_1));
		break;
	case AWFSM_2016_READ_01:
		sm_read_reg(ssm, 0x01);
		break;
	case AWFSM_2016_WRITE_01:
		if (sdev->read_reg_result != 0xc6)
			sm_write_reg(ssm, 0x01, 0x46);
		else
			sm_write_reg(ssm, 0x01, 0xc6);
		break;
	case AWFSM_2016_WRITEV_2:
		sm_write_regs(ssm, awfsm_2016_writev_2, G_N_ELEMENTS(awfsm_2016_writev_2));
		break;
	case AWFSM_2016_READ_13:
		sm_read_reg(ssm, 0x13);
		break;
	case AWFSM_2016_WRITE_13:
		if (sdev->read_reg_result != 0x45)
			sm_write_reg(ssm, 0x13, 0x05);
		else
			sm_write_reg(ssm, 0x13, 0x45);
		break;
	case AWFSM_2016_WRITEV_3:
		sm_write_regs(ssm, awfsm_2016_writev_3, G_N_ELEMENTS(awfsm_2016_writev_3));
		break;
	case AWFSM_2016_READ_07:
		sm_read_reg(ssm, 0x07);
		break;
	case AWFSM_2016_WRITE_07:
		if (sdev->read_reg_result != 0x10 && sdev->read_reg_result != 0x90)
			fp_warn("odd reg7 value %x", sdev->read_reg_result);
		sm_write_reg(ssm, 0x07, sdev->read_reg_result);
		break;
	case AWFSM_2016_WRITEV_4:
		sm_write_regs(ssm, awfsm_2016_writev_4, G_N_ELEMENTS(awfsm_2016_writev_4));
		break;
	}
}

static void awfsm_1000_run_state(struct fpi_ssm *ssm)
{
	switch (ssm->cur_state) {
	case AWFSM_1000_WRITEV_1:
		sm_write_regs(ssm, awfsm_1000_writev_1, G_N_ELEMENTS(awfsm_1000_writev_1));
		break;
	case AWFSM_1000_WRITEV_2:
		sm_write_regs(ssm, awfsm_1000_writev_2, G_N_ELEMENTS(awfsm_1000_writev_2));
		break;
	}
}

/***** CAPTURE MODE *****/

enum capsm_2016_states {
	CAPSM_2016_INIT,
	CAPSM_2016_WRITE_15,
	CAPSM_2016_WRITE_30,
	CAPSM_2016_FIRE_BULK,
	CAPSM_2016_WRITEV,
	CAPSM_2016_NUM_STATES,
};

enum capsm_1000_states {
	CAPSM_1000_INIT,
	CAPSM_1000_FIRE_BULK,
	CAPSM_1000_WRITEV,
	CAPSM_1000_NUM_STATES,
};

enum capsm_1001_states {
	CAPSM_1001_INIT,
	CAPSM_1001_FIRE_BULK,
	CAPSM_1001_WRITEV_1,
	CAPSM_1001_WRITEV_2,
	CAPSM_1001_WRITEV_3,
	CAPSM_1001_WRITEV_4,
	CAPSM_1001_WRITEV_5,
	CAPSM_1001_NUM_STATES,
};

static void capsm_fire_bulk(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct sonly_dev *sdev = dev->priv;
	int i;
	for (i = 0; i < NUM_BULK_TRANSFERS; i++) {
		int r = libusb_submit_transfer(sdev->img_transfer[i]);
		if (r < 0) {
			if (i == 0) {
				/* first one failed: easy peasy */
				fpi_ssm_mark_aborted(ssm, r);
				return;
			}

			/* cancel all flying transfers, and request that the SSM
			 * gets aborted when the last transfer has dropped out of
			 * the sky */
			sdev->killing_transfers = ABORT_SSM;
			sdev->kill_ssm = ssm;
			sdev->kill_status_code = r;
			cancel_img_transfers(dev);
			return;
		}
		sdev->img_transfer_data[i].flying = TRUE;
		sdev->num_flying++;
	}
	sdev->capturing = TRUE;
	fpi_ssm_next_state(ssm);
}

static void capsm_2016_run_state(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct sonly_dev *sdev = dev->priv;

	switch (ssm->cur_state) {
	case CAPSM_2016_INIT:
		sdev->rowbuf_offset = -1;
		sdev->num_rows = 0;
		sdev->wraparounds = -1;
		sdev->num_blank = 0;
		sdev->num_nonblank = 0;
		sdev->finger_state = FINGER_DETECTED;
		sdev->last_seqnum = 16383;
		sdev->killing_transfers = 0;
		fpi_ssm_next_state(ssm);
		break;
	case CAPSM_2016_WRITE_15:
		sm_write_reg(ssm, 0x15, 0x20);
		break;
	case CAPSM_2016_WRITE_30:
		sm_write_reg(ssm, 0x30, 0xe0);
		break;
	case CAPSM_2016_FIRE_BULK: ;
		capsm_fire_bulk (ssm);
		break;
	case CAPSM_2016_WRITEV:
		sm_write_regs(ssm, capsm_2016_writev, G_N_ELEMENTS(capsm_2016_writev));
		break;
	}
}

static void capsm_1000_run_state(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct sonly_dev *sdev = dev->priv;

	switch (ssm->cur_state) {
	case CAPSM_1000_INIT:
		sdev->rowbuf_offset = -1;
		sdev->num_rows = 0;
		sdev->wraparounds = -1;
		sdev->num_blank = 0;
		sdev->num_nonblank = 0;
		sdev->finger_state = FINGER_DETECTED;
		sdev->last_seqnum = 16383;
		sdev->killing_transfers = 0;
		fpi_ssm_next_state(ssm);
		break;
	case CAPSM_1000_FIRE_BULK: ;
		capsm_fire_bulk (ssm);
		break;
	case CAPSM_1000_WRITEV:
		sm_write_regs(ssm, capsm_1000_writev, G_N_ELEMENTS(capsm_1000_writev));
		break;
	}
}

static void capsm_1001_run_state(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct sonly_dev *sdev = dev->priv;

	switch (ssm->cur_state) {
	case CAPSM_1001_INIT:
		sdev->rowbuf_offset = -1;
		sdev->num_rows = 0;
		sdev->wraparounds = -1;
		sdev->num_blank = 0;
		sdev->num_nonblank = 0;
		sdev->finger_state = AWAIT_FINGER;
		sdev->last_seqnum = 16383;
		sdev->killing_transfers = 0;
		fpi_ssm_next_state(ssm);
		break;
	case CAPSM_1001_FIRE_BULK: ;
		capsm_fire_bulk (ssm);
		break;
	case CAPSM_1001_WRITEV_1:
		sm_write_regs(ssm, capsm_1001_writev_1, G_N_ELEMENTS(capsm_1001_writev_1));
		break;
	case CAPSM_1001_WRITEV_2:
		sm_write_regs(ssm, capsm_1001_writev_2, G_N_ELEMENTS(capsm_1001_writev_2));
		break;
	case CAPSM_1001_WRITEV_3:
		sm_write_regs(ssm, capsm_1001_writev_3, G_N_ELEMENTS(capsm_1001_writev_3));
		break;
	case CAPSM_1001_WRITEV_4:
		sm_write_regs(ssm, capsm_1001_writev_4, G_N_ELEMENTS(capsm_1001_writev_4));
		break;
	case CAPSM_1001_WRITEV_5:
		sm_write_regs(ssm, capsm_1001_writev_5, G_N_ELEMENTS(capsm_1001_writev_5));
		break;
	}
}

/***** DEINITIALIZATION *****/

enum deinitsm_2016_states {
	DEINITSM_2016_WRITEV,
	DEINITSM_2016_NUM_STATES,
};

enum deinitsm_1000_states {
	DEINITSM_1000_WRITEV,
	DEINITSM_1000_NUM_STATES,
};

enum deinitsm_1001_states {
	DEINITSM_1001_WRITEV,
	DEINITSM_1001_NUM_STATES,
};

static void deinitsm_2016_run_state(struct fpi_ssm *ssm)
{
	switch (ssm->cur_state) {
	case DEINITSM_2016_WRITEV:
		sm_write_regs(ssm, deinitsm_2016_writev, G_N_ELEMENTS(deinitsm_2016_writev));
		break;
	}
}

static void deinitsm_1000_run_state(struct fpi_ssm *ssm)
{
	switch (ssm->cur_state) {
	case DEINITSM_1000_WRITEV:
		sm_write_regs(ssm, deinitsm_1000_writev, G_N_ELEMENTS(deinitsm_1000_writev));
		break;
	}
}

static void deinitsm_1001_run_state(struct fpi_ssm *ssm)
{
	switch (ssm->cur_state) {
	case DEINITSM_1001_WRITEV:
		sm_write_regs(ssm, deinitsm_1001_writev, G_N_ELEMENTS(deinitsm_1001_writev));
		break;
	}
}

/***** INITIALIZATION *****/

enum initsm_2016_states {
	INITSM_2016_WRITEV_1,
	INITSM_2016_READ_09,
	INITSM_2016_WRITE_09,
	INITSM_2016_READ_13,
	INITSM_2016_WRITE_13,
	INITSM_2016_WRITE_04,
	INITSM_2016_WRITE_05,
	INITSM_2016_NUM_STATES,
};

enum initsm_1000_states {
	INITSM_1000_WRITEV_1,
	INITSM_1000_NUM_STATES,
};

enum initsm_1001_states {
	INITSM_1001_WRITEV_1,
	INITSM_1001_WRITEV_2,
	INITSM_1001_WRITEV_3,
	INITSM_1001_WRITEV_4,
	INITSM_1001_WRITEV_5,
	INITSM_1001_NUM_STATES,
};

static void initsm_2016_run_state(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct sonly_dev *sdev = dev->priv;

	switch (ssm->cur_state) {
	case INITSM_2016_WRITEV_1:
		sm_write_regs(ssm, initsm_2016_writev_1, G_N_ELEMENTS(initsm_2016_writev_1));
		break;
	case INITSM_2016_READ_09:
		sm_read_reg(ssm, 0x09);
		break;
	case INITSM_2016_WRITE_09:
		sm_write_reg(ssm, 0x09, sdev->read_reg_result & ~0x08);
		break;
	case INITSM_2016_READ_13:
		sm_read_reg(ssm, 0x13);
		break;
	case INITSM_2016_WRITE_13:
		sm_write_reg(ssm, 0x13, sdev->read_reg_result & ~0x10);
		break;
	case INITSM_2016_WRITE_04:
		sm_write_reg(ssm, 0x04, 0x00);
		break;
	case INITSM_2016_WRITE_05:
		sm_write_reg(ssm, 0x05, 0x00);
		break;
	}
}

static void initsm_1000_run_state(struct fpi_ssm *ssm)
{
	switch (ssm->cur_state) {
	case INITSM_1000_WRITEV_1:
		sm_write_regs(ssm, initsm_1000_writev_1, G_N_ELEMENTS(initsm_1000_writev_1));
		break;
	}
}

static void initsm_1001_run_state(struct fpi_ssm *ssm)
{
	switch (ssm->cur_state) {
	case INITSM_1001_WRITEV_1:
		sm_write_regs(ssm, initsm_1001_writev_1, G_N_ELEMENTS(initsm_1001_writev_1));
		break;
	case INITSM_1001_WRITEV_2:
		sm_write_regs(ssm, initsm_1001_writev_2, G_N_ELEMENTS(initsm_1001_writev_2));
		break;
	case INITSM_1001_WRITEV_3:
		sm_write_regs(ssm, initsm_1001_writev_3, G_N_ELEMENTS(initsm_1001_writev_3));
		break;
	case INITSM_1001_WRITEV_4:
		sm_write_regs(ssm, initsm_1001_writev_4, G_N_ELEMENTS(initsm_1001_writev_4));
		break;
	case INITSM_1001_WRITEV_5:
		sm_write_regs(ssm, initsm_1001_writev_5, G_N_ELEMENTS(initsm_1001_writev_5));
		break;
	}
}

/***** CAPTURE LOOP *****/

enum loopsm_states {
	LOOPSM_RUN_AWFSM,
	LOOPSM_AWAIT_FINGER,
	LOOPSM_RUN_CAPSM,
	LOOPSM_CAPTURE,
	LOOPSM_RUN_DEINITSM,
	LOOPSM_FINAL,
	LOOPSM_NUM_STATES,
};

static void loopsm_run_state(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct sonly_dev *sdev = dev->priv;

	switch (ssm->cur_state) {
	case LOOPSM_RUN_AWFSM: ;
		switch (sdev->dev_model) {
		case UPEKSONLY_1001:
			if (sdev->deactivating) {
				fpi_ssm_mark_completed(ssm);
			} else {
				fpi_ssm_next_state(ssm);
			}
		break;
		default:
			if (sdev->deactivating) {
				fpi_ssm_mark_completed(ssm);
			} else {
				struct fpi_ssm *awfsm = NULL;
				switch (sdev->dev_model) {
				case UPEKSONLY_2016:
					awfsm = fpi_ssm_new(dev->dev, awfsm_2016_run_state,
						AWFSM_2016_NUM_STATES);
					break;
				case UPEKSONLY_1000:
					awfsm = fpi_ssm_new(dev->dev, awfsm_1000_run_state,
						AWFSM_1000_NUM_STATES);
					break;
				}
				awfsm->priv = dev;
				fpi_ssm_start_subsm(ssm, awfsm);
			}
			break;
		}
		break;
	case LOOPSM_AWAIT_FINGER:
		switch (sdev->dev_model) {
		case UPEKSONLY_1001:
			fpi_ssm_next_state(ssm);
			break;
		default:
			sm_await_intr(ssm);
			break;
		}
		break;
	case LOOPSM_RUN_CAPSM: ;
		struct fpi_ssm *capsm = NULL;
		switch (sdev->dev_model) {
		case UPEKSONLY_2016:
			capsm = fpi_ssm_new(dev->dev, capsm_2016_run_state,
				CAPSM_2016_NUM_STATES);
			break;
		case UPEKSONLY_1000:
			capsm = fpi_ssm_new(dev->dev, capsm_1000_run_state,
				CAPSM_1000_NUM_STATES);
			break;
		case UPEKSONLY_1001:
			capsm = fpi_ssm_new(dev->dev, capsm_1001_run_state,
				CAPSM_1001_NUM_STATES);
			break;
		}
		capsm->priv = dev;
		fpi_ssm_start_subsm(ssm, capsm);
		break;
	case LOOPSM_CAPTURE:
		break;
	case LOOPSM_RUN_DEINITSM: ;
		struct fpi_ssm *deinitsm = NULL;
		switch (sdev->dev_model) {
		case UPEKSONLY_2016:
			deinitsm = fpi_ssm_new(dev->dev, deinitsm_2016_run_state,
				DEINITSM_2016_NUM_STATES);
			break;
		case UPEKSONLY_1000:
			deinitsm = fpi_ssm_new(dev->dev, deinitsm_1000_run_state,
				DEINITSM_1000_NUM_STATES);
			break;
		case UPEKSONLY_1001:
			deinitsm = fpi_ssm_new(dev->dev, deinitsm_1001_run_state,
				DEINITSM_1001_NUM_STATES);
			break;
		}
		sdev->capturing = FALSE;
		deinitsm->priv = dev;
		fpi_ssm_start_subsm(ssm, deinitsm);
		break;
	case LOOPSM_FINAL:
		fpi_ssm_jump_to_state(ssm, LOOPSM_RUN_AWFSM);
		break;
	}
		
}

/***** DRIVER STUFF *****/

static void deactivate_done(struct fp_img_dev *dev)
{
	struct sonly_dev *sdev = dev->priv;

	fp_dbg("");
	free_img_transfers(sdev);
	g_free(sdev->rowbuf);
	sdev->rowbuf = NULL;

	if (sdev->rows) {
		g_slist_foreach(sdev->rows, (GFunc) g_free, NULL);
		sdev->rows = NULL;
	}

	fpi_imgdev_deactivate_complete(dev);
}

static void dev_deactivate(struct fp_img_dev *dev)
{
	struct sonly_dev *sdev = dev->priv;

	if (!sdev->capturing) {
		deactivate_done(dev);
		return;
	}

	sdev->deactivating = TRUE;
	sdev->killing_transfers = ITERATE_SSM;
	sdev->kill_ssm = sdev->loopsm;
	cancel_img_transfers(dev);
}

static void loopsm_complete(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct sonly_dev *sdev = dev->priv;
	int r = ssm->error;

	fpi_ssm_free(ssm);

	if (sdev->deactivating) {
		deactivate_done(dev);
		return;
	}

	if (r) {
		fpi_imgdev_session_error(dev, r);
		return;
	}
}

static void initsm_complete(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct sonly_dev *sdev = dev->priv;
	int r = ssm->error;

	fpi_ssm_free(ssm);
	fpi_imgdev_activate_complete(dev, r);
	if (r != 0)
		return;

	sdev->loopsm = fpi_ssm_new(dev->dev, loopsm_run_state, LOOPSM_NUM_STATES);
	sdev->loopsm->priv = dev;
	fpi_ssm_start(sdev->loopsm, loopsm_complete);
}

static int dev_activate(struct fp_img_dev *dev, enum fp_imgdev_state state)
{
	struct sonly_dev *sdev = dev->priv;
	struct fpi_ssm *ssm = NULL;
	int i;

	sdev->deactivating = FALSE;
	sdev->capturing = FALSE;

	memset(sdev->img_transfer, 0,
		NUM_BULK_TRANSFERS * sizeof(struct libusb_transfer *));
	sdev->img_transfer_data =
		g_malloc0(sizeof(struct img_transfer_data) * NUM_BULK_TRANSFERS);
	sdev->num_flying = 0;
	for (i = 0; i < NUM_BULK_TRANSFERS; i++) {
		unsigned char *data;
		sdev->img_transfer[i] = libusb_alloc_transfer(0);
		if (!sdev->img_transfer[i]) {
			free_img_transfers(sdev);
			return -ENOMEM;
		}
		sdev->img_transfer_data[i].idx = i;
		sdev->img_transfer_data[i].dev = dev;
		data = g_malloc(4096);
		libusb_fill_bulk_transfer(sdev->img_transfer[i], dev->udev, 0x81, data,
			4096, img_data_cb, &sdev->img_transfer_data[i], 0);
	}

	switch (sdev->dev_model) {
	case UPEKSONLY_2016:
		ssm = fpi_ssm_new(dev->dev, initsm_2016_run_state, INITSM_2016_NUM_STATES);
		break;
	case UPEKSONLY_1000:
		ssm = fpi_ssm_new(dev->dev, initsm_1000_run_state, INITSM_1000_NUM_STATES);
		break;
	case UPEKSONLY_1001:
		ssm = fpi_ssm_new(dev->dev, initsm_1001_run_state, INITSM_1001_NUM_STATES);
		break;
	}
	ssm->priv = dev;
	fpi_ssm_start(ssm, initsm_complete);
	return 0;
}

static int dev_init(struct fp_img_dev *dev, unsigned long driver_data)
{
	int r;
	struct sonly_dev *sdev;

	r = libusb_set_configuration(dev->udev, 1);
	if (r < 0) {
		fp_err("could not set configuration 1");
		return r;
	}

	r = libusb_claim_interface(dev->udev, 0);
	if (r < 0) {
		fp_err("could not claim interface 0: %s", libusb_error_name(r));
		return r;
	}

	sdev = dev->priv = g_malloc0(sizeof(struct sonly_dev));
	sdev->dev_model = (int)driver_data;
	switch (driver_data) {
	case UPEKSONLY_1000:
		sdev->img_width = IMG_WIDTH_1000;
		upeksonly_driver.img_width = IMG_WIDTH_1000;
		assembling_ctx.line_width = IMG_WIDTH_1000;
		break;
	case UPEKSONLY_1001:
		sdev->img_width = IMG_WIDTH_1001;
		upeksonly_driver.img_width = IMG_WIDTH_1001;
		upeksonly_driver.bz3_threshold = 25;
		assembling_ctx.line_width = IMG_WIDTH_1001;
		break;
	case UPEKSONLY_2016:
		sdev->img_width = IMG_WIDTH_2016;
		upeksonly_driver.img_width = IMG_WIDTH_2016;
		assembling_ctx.line_width = IMG_WIDTH_2016;
		break;
	}
	fpi_imgdev_open_complete(dev, 0);
	return 0;
}

static void dev_deinit(struct fp_img_dev *dev)
{
	g_free(dev->priv);
	libusb_release_interface(dev->udev, 0);
	fpi_imgdev_close_complete(dev);
}

static int dev_discover(struct libusb_device_descriptor *dsc, uint32_t *devtype)
{
	if (dsc->idProduct == 0x2016) {
		if (dsc->bcdDevice == 1) /* Revision 1 is what we're interested in */
			return 1;
	}
	if (dsc->idProduct == 0x1000) {
		if (dsc->bcdDevice == 0x0033) /* Looking for revision 0.33 */
			return 1;
	}

	if (dsc->idProduct == 0x1001)
		return 1;

	return 0;
}

static const struct usb_id id_table[] = {
	{ .vendor = 0x147e, .product = 0x2016, .driver_data = UPEKSONLY_2016 },
	{ .vendor = 0x147e, .product = 0x1000, .driver_data = UPEKSONLY_1000 },
	{ .vendor = 0x147e, .product = 0x1001, .driver_data = UPEKSONLY_1001 },
	{ 0, 0, 0, },
};

struct fp_img_driver upeksonly_driver = {
	.driver = {
		.id = UPEKSONLY_ID,
		.name = FP_COMPONENT,
		.full_name = "UPEK TouchStrip Sensor-Only",
		.id_table = id_table,
		.scan_type = FP_SCAN_TYPE_SWIPE,
		.discover = dev_discover,
	},
	.flags = 0,
	.img_width = -1,
	.img_height = -1,

	.open = dev_init,
	.close = dev_deinit,
	.activate = dev_activate,
	.deactivate = dev_deactivate,
};

