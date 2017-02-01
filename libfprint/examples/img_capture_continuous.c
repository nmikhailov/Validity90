/*
 * Example libfprint continuous image capture program
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libfprint/fprint.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/Xvlib.h>

#define FORMAT 0x32595559

static int adaptor = -1;
static char *framebuffer = NULL;

static Display *display = NULL;
static Window window=(Window)NULL;
static XvImage *xv_image = NULL;
static XvAdaptorInfo *info;
static GC gc;
static int connection = -1;

/* based on macro by Bart Nabbe */
#define GREY2YUV(grey, y, u, v)\
  y = (9798*grey + 19235*grey + 3736*grey)  / 32768;\
  u = (-4784*grey - 9437*grey + 14221*grey)  / 32768 + 128;\
  v = (20218*grey - 16941*grey - 3277*grey) / 32768 + 128;\
  y = y < 0 ? 0 : y;\
  u = u < 0 ? 0 : u;\
  v = v < 0 ? 0 : v;\
  y = y > 255 ? 255 : y;\
  u = u > 255 ? 255 : u;\
  v = v > 255 ? 255 : v

static void grey2yuy2 (unsigned char *grey, char *YUV, int num) {
	int i, j;
	int y0, y1, u0, u1, v0, v1;
	int gval;

	for (i = 0, j = 0; i < num; i += 2, j += 4)
	{
		gval = grey[i];
		GREY2YUV (gval, y0, u0 , v0);
		gval = grey[i + 1];
		GREY2YUV (gval, y1, u1 , v1);
		YUV[j + 0] = y0;
		YUV[j + 1] = (u0+u1)/2;
		YUV[j + 2] = y1;
		YUV[j + 3] = (v0+v1)/2;
	}
}

static void display_frame(struct fp_img *img)
{
	int width = fp_img_get_width(img);
	int height = fp_img_get_height(img);
	unsigned char *data = fp_img_get_data(img);

	if (adaptor < 0)
		return;

	grey2yuy2(data, framebuffer, width * height);
	xv_image = XvCreateImage(display, info[adaptor].base_id, FORMAT,
			framebuffer, width, height);
	XvPutImage(display, info[adaptor].base_id, window, gc, xv_image,
			0, 0, width, height, 0, 0, width, height);
}

static void QueryXv()
{
	unsigned int num_adaptors;
	int num_formats;
	XvImageFormatValues *formats = NULL;
	int i,j;
	char xv_name[5];

	XvQueryAdaptors(display, DefaultRootWindow(display), &num_adaptors,
			&info);

	for(i = 0; i < num_adaptors; i++) {
		formats = XvListImageFormats(display, info[i].base_id,
				&num_formats);
		for(j = 0; j < num_formats; j++) {
			xv_name[4] = 0;
			memcpy(xv_name, &formats[j].id, 4);
			if(formats[j].id == FORMAT) {
				printf("using Xv format 0x%x %s %s\n",
						formats[j].id, xv_name,
						(formats[j].format==XvPacked)
						? "packed" : "planar");
				if (adaptor < 0)
					adaptor = i;
			}
		}
	}
	XFree(formats);
	if (adaptor < 0)
		printf("No suitable Xv adaptor found\n");
}

struct fp_dscv_dev *discover_device(struct fp_dscv_dev **discovered_devs)
{
	struct fp_dscv_dev *ddev = discovered_devs[0];
	struct fp_driver *drv;
	if (!ddev)
		return NULL;
	
	drv = fp_dscv_dev_get_driver(ddev);
	printf("Found device claimed by %s driver\n", fp_driver_get_full_name(drv));
	return ddev;
}

int main(void)
{
	int r = 1;
	XEvent xev;
	XGCValues xgcv;
	long background=0x010203;
	struct fp_dscv_dev *ddev;
	struct fp_dscv_dev **discovered_devs;
	struct fp_dev *dev;
	int img_width;
	int img_height;
	int standardize = 0;

	r = fp_init();
	if (r < 0) {
		fprintf(stderr, "Failed to initialize libfprint\n");
		exit(1);
	}
	fp_set_debug(3);

	discovered_devs = fp_discover_devs();
	if (!discovered_devs) {
		fprintf(stderr, "Could not discover devices\n");
		goto out;
	}

	ddev = discover_device(discovered_devs);
	if (!ddev) {
		fprintf(stderr, "No devices detected.\n");
		goto out;
	}

	dev = fp_dev_open(ddev);
	fp_dscv_devs_free(discovered_devs);
	if (!dev) {
		fprintf(stderr, "Could not open device.\n");
		goto out;
	}

	if (!fp_dev_supports_imaging(dev)) {
		fprintf(stderr, "this device does not have imaging capabilities.\n");
		goto out_close;
	}

	img_width = fp_dev_get_img_width(dev);
	img_height = fp_dev_get_img_height(dev);
	if (img_width <= 0 || img_height <= 0) {
		fprintf(stderr, "this device returns images with variable dimensions,"
			" this example does not support that.\n");
		goto out_close;
	}
	framebuffer = malloc(img_width * img_height * 2);
	if (!framebuffer)
		goto out_close;

	/* make the window */
	display = XOpenDisplay(getenv("DISPLAY"));
	if(display == NULL) {
		fprintf(stderr,"Could not open display \"%s\"\n",
				getenv("DISPLAY"));
		goto out_close;
	}

	QueryXv();

	if (adaptor < 0)
		goto out_close;

	window = XCreateSimpleWindow(display, DefaultRootWindow(display), 0, 0,
			img_width, img_height, 0,
			WhitePixel(display, DefaultScreen(display)), background);

	XSelectInput(display, window, StructureNotifyMask | KeyPressMask);
	XMapWindow(display, window);
	connection = ConnectionNumber(display);

	gc = XCreateGC(display, window, 0, &xgcv);

	printf("Press S to toggle standardized mode, Q to quit\n");
	
	while (1) { /* event loop */
		struct fp_img *img;

		r = fp_dev_img_capture(dev, 1, &img);
		if (r) {
			fprintf(stderr, "image capture failed, code %d\n", r);
			goto out_close;
		}
		if (standardize)
			fp_img_standardize(img);

		display_frame(img);
		fp_img_free(img);
		XFlush(display);

		while (XPending(display) > 0) {
			XNextEvent(display, &xev);
			if (xev.type != KeyPress)
				continue;

			switch (XKeycodeToKeysym(display, xev.xkey.keycode, 0)) {
			case XK_q:
			case XK_Q:
				r = 0;
				goto out_close;
				break;
			case XK_s:
			case XK_S:
				standardize = !standardize;
				break;
			}
		} /* XPending */
	}

	r = 0;
out_close:
	if (framebuffer)
		free(framebuffer);
	fp_dev_close(dev);
	if ((void *) window != NULL)
		XUnmapWindow(display, window);
	if (display != NULL)
		XFlush(display);
out:
	fp_exit();
	return r;
}


