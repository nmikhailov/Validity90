/*
 * Polling/timing management
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

#define FP_COMPONENT "poll"

#include <config.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>

#include <glib.h>
#include <libusb.h>

#include "fp_internal.h"

/**
 * @defgroup poll Polling and timing operations
 * These functions are only applicable to users of libfprint's asynchronous
 * API.
 *
 * libfprint does not create internal library threads and hence can only
 * execute when your application is calling a libfprint function. However,
 * libfprint often has work to be do, such as handling of completed USB
 * transfers, and processing of timeouts required in order for the library
 * to function. Therefore it is essential that your own application must
 * regularly "phone into" libfprint so that libfprint can handle any pending
 * events.
 *
 * The function you must call is fp_handle_events() or a variant of it. This
 * function will handle any pending events, and it is from this context that
 * all asynchronous event callbacks from the library will occur. You can view
 * this function as a kind of iteration function.
 *
 * If there are no events pending, fp_handle_events() will block for a few
 * seconds (and will handle any new events should anything occur in that time).
 * If you wish to customise this timeout, you can use
 * fp_handle_events_timeout() instead. If you wish to do a nonblocking
 * iteration, call fp_handle_events_timeout() with a zero timeout.
 *
 * TODO: document how application is supposed to know when to call these
 * functions.
 */

/* this is a singly-linked list of pending timers, sorted with the timer that
 * is expiring soonest at the head. */
static GSList *active_timers = NULL;

/* notifiers for added or removed poll fds */
static fp_pollfd_added_cb fd_added_cb = NULL;
static fp_pollfd_removed_cb fd_removed_cb = NULL;

struct fpi_timeout {
	struct timeval expiry;
	fpi_timeout_fn callback;
	void *data;
};

static int timeout_sort_fn(gconstpointer _a, gconstpointer _b)
{
	struct fpi_timeout *a = (struct fpi_timeout *) _a;
	struct fpi_timeout *b = (struct fpi_timeout *) _b;
	struct timeval *tv_a = &a->expiry;
	struct timeval *tv_b = &b->expiry;

	if (timercmp(tv_a, tv_b, <))
		return -1;
	else if (timercmp(tv_a, tv_b, >))
		return 1;
	else
		return 0;
}

/* A timeout is the asynchronous equivalent of sleeping. You create a timeout
 * saying that you'd like to have a function invoked at a certain time in
 * the future. */
struct fpi_timeout *fpi_timeout_add(unsigned int msec, fpi_timeout_fn callback,
	void *data)
{
	struct timespec ts;
	struct timeval add_msec;
	struct fpi_timeout *timeout;
	int r;

	fp_dbg("in %dms", msec);

	r = clock_gettime(CLOCK_MONOTONIC, &ts);
	if (r < 0) {
		fp_err("failed to read monotonic clock, errno=%d", errno);
		return NULL;
	}

	timeout = g_malloc(sizeof(*timeout));
	timeout->callback = callback;
	timeout->data = data;
	TIMESPEC_TO_TIMEVAL(&timeout->expiry, &ts);

	/* calculate timeout expiry by adding delay to current monotonic clock */
	timerclear(&add_msec);
	add_msec.tv_sec = msec / 1000;
	add_msec.tv_usec = (msec % 1000) * 1000;
	timeradd(&timeout->expiry, &add_msec, &timeout->expiry);

	active_timers = g_slist_insert_sorted(active_timers, timeout,
		timeout_sort_fn);

	return timeout;
}

void fpi_timeout_cancel(struct fpi_timeout *timeout)
{
	fp_dbg("");
	active_timers = g_slist_remove(active_timers, timeout);
	g_free(timeout);
}

/* get the expiry time and optionally the timeout structure for the next
 * timeout. returns 0 if there are no expired timers, or 1 if the
 * timeval/timeout output parameters were populated. if the returned timeval
 * is zero then it means the timeout has already expired and should be handled
 * ASAP. */
static int get_next_timeout_expiry(struct timeval *out,
	struct fpi_timeout **out_timeout)
{
	struct timespec ts;
	struct timeval tv;
	struct fpi_timeout *next_timeout;
	int r;

	if (active_timers == NULL)
		return 0;

	r = clock_gettime(CLOCK_MONOTONIC, &ts);
	if (r < 0) {
		fp_err("failed to read monotonic clock, errno=%d", errno);
		return r;
	}
	TIMESPEC_TO_TIMEVAL(&tv, &ts);

	next_timeout = active_timers->data;
	if (out_timeout)
		*out_timeout = next_timeout;

	if (timercmp(&tv, &next_timeout->expiry, >=)) {
		fp_dbg("first timeout already expired");
		timerclear(out);
	} else {
		timersub(&next_timeout->expiry, &tv, out);
		fp_dbg("next timeout in %d.%06ds", out->tv_sec, out->tv_usec);
	}

	return 1;
}

/* handle a timeout that has expired */
static void handle_timeout(struct fpi_timeout *timeout)
{
	fp_dbg("");
	timeout->callback(timeout->data);
	active_timers = g_slist_remove(active_timers, timeout);
	g_free(timeout);
}

static int handle_timeouts(void)
{
	struct timeval next_timeout_expiry;
	struct fpi_timeout *next_timeout;
	int r;

	r = get_next_timeout_expiry(&next_timeout_expiry, &next_timeout);
	if (r <= 0)
		return r;

	if (!timerisset(&next_timeout_expiry))
		handle_timeout(next_timeout);

	return 0;
}

/** \ingroup poll
 * Handle any pending events. If a non-zero timeout is specified, the function
 * will potentially block for the specified amount of time, although it may
 * return sooner if events have been handled. The function acts as non-blocking
 * for a zero timeout.
 *
 * \param timeout Maximum timeout for this blocking function
 * \returns 0 on success, non-zero on error.
 */
API_EXPORTED int fp_handle_events_timeout(struct timeval *timeout)
{
	struct timeval next_timeout_expiry;
	struct timeval select_timeout;
	struct fpi_timeout *next_timeout;
	int r;

	r = get_next_timeout_expiry(&next_timeout_expiry, &next_timeout);
	if (r < 0)
		return r;

	if (r) {
		/* timer already expired? */
		if (!timerisset(&next_timeout_expiry)) {
			handle_timeout(next_timeout);
			return 0;
		}

		/* choose the smallest of next URB timeout or user specified timeout */
		if (timercmp(&next_timeout_expiry, timeout, <))
			select_timeout = next_timeout_expiry;
		else
			select_timeout = *timeout;
	} else {
		select_timeout = *timeout;
	}

	r = libusb_handle_events_timeout(fpi_usb_ctx, &select_timeout);
	*timeout = select_timeout;
	if (r < 0)
		return r;

	return handle_timeouts();
}

/** \ingroup poll
 * Convenience function for calling fp_handle_events_timeout() with a sensible
 * default timeout value of two seconds (subject to change if we decide another
 * value is more sensible).
 *
 * \returns 0 on success, non-zero on error.
 */
API_EXPORTED int fp_handle_events(void)
{
	struct timeval tv;
	tv.tv_sec = 2;
	tv.tv_usec = 0;
	return fp_handle_events_timeout(&tv);
}

/* FIXME: docs
 * returns 0 if no timeouts active
 * returns 1 if timeout returned
 * zero timeout means events are to be handled immediately */
API_EXPORTED int fp_get_next_timeout(struct timeval *tv)
{
	struct timeval fprint_timeout;
	struct timeval libusb_timeout;
	int r_fprint;
	int r_libusb;

	r_fprint = get_next_timeout_expiry(&fprint_timeout, NULL);
	r_libusb = libusb_get_next_timeout(fpi_usb_ctx, &libusb_timeout);

	/* if we have no pending timeouts and the same is true for libusb,
	 * indicate that we have no pending timouts */
	if (r_fprint == 0 && r_libusb == 0)
		return 0;

	/* if fprint have no pending timeouts return libusb timeout */
	else if (r_fprint == 0)
		*tv = libusb_timeout;

	/* if libusb have no pending timeouts return fprint timeout */
	else if (r_libusb == 0)
		*tv = fprint_timeout;

	/* otherwise return the smaller of the 2 timeouts */
	else if (timercmp(&fprint_timeout, &libusb_timeout, <))
		*tv = fprint_timeout;
	else
		*tv = libusb_timeout;
	return 1;
}

/** \ingroup poll
 * Retrieve a list of file descriptors that should be polled for events
 * interesting to libfprint. This function is only for users who wish to
 * combine libfprint's file descriptor set with other event sources - more
 * simplistic users will be able to call fp_handle_events() or a variant
 * directly.
 *
 * \param pollfds output location for a list of pollfds. If non-NULL, must be
 * released with free() when done.
 * \returns the number of pollfds in the resultant list, or negative on error.
 */
API_EXPORTED size_t fp_get_pollfds(struct fp_pollfd **pollfds)
{
	const struct libusb_pollfd **usbfds;
	const struct libusb_pollfd *usbfd;
	struct fp_pollfd *ret;
	size_t cnt = 0;
	size_t i = 0;

	usbfds = libusb_get_pollfds(fpi_usb_ctx);
	if (!usbfds) {
		*pollfds = NULL;
		return -EIO;
	}

	while ((usbfd = usbfds[i++]) != NULL)
		cnt++;

	ret = g_malloc(sizeof(struct fp_pollfd) * cnt);
	i = 0;
	while ((usbfd = usbfds[i]) != NULL) {
		ret[i].fd = usbfd->fd;
		ret[i].events = usbfd->events;
		i++;
	}

	*pollfds = ret;
	return cnt;
}

/* FIXME: docs */
API_EXPORTED void fp_set_pollfd_notifiers(fp_pollfd_added_cb added_cb,
	fp_pollfd_removed_cb removed_cb)
{
	fd_added_cb = added_cb;
	fd_removed_cb = removed_cb;
}

static void add_pollfd(int fd, short events, void *user_data)
{
	if (fd_added_cb)
		fd_added_cb(fd, events);
}

static void remove_pollfd(int fd, void *user_data)
{
	if (fd_removed_cb)
		fd_removed_cb(fd);
}

void fpi_poll_init(void)
{
	libusb_set_pollfd_notifiers(fpi_usb_ctx, add_pollfd, remove_pollfd, NULL);
}

void fpi_poll_exit(void)
{
	g_slist_free(active_timers);
	active_timers = NULL;
	fd_added_cb = NULL;
	fd_removed_cb = NULL;
	libusb_set_pollfd_notifiers(fpi_usb_ctx, NULL, NULL, NULL);
}

