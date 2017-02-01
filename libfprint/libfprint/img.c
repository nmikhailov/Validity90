/*
 * Image management functions for libfprint
 * Copyright (C) 2007 Daniel Drake <dsd@gentoo.org>
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

#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <glib.h>

#include "fp_internal.h"
#include "nbis/include/bozorth.h"
#include "nbis/include/lfs.h"

/** @defgroup img Image operations
 * libfprint offers several ways of retrieving images from imaging devices,
 * one example being the fp_dev_img_capture() function. The functions
 * documented below allow you to work with such images.
 *
 * \section img_fmt Image format
 * All images are represented as 8-bit greyscale data.
 *
 * \section img_std Image standardization
 * In some contexts, images you are provided through libfprint are raw images
 * from the hardware. The orientation of these varies from device-to-device,
 * as does the color scheme (black-on-white or white-on-black?). libfprint
 * provides the fp_img_standardize function to convert images into standard
 * form, which is defined to be: finger flesh as black on white surroundings,
 * natural upright orientation.
 */

struct fp_img *fpi_img_new(size_t length)
{
	struct fp_img *img = g_malloc0(sizeof(*img) + length);
	fp_dbg("length=%zd", length);
	img->length = length;
	return img;
}

struct fp_img *fpi_img_new_for_imgdev(struct fp_img_dev *imgdev)
{
	struct fp_img_driver *imgdrv = fpi_driver_to_img_driver(imgdev->dev->drv);
	int width = imgdrv->img_width;
	int height = imgdrv->img_height;
	struct fp_img *img = fpi_img_new(width * height);
	img->width = width;
	img->height = height;
	return img;
}

gboolean fpi_img_is_sane(struct fp_img *img)
{
	/* basic checks */
	if (!img->length || !img->width || !img->height)
		return FALSE;

	/* buffer is big enough? */
	if ((img->length * img->height) < img->length)
		return FALSE;

	return TRUE;
}

struct fp_img *fpi_img_resize(struct fp_img *img, size_t newsize)
{
	return g_realloc(img, sizeof(*img) + newsize);
}

/** \ingroup img
 * Frees an image. Must be called when you are finished working with an image.
 * \param img the image to destroy. If NULL, function simply returns.
 */
API_EXPORTED void fp_img_free(struct fp_img *img)
{
	if (!img)
		return;

	if (img->minutiae)
		free_minutiae(img->minutiae);
	if (img->binarized)
		free(img->binarized);
	g_free(img);
}

/** \ingroup img
 * Gets the pixel height of an image.
 * \param img an image
 * \returns the height of the image
 */
API_EXPORTED int fp_img_get_height(struct fp_img *img)
{
	return img->height;
}

/** \ingroup img
 * Gets the pixel width of an image.
 * \param img an image
 * \returns the width of the image
 */
API_EXPORTED int fp_img_get_width(struct fp_img *img)
{
	return img->width;
}

/** \ingroup img
 * Gets the greyscale data for an image. This data must not be modified or
 * freed, and must not be used after fp_img_free() has been called.
 * \param img an image
 * \returns a pointer to libfprint's internal data for the image
 */
API_EXPORTED unsigned char *fp_img_get_data(struct fp_img *img)
{
	return img->data;
}

/** \ingroup img
 * A quick convenience function to save an image to a file in
 * <a href="http://netpbm.sourceforge.net/doc/pgm.html">PGM format</a>.
 * \param img the image to save
 * \param path the path to save the image. Existing files will be overwritten.
 * \returns 0 on success, non-zero on error.
 */
API_EXPORTED int fp_img_save_to_file(struct fp_img *img, char *path)
{
	FILE *fd = fopen(path, "w");
	size_t write_size = img->width * img->height;
	int r;

	if (!fd) {
		fp_dbg("could not open '%s' for writing: %d", path, errno);
		return -errno;
	}

	r = fprintf(fd, "P5 %d %d 255\n", img->width, img->height);
	if (r < 0) {
		fp_err("pgm header write failed, error %d", r);
		return r;
	}

	r = fwrite(img->data, 1, write_size, fd);
	if (r < write_size) {
		fp_err("short write (%d)", r);
		return -EIO;
	}

	fclose(fd);
	fp_dbg("written to '%s'", path);
	return 0;
}

static void vflip(struct fp_img *img)
{
	int width = img->width;
	int data_len = img->width * img->height;
	unsigned char rowbuf[width];
	int i;

	for (i = 0; i < img->height / 2; i++) {
		int offset = i * width;
		int swap_offset = data_len - (width * (i + 1));

		/* copy top row into buffer */
		memcpy(rowbuf, img->data + offset, width);

		/* copy lower row over upper row */
		memcpy(img->data + offset, img->data + swap_offset, width);

		/* copy buffer over lower row */
		memcpy(img->data + swap_offset, rowbuf, width);
	}
}

static void hflip(struct fp_img *img)
{
	int width = img->width;
	unsigned char rowbuf[width];
	int i, j;

	for (i = 0; i < img->height; i++) {
		int offset = i * width;

		memcpy(rowbuf, img->data + offset, width);
		for (j = 0; j < width; j++)
			img->data[offset + j] = rowbuf[width - j - 1];
	}
}

static void invert_colors(struct fp_img *img)
{
	int data_len = img->width * img->height;
	int i;
	for (i = 0; i < data_len; i++)
		img->data[i] = 0xff - img->data[i];
}

/** \ingroup img
 * \ref img_std "Standardizes" an image by normalizing its orientation, colors,
 * etc. It is safe to call this multiple times on an image, libfprint keeps
 * track of the work it needs to do to make an image standard and will not
 * perform these operations more than once for a given image.
 * \param img the image to standardize
 */
API_EXPORTED void fp_img_standardize(struct fp_img *img)
{
	if (img->flags & FP_IMG_V_FLIPPED) {
		vflip(img);
		img->flags &= ~FP_IMG_V_FLIPPED;
	}
	if (img->flags & FP_IMG_H_FLIPPED) {
		hflip(img);
		img->flags &= ~FP_IMG_H_FLIPPED;
	}
	if (img->flags & FP_IMG_COLORS_INVERTED) {
		invert_colors(img);
		img->flags &= ~FP_IMG_COLORS_INVERTED;
	}
}

/* Based on write_minutiae_XYTQ and bz_load */
static void minutiae_to_xyt(struct fp_minutiae *minutiae, int bwidth,
	int bheight, unsigned char *buf)
{
	int i;
	struct fp_minutia *minutia;
	struct minutiae_struct c[MAX_FILE_MINUTIAE];
	struct xyt_struct *xyt = (struct xyt_struct *) buf;

	/* FIXME: only considers first 150 minutiae (MAX_FILE_MINUTIAE) */
	/* nist does weird stuff with 150 vs 1000 limits */
	int nmin = min(minutiae->num, MAX_FILE_MINUTIAE);

	for (i = 0; i < nmin; i++){
		minutia = minutiae->list[i];

		lfs2nist_minutia_XYT(&c[i].col[0], &c[i].col[1], &c[i].col[2],
				minutia, bwidth, bheight);
		c[i].col[3] = sround(minutia->reliability * 100.0);

		if (c[i].col[2] > 180)
			c[i].col[2] -= 360;
	}

	qsort((void *) &c, (size_t) nmin, sizeof(struct minutiae_struct),
			sort_x_y);

	for (i = 0; i < nmin; i++) {
		xyt->xcol[i]     = c[i].col[0];
		xyt->ycol[i]     = c[i].col[1];
		xyt->thetacol[i] = c[i].col[2];
	}
	xyt->nrows = nmin;
}

int fpi_img_detect_minutiae(struct fp_img *img)
{
	struct fp_minutiae *minutiae;
	int r;
	int *direction_map, *low_contrast_map, *low_flow_map;
	int *high_curve_map, *quality_map;
	int map_w, map_h;
	unsigned char *bdata;
	int bw, bh, bd;
	GTimer *timer;

	if (img->flags & FP_IMG_STANDARDIZATION_FLAGS) {
		fp_err("cant detect minutiae for non-standardized image");
		return -EINVAL;
	}

	/* Remove perimeter points from partial image */
	g_lfsparms_V2.remove_perimeter_pts = img->flags & FP_IMG_PARTIAL ? TRUE : FALSE;

	/* 25.4 mm per inch */
	timer = g_timer_new();
	r = get_minutiae(&minutiae, &quality_map, &direction_map,
                         &low_contrast_map, &low_flow_map, &high_curve_map,
                         &map_w, &map_h, &bdata, &bw, &bh, &bd,
                         img->data, img->width, img->height, 8,
						 DEFAULT_PPI / (double)25.4, &g_lfsparms_V2);
	g_timer_stop(timer);
	fp_dbg("minutiae scan completed in %f secs", g_timer_elapsed(timer, NULL));
	g_timer_destroy(timer);
	if (r) {
		fp_err("get minutiae failed, code %d", r);
		return r;
	}
	fp_dbg("detected %d minutiae", minutiae->num);
	img->minutiae = minutiae;
	img->binarized = bdata;

	free(quality_map);
	free(direction_map);
	free(low_contrast_map);
	free(low_flow_map);
	free(high_curve_map);
	return minutiae->num;
}

int fpi_img_to_print_data(struct fp_img_dev *imgdev, struct fp_img *img,
	struct fp_print_data **ret)
{
	struct fp_print_data *print;
	struct fp_print_data_item *item;
	int r;

	if (!img->minutiae) {
		r = fpi_img_detect_minutiae(img);
		if (r < 0)
			return r;
		if (!img->minutiae) {
			fp_err("no minutiae after successful detection?");
			return -ENOENT;
		}
	}

	/* FIXME: space is wasted if we dont hit the max minutiae count. would
	 * be good to make this dynamic. */
	print = fpi_print_data_new(imgdev->dev);
	item = fpi_print_data_item_new(sizeof(struct xyt_struct));
	print->type = PRINT_DATA_NBIS_MINUTIAE;
	minutiae_to_xyt(img->minutiae, img->width, img->height, item->data);
	print->prints = g_slist_prepend(print->prints, item);

	/* FIXME: the print buffer at this point is endian-specific, and will
	 * only work when loaded onto machines with identical endianness. not good!
	 * data format should be platform-independent. */
	*ret = print;

	return 0;
}

int fpi_img_compare_print_data(struct fp_print_data *enrolled_print,
	struct fp_print_data *new_print)
{
	int score, max_score = 0, probe_len;
	struct xyt_struct *pstruct = NULL;
	struct xyt_struct *gstruct = NULL;
	struct fp_print_data_item *data_item;
	GSList *list_item;

	if (enrolled_print->type != PRINT_DATA_NBIS_MINUTIAE ||
	     new_print->type != PRINT_DATA_NBIS_MINUTIAE) {
		fp_err("invalid print format");
		return -EINVAL;
	}

	if (g_slist_length(new_print->prints) != 1) {
		fp_err("new_print contains more than one sample, is it enrolled print?");
		return -EINVAL;
	}

	data_item = new_print->prints->data;
	pstruct = (struct xyt_struct *)data_item->data;

	probe_len = bozorth_probe_init(pstruct);
	list_item = enrolled_print->prints;
	do {
		data_item = list_item->data;
		gstruct = (struct xyt_struct *)data_item->data;
		score = bozorth_to_gallery(probe_len, pstruct, gstruct);
		fp_dbg("score %d", score);
		max_score = max(score, max_score);
		list_item = g_slist_next(list_item);
	} while (list_item);

	return max_score;
}

int fpi_img_compare_print_data_to_gallery(struct fp_print_data *print,
	struct fp_print_data **gallery, int match_threshold, size_t *match_offset)
{
	struct xyt_struct *pstruct;
	struct xyt_struct *gstruct;
	struct fp_print_data *gallery_print;
	struct fp_print_data_item *data_item;
	int probe_len;
	size_t i = 0;
	int r;
	GSList *list_item;

	if (g_slist_length(print->prints) != 1) {
		fp_err("new_print contains more than one sample, is it enrolled print?");
		return -EINVAL;
	}

	data_item = print->prints->data;
	pstruct = (struct xyt_struct *)data_item->data;

	probe_len = bozorth_probe_init(pstruct);
	while ((gallery_print = gallery[i++])) {
		list_item = gallery_print->prints;
		do {
			data_item = list_item->data;
			gstruct = (struct xyt_struct *)data_item->data;
			r = bozorth_to_gallery(probe_len, pstruct, gstruct);
			if (r >= match_threshold) {
				*match_offset = i - 1;
				return FP_VERIFY_MATCH;
			}
			list_item = g_slist_next(list_item);
		} while (list_item);
	}
	return FP_VERIFY_NO_MATCH;
}

/** \ingroup img
 * Get a binarized form of a standardized scanned image. This is where the
 * fingerprint image has been "enhanced" and is a set of pure black ridges
 * on a pure white background. Internally, image processing happens on top
 * of the binarized image.
 *
 * The image must have been \ref img_std "standardized" otherwise this function
 * will fail.
 *
 * It is safe to binarize an image and free the original while continuing
 * to use the binarized version.
 *
 * You cannot binarize an image twice.
 *
 * \param img a standardized image
 * \returns a new image representing the binarized form of the original, or
 * NULL on error. Must be freed with fp_img_free() after use.
 */
API_EXPORTED struct fp_img *fp_img_binarize(struct fp_img *img)
{
	struct fp_img *ret;
	int height = img->height;
	int width = img->width;
	int imgsize = height * width;

	if (img->flags & FP_IMG_BINARIZED_FORM) {
		fp_err("image already binarized");
		return NULL;
	}

	if (!img->binarized) {
		int r = fpi_img_detect_minutiae(img);
		if (r < 0)
			return NULL;
		if (!img->binarized) {
			fp_err("no minutiae after successful detection?");
			return NULL;
		}
	}

	ret = fpi_img_new(imgsize);
	ret->flags |= FP_IMG_BINARIZED_FORM;
	ret->width = width;
	ret->height = height;
	memcpy(ret->data, img->binarized, imgsize);
	return ret;
}

/** \ingroup img
 * Get a list of minutiae detected in an image. A minutia point is a feature
 * detected on a fingerprint, typically where ridges end or split.
 * libfprint's image processing code relies upon comparing sets of minutiae,
 * so accurate placement of minutia points is critical for good imaging
 * performance.
 *
 * The image must have been \ref img_std "standardized" otherwise this function
 * will fail.
 *
 * You cannot pass a binarized image to this function. Instead, pass the
 * original image.
 *
 * Returns a list of pointers to minutiae, where the list is of length
 * indicated in the nr_minutiae output parameter. The returned list is only
 * valid while the parent image has not been freed, and the minutiae data
 * must not be modified or freed.
 *
 * \param img a standardized image
 * \param nr_minutiae an output location to store minutiae list length
 * \returns a list of minutiae points. Must not be modified or freed.
 */
API_EXPORTED struct fp_minutia **fp_img_get_minutiae(struct fp_img *img,
	int *nr_minutiae)
{
	if (img->flags & FP_IMG_BINARIZED_FORM) {
		fp_err("image is binarized");
		return NULL;
	}

	if (!img->minutiae) {
		int r = fpi_img_detect_minutiae(img);
		if (r < 0)
			return NULL;
		if (!img->minutiae) {
			fp_err("no minutiae after successful detection?");
			return NULL;
		}
	}

	*nr_minutiae = img->minutiae->num;
	return img->minutiae->list;
}

/* Calculate squared standand deviation */
int fpi_std_sq_dev(const unsigned char *buf, int size)
{
	int res = 0, mean = 0, i;

	if (size > (INT_MAX / 65536)) {
		fp_err("%s: we might get an overflow!", __func__);
		return -EOVERFLOW;
	}

	for (i = 0; i < size; i++)
		mean += buf[i];

	mean /= size;

	for (i = 0; i < size; i++) {
		int dev = (int)buf[i] - mean;
		res += dev*dev;
	}

	return res / size;
}

/* Calculate normalized mean square difference of two lines */
int fpi_mean_sq_diff_norm(unsigned char *buf1, unsigned char *buf2, int size)
{
	int res = 0, i;
	for (i = 0; i < size; i++) {
		int dev = (int)buf1[i] - (int)buf2[i];
		res += dev * dev;
	}

	return res / size;
}
