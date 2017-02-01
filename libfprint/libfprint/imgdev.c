/*
 * Core imaging device functions for libfprint
 * Copyright (C) 2007-2008 Daniel Drake <dsd@gentoo.org>
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

#include <glib.h>

#include "fp_internal.h"

#define MIN_ACCEPTABLE_MINUTIAE 10
#define BOZORTH3_DEFAULT_THRESHOLD 40
#define IMG_ENROLL_STAGES 5

static int img_dev_open(struct fp_dev *dev, unsigned long driver_data)
{
	struct fp_img_dev *imgdev = g_malloc0(sizeof(*imgdev));
	struct fp_img_driver *imgdrv = fpi_driver_to_img_driver(dev->drv);
	int r = 0;

	imgdev->dev = dev;
	imgdev->enroll_stage = 0;
	dev->priv = imgdev;
	dev->nr_enroll_stages = IMG_ENROLL_STAGES;

	/* for consistency in driver code, allow udev access through imgdev */
	imgdev->udev = dev->udev;

	if (imgdrv->open) {
		r = imgdrv->open(imgdev, driver_data);
		if (r)
			goto err;
	} else {
		fpi_drvcb_open_complete(dev, 0);
	}

	return 0;
err:
	g_free(imgdev);
	return r;
}

void fpi_imgdev_open_complete(struct fp_img_dev *imgdev, int status)
{
	fpi_drvcb_open_complete(imgdev->dev, status);
}

static void img_dev_close(struct fp_dev *dev)
{
	struct fp_img_dev *imgdev = dev->priv;
	struct fp_img_driver *imgdrv = fpi_driver_to_img_driver(dev->drv);

	if (imgdrv->close)
		imgdrv->close(imgdev);
	else
		fpi_drvcb_close_complete(dev);
}

void fpi_imgdev_close_complete(struct fp_img_dev *imgdev)
{
	fpi_drvcb_close_complete(imgdev->dev);
	g_free(imgdev);
}

static int dev_change_state(struct fp_img_dev *imgdev,
	enum fp_imgdev_state state)
{
	struct fp_driver *drv = imgdev->dev->drv;
	struct fp_img_driver *imgdrv = fpi_driver_to_img_driver(drv);

	if (!imgdrv->change_state)
		return 0;
	return imgdrv->change_state(imgdev, state);
}

/* check image properties and resize it if necessary. potentially returns a new
 * image after freeing the old one. */
static int sanitize_image(struct fp_img_dev *imgdev, struct fp_img **_img)
{
	struct fp_driver *drv = imgdev->dev->drv;
	struct fp_img_driver *imgdrv = fpi_driver_to_img_driver(drv);
	struct fp_img *img = *_img;

	if (imgdrv->img_width > 0) {
		img->width = imgdrv->img_width;
	} else if (img->width <= 0) {
		fp_err("no image width assigned");
		return -EINVAL;
	}

	if (imgdrv->img_height > 0) {
		img->height = imgdrv->img_height;
	} else if (img->height <= 0) {
		fp_err("no image height assigned");
		return -EINVAL;
	}

	if (!fpi_img_is_sane(img)) {
		fp_err("image is not sane!");
		return -EINVAL;
	}

	return 0;
}

void fpi_imgdev_report_finger_status(struct fp_img_dev *imgdev,
	gboolean present)
{
	int r = imgdev->action_result;
	struct fp_print_data *data = imgdev->acquire_data;
	struct fp_img *img = imgdev->acquire_img;

	fp_dbg(present ? "finger on sensor" : "finger removed");

	if (present && imgdev->action_state == IMG_ACQUIRE_STATE_AWAIT_FINGER_ON) {
		dev_change_state(imgdev, IMGDEV_STATE_CAPTURE);
		imgdev->action_state = IMG_ACQUIRE_STATE_AWAIT_IMAGE;
		return;
	} else if (present
			|| imgdev->action_state != IMG_ACQUIRE_STATE_AWAIT_FINGER_OFF) {
		fp_dbg("ignoring status report");
		return;
	}

	/* clear these before reporting results to avoid complications with
	 * call cascading in and out of the library */
	imgdev->acquire_img = NULL;
	imgdev->acquire_data = NULL;

	/* finger removed, report results */
	switch (imgdev->action) {
	case IMG_ACTION_ENROLL:
		fp_dbg("reporting enroll result");
		data = imgdev->enroll_data;
		if (r == FP_ENROLL_COMPLETE) {
			imgdev->enroll_data = NULL;
		}
		fpi_drvcb_enroll_stage_completed(imgdev->dev, r,
			r == FP_ENROLL_COMPLETE ? data : NULL,
			img);
		/* the callback can cancel enrollment, so recheck current
		 * action and the status to see if retry is needed */
		if (imgdev->action == IMG_ACTION_ENROLL &&
		    r > 0 && r != FP_ENROLL_COMPLETE && r != FP_ENROLL_FAIL) {
			imgdev->action_result = 0;
			imgdev->action_state = IMG_ACQUIRE_STATE_AWAIT_FINGER_ON;
			dev_change_state(imgdev, IMGDEV_STATE_AWAIT_FINGER_ON);
		}
		break;
	case IMG_ACTION_VERIFY:
		fpi_drvcb_report_verify_result(imgdev->dev, r, img);
		imgdev->action_result = 0;
		fp_print_data_free(data);
		break;
	case IMG_ACTION_IDENTIFY:
		fpi_drvcb_report_identify_result(imgdev->dev, r,
			imgdev->identify_match_offset, img);
		imgdev->action_result = 0;
		fp_print_data_free(data);
		break;
	case IMG_ACTION_CAPTURE:
		fpi_drvcb_report_capture_result(imgdev->dev, r, img);
		imgdev->action_result = 0;
		break;
	default:
		fp_err("unhandled action %d", imgdev->action);
		break;
	}
}

static void verify_process_img(struct fp_img_dev *imgdev)
{
	struct fp_img_driver *imgdrv = fpi_driver_to_img_driver(imgdev->dev->drv);
	int match_score = imgdrv->bz3_threshold;
	int r;

	if (match_score == 0)
		match_score = BOZORTH3_DEFAULT_THRESHOLD;

	r = fpi_img_compare_print_data(imgdev->dev->verify_data,
		imgdev->acquire_data);

	if (r >= match_score)
		r = FP_VERIFY_MATCH;
	else if (r >= 0)
		r = FP_VERIFY_NO_MATCH;

	imgdev->action_result = r;
}

static void identify_process_img(struct fp_img_dev *imgdev)
{
	struct fp_img_driver *imgdrv = fpi_driver_to_img_driver(imgdev->dev->drv);
	int match_score = imgdrv->bz3_threshold;
	size_t match_offset;
	int r;

	if (match_score == 0)
		match_score = BOZORTH3_DEFAULT_THRESHOLD;

	r = fpi_img_compare_print_data_to_gallery(imgdev->acquire_data,
		imgdev->dev->identify_gallery, match_score, &match_offset);

	imgdev->action_result = r;
	imgdev->identify_match_offset = match_offset;
}

void fpi_imgdev_abort_scan(struct fp_img_dev *imgdev, int result)
{
	imgdev->action_result = result;
	imgdev->action_state = IMG_ACQUIRE_STATE_AWAIT_FINGER_OFF;
	dev_change_state(imgdev, IMGDEV_STATE_AWAIT_FINGER_OFF);
}

void fpi_imgdev_image_captured(struct fp_img_dev *imgdev, struct fp_img *img)
{
	struct fp_print_data *print;
	int r;
	fp_dbg("");

	if (imgdev->action_state != IMG_ACQUIRE_STATE_AWAIT_IMAGE) {
		fp_dbg("ignoring due to current state %d", imgdev->action_state);
		return;
	}

	if (imgdev->action_result) {
		fp_dbg("not overwriting existing action result");
		return;
	}

	r = sanitize_image(imgdev, &img);
	if (r < 0) {
		imgdev->action_result = r;
		fp_img_free(img);
		goto next_state;
	}

	fp_img_standardize(img);
	imgdev->acquire_img = img;
	if (imgdev->action != IMG_ACTION_CAPTURE) {
		r = fpi_img_to_print_data(imgdev, img, &print);
		if (r < 0) {
			fp_dbg("image to print data conversion error: %d", r);
			imgdev->action_result = FP_ENROLL_RETRY;
			goto next_state;
		} else if (img->minutiae->num < MIN_ACCEPTABLE_MINUTIAE) {
			fp_dbg("not enough minutiae, %d/%d", img->minutiae->num,
				MIN_ACCEPTABLE_MINUTIAE);
			fp_print_data_free(print);
			/* depends on FP_ENROLL_RETRY == FP_VERIFY_RETRY */
			imgdev->action_result = FP_ENROLL_RETRY;
			goto next_state;
		}
	}

	imgdev->acquire_data = print;
	switch (imgdev->action) {
	case IMG_ACTION_ENROLL:
		if (!imgdev->enroll_data) {
			imgdev->enroll_data = fpi_print_data_new(imgdev->dev);
		}
		BUG_ON(g_slist_length(print->prints) != 1);
		/* Move print data from acquire data into enroll_data */
		imgdev->enroll_data->prints =
			g_slist_prepend(imgdev->enroll_data->prints, print->prints->data);
		print->prints = g_slist_remove(print->prints, print->prints->data);

		fp_print_data_free(imgdev->acquire_data);
		imgdev->acquire_data = NULL;
		imgdev->enroll_stage++;
		if (imgdev->enroll_stage == imgdev->dev->nr_enroll_stages)
			imgdev->action_result = FP_ENROLL_COMPLETE;
		else
			imgdev->action_result = FP_ENROLL_PASS;
		break;
	case IMG_ACTION_VERIFY:
		verify_process_img(imgdev);
		break;
	case IMG_ACTION_IDENTIFY:
		identify_process_img(imgdev);
		break;
	case IMG_ACTION_CAPTURE:
		imgdev->action_result = FP_CAPTURE_COMPLETE;
		break;
	default:
		BUG();
		break;
	}

next_state:
	imgdev->action_state = IMG_ACQUIRE_STATE_AWAIT_FINGER_OFF;
	dev_change_state(imgdev, IMGDEV_STATE_AWAIT_FINGER_OFF);
}

void fpi_imgdev_session_error(struct fp_img_dev *imgdev, int error)
{
	fp_dbg("error %d", error);
	BUG_ON(error == 0);
	switch (imgdev->action) {
	case IMG_ACTION_ENROLL:
		fpi_drvcb_enroll_stage_completed(imgdev->dev, error, NULL, NULL);
		break;
	case IMG_ACTION_VERIFY:
		fpi_drvcb_report_verify_result(imgdev->dev, error, NULL);
		break;
	case IMG_ACTION_IDENTIFY:
		fpi_drvcb_report_identify_result(imgdev->dev, error, 0, NULL);
		break;
	case IMG_ACTION_CAPTURE:
		fpi_drvcb_report_capture_result(imgdev->dev, error, NULL);
		break;
	default:
		fp_err("unhandled action %d", imgdev->action);
		break;
	}
}

void fpi_imgdev_activate_complete(struct fp_img_dev *imgdev, int status)
{
	fp_dbg("status %d", status);

	switch (imgdev->action) {
	case IMG_ACTION_ENROLL:
		fpi_drvcb_enroll_started(imgdev->dev, status);
		break;
	case IMG_ACTION_VERIFY:
		fpi_drvcb_verify_started(imgdev->dev, status);
		break;
	case IMG_ACTION_IDENTIFY:
		fpi_drvcb_identify_started(imgdev->dev, status);
		break;
	case IMG_ACTION_CAPTURE:
		fpi_drvcb_capture_started(imgdev->dev, status);
		break;
	default:
		fp_err("unhandled action %d", imgdev->action);
		return;
	}

	if (status == 0) {
		imgdev->action_state = IMG_ACQUIRE_STATE_AWAIT_FINGER_ON;
		dev_change_state(imgdev, IMGDEV_STATE_AWAIT_FINGER_ON);
	}
}

void fpi_imgdev_deactivate_complete(struct fp_img_dev *imgdev)
{
	fp_dbg("");

	switch (imgdev->action) {
	case IMG_ACTION_ENROLL:
		fpi_drvcb_enroll_stopped(imgdev->dev);
		break;
	case IMG_ACTION_VERIFY:
		fpi_drvcb_verify_stopped(imgdev->dev);
		break;
	case IMG_ACTION_IDENTIFY:
		fpi_drvcb_identify_stopped(imgdev->dev);
		break;
	case IMG_ACTION_CAPTURE:
		fpi_drvcb_capture_stopped(imgdev->dev);
		break;
	default:
		fp_err("unhandled action %d", imgdev->action);
		break;
	}

	imgdev->action = IMG_ACTION_NONE;
	imgdev->action_state = 0;
}

int fpi_imgdev_get_img_width(struct fp_img_dev *imgdev)
{
	struct fp_driver *drv = imgdev->dev->drv;
	struct fp_img_driver *imgdrv = fpi_driver_to_img_driver(drv);
	int width = imgdrv->img_width;

	if (width == -1)
		width = 0;

	return width;
}

int fpi_imgdev_get_img_height(struct fp_img_dev *imgdev)
{
	struct fp_driver *drv = imgdev->dev->drv;
	struct fp_img_driver *imgdrv = fpi_driver_to_img_driver(drv);
	int height = imgdrv->img_height;

	if (height == -1)
		height = 0;

	return height;
}

static int dev_activate(struct fp_img_dev *imgdev, enum fp_imgdev_state state)
{
	struct fp_driver *drv = imgdev->dev->drv;
	struct fp_img_driver *imgdrv = fpi_driver_to_img_driver(drv);

	if (!imgdrv->activate)
		return 0;
	return imgdrv->activate(imgdev, state);
}

static void dev_deactivate(struct fp_img_dev *imgdev)
{
	struct fp_driver *drv = imgdev->dev->drv;
	struct fp_img_driver *imgdrv = fpi_driver_to_img_driver(drv);

	if (!imgdrv->deactivate)
		return;
	return imgdrv->deactivate(imgdev);
}

static int generic_acquire_start(struct fp_dev *dev, int action)
{
	struct fp_img_dev *imgdev = dev->priv;
	int r;
	fp_dbg("action %d", action);
	imgdev->action = action;
	imgdev->action_state = IMG_ACQUIRE_STATE_ACTIVATING;
	imgdev->enroll_stage = 0;

	r = dev_activate(imgdev, IMGDEV_STATE_AWAIT_FINGER_ON);
	if (r < 0)
		fp_err("activation failed with error %d", r);

	return r;

}

static void generic_acquire_stop(struct fp_img_dev *imgdev)
{
	imgdev->action_state = IMG_ACQUIRE_STATE_DEACTIVATING;
	dev_deactivate(imgdev);

	fp_print_data_free(imgdev->acquire_data);
	fp_print_data_free(imgdev->enroll_data);
	fp_img_free(imgdev->acquire_img);
	imgdev->acquire_data = NULL;
	imgdev->enroll_data = NULL;
	imgdev->acquire_img = NULL;
	imgdev->action_result = 0;
}

static int img_dev_enroll_start(struct fp_dev *dev)
{
	return generic_acquire_start(dev, IMG_ACTION_ENROLL);
}

static int img_dev_verify_start(struct fp_dev *dev)
{
	return generic_acquire_start(dev, IMG_ACTION_VERIFY);
}

static int img_dev_identify_start(struct fp_dev *dev)
{
	return generic_acquire_start(dev, IMG_ACTION_IDENTIFY);
}

static int img_dev_capture_start(struct fp_dev *dev)
{
	/* Unconditional capture is not supported yet */
	if (dev->unconditional_capture)
		return -ENOTSUP;
	return generic_acquire_start(dev, IMG_ACTION_CAPTURE);
}

static int img_dev_enroll_stop(struct fp_dev *dev)
{
	struct fp_img_dev *imgdev = dev->priv;
	BUG_ON(imgdev->action != IMG_ACTION_ENROLL);
	generic_acquire_stop(imgdev);
	return 0;
}

static int img_dev_verify_stop(struct fp_dev *dev, gboolean iterating)
{
	struct fp_img_dev *imgdev = dev->priv;
	BUG_ON(imgdev->action != IMG_ACTION_VERIFY);
	generic_acquire_stop(imgdev);
	return 0;
}

static int img_dev_identify_stop(struct fp_dev *dev, gboolean iterating)
{
	struct fp_img_dev *imgdev = dev->priv;
	BUG_ON(imgdev->action != IMG_ACTION_IDENTIFY);
	generic_acquire_stop(imgdev);
	imgdev->identify_match_offset = 0;
	return 0;
}

static int img_dev_capture_stop(struct fp_dev *dev)
{
	struct fp_img_dev *imgdev = dev->priv;
	BUG_ON(imgdev->action != IMG_ACTION_CAPTURE);
	generic_acquire_stop(imgdev);
	return 0;
}

void fpi_img_driver_setup(struct fp_img_driver *idriver)
{
	idriver->driver.type = DRIVER_IMAGING;
	idriver->driver.open = img_dev_open;
	idriver->driver.close = img_dev_close;
	idriver->driver.enroll_start = img_dev_enroll_start;
	idriver->driver.enroll_stop = img_dev_enroll_stop;
	idriver->driver.verify_start = img_dev_verify_start;
	idriver->driver.verify_stop = img_dev_verify_stop;
	idriver->driver.identify_start = img_dev_identify_start;
	idriver->driver.identify_stop = img_dev_identify_stop;
	idriver->driver.capture_start = img_dev_capture_start;
	idriver->driver.capture_stop = img_dev_capture_stop;
}

