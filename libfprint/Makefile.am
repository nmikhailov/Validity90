ACLOCAL_AMFLAGS = -I m4
EXTRA_DIST = THANKS TODO HACKING libfprint.pc.in
DISTCLEANFILES = ChangeLog libfprint.pc

SUBDIRS = libfprint doc

if BUILD_EXAMPLES
SUBDIRS += examples
endif

DIST_SUBDIRS = libfprint doc examples

DISTCHECK_CONFIGURE_FLAGS = --with-drivers=all --enable-examples-build --enable-x11-examples-build --with-udev-rules-dir='$${libdir}/udev/rules.d-distcheck'

pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA=libfprint.pc

.PHONY: ChangeLog dist-up
ChangeLog:
	git --git-dir $(top_srcdir)/.git log > ChangeLog || touch ChangeLog

dist-hook: ChangeLog

dist-up: dist
	rsync $(distdir).tar.bz2 frs.sourceforge.net:uploads/
