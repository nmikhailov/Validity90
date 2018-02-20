/*
 * Internal/private definitions for libfprint
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

#ifndef __FPRINT_INTERNAL_H__
#define __FPRINT_INTERNAL_H__

#include <config.h>
#include <stdint.h>

#include <glib.h>
#include <libusb.h>

#include <fprint.h>

#define array_n_elements(array) (sizeof(array) / sizeof(array[0]))

#define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) );})

enum fpi_log_level {
	FPRINT_LOG_LEVEL_DEBUG,
	FPRINT_LOG_LEVEL_INFO,
	FPRINT_LOG_LEVEL_WARNING,
	FPRINT_LOG_LEVEL_ERROR,
};

void fpi_log(enum fpi_log_level, const char *component, const char *function,
	const char *format, ...);

#ifndef FP_COMPONENT
#define FP_COMPONENT NULL
#endif

#ifdef ENABLE_LOGGING
#define _fpi_log(level, fmt...) fpi_log(level, FP_COMPONENT, __FUNCTION__, fmt)
#else
#define _fpi_log(level, fmt...)
#endif

#ifdef ENABLE_DEBUG_LOGGING
#define fp_dbg(fmt...) _fpi_log(FPRINT_LOG_LEVEL_DEBUG, fmt)
#else
#define fp_dbg(fmt...)
#endif

#define fp_info(fmt...) _fpi_log(FPRINT_LOG_LEVEL_INFO, fmt)
#define fp_warn(fmt...) _fpi_log(FPRINT_LOG_LEVEL_WARNING, fmt)
#define fp_err(fmt...) _fpi_log(FPRINT_LOG_LEVEL_ERROR, fmt)

#ifndef NDEBUG
#define BUG_ON(condition) \
	if ((condition)) fp_err("BUG at %s:%d", __FILE__, __LINE__)
#else
#define BUG_ON(condition)
#endif

#define BUG() BUG_ON(1)

enum fp_dev_state {
	DEV_STATE_INITIAL = 0,
	DEV_STATE_ERROR,
	DEV_STATE_INITIALIZING,
	DEV_STATE_INITIALIZED,
	DEV_STATE_DEINITIALIZING,
	DEV_STATE_DEINITIALIZED,
	DEV_STATE_ENROLL_STARTING,
	DEV_STATE_ENROLLING,
	DEV_STATE_ENROLL_STOPPING,
	DEV_STATE_VERIFY_STARTING,
	DEV_STATE_VERIFYING,
	DEV_STATE_VERIFY_DONE,
	DEV_STATE_VERIFY_STOPPING,
	DEV_STATE_IDENTIFY_STARTING,
	DEV_STATE_IDENTIFYING,
	DEV_STATE_IDENTIFY_DONE,
	DEV_STATE_IDENTIFY_STOPPING,
	DEV_STATE_CAPTURE_STARTING,
	DEV_STATE_CAPTURING,
	DEV_STATE_CAPTURE_DONE,
	DEV_STATE_CAPTURE_STOPPING,
};

struct fp_driver **fprint_get_drivers (void);

struct fp_dev {
	struct fp_driver *drv;
	libusb_device_handle *udev;
	uint32_t devtype;
	void *priv;

	int nr_enroll_stages;

	/* read-only to drivers */
	struct fp_print_data *verify_data;

	/* drivers should not mess with any of the below */
	enum fp_dev_state state;
	int __enroll_stage;
	int unconditional_capture;

	/* async I/O callbacks and data */
	/* FIXME: convert this to generic state operational data mechanism? */
	fp_dev_open_cb open_cb;
	void *open_cb_data;
	fp_dev_close_cb close_cb;
	void *close_cb_data;
	fp_enroll_stage_cb enroll_stage_cb;
	void *enroll_stage_cb_data;
	fp_enroll_stop_cb enroll_stop_cb;
	void *enroll_stop_cb_data;
	fp_verify_cb verify_cb;
	void *verify_cb_data;
	fp_verify_stop_cb verify_stop_cb;
	void *verify_stop_cb_data;
	fp_identify_cb identify_cb;
	void *identify_cb_data;
	fp_identify_stop_cb identify_stop_cb;
	void *identify_stop_cb_data;
	fp_capture_cb capture_cb;
	void *capture_cb_data;
	fp_capture_stop_cb capture_stop_cb;
	void *capture_stop_cb_data;

	/* FIXME: better place to put this? */
	struct fp_print_data **identify_gallery;
};

enum fp_imgdev_state {
	IMGDEV_STATE_INACTIVE,
	IMGDEV_STATE_AWAIT_FINGER_ON,
	IMGDEV_STATE_CAPTURE,
	IMGDEV_STATE_AWAIT_FINGER_OFF,
};

enum fp_imgdev_action {
	IMG_ACTION_NONE = 0,
	IMG_ACTION_ENROLL,
	IMG_ACTION_VERIFY,
	IMG_ACTION_IDENTIFY,
	IMG_ACTION_CAPTURE,
};

enum fp_imgdev_enroll_state {
	IMG_ACQUIRE_STATE_NONE = 0,
	IMG_ACQUIRE_STATE_ACTIVATING,
	IMG_ACQUIRE_STATE_AWAIT_FINGER_ON,
	IMG_ACQUIRE_STATE_AWAIT_IMAGE,
	IMG_ACQUIRE_STATE_AWAIT_FINGER_OFF,
	IMG_ACQUIRE_STATE_DONE,
	IMG_ACQUIRE_STATE_DEACTIVATING,
};

enum fp_imgdev_verify_state {
	IMG_VERIFY_STATE_NONE = 0,
	IMG_VERIFY_STATE_ACTIVATING
};

struct fp_img_dev {
	struct fp_dev *dev;
	libusb_device_handle *udev;
	enum fp_imgdev_action action;
	int action_state;

	struct fp_print_data *acquire_data;
	struct fp_print_data *enroll_data;
	struct fp_img *acquire_img;
	int enroll_stage;
	int action_result;

	/* FIXME: better place to put this? */
	size_t identify_match_offset;

	void *priv;
};

int fpi_imgdev_get_img_width(struct fp_img_dev *imgdev);
int fpi_imgdev_get_img_height(struct fp_img_dev *imgdev);

struct usb_id {
	uint16_t vendor;
	uint16_t product;
	unsigned long driver_data;
};

enum fp_driver_type {
	DRIVER_PRIMITIVE = 0,
	DRIVER_IMAGING = 1,
};

struct fp_driver {
	const uint16_t id;
	const char *name;
	const char *full_name;
	const struct usb_id * const id_table;
	enum fp_driver_type type;
	enum fp_scan_type scan_type;

	void *priv;

	/* Device operations */
	int (*discover)(struct libusb_device_descriptor *dsc, uint32_t *devtype);
	int (*open)(struct fp_dev *dev, unsigned long driver_data);
	void (*close)(struct fp_dev *dev);
	int (*enroll_start)(struct fp_dev *dev);
	int (*enroll_stop)(struct fp_dev *dev);
	int (*verify_start)(struct fp_dev *dev);
	int (*verify_stop)(struct fp_dev *dev, gboolean iterating);
	int (*identify_start)(struct fp_dev *dev);
	int (*identify_stop)(struct fp_dev *dev, gboolean iterating);
	int (*capture_start)(struct fp_dev *dev);
	int (*capture_stop)(struct fp_dev *dev);
};

enum fp_print_data_type fpi_driver_get_data_type(struct fp_driver *drv);

/* flags for fp_img_driver.flags */
#define FP_IMGDRV_SUPPORTS_UNCONDITIONAL_CAPTURE (1 << 0)

struct fp_img_driver {
	struct fp_driver driver;
	uint16_t flags;
	int img_width;
	int img_height;
	int bz3_threshold;

	/* Device operations */
	int (*open)(struct fp_img_dev *dev, unsigned long driver_data);
	void (*close)(struct fp_img_dev *dev);
	int (*activate)(struct fp_img_dev *dev, enum fp_imgdev_state state);
	int (*change_state)(struct fp_img_dev *dev, enum fp_imgdev_state state);
	void (*deactivate)(struct fp_img_dev *dev);
};

#ifdef ENABLE_UPEKTS
extern struct fp_driver upekts_driver;
#endif
#ifdef ENABLE_UPEKE2
extern struct fp_driver upeke2_driver;
#endif
#ifdef ENABLE_UPEKTC
extern struct fp_img_driver upektc_driver;
#endif
#ifdef ENABLE_UPEKSONLY
extern struct fp_img_driver upeksonly_driver;
#endif
#ifdef ENABLE_URU4000
extern struct fp_img_driver uru4000_driver;
#endif
#ifdef ENABLE_AES1610
extern struct fp_img_driver aes1610_driver;
#endif
#ifdef ENABLE_AES1660
extern struct fp_img_driver aes1660_driver;
#endif
#ifdef ENABLE_AES2501
extern struct fp_img_driver aes2501_driver;
#endif
#ifdef ENABLE_AES2550
extern struct fp_img_driver aes2550_driver;
#endif
#ifdef ENABLE_AES2660
extern struct fp_img_driver aes2660_driver;
#endif
#ifdef ENABLE_AES3500
extern struct fp_img_driver aes3500_driver;
#endif
#ifdef ENABLE_AES4000
extern struct fp_img_driver aes4000_driver;
#endif
#ifdef ENABLE_FDU2000
extern struct fp_img_driver fdu2000_driver;
#endif
#ifdef ENABLE_VCOM5S
extern struct fp_img_driver vcom5s_driver;
#endif
#ifdef ENABLE_VFS101
extern struct fp_img_driver vfs101_driver;
#endif
#ifdef ENABLE_VFS301
extern struct fp_img_driver vfs301_driver;
#endif
#ifdef ENABLE_VFS5011
extern struct fp_img_driver vfs5011_driver;
#endif
#ifdef ENABLE_UPEKTC_IMG
extern struct fp_img_driver upektc_img_driver;
#endif
#ifdef ENABLE_ETES603
extern struct fp_img_driver etes603_driver;
#endif
#ifdef ENABLE_VFS0050
extern struct fp_img_driver vfs0050_driver;
#endif
#ifdef ENABLE_ELAN
extern struct fp_img_driver elan_driver;
#endif

extern libusb_context *fpi_usb_ctx;
extern GSList *opened_devices;

void fpi_img_driver_setup(struct fp_img_driver *idriver);

#define fpi_driver_to_img_driver(drv) \
	container_of((drv), struct fp_img_driver, driver)

struct fp_dscv_dev {
	struct libusb_device *udev;
	struct fp_driver *drv;
	unsigned long driver_data;
	uint32_t devtype;
};

struct fp_dscv_print {
	uint16_t driver_id;
	uint32_t devtype;
	enum fp_finger finger;
	char *path;
};

enum fp_print_data_type {
	PRINT_DATA_RAW = 0, /* memset-imposed default */
	PRINT_DATA_NBIS_MINUTIAE,
};

struct fp_print_data_item {
	size_t length;
	unsigned char data[0];
};

struct fp_print_data {
	uint16_t driver_id;
	uint32_t devtype;
	enum fp_print_data_type type;
	GSList *prints;
};

struct fpi_print_data_fp2 {
	char prefix[3];
	uint16_t driver_id;
	uint32_t devtype;
	unsigned char data_type;
	unsigned char data[0];
} __attribute__((__packed__));

struct fpi_print_data_item_fp2 {
	uint32_t length;
	unsigned char data[0];
} __attribute__((__packed__));

void fpi_data_exit(void);
struct fp_print_data *fpi_print_data_new(struct fp_dev *dev);
struct fp_print_data_item *fpi_print_data_item_new(size_t length);
gboolean fpi_print_data_compatible(uint16_t driver_id1, uint32_t devtype1,
	enum fp_print_data_type type1, uint16_t driver_id2, uint32_t devtype2,
	enum fp_print_data_type type2);

struct fp_minutiae {
	int alloc;
	int num;
	struct fp_minutia **list;
};

/* bit values for fp_img.flags */
#define FP_IMG_V_FLIPPED	(1<<0)
#define FP_IMG_H_FLIPPED	(1<<1)
#define FP_IMG_COLORS_INVERTED	(1<<2)
#define FP_IMG_BINARIZED_FORM	(1<<3)
#define FP_IMG_PARTIAL		(1<<4)

#define FP_IMG_STANDARDIZATION_FLAGS (FP_IMG_V_FLIPPED | FP_IMG_H_FLIPPED \
	| FP_IMG_COLORS_INVERTED)

struct fp_img {
	int width;
	int height;
	size_t length;
	uint16_t flags;
	struct fp_minutiae *minutiae;
	unsigned char *binarized;
	unsigned char data[0];
};

struct fp_img *fpi_img_new(size_t length);
struct fp_img *fpi_img_new_for_imgdev(struct fp_img_dev *dev);
struct fp_img *fpi_img_resize(struct fp_img *img, size_t newsize);
gboolean fpi_img_is_sane(struct fp_img *img);
int fpi_img_detect_minutiae(struct fp_img *img);
int fpi_img_to_print_data(struct fp_img_dev *imgdev, struct fp_img *img,
	struct fp_print_data **ret);
int fpi_img_compare_print_data(struct fp_print_data *enrolled_print,
	struct fp_print_data *new_print);
int fpi_img_compare_print_data_to_gallery(struct fp_print_data *print,
	struct fp_print_data **gallery, int match_threshold, size_t *match_offset);
struct fp_img *fpi_im_resize(struct fp_img *img, unsigned int w_factor, unsigned int h_factor);

/* polling and timeouts */

void fpi_poll_init(void);
void fpi_poll_exit(void);

typedef void (*fpi_timeout_fn)(void *data);

struct fpi_timeout;
struct fpi_timeout *fpi_timeout_add(unsigned int msec, fpi_timeout_fn callback,
	void *data);
void fpi_timeout_cancel(struct fpi_timeout *timeout);

/* async drv <--> lib comms */

struct fpi_ssm;
typedef void (*ssm_completed_fn)(struct fpi_ssm *ssm);
typedef void (*ssm_handler_fn)(struct fpi_ssm *ssm);

/* sequential state machine: state machine that iterates sequentially over
 * a predefined series of states. can be aborted by either completion or
 * abortion error conditions. */
struct fpi_ssm {
	struct fp_dev *dev;
	struct fpi_ssm *parentsm;
	void *priv;
	int nr_states;
	int cur_state;
	gboolean completed;
	int error;
	ssm_completed_fn callback;
	ssm_handler_fn handler;
};


/* for library and drivers */
struct fpi_ssm *fpi_ssm_new(struct fp_dev *dev, ssm_handler_fn handler,
	int nr_states);
void fpi_ssm_free(struct fpi_ssm *machine);
void fpi_ssm_start(struct fpi_ssm *machine, ssm_completed_fn callback);
void fpi_ssm_start_subsm(struct fpi_ssm *parent, struct fpi_ssm *child);
int fpi_ssm_has_completed(struct fpi_ssm *machine);

/* for drivers */
void fpi_ssm_next_state(struct fpi_ssm *machine);
void fpi_ssm_jump_to_state(struct fpi_ssm *machine, int state);
void fpi_ssm_mark_completed(struct fpi_ssm *machine);
void fpi_ssm_mark_aborted(struct fpi_ssm *machine, int error);

void fpi_drvcb_open_complete(struct fp_dev *dev, int status);
void fpi_drvcb_close_complete(struct fp_dev *dev);

void fpi_drvcb_enroll_started(struct fp_dev *dev, int status);
void fpi_drvcb_enroll_stage_completed(struct fp_dev *dev, int result,
	struct fp_print_data *data, struct fp_img *img);
void fpi_drvcb_enroll_stopped(struct fp_dev *dev);

void fpi_drvcb_verify_started(struct fp_dev *dev, int status);
void fpi_drvcb_report_verify_result(struct fp_dev *dev, int result,
	struct fp_img *img);
void fpi_drvcb_verify_stopped(struct fp_dev *dev);

void fpi_drvcb_identify_started(struct fp_dev *dev, int status);
void fpi_drvcb_report_identify_result(struct fp_dev *dev, int result,
	size_t match_offset, struct fp_img *img);
void fpi_drvcb_identify_stopped(struct fp_dev *dev);

void fpi_drvcb_capture_started(struct fp_dev *dev, int status);
void fpi_drvcb_report_capture_result(struct fp_dev *dev, int result,
	struct fp_img *img);
void fpi_drvcb_capture_stopped(struct fp_dev *dev);

/* for image drivers */
void fpi_imgdev_open_complete(struct fp_img_dev *imgdev, int status);
void fpi_imgdev_close_complete(struct fp_img_dev *imgdev);
void fpi_imgdev_activate_complete(struct fp_img_dev *imgdev, int status);
void fpi_imgdev_deactivate_complete(struct fp_img_dev *imgdev);
void fpi_imgdev_report_finger_status(struct fp_img_dev *imgdev,
	gboolean present);
void fpi_imgdev_image_captured(struct fp_img_dev *imgdev, struct fp_img *img);
void fpi_imgdev_abort_scan(struct fp_img_dev *imgdev, int result);
void fpi_imgdev_session_error(struct fp_img_dev *imgdev, int error);

/* utils */
int fpi_std_sq_dev(const unsigned char *buf, int size);
int fpi_mean_sq_diff_norm(unsigned char *buf1, unsigned char *buf2, int size);

#endif

