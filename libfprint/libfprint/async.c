/*
 * Asynchronous I/O functionality
 * Copyright (C) 2008 Daniel Drake <dsd@gentoo.org>
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

#define FP_COMPONENT "async"

#include <config.h>
#include <errno.h>
#include <glib.h>

#include "fp_internal.h"

/* Drivers call this when device initialisation has completed */
void fpi_drvcb_open_complete(struct fp_dev *dev, int status)
{
	fp_dbg("status %d", status);
	BUG_ON(dev->state != DEV_STATE_INITIALIZING);
	dev->state = (status) ? DEV_STATE_ERROR : DEV_STATE_INITIALIZED;
	opened_devices = g_slist_prepend(opened_devices, dev);
	if (dev->open_cb)
		dev->open_cb(dev, status, dev->open_cb_data);
}

API_EXPORTED int fp_async_dev_open(struct fp_dscv_dev *ddev, fp_dev_open_cb cb,
	void *user_data)
{
	struct fp_driver *drv = ddev->drv;
	struct fp_dev *dev;
	libusb_device_handle *udevh;
	int r;

	fp_dbg("");
	r = libusb_open(ddev->udev, &udevh);
	if (r < 0) {
		fp_err("usb_open failed, error %d", r);
		return r;
	}

	dev = g_malloc0(sizeof(*dev));
	dev->drv = drv;
	dev->udev = udevh;
	dev->__enroll_stage = -1;
	dev->state = DEV_STATE_INITIALIZING;
	dev->open_cb = cb;
	dev->open_cb_data = user_data;

	if (!drv->open) {
		fpi_drvcb_open_complete(dev, 0);
		return 0;
	}

	dev->state = DEV_STATE_INITIALIZING;
	r = drv->open(dev, ddev->driver_data);
	if (r) {
		fp_err("device initialisation failed, driver=%s", drv->name);
		libusb_close(udevh);
		g_free(dev);
	}

	return r;
}

/* Drivers call this when device deinitialisation has completed */
void fpi_drvcb_close_complete(struct fp_dev *dev)
{
	fp_dbg("");
	BUG_ON(dev->state != DEV_STATE_DEINITIALIZING);
	dev->state = DEV_STATE_DEINITIALIZED;
	libusb_close(dev->udev);
	if (dev->close_cb)
		dev->close_cb(dev, dev->close_cb_data);
	g_free(dev);
}

API_EXPORTED void fp_async_dev_close(struct fp_dev *dev,
	fp_dev_close_cb callback, void *user_data)
{
	struct fp_driver *drv = dev->drv;

	if (g_slist_index(opened_devices, (gconstpointer) dev) == -1)
		fp_err("device %p not in opened list!", dev);
	opened_devices = g_slist_remove(opened_devices, (gconstpointer) dev);

	dev->close_cb = callback;
	dev->close_cb_data = user_data;

	if (!drv->close) {
		fpi_drvcb_close_complete(dev);
		return;
	}

	dev->state = DEV_STATE_DEINITIALIZING;
	drv->close(dev);
}

/* Drivers call this when enrollment has started */
void fpi_drvcb_enroll_started(struct fp_dev *dev, int status)
{
	fp_dbg("status %d", status);
	BUG_ON(dev->state != DEV_STATE_ENROLL_STARTING);
	if (status) {
		if (status > 0) {
			status = -status;
			fp_dbg("adjusted to %d", status);
		}
		dev->state = DEV_STATE_ERROR;
		if (dev->enroll_stage_cb)
			dev->enroll_stage_cb(dev, status, NULL, NULL,
				dev->enroll_stage_cb_data);
	} else {
		dev->state = DEV_STATE_ENROLLING;
	}
}

API_EXPORTED int fp_async_enroll_start(struct fp_dev *dev,
	fp_enroll_stage_cb callback, void *user_data)
{
	struct fp_driver *drv = dev->drv;
	int r;

	if (!dev->nr_enroll_stages || !drv->enroll_start) {
		fp_err("driver %s has 0 enroll stages or no enroll func",
			drv->name);
		return -ENOTSUP;
	}

	fp_dbg("starting enrollment");
	dev->enroll_stage_cb = callback;
	dev->enroll_stage_cb_data = user_data;

	dev->state = DEV_STATE_ENROLL_STARTING;
	r = drv->enroll_start(dev);
	if (r < 0) {
		dev->enroll_stage_cb = NULL;
		fp_err("failed to start enrollment");
		dev->state = DEV_STATE_ERROR;
	}

	return r;
}

/* Drivers call this when an enroll stage has completed */
void fpi_drvcb_enroll_stage_completed(struct fp_dev *dev, int result,
	struct fp_print_data *data, struct fp_img *img)
{
	BUG_ON(dev->state != DEV_STATE_ENROLLING);
	fp_dbg("result %d", result);
	if (!dev->enroll_stage_cb) {
		fp_dbg("ignoring enroll result as no callback is subscribed");
		return;
	}
	if (result == FP_ENROLL_COMPLETE && !data) {
		fp_err("BUG: complete but no data?");
		result = FP_ENROLL_FAIL;
	}
	dev->enroll_stage_cb(dev, result, data, img, dev->enroll_stage_cb_data);
}

/* Drivers call this when enrollment has stopped */
void fpi_drvcb_enroll_stopped(struct fp_dev *dev)
{
	fp_dbg("");
	BUG_ON(dev->state != DEV_STATE_ENROLL_STOPPING);
	dev->state = DEV_STATE_INITIALIZED;
	if (dev->enroll_stop_cb)
		dev->enroll_stop_cb(dev, dev->enroll_stop_cb_data);
}

API_EXPORTED int fp_async_enroll_stop(struct fp_dev *dev,
	fp_enroll_stop_cb callback, void *user_data)
{
	struct fp_driver *drv = dev->drv;
	int r;

	fp_dbg("");
	if (!drv->enroll_start)
		return -ENOTSUP;

	dev->enroll_stage_cb = NULL;
	dev->enroll_stop_cb = callback;
	dev->enroll_stop_cb_data = user_data;
	dev->state = DEV_STATE_ENROLL_STOPPING;

	if (!drv->enroll_stop) {
		fpi_drvcb_enroll_stopped(dev);
		return 0;
	}

	r = drv->enroll_stop(dev);
	if (r < 0) {
		fp_err("failed to stop enrollment");
		dev->enroll_stop_cb = NULL;
	}

	return r;
}

API_EXPORTED int fp_async_verify_start(struct fp_dev *dev,
	struct fp_print_data *data, fp_verify_cb callback, void *user_data)
{
	struct fp_driver *drv = dev->drv;
	int r;

	fp_dbg("");
	if (!drv->verify_start)
		return -ENOTSUP;

	dev->state = DEV_STATE_VERIFY_STARTING;
	dev->verify_cb = callback;
	dev->verify_cb_data = user_data;
	dev->verify_data = data;

	r = drv->verify_start(dev);
	if (r < 0) {
		dev->verify_cb = NULL;
		dev->state = DEV_STATE_ERROR;
		fp_err("failed to start verification, error %d", r);
	}
	return r;
}

/* Drivers call this when verification has started */
void fpi_drvcb_verify_started(struct fp_dev *dev, int status)
{
	fp_dbg("");
	BUG_ON(dev->state != DEV_STATE_VERIFY_STARTING);
	if (status) {
		if (status > 0) {
			status = -status;
			fp_dbg("adjusted to %d", status);
		}
		dev->state = DEV_STATE_ERROR;
		if (dev->verify_cb)
			dev->verify_cb(dev, status, NULL, dev->verify_cb_data);
	} else {
		dev->state = DEV_STATE_VERIFYING;
	}
}

/* Drivers call this to report a verify result (which might mark completion) */
void fpi_drvcb_report_verify_result(struct fp_dev *dev, int result,
	struct fp_img *img)
{
	fp_dbg("result %d", result);
	BUG_ON(dev->state != DEV_STATE_VERIFYING);
	if (result < 0 || result == FP_VERIFY_NO_MATCH
			|| result == FP_VERIFY_MATCH)
		dev->state = DEV_STATE_VERIFY_DONE;

	if (dev->verify_cb)
		dev->verify_cb(dev, result, img, dev->verify_cb_data);
	else
		fp_dbg("ignoring verify result as no callback is subscribed");
}

/* Drivers call this when verification has stopped */
void fpi_drvcb_verify_stopped(struct fp_dev *dev)
{
	fp_dbg("");
	BUG_ON(dev->state != DEV_STATE_VERIFY_STOPPING);
	dev->state = DEV_STATE_INITIALIZED;
	if (dev->verify_stop_cb)
		dev->verify_stop_cb(dev, dev->verify_stop_cb_data);
}

API_EXPORTED int fp_async_verify_stop(struct fp_dev *dev,
	fp_verify_stop_cb callback, void *user_data)
{
	struct fp_driver *drv = dev->drv;
	gboolean iterating = (dev->state == DEV_STATE_VERIFYING);
	int r;

	fp_dbg("");
	BUG_ON(dev->state != DEV_STATE_ERROR
		&& dev->state != DEV_STATE_VERIFYING
		&& dev->state != DEV_STATE_VERIFY_DONE);

	dev->verify_cb = NULL;
	dev->verify_stop_cb = callback;
	dev->verify_stop_cb_data = user_data;
	dev->state = DEV_STATE_VERIFY_STOPPING;

	if (!drv->verify_start)
		return -ENOTSUP;
	if (!drv->verify_stop) {
		dev->state = DEV_STATE_INITIALIZED;
		fpi_drvcb_verify_stopped(dev);
		return 0;
	}

	r = drv->verify_stop(dev, iterating);
	if (r < 0) {
		fp_err("failed to stop verification");
		dev->verify_stop_cb = NULL;
	}
	return r;
}

API_EXPORTED int fp_async_identify_start(struct fp_dev *dev,
	struct fp_print_data **gallery, fp_identify_cb callback, void *user_data)
{
	struct fp_driver *drv = dev->drv;
	int r;

	fp_dbg("");
	if (!drv->identify_start)
		return -ENOTSUP;
	dev->state = DEV_STATE_IDENTIFY_STARTING;
	dev->identify_cb = callback;
	dev->identify_cb_data = user_data;
	dev->identify_gallery = gallery;

	r = drv->identify_start(dev);
	if (r < 0) {
		fp_err("identify_start failed with error %d", r);
		dev->identify_cb = NULL;
		dev->state = DEV_STATE_ERROR;
	}
	return r;
}

/* Driver-lib: identification has started, expect results soon */
void fpi_drvcb_identify_started(struct fp_dev *dev, int status)
{
	fp_dbg("status %d", status);
	BUG_ON(dev->state != DEV_STATE_IDENTIFY_STARTING);
	if (status) {
		if (status > 0) {
			status = -status;
			fp_dbg("adjusted to %d", status);
		}
		dev->state = DEV_STATE_ERROR;
		if (dev->identify_cb)
			dev->identify_cb(dev, status, 0, NULL, dev->identify_cb_data);
	} else {
		dev->state = DEV_STATE_IDENTIFYING;
	}
}

/* Drivers report an identify result (which might mark completion) */
void fpi_drvcb_report_identify_result(struct fp_dev *dev, int result,
	size_t match_offset, struct fp_img *img)
{
	fp_dbg("result %d", result);
	BUG_ON(dev->state != DEV_STATE_IDENTIFYING
		&& dev->state != DEV_STATE_ERROR);
	if (result < 0 || result == FP_VERIFY_NO_MATCH
			|| result == FP_VERIFY_MATCH)
		dev->state = DEV_STATE_IDENTIFY_DONE;

	if (dev->identify_cb)
		dev->identify_cb(dev, result, match_offset, img, dev->identify_cb_data);
	else
		fp_dbg("ignoring verify result as no callback is subscribed");
}

API_EXPORTED int fp_async_identify_stop(struct fp_dev *dev,
	fp_identify_stop_cb callback, void *user_data)
{
	struct fp_driver *drv = dev->drv;
	gboolean iterating = (dev->state == DEV_STATE_IDENTIFYING);
	int r;

	fp_dbg("");
	BUG_ON(dev->state != DEV_STATE_IDENTIFYING
		&& dev->state != DEV_STATE_IDENTIFY_DONE);

	dev->state = DEV_STATE_IDENTIFY_STOPPING;
	dev->identify_cb = NULL;
	dev->identify_stop_cb = callback;
	dev->identify_stop_cb_data = user_data;

	if (!drv->identify_start)
		return -ENOTSUP;	
	if (!drv->identify_stop) {
		dev->state = DEV_STATE_INITIALIZED;
		fpi_drvcb_identify_stopped(dev);
		return 0;
	}

	r = drv->identify_stop(dev, iterating);
	if (r < 0) {
		fp_err("failed to stop identification");
		dev->identify_stop_cb = NULL;
	}

	return r;
}

/* Drivers call this when identification has stopped */
void fpi_drvcb_identify_stopped(struct fp_dev *dev)
{
	fp_dbg("");
	BUG_ON(dev->state != DEV_STATE_IDENTIFY_STOPPING);
	dev->state = DEV_STATE_INITIALIZED;
	if (dev->identify_stop_cb)
		dev->identify_stop_cb(dev, dev->identify_stop_cb_data);
}

API_EXPORTED int fp_async_capture_start(struct fp_dev *dev, int unconditional,
	fp_capture_cb callback, void *user_data)
{
	struct fp_driver *drv = dev->drv;
	int r;

	fp_dbg("");
	if (!drv->capture_start)
		return -ENOTSUP;

	dev->state = DEV_STATE_CAPTURE_STARTING;
	dev->capture_cb = callback;
	dev->capture_cb_data = user_data;
	dev->unconditional_capture = unconditional;

	r = drv->capture_start(dev);
	if (r < 0) {
		dev->capture_cb = NULL;
		dev->state = DEV_STATE_ERROR;
		fp_err("failed to start verification, error %d", r);
	}
	return r;
}

/* Drivers call this when capture has started */
void fpi_drvcb_capture_started(struct fp_dev *dev, int status)
{
	fp_dbg("");
	BUG_ON(dev->state != DEV_STATE_CAPTURE_STARTING);
	if (status) {
		if (status > 0) {
			status = -status;
			fp_dbg("adjusted to %d", status);
		}
		dev->state = DEV_STATE_ERROR;
		if (dev->capture_cb)
			dev->capture_cb(dev, status, NULL, dev->capture_cb_data);
	} else {
		dev->state = DEV_STATE_CAPTURING;
	}
}

/* Drivers call this to report a capture result (which might mark completion) */
void fpi_drvcb_report_capture_result(struct fp_dev *dev, int result,
	struct fp_img *img)
{
	fp_dbg("result %d", result);
	BUG_ON(dev->state != DEV_STATE_CAPTURING);
	if (result < 0 || result == FP_CAPTURE_COMPLETE)
		dev->state = DEV_STATE_CAPTURE_DONE;

	if (dev->capture_cb)
		dev->capture_cb(dev, result, img, dev->capture_cb_data);
	else
		fp_dbg("ignoring capture result as no callback is subscribed");
}

/* Drivers call this when capture has stopped */
void fpi_drvcb_capture_stopped(struct fp_dev *dev)
{
	fp_dbg("");
	BUG_ON(dev->state != DEV_STATE_CAPTURE_STOPPING);
	dev->state = DEV_STATE_INITIALIZED;
	if (dev->capture_stop_cb)
		dev->capture_stop_cb(dev, dev->capture_stop_cb_data);
}

API_EXPORTED int fp_async_capture_stop(struct fp_dev *dev,
	fp_capture_stop_cb callback, void *user_data)
{
	struct fp_driver *drv = dev->drv;
	int r;

	fp_dbg("");
	BUG_ON(dev->state != DEV_STATE_ERROR
		&& dev->state != DEV_STATE_CAPTURING
		&& dev->state != DEV_STATE_CAPTURE_DONE);

	dev->capture_cb = NULL;
	dev->capture_stop_cb = callback;
	dev->capture_stop_cb_data = user_data;
	dev->state = DEV_STATE_CAPTURE_STOPPING;

	if (!drv->capture_start)
		return -ENOTSUP;
	if (!drv->capture_stop) {
		dev->state = DEV_STATE_INITIALIZED;
		fpi_drvcb_capture_stopped(dev);
		return 0;
	}

	r = drv->capture_stop(dev);
	if (r < 0) {
		fp_err("failed to stop verification");
		dev->capture_stop_cb = NULL;
	}
	return r;
}
