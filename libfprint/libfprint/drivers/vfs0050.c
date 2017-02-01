/*
 * Validity VFS0050 driver for libfprint
 * Copyright (C) 2015-2016 Konstantin Semenov <zemen17@gmail.com>
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

#define FP_COMPONENT "vfs0050"

#include <errno.h>
#include <string.h>
#include <fp_internal.h>
#include <assembling.h>
#include "driver_ids.h"

#include "vfs0050.h"

/* USB functions */

/* Callback for async_write */
static void async_write_callback(struct libusb_transfer *transfer)
{
	struct fpi_ssm *ssm = transfer->user_data;
	struct fp_img_dev *idev = ssm->priv;

	int transferred = transfer->actual_length, error =
	    transfer->status, len = transfer->length;

	if (error != 0) {
		fp_err("USB write transfer: %s", libusb_error_name(error));
		fpi_imgdev_session_error(idev, -EIO);
		fpi_ssm_mark_aborted(ssm, -EIO);
		return;
	}

	if (transferred != len) {
		fp_err("Written only %d of %d bytes", transferred, len);
		fpi_imgdev_session_error(idev, -EIO);
		fpi_ssm_mark_aborted(ssm, -EIO);
		return;
	}

	fpi_ssm_next_state(ssm);
}

/* Send data to EP1, the only out endpoint */
static void async_write(struct fpi_ssm *ssm, void *data, int len)
{
	struct fp_img_dev *idev = ssm->priv;
	struct libusb_device_handle *udev = idev->udev;
	struct vfs_dev_t *vdev = idev->priv;

	vdev->transfer = libusb_alloc_transfer(0);
	vdev->transfer->flags |= LIBUSB_TRANSFER_FREE_TRANSFER;
	libusb_fill_bulk_transfer(vdev->transfer, udev, 0x01, data, len,
				  async_write_callback, ssm, VFS_USB_TIMEOUT);
	libusb_submit_transfer(vdev->transfer);
}

/* Callback for async_read */
static void async_read_callback(struct libusb_transfer *transfer)
{
	struct fpi_ssm *ssm = transfer->user_data;
	struct fp_img_dev *idev = ssm->priv;

	int transferred = transfer->actual_length, error =
	    transfer->status, len = transfer->length;
	int ep = transfer->endpoint;

	if (error != 0) {
		fp_err("USB read transfer on endpoint %d: %s", ep - 0x80,
		       libusb_error_name(error));
		fpi_imgdev_session_error(idev, -EIO);
		fpi_ssm_mark_aborted(ssm, -EIO);
		return;
	}

	if (transferred != len) {
		fp_err("Received %d instead of %d bytes", transferred, len);
		fpi_imgdev_session_error(idev, -EIO);
		fpi_ssm_mark_aborted(ssm, -EIO);
		return;
	}

	fpi_ssm_next_state(ssm);
}

/* Receive data from the given ep and compare with expected */
static void async_read(struct fpi_ssm *ssm, int ep, void *data, int len)
{
	struct fp_img_dev *idev = ssm->priv;
	struct libusb_device_handle *udev = idev->udev;
	struct vfs_dev_t *vdev = idev->priv;

	ep |= LIBUSB_ENDPOINT_IN;

	vdev->transfer = libusb_alloc_transfer(0);
	vdev->transfer->flags |= LIBUSB_TRANSFER_FREE_TRANSFER;

	/* 0x83 is the only interrupt endpoint */
	if (ep == EP3_IN)
		libusb_fill_interrupt_transfer(vdev->transfer, udev, ep, data,
					       len, async_read_callback, ssm,
					       VFS_USB_TIMEOUT);
	else
		libusb_fill_bulk_transfer(vdev->transfer, udev, ep, data, len,
					  async_read_callback, ssm,
					  VFS_USB_TIMEOUT);
	libusb_submit_transfer(vdev->transfer);
}

/* Callback for async_read */
static void async_abort_callback(struct libusb_transfer *transfer)
{
	struct fpi_ssm *ssm = transfer->user_data;
	struct fp_img_dev *idev = ssm->priv;

	int transferred = transfer->actual_length, error = transfer->status;
	int ep = transfer->endpoint;

	/* In normal case endpoint is empty */
	if (error == LIBUSB_TRANSFER_TIMED_OUT) {
		fpi_ssm_next_state(ssm);
		return;
	}

	if (error != 0) {
		fp_err("USB write transfer: %s", libusb_error_name(error));
		fpi_imgdev_session_error(idev, -EIO);
		fpi_ssm_mark_aborted(ssm, -EIO);
		return;
	}

	/* Don't stop process, only print warning */
	if (transferred > 0)
		fp_warn("Endpoint %d had extra %d bytes", ep - 0x80,
			transferred);

	fpi_ssm_jump_to_state(ssm, ssm->cur_state);
}

/* Receive data from the given ep and compare with expected */
static void async_abort(struct fpi_ssm *ssm, int ep)
{
	struct fp_img_dev *idev = ssm->priv;
	struct libusb_device_handle *udev = idev->udev;
	struct vfs_dev_t *vdev = idev->priv;

	int len = VFS_USB_BUFFER_SIZE;
	unsigned char *data = g_malloc(VFS_USB_BUFFER_SIZE);

	ep |= LIBUSB_ENDPOINT_IN;

	vdev->transfer = libusb_alloc_transfer(0);
	vdev->transfer->flags |=
	    LIBUSB_TRANSFER_FREE_TRANSFER | LIBUSB_TRANSFER_FREE_BUFFER;

	/* 0x83 is the only interrupt endpoint */
	if (ep == EP3_IN)
		libusb_fill_interrupt_transfer(vdev->transfer, udev, ep, data,
					       len, async_abort_callback, ssm,
					       VFS_USB_ABORT_TIMEOUT);
	else
		libusb_fill_bulk_transfer(vdev->transfer, udev, ep, data, len,
					  async_abort_callback, ssm,
					  VFS_USB_ABORT_TIMEOUT);
	libusb_submit_transfer(vdev->transfer);
}

/* Image processing functions */

/* Pixel getter for fpi_assemble_lines */
static unsigned char vfs0050_get_pixel(struct fpi_line_asmbl_ctx *ctx,
				       GSList * line, unsigned int x)
{
	return ((struct vfs_line *)line->data)->data[x];
}

/* Deviation getter for fpi_assemble_lines */
static int vfs0050_get_difference(struct fpi_line_asmbl_ctx *ctx,
				  GSList * line_list_1, GSList * line_list_2)
{
	struct vfs_line *line1 = line_list_1->data;
	struct vfs_line *line2 = line_list_2->data;
	const int shift = (VFS_IMAGE_WIDTH - VFS_NEXT_LINE_WIDTH) / 2 - 1;
	int res = 0;
	for (int i = 0; i < VFS_NEXT_LINE_WIDTH; ++i) {
		int x =
		    (int)line1->next_line_part[i] - (int)line2->data[shift + i];
		res += x * x;
	}
	return res;
}

#define VFS_NOISE_THRESHOLD 40

/* Checks whether line is noise or not using hardware parameters */
static char is_noise(struct vfs_line *line)
{
	int val1 = line->noise_hash_1;
	int val2 = line->noise_hash_2;
	if (val1 > VFS_NOISE_THRESHOLD
	    && val1 < 256 - VFS_NOISE_THRESHOLD
	    && val2 > VFS_NOISE_THRESHOLD && val2 < 256 - VFS_NOISE_THRESHOLD)
		return 1;
	return 0;
}

/* Parameters for fpi_assemble_lines */
static struct fpi_line_asmbl_ctx assembling_ctx = {
	.line_width = VFS_IMAGE_WIDTH,
	.max_height = VFS_MAX_HEIGHT,
	.resolution = 10,
	.median_filter_size = 25,
	.max_search_offset = 100,
	.get_deviation = vfs0050_get_difference,
	.get_pixel = vfs0050_get_pixel,
};

/* Processes image before submitting */
static struct fp_img *prepare_image(struct vfs_dev_t *vdev)
{
	int height = vdev->bytes / VFS_LINE_SIZE;

	/* Noise cleaning. IMHO, it works pretty well
	   I've not detected cases when it doesn't work or cuts a part of the finger
	   Noise arises at the end of scan when some water remains on the scanner */
	while (height > 0) {
		if (!is_noise(vdev->lines_buffer + height - 1))
			break;
		--height;
	}
	if (height > VFS_MAX_HEIGHT)
		height = VFS_MAX_HEIGHT;

	/* If image is not good enough */
	if (height < VFS_IMAGE_WIDTH)
		return NULL;

	/* Building GSList */
	GSList *lines = NULL;
	for (int i = height - 1; i >= 0; --i)
		lines = g_slist_prepend(lines, vdev->lines_buffer + i);

	/* Perform line assembling */
	struct fp_img *img = fpi_assemble_lines(&assembling_ctx, lines, height);

	g_slist_free(lines);
	return img;
}

/* Processes and submits image after fingerprint received */
static void submit_image(struct fp_img_dev *idev)
{
	struct vfs_dev_t *vdev = idev->priv;

	/* We were not asked to submit image actually */
	if (!vdev->active)
		return;

	struct fp_img *img = prepare_image(vdev);

	if (!img)
		fpi_imgdev_abort_scan(idev, FP_VERIFY_RETRY_TOO_SHORT);
	else
		fpi_imgdev_image_captured(idev, img);

	/* Finger not on the scanner */
	fpi_imgdev_report_finger_status(idev, 0);
}

/* Proto functions */

/* SSM loop for clear_ep2 */
static void clear_ep2_ssm(struct fpi_ssm *ssm)
{
	struct fp_img_dev *idev = ssm->priv;

	short result;
	char command04 = 0x04;

	switch (ssm->cur_state) {
	case SUBSM1_COMMAND_04:
		async_write(ssm, &command04, sizeof(command04));
		break;

	case SUBSM1_RETURN_CODE:
		async_read(ssm, 1, &result, sizeof(result));
		break;

	case SUBSM1_ABORT_2:
		async_abort(ssm, 2);
		break;

	default:
		fp_err("Unknown SUBSM1 state");
		fpi_imgdev_session_error(idev, -EIO);
		fpi_ssm_mark_aborted(ssm, -EIO);
	}
}

/* Send command to clear EP2 */
static void clear_ep2(struct fpi_ssm *ssm)
{
	struct fp_img_dev *idev = ssm->priv;

	struct fpi_ssm *subsm =
	    fpi_ssm_new(idev->dev, clear_ep2_ssm, SUBSM1_STATES);
	subsm->priv = idev;
	fpi_ssm_start_subsm(ssm, subsm);
}

static void send_control_packet_ssm(struct fpi_ssm *ssm)
{
	struct fp_img_dev *idev = ssm->priv;
	struct vfs_dev_t *vdev = idev->priv;

	short result;
	unsigned char *commit_result = NULL;

	switch (ssm->cur_state) {
	case SUBSM2_SEND_CONTROL:
		async_write(ssm, vdev->control_packet, VFS_CONTROL_PACKET_SIZE);
		break;

	case SUBSM2_RETURN_CODE:
		async_read(ssm, 1, &result, sizeof(result));
		break;

	case SUBSM2_SEND_COMMIT:
		/* next_receive_* packets could be sent only in pair */
		if (vdev->control_packet == next_receive_1) {
			vdev->control_packet = next_receive_2;
			fpi_ssm_jump_to_state(ssm, SUBSM2_SEND_CONTROL);
			break;
		}
		/* commit_out in Windows differs in each commit, but I send the same each time */
		async_write(ssm, commit_out, sizeof(commit_out));
		break;

	case SUBSM2_COMMIT_RESPONSE:
		commit_result = g_malloc(VFS_COMMIT_RESPONSE_SIZE);
		async_read(ssm, 1, commit_result, VFS_COMMIT_RESPONSE_SIZE);
		break;

	case SUBSM2_READ_EMPTY_INTERRUPT:
		/* I don't know how to check result, it could be different */
		g_free(commit_result);

		async_read(ssm, 3, vdev->interrupt, VFS_INTERRUPT_SIZE);
		break;

	case SUBSM2_ABORT_3:
		/* Check that interrupt is empty */
		if (memcmp
		    (vdev->interrupt, empty_interrupt, VFS_INTERRUPT_SIZE)) {
			fp_err("Unknown SUBSM2 state");
			fpi_imgdev_session_error(idev, -EIO);
			fpi_ssm_mark_aborted(ssm, -EIO);
			break;
		}
		async_abort(ssm, 3);
		break;

	case SUBSM2_CLEAR_EP2:
		/* After turn_on Windows doesn't clear EP2 */
		if (vdev->control_packet != turn_on)
			clear_ep2(ssm);
		else
			fpi_ssm_next_state(ssm);
		break;

	default:
		fp_err("Unknown SUBSM2 state");
		fpi_imgdev_session_error(idev, -EIO);
		fpi_ssm_mark_aborted(ssm, -EIO);
	}
}

/* Send device state control packet */
static void send_control_packet(struct fpi_ssm *ssm)
{
	struct fp_img_dev *idev = ssm->priv;

	struct fpi_ssm *subsm =
	    fpi_ssm_new(idev->dev, send_control_packet_ssm, SUBSM2_STATES);
	subsm->priv = idev;
	fpi_ssm_start_subsm(ssm, subsm);
}

/* Clears all fprint data */
static void clear_data(struct vfs_dev_t *vdev)
{
	g_free(vdev->lines_buffer);
	vdev->lines_buffer = NULL;
	vdev->memory = vdev->bytes = 0;
}

/* After receiving interrupt from EP3 */
static void interrupt_callback(struct libusb_transfer *transfer)
{
	struct fpi_ssm *ssm = transfer->user_data;
	struct fp_img_dev *idev = ssm->priv;
	struct vfs_dev_t *vdev = idev->priv;

	char *interrupt = vdev->interrupt;
	int error = transfer->status, transferred = transfer->actual_length;

	vdev->wait_interrupt = 0;

	/* When we have cancelled transfer, error is ok actually */
	if (!vdev->active && error == LIBUSB_TRANSFER_CANCELLED)
		return;

	if (error != 0) {
		fp_err("USB read interrupt transfer: %s",
		       libusb_error_name(error));
		fpi_imgdev_session_error(idev, -EIO);
		fpi_ssm_mark_aborted(ssm, -EIO);
		return;
	}

	/* Interrupt size is VFS_INTERRUPT_SIZE bytes in all known cases */
	if (transferred != VFS_INTERRUPT_SIZE) {
		fp_err("Unknown interrupt size %d", transferred);
		/* Abort ssm */
		fpi_imgdev_session_error(idev, -EIO);
		fpi_ssm_mark_aborted(ssm, -EIO);
		return;
	}

	/* Standard interrupts */
	if (memcmp(interrupt, interrupt1, VFS_INTERRUPT_SIZE) == 0 ||
	    memcmp(interrupt, interrupt2, VFS_INTERRUPT_SIZE) == 0 ||
	    memcmp(interrupt, interrupt3, VFS_INTERRUPT_SIZE) == 0) {
		/* Go to the next ssm stage */
		fpi_ssm_next_state(ssm);
		return;
	}

	/* When finger is on the scanner before turn_on */
	if (interrupt[0] == 0x01) {
		fp_warn("Finger is already on the scanner");

		/* Go to the next ssm stage */
		fpi_ssm_next_state(ssm);
		return;
	}

	/* Unknown interrupt; abort the session */
	fp_err("Unknown interrupt '%02x:%02x:%02x:%02x:%02x'!",
	       interrupt[0] & 0xff, interrupt[1] & 0xff, interrupt[2] & 0xff,
	       interrupt[3] & 0xff, interrupt[4] & 0xff);

	/* Abort ssm */
	fpi_imgdev_session_error(idev, -EIO);
	fpi_ssm_mark_aborted(ssm, -EIO);
}

static void receive_callback(struct libusb_transfer *transfer)
{
	struct fpi_ssm *ssm = transfer->user_data;
	struct fp_img_dev *idev = ssm->priv;
	struct vfs_dev_t *vdev = idev->priv;

	int transferred = transfer->actual_length, error = transfer->status;

	if (error != 0 && error != LIBUSB_TRANSFER_TIMED_OUT) {
		fp_err("USB read transfer: %s", libusb_error_name(error));

		fpi_imgdev_session_error(idev, -EIO);
		fpi_ssm_mark_aborted(ssm, -EIO);
		return;
	}

	/* Check if fingerprint data is over */
	if (transferred == 0) {
		fpi_ssm_next_state(ssm);
	} else {
		vdev->bytes += transferred;

		/* We need more data */
		fpi_ssm_jump_to_state(ssm, ssm->cur_state);
	}
}

/* Stub to keep SSM alive when waiting an interrupt */
static void wait_interrupt(void *data)
{
	struct fpi_ssm *ssm = data;
	struct fp_img_dev *idev = ssm->priv;
	struct vfs_dev_t *vdev = idev->priv;

	/* Keep sleeping while this flag is on */
	if (vdev->wait_interrupt)
		fpi_ssm_jump_to_state(ssm, ssm->cur_state);
}

/* SSM stub to prepare device to another scan after orange light was on */
static void another_scan(void *data)
{
	struct fpi_ssm *ssm = data;
	fpi_ssm_jump_to_state(ssm, SSM_TURN_ON);
}

/* Another SSM stub to continue after waiting for probable vdev->active changes */
static void scan_completed(void *data)
{
	struct fpi_ssm *ssm = data;
	fpi_ssm_next_state(ssm);
}

/* Main SSM loop */
static void activate_ssm(struct fpi_ssm *ssm)
{
	struct fp_img_dev *idev = ssm->priv;
	struct libusb_device_handle *udev = idev->udev;
	struct vfs_dev_t *vdev = idev->priv;

	switch (ssm->cur_state) {
	case SSM_INITIAL_ABORT_1:
		async_abort(ssm, 1);
		break;

	case SSM_INITIAL_ABORT_2:
		async_abort(ssm, 2);
		break;

	case SSM_INITIAL_ABORT_3:
		async_abort(ssm, 3);
		break;

	case SSM_CLEAR_EP2:
		clear_ep2(ssm);
		break;

	case SSM_TURN_OFF:
		/* Set control_packet argument */
		vdev->control_packet = turn_off;

		send_control_packet(ssm);
		break;

	case SSM_TURN_ON:
		if (!vdev->active) {
			/* The only correct exit */
			fpi_ssm_mark_completed(ssm);

			if (vdev->need_report) {
				fpi_imgdev_deactivate_complete(idev);
				vdev->need_report = 0;
			}
			break;
		}
		/* Set control_packet argument */
		vdev->control_packet = turn_on;

		send_control_packet(ssm);
		break;

	case SSM_ASK_INTERRUPT:
		/* Activated, light must be blinking now */

		/* If we first time here, report that activate completed */
		if (vdev->need_report) {
			fpi_imgdev_activate_complete(idev, 0);
			vdev->need_report = 0;
		}

		/* Asyncronously enquire an interrupt */
		vdev->transfer = libusb_alloc_transfer(0);
		vdev->transfer->flags |= LIBUSB_TRANSFER_FREE_TRANSFER;
		libusb_fill_interrupt_transfer(vdev->transfer, udev, 0x83,
					       vdev->interrupt,
					       VFS_INTERRUPT_SIZE,
					       interrupt_callback, ssm, 0);
		libusb_submit_transfer(vdev->transfer);

		/* This flag could be turned off only in callback function */
		vdev->wait_interrupt = 1;

		/* I've put it here to be sure that data is cleared */
		clear_data(vdev);

		fpi_ssm_next_state(ssm);
		break;

	case SSM_WAIT_INTERRUPT:
		/* Check if user had interrupted the process */
		if (!vdev->active) {
			libusb_cancel_transfer(vdev->transfer);
			fpi_ssm_jump_to_state(ssm, SSM_CLEAR_EP2);
			break;
		}

		if (vdev->wait_interrupt)
			fpi_timeout_add(VFS_SSM_TIMEOUT, wait_interrupt, ssm);
		break;

	case SSM_RECEIVE_FINGER:
		if (vdev->memory == 0) {
			/* Initialize fingerprint buffer */
			g_free(vdev->lines_buffer);
			vdev->memory = VFS_USB_BUFFER_SIZE;
			vdev->lines_buffer = g_malloc(vdev->memory);
			vdev->bytes = 0;

			/* Finger is on the scanner */
			fpi_imgdev_report_finger_status(idev, 1);
		}

		/* Increase buffer size while it's insufficient */
		while (vdev->bytes + VFS_USB_BUFFER_SIZE > vdev->memory) {
			vdev->memory <<= 1;
			vdev->lines_buffer =
			    (struct vfs_line *)g_realloc(vdev->lines_buffer,
							 vdev->memory);
		}

		/* Receive chunk of data */
		vdev->transfer = libusb_alloc_transfer(0);
		vdev->transfer->flags |= LIBUSB_TRANSFER_FREE_TRANSFER;
		libusb_fill_bulk_transfer(vdev->transfer, udev, 0x82,
					  (void *)vdev->lines_buffer +
					  vdev->bytes, VFS_USB_BUFFER_SIZE,
					  receive_callback, ssm,
					  VFS_USB_TIMEOUT);
		libusb_submit_transfer(vdev->transfer);
		break;

	case SSM_SUBMIT_IMAGE:
		submit_image(idev);
		clear_data(vdev);

		/* Wait for probable vdev->active changing */
		fpi_timeout_add(VFS_SSM_TIMEOUT, scan_completed, ssm);
		break;

	case SSM_NEXT_RECEIVE:
		if (!vdev->active) {
			/* It's the last scan */
			fpi_ssm_jump_to_state(ssm, SSM_CLEAR_EP2);
			break;
		}

		/* Set control_packet argument */
		vdev->control_packet = next_receive_1;

		send_control_packet(ssm);
		break;

	case SSM_WAIT_ANOTHER_SCAN:
		/* Orange light is on now */
		fpi_timeout_add(VFS_SSM_ORANGE_TIMEOUT, another_scan, ssm);
		break;

	default:
		fp_err("Unknown state");
		fpi_imgdev_session_error(idev, -EIO);
		fpi_ssm_mark_aborted(ssm, -EIO);
	}
}

/* Driver functions */

/* Callback for dev_activate ssm */
static void dev_activate_callback(struct fpi_ssm *ssm)
{
	struct fp_img_dev *idev = ssm->priv;
	struct vfs_dev_t *vdev = idev->priv;

	vdev->ssm_active = 0;

	fpi_ssm_free(ssm);
}

/* Activate device */
static int dev_activate(struct fp_img_dev *idev, enum fp_imgdev_state state)
{
	struct vfs_dev_t *vdev = idev->priv;

	/* Initialize flags */
	vdev->active = 1;
	vdev->need_report = 1;
	vdev->ssm_active = 1;

	struct fpi_ssm *ssm = fpi_ssm_new(idev->dev, activate_ssm, SSM_STATES);
	ssm->priv = idev;
	fpi_ssm_start(ssm, dev_activate_callback);
	return 0;
}

/* Deactivate device */
static void dev_deactivate(struct fp_img_dev *idev)
{
	struct vfs_dev_t *vdev = idev->priv;

	if (!vdev->ssm_active) {
		fpi_imgdev_deactivate_complete(idev);
		return;
	}

	/* Initialize flags */
	vdev->active = 0;
	vdev->need_report = 1;
}

/* Callback for dev_open ssm */
static void dev_open_callback(struct fpi_ssm *ssm)
{
	/* Notify open complete */
	fpi_imgdev_open_complete((struct fp_img_dev *)ssm->priv, 0);
	fpi_ssm_free(ssm);
}

/* Open device */
static int dev_open(struct fp_img_dev *idev, unsigned long driver_data)
{
	/* Claim usb interface */
	int error = libusb_claim_interface(idev->udev, 0);
	if (error < 0) {
		/* Interface not claimed, return error */
		fp_err("could not claim interface 0");
		return error;
	}

	/* Initialize private structure */
	struct vfs_dev_t *vdev = g_malloc0(sizeof(struct vfs_dev_t));
	idev->priv = vdev;

	/* Clearing previous device state */
	struct fpi_ssm *ssm = fpi_ssm_new(idev->dev, activate_ssm, SSM_STATES);
	ssm->priv = idev;
	fpi_ssm_start(ssm, dev_open_callback);
	return 0;
}

/* Close device */
static void dev_close(struct fp_img_dev *idev)
{
	/* Release private structure */
	g_free(idev->priv);

	/* Release usb interface */
	libusb_release_interface(idev->udev, 0);

	/* Notify close complete */
	fpi_imgdev_close_complete(idev);
}

/* Usb id table of device */
static const struct usb_id id_table[] = {
	{.vendor = 0x138a,.product = 0x0050},
	{0, 0, 0,},
};

/* Device driver definition */
struct fp_img_driver vfs0050_driver = {
	/* Driver specification */
	.driver = {
		   .id = VFS0050_ID,
		   .name = FP_COMPONENT,
		   .full_name = "Validity VFS0050",
		   .id_table = id_table,
		   .scan_type = FP_SCAN_TYPE_SWIPE,
		   },

	/* Image specification */
	.flags = 0,
	.img_width = VFS_IMAGE_WIDTH,
	.img_height = -1,
	.bz3_threshold = 24,

	/* Routine specification */
	.open = dev_open,
	.close = dev_close,
	.activate = dev_activate,
	.deactivate = dev_deactivate,
};
