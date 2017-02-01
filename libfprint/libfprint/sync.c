/*
 * Synchronous I/O functionality
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

#define FP_COMPONENT "sync"

#include <config.h>
#include <errno.h>

#include "fp_internal.h"

struct sync_open_data {
	struct fp_dev *dev;
	int status;
};

static void sync_open_cb(struct fp_dev *dev, int status, void *user_data)
{
	struct sync_open_data *odata = user_data;
	fp_dbg("status %d", status);
	odata->dev = dev;
	odata->status = status;
}

/** \ingroup dev
 * Opens and initialises a device. This is the function you call in order
 * to convert a \ref dscv_dev "discovered device" into an actual device handle
 * that you can perform operations with.
 * \param ddev the discovered device to open
 * \returns the opened device handle, or NULL on error
 */
API_EXPORTED struct fp_dev *fp_dev_open(struct fp_dscv_dev *ddev)
{
	struct fp_dev *dev = NULL;
	struct sync_open_data *odata = g_malloc0(sizeof(*odata));
	int r;

	fp_dbg("");
	r = fp_async_dev_open(ddev, sync_open_cb, odata);
	if (r)
		goto out;

	while (!odata->dev)
		if (fp_handle_events() < 0)
			goto out;

	if (odata->status == 0)
		dev = odata->dev;
	else
		fp_dev_close(odata->dev);

out:
	g_free(odata);
	return dev;
}

static void sync_close_cb(struct fp_dev *dev, void *user_data)
{
	fp_dbg("");
	gboolean *closed = user_data;
	*closed = TRUE;
}

/** \ingroup dev
 * Close a device. You must call this function when you are finished using
 * a fingerprint device.
 * \param dev the device to close. If NULL, function simply returns.
 */
API_EXPORTED void fp_dev_close(struct fp_dev *dev)
{
	gboolean closed = FALSE;

	if (!dev)
		return;

	fp_dbg("");
	fp_async_dev_close(dev, sync_close_cb, &closed);
	while (!closed)
		if (fp_handle_events() < 0)
			break;
}

struct sync_enroll_data {
	gboolean populated;
	int result;
	struct fp_print_data *data;
	struct fp_img *img;
};

static void sync_enroll_cb(struct fp_dev *dev, int result,
	struct fp_print_data *data, struct fp_img *img, void *user_data)
{
	struct sync_enroll_data *edata = user_data;
	fp_dbg("result %d", result);
	edata->result = result;
	edata->data = data;
	edata->img = img;
	edata->populated = TRUE;
}

static void enroll_stop_cb(struct fp_dev *dev, void *user_data)
{
	gboolean *stopped = user_data;
	fp_dbg("");
	*stopped = TRUE;
}

/** \ingroup dev
 * Performs an enroll stage. See \ref enrolling for an explanation of enroll
 * stages.
 *
 * If no enrollment is in process, this kicks of the process and runs the
 * first stage. If an enrollment is already in progress, calling this
 * function runs the next stage, which may well be the last.
 *
 * A negative error code may be returned from any stage. When this occurs,
 * further calls to the enroll function will start a new enrollment process,
 * i.e. a negative error code indicates that the enrollment process has been
 * aborted. These error codes only ever indicate unexpected internal errors
 * or I/O problems.
 *
 * The RETRY codes from #fp_enroll_result may be returned from any enroll
 * stage. These codes indicate that the scan was not succesful in that the
 * user did not position their finger correctly or similar. When a RETRY code
 * is returned, the enrollment stage is <b>not</b> advanced, so the next call
 * into this function will retry the current stage again. The current stage may
 * need to be retried several times.
 *
 * The fp_enroll_result#FP_ENROLL_FAIL code may be returned from any enroll
 * stage. This code indicates that even though the scans themselves have been
 * acceptable, data processing applied to these scans produces incomprehensible
 * results. In other words, the user may have been scanning a different finger
 * for each stage or something like that. Like negative error codes, this
 * return code indicates that the enrollment process has been aborted.
 *
 * The fp_enroll_result#FP_ENROLL_PASS code will only ever be returned for
 * non-final stages. This return code indicates that the scan was acceptable
 * and the next call into this function will advance onto the next enroll
 * stage.
 *
 * The fp_enroll_result#FP_ENROLL_COMPLETE code will only ever be returned
 * from the final enroll stage. It indicates that enrollment completed
 * successfully, and that print_data has been assigned to point to the
 * resultant enrollment data. The print_data parameter will not be modified
 * during any other enrollment stages, hence it is actually legal to pass NULL
 * as this argument for all but the final stage.
 * 
 * If the device is an imaging device, it can also return the image from
 * the scan, even when the enroll fails with a RETRY or FAIL code. It is legal
 * to call this function even on non-imaging devices, just don't expect them to
 * provide images.
 *
 * \param dev the device
 * \param print_data a location to return the resultant enrollment data from
 * the final stage. Must be freed with fp_print_data_free() after use.
 * \param img location to store the scan image. accepts NULL for no image
 * storage. If an image is returned, it must be freed with fp_img_free() after
 * use.
 * \return negative code on error, otherwise a code from #fp_enroll_result
 */
API_EXPORTED int fp_enroll_finger_img(struct fp_dev *dev,
	struct fp_print_data **print_data, struct fp_img **img)
{
	struct fp_driver *drv = dev->drv;
	int stage = dev->__enroll_stage;
	gboolean final = FALSE;
	gboolean stopped = FALSE;
	struct sync_enroll_data *edata = NULL;
	int r;
	fp_dbg("");

	/* FIXME __enroll_stage is ugly, can we replace it by some function that
	 * says whether we're enrolling or not, and then put __enroll_stage into
	 * edata? */

	if (stage == -1) {
		edata = g_malloc0(sizeof(struct sync_enroll_data));
		r = fp_async_enroll_start(dev, sync_enroll_cb, edata);
		if (r < 0) {
			g_free(edata);
			return r;
		}

		dev->__enroll_stage = ++stage;
	} else if (stage >= dev->nr_enroll_stages) {
		fp_err("exceeding number of enroll stages for device claimed by "
			"driver %s (%d stages)", drv->name, dev->nr_enroll_stages);
		dev->__enroll_stage = -1;
		r = -EINVAL;
		final = TRUE;
		goto out;
	}
	fp_dbg("%s will handle enroll stage %d/%d", drv->name, stage,
		dev->nr_enroll_stages - 1);

	/* FIXME this isn't very clean */
	edata = dev->enroll_stage_cb_data;

	while (!edata->populated) {
		r = fp_handle_events();
		if (r < 0) {
			g_free(edata);
			goto err;
		}
	}

	edata->populated = FALSE;

	if (img)
		*img = edata->img;
	else
		fp_img_free(edata->img);

	r = edata->result;
	switch (r) {
	case FP_ENROLL_PASS:
		fp_dbg("enroll stage passed");
		dev->__enroll_stage = stage + 1;
		break;
	case FP_ENROLL_COMPLETE:
		fp_dbg("enroll complete");
		dev->__enroll_stage = -1;
		*print_data = edata->data;
		final = TRUE;
		break;
	case FP_ENROLL_RETRY:
		fp_dbg("enroll should retry");
		break;
	case FP_ENROLL_RETRY_TOO_SHORT:
		fp_dbg("swipe was too short, enroll should retry");
		break;
	case FP_ENROLL_RETRY_CENTER_FINGER:
		fp_dbg("finger was not centered, enroll should retry");
		break;
	case FP_ENROLL_RETRY_REMOVE_FINGER:
		fp_dbg("scan failed, remove finger and retry");
		break;
	case FP_ENROLL_FAIL:
		fp_err("enroll failed");
		dev->__enroll_stage = -1;
		final = TRUE;
		break;
	default:
		fp_err("unrecognised return code %d", r);
		dev->__enroll_stage = -1;
		r = -EINVAL;
		final = TRUE;
		break;
	}

	if (!final)
		return r;

out:
	if (final) {
		fp_dbg("ending enrollment");
		g_free(edata);
	}

err:
	if (fp_async_enroll_stop(dev, enroll_stop_cb, &stopped) == 0)
		while (!stopped)
			if (fp_handle_events() < 0)
				break;
	return r;
}

struct sync_verify_data {
	gboolean populated;
	int result;
	struct fp_img *img;
};

static void sync_verify_cb(struct fp_dev *dev, int result, struct fp_img *img,
	void *user_data)
{
	struct sync_verify_data *vdata = user_data;
	vdata->result = result;
	vdata->img = img;
	vdata->populated = TRUE;
}

static void verify_stop_cb(struct fp_dev *dev, void *user_data)
{
	gboolean *stopped = user_data;
	fp_dbg("");
	*stopped = TRUE;
}

/** \ingroup dev
 * Performs a new scan and verify it against a previously enrolled print.
 * If the device is an imaging device, it can also return the image from
 * the scan, even when the verify fails with a RETRY code. It is legal to
 * call this function even on non-imaging devices, just don't expect them to
 * provide images.
 *
 * \param dev the device to perform the scan.
 * \param enrolled_print the print to verify against. Must have been previously
 * enrolled with a device compatible to the device selected to perform the scan.
 * \param img location to store the scan image. accepts NULL for no image
 * storage. If an image is returned, it must be freed with fp_img_free() after
 * use.
 * \return negative code on error, otherwise a code from #fp_verify_result
 */
API_EXPORTED int fp_verify_finger_img(struct fp_dev *dev,
	struct fp_print_data *enrolled_print, struct fp_img **img)
{
	struct sync_verify_data *vdata;
	gboolean stopped = FALSE;
	int r;

	if (!enrolled_print) {
		fp_err("no print given");
		return -EINVAL;
	}

	if (!fp_dev_supports_print_data(dev, enrolled_print)) {
		fp_err("print is not compatible with device");
		return -EINVAL;
	}

	fp_dbg("to be handled by %s", dev->drv->name);
	vdata = g_malloc0(sizeof(struct sync_verify_data));
	r = fp_async_verify_start(dev, enrolled_print, sync_verify_cb, vdata);
	if (r < 0) {
		fp_dbg("verify_start error %d", r);
		g_free(vdata);
		return r;
	}

	while (!vdata->populated) {
		r = fp_handle_events();
		if (r < 0) {
			g_free(vdata);
			goto err;
		}
	}

	if (img)
		*img = vdata->img;
	else
		fp_img_free(vdata->img);

	r = vdata->result;
	g_free(vdata);
	switch (r) {
	case FP_VERIFY_NO_MATCH:
		fp_dbg("result: no match");
		break;
	case FP_VERIFY_MATCH:
		fp_dbg("result: match");
		break;
	case FP_VERIFY_RETRY:
		fp_dbg("verify should retry");
		break;
	case FP_VERIFY_RETRY_TOO_SHORT:
		fp_dbg("swipe was too short, verify should retry");
		break;
	case FP_VERIFY_RETRY_CENTER_FINGER:
		fp_dbg("finger was not centered, verify should retry");
		break;
	case FP_VERIFY_RETRY_REMOVE_FINGER:
		fp_dbg("scan failed, remove finger and retry");
		break;
	default:
		fp_err("unrecognised return code %d", r);
		r = -EINVAL;
	}

err:
	fp_dbg("ending verification");
	if (fp_async_verify_stop(dev, verify_stop_cb, &stopped) == 0)
		while (!stopped)
			if (fp_handle_events() < 0)
				break;

	return r;
}

struct sync_identify_data {
	gboolean populated;
	int result;
	size_t match_offset;
	struct fp_img *img;
};

static void sync_identify_cb(struct fp_dev *dev, int result,
	size_t match_offset, struct fp_img *img, void *user_data)
{
	struct sync_identify_data *idata = user_data;
	idata->result = result;
	idata->match_offset = match_offset;
	idata->img = img;
	idata->populated = TRUE;
}

static void identify_stop_cb(struct fp_dev *dev, void *user_data)
{
	gboolean *stopped = user_data;
	fp_dbg("");
	*stopped = TRUE;
}

/** \ingroup dev
 * Performs a new scan and attempts to identify the scanned finger against
 * a collection of previously enrolled fingerprints.
 * If the device is an imaging device, it can also return the image from
 * the scan, even when identification fails with a RETRY code. It is legal to
 * call this function even on non-imaging devices, just don't expect them to
 * provide images.
 *
 * This function returns codes from #fp_verify_result. The return code
 * fp_verify_result#FP_VERIFY_MATCH indicates that the scanned fingerprint
 * does appear in the print gallery, and the match_offset output parameter
 * will indicate the index into the print gallery array of the matched print.
 *
 * This function will not necessarily examine the whole print gallery, it
 * will return as soon as it finds a matching print.
 *
 * Not all devices support identification. -ENOTSUP will be returned when
 * this is the case.
 *
 * \param dev the device to perform the scan.
 * \param print_gallery NULL-terminated array of pointers to the prints to
 * identify against. Each one must have been previously enrolled with a device
 * compatible to the device selected to perform the scan.
 * \param match_offset output location to store the array index of the matched
 * gallery print (if any was found). Only valid if FP_VERIFY_MATCH was
 * returned.
 * \param img location to store the scan image. accepts NULL for no image
 * storage. If an image is returned, it must be freed with fp_img_free() after
 * use.
 * \return negative code on error, otherwise a code from #fp_verify_result
 */
API_EXPORTED int fp_identify_finger_img(struct fp_dev *dev,
	struct fp_print_data **print_gallery, size_t *match_offset,
	struct fp_img **img)
{
	gboolean stopped = FALSE;
	struct sync_identify_data *idata
		= g_malloc0(sizeof(struct sync_identify_data));
	int r;

	fp_dbg("to be handled by %s", dev->drv->name);

	r = fp_async_identify_start(dev, print_gallery, sync_identify_cb, idata);
	if (r < 0) {
		fp_err("identify_start error %d", r);
		goto err;
	}

	while (!idata->populated) {
		r = fp_handle_events();
		if (r < 0)
			goto err_stop;
	}

	if (img)
		*img = idata->img;
	else
		fp_img_free(idata->img);

	r = idata->result;
	switch (idata->result) {
	case FP_VERIFY_NO_MATCH:
		fp_dbg("result: no match");
		break;
	case FP_VERIFY_MATCH:
		fp_dbg("result: match at offset %zd", idata->match_offset);
		*match_offset = idata->match_offset;
		break;
	case FP_VERIFY_RETRY:
		fp_dbg("verify should retry");
		break;
	case FP_VERIFY_RETRY_TOO_SHORT:
		fp_dbg("swipe was too short, verify should retry");
		break;
	case FP_VERIFY_RETRY_CENTER_FINGER:
		fp_dbg("finger was not centered, verify should retry");
		break;
	case FP_VERIFY_RETRY_REMOVE_FINGER:
		fp_dbg("scan failed, remove finger and retry");
		break;
	default:
		fp_err("unrecognised return code %d", r);
		r = -EINVAL;
	}

err_stop:
	if (fp_async_identify_stop(dev, identify_stop_cb, &stopped) == 0)
		while (!stopped)
			if (fp_handle_events() < 0)
				break;

err:
	g_free(idata);
	return r;
}

struct sync_capture_data {
	gboolean populated;
	int result;
	struct fp_img *img;
};

static void sync_capture_cb(struct fp_dev *dev, int result, struct fp_img *img,
	void *user_data)
{
	struct sync_capture_data *vdata = user_data;
	vdata->result = result;
	vdata->img = img;
	vdata->populated = TRUE;
}

static void capture_stop_cb(struct fp_dev *dev, void *user_data)
{
	gboolean *stopped = user_data;
	fp_dbg("");
	*stopped = TRUE;
}
/** \ingroup dev
 * Captures an \ref img "image" from a device. The returned image is the raw
 * image provided by the device, you may wish to \ref img_std "standardize" it.
 *
 * If set, the <tt>unconditional</tt> flag indicates that the device should
 * capture an image unconditionally, regardless of whether a finger is there
 * or not. If unset, this function will block until a finger is detected on
 * the sensor.
 *
 * \param dev the device
 * \param unconditional whether to unconditionally capture an image, or to only capture when a finger is detected
 * \param img a location to return the captured image. Must be freed with
 * fp_img_free() after use.
 * \return 0 on success, non-zero on error. -ENOTSUP indicates that either the
 * unconditional flag was set but the device does not support this, or that the
 * device does not support imaging.
 * \sa fp_dev_supports_imaging()
 */
API_EXPORTED int fp_dev_img_capture(struct fp_dev *dev, int unconditional,
	struct fp_img **img)
{
	struct sync_capture_data *vdata;
	gboolean stopped = FALSE;
	int r;

	if (!dev->drv->capture_start) {
		fp_dbg("image capture is not supported on %s device", dev->drv->name);
		return -ENOTSUP;
	}

	fp_dbg("to be handled by %s", dev->drv->name);
	vdata = g_malloc0(sizeof(struct sync_capture_data));
	r = fp_async_capture_start(dev, unconditional, sync_capture_cb, vdata);
	if (r < 0) {
		fp_dbg("capture_start error %d", r);
		g_free(vdata);
		return r;
	}

	while (!vdata->populated) {
		r = fp_handle_events();
		if (r < 0) {
			g_free(vdata);
			goto err;
		}
	}

	if (img)
		*img = vdata->img;
	else
		fp_img_free(vdata->img);

	r = vdata->result;
	g_free(vdata);
	switch (r) {
	case FP_CAPTURE_COMPLETE:
		fp_dbg("result: complete");
		break;
	case FP_CAPTURE_FAIL:
		fp_dbg("result: fail");
		break;
	default:
		fp_err("unrecognised return code %d", r);
		r = -EINVAL;
	}

err:
	fp_dbg("ending capture");
	if (fp_async_capture_stop(dev, capture_stop_cb, &stopped) == 0)
		while (!stopped)
			if (fp_handle_events() < 0)
				break;

	return r;
}

