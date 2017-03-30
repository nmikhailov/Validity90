/*
 * Validity VFS0090 driver for libfprint
 * Copyright (C) 2017 Nikita Mikhailov <nikita.s.mikhailov@gmail.com>
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

#define FP_COMPONENT "vfs0090"

#include <errno.h>
#include <string.h>
#include <fp_internal.h>
#include <assembling.h>
#include "driver_ids.h"

#include "vfs0090.h"

/* The main driver structure */
struct vfs_dev_t {
    /* One if we were asked to read fingerprint, zero otherwise */
    char active;

    /* Control packet parameter for send_control_packet */
    unsigned char *message;
    unsigned int message_length;

    /* For dev_deactivate to check whether ssm still running or not */
    char ssm_active;

    /* Current async transfer */
    struct libusb_transfer *transfer;
};

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
static void send_packet(struct fpi_ssm *ssm)
{
    struct fp_img_dev *idev = ssm->priv;

    struct fpi_ssm *subsm =
        fpi_ssm_new(idev->dev, send_control_packet_ssm, SUBSM2_STATE_LAST);
    subsm->priv = idev;
    fpi_ssm_start_subsm(ssm, subsm);
}

/* Main SSM loop */
static void activate_ssm(struct fpi_ssm *ssm)
{
    struct fp_img_dev *idev = ssm->priv;
    struct libusb_device_handle *udev = idev->udev;
    struct vfs_dev_t *vdev = idev->priv;

    switch (ssm->cur_state) {
    case SSM_INITIALIZATION_1:
        send(blob_init_s_1);
        break;


    default:
        fp_err("Unknown state");
        fpi_imgdev_session_error(idev, -EIO);
        fpi_ssm_mark_aborted(ssm, -EIO);
    }
}

/* Callback for dev_open ssm */
static void dev_open_callback(struct fpi_ssm *ssm)
{
    /* Notify open complete */
    fpi_imgdev_open_complete((struct fp_img_dev *)ssm->priv, 0);
    fpi_ssm_free(ssm);
}

static int dev_activate(struct fp_img_dev *idev, enum fp_imgdev_state state)
{
	return -1;
}

static void dev_deactivate(struct fp_img_dev *idev)
{
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
    struct fpi_ssm *ssm = fpi_ssm_new(idev->dev, activate_ssm, SSM_STATE_LAST);
    ssm->priv = idev;
    fpi_ssm_start(ssm, dev_open_callback);
    return 0;
}

static void dev_close(struct fp_img_dev *idev)
{
}

/* Usb id table of device */
static const struct usb_id id_table[] = {
	{.vendor = 0x138a,.product = 0x0090},
	{0, 0, 0,},
};

/* Device driver definition */
struct fp_img_driver vfs0090_driver = {
	/* Driver specification */
	.driver = {
	   .id = VFS0090_ID,
	   .name = FP_COMPONENT,
	   .full_name = "Validity VFS0090",
	   .id_table = id_table,
	   .scan_type = FP_SCAN_TYPE_SWIPE,
	},

	/* Image specification */
	.flags = 0,
	.img_width = 0,
	.img_height = 0,
	.bz3_threshold = 0,

	/* Routine specification */
	.open = dev_open,
	.close = dev_close,
	.activate = dev_activate,
	.deactivate = dev_deactivate,
};
