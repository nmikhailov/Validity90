lib_LTLIBRARIES = libfprint.la
noinst_PROGRAMS = fprint-list-udev-rules
MOSTLYCLEANFILES = $(udev_rules_DATA)

UPEKE2_SRC = drivers/upeke2.c
UPEKTS_SRC = drivers/upekts.c
UPEKTC_SRC = drivers/upektc.c drivers/upektc.h
UPEKSONLY_SRC = drivers/upeksonly.c drivers/upeksonly.h
URU4000_SRC = drivers/uru4000.c
AES1610_SRC = drivers/aes1610.c
AES1660_SRC = drivers/aes1660.c drivers/aes1660.h
AES2501_SRC = drivers/aes2501.c drivers/aes2501.h
AES2550_SRC = drivers/aes2550.c drivers/aes2550.h
AES2660_SRC = drivers/aes2660.c drivers/aes2660.h
AES3500_SRC = drivers/aes3500.c
AES4000_SRC = drivers/aes4000.c
FDU2000_SRC = drivers/fdu2000.c
VCOM5S_SRC = drivers/vcom5s.c
VFS101_SRC = drivers/vfs101.c
VFS301_SRC = drivers/vfs301.c drivers/vfs301_proto.c  drivers/vfs301_proto.h drivers/vfs301_proto_fragments.h
VFS5011_SRC = drivers/vfs5011.c drivers/vfs5011_proto.h
UPEKTC_IMG_SRC = drivers/upektc_img.c drivers/upektc_img.h
ETES603_SRC = drivers/etes603.c
VFS0050_SRC = drivers/vfs0050.c drivers/vfs0050.h
ELAN_SRC = drivers/elan.c drivers/elan.h

EXTRA_DIST = \
	$(UPEKE2_SRC)		\
	$(UPEKTS_SRC)		\
	$(UPEKTC_SRC)		\
	$(UPEKSONLY_SRC)	\
	$(URU4000_SRC)		\
	$(AES1610_SRC)		\
	$(AES1660_SRC)		\
	$(AES2501_SRC)		\
	$(AES2550_SRC)		\
	$(AES2660_SRC)		\
	$(AES3500_SRC)		\
	$(AES4000_SRC)		\
	$(FDU2000_SRC)		\
	$(VCOM5S_SRC)		\
	$(VFS101_SRC)		\
	$(VFS301_SRC)		\
	$(VFS5011_SRC)		\
	$(UPEKTC_IMG_SRC)	\
	$(ETES603_SRC)		\
	$(VFS0050_SRC)		\
	$(ELAN_SRC)		\
	drivers/aesx660.c	\
	drivers/aesx660.h	\
	drivers/aes3k.c 	\
	drivers/aes3k.h 	\
	drivers/driver_ids.h	\
	aeslib.c aeslib.h	\
	assembling.c		\
	assembling.h		\
	pixman.c		\
	60-fprint-autosuspend.rules

DRIVER_SRC =
OTHER_SRC =

NBIS_SRC = \
	nbis/include/bozorth.h \
	nbis/include/bz_array.h \
	nbis/include/defs.h \
	nbis/include/lfs.h \
	nbis/include/log.h \
	nbis/include/morph.h \
	nbis/include/sunrast.h \
	nbis/bozorth3/bozorth3.c \
	nbis/bozorth3/bz_alloc.c \
	nbis/bozorth3/bz_drvrs.c \
	nbis/bozorth3/bz_gbls.c \
	nbis/bozorth3/bz_io.c \
	nbis/bozorth3/bz_sort.c \
	nbis/mindtct/binar.c \
	nbis/mindtct/block.c \
	nbis/mindtct/contour.c \
	nbis/mindtct/detect.c \
	nbis/mindtct/dft.c \
	nbis/mindtct/free.c \
	nbis/mindtct/globals.c \
	nbis/mindtct/imgutil.c \
	nbis/mindtct/init.c \
	nbis/mindtct/line.c \
	nbis/mindtct/log.c \
	nbis/mindtct/loop.c \
	nbis/mindtct/maps.c \
	nbis/mindtct/matchpat.c \
	nbis/mindtct/minutia.c \
	nbis/mindtct/morph.c \
	nbis/mindtct/quality.c \
	nbis/mindtct/remove.c \
	nbis/mindtct/ridges.c \
	nbis/mindtct/shape.c \
	nbis/mindtct/sort.c \
	nbis/mindtct/util.c

libfprint_la_CFLAGS = -fvisibility=hidden -I$(srcdir)/nbis/include $(LIBUSB_CFLAGS) $(GLIB_CFLAGS) $(CRYPTO_CFLAGS) $(AM_CFLAGS)
libfprint_la_LDFLAGS = -version-info @lt_major@:@lt_revision@:@lt_age@
libfprint_la_LIBADD = -lm $(LIBUSB_LIBS) $(GLIB_LIBS) $(CRYPTO_LIBS)

fprint_list_udev_rules_SOURCES = fprint-list-udev-rules.c
fprint_list_udev_rules_CFLAGS = -fvisibility=hidden -I$(srcdir)/nbis/include $(LIBUSB_CFLAGS) $(GLIB_CFLAGS) $(CRYPTO_CFLAGS) $(AM_CFLAGS)
fprint_list_udev_rules_LDADD = $(builddir)/libfprint.la $(GLIB_LIBS)

udev_rules_DATA = 60-fprint-autosuspend.rules

if ENABLE_UDEV_RULES
$(udev_rules_DATA): fprint-list-udev-rules
	$(builddir)/fprint-list-udev-rules > $@
endif

if ENABLE_UPEKE2
DRIVER_SRC += $(UPEKE2_SRC)
endif

if ENABLE_UPEKTS
DRIVER_SRC += $(UPEKTS_SRC)
endif

if ENABLE_UPEKSONLY
DRIVER_SRC += $(UPEKSONLY_SRC)
endif

if ENABLE_UPEKTC
DRIVER_SRC += $(UPEKTC_SRC)
endif

if ENABLE_URU4000
DRIVER_SRC += $(URU4000_SRC)
endif

if ENABLE_VCOM5S
DRIVER_SRC += $(VCOM5S_SRC)
endif

#if ENABLE_FDU2000
#DRIVER_SRC += $(FDU2000_SRC)
#endif

if ENABLE_AES1610
DRIVER_SRC += $(AES1610_SRC)
endif

if ENABLE_AES1660
DRIVER_SRC += $(AES1660_SRC)
endif

if ENABLE_AES2501
DRIVER_SRC += $(AES2501_SRC)
endif

if ENABLE_AES2550
DRIVER_SRC += $(AES2550_SRC)
endif

if ENABLE_AES2660
DRIVER_SRC += $(AES2660_SRC)
endif

if ENABLE_AES3500
DRIVER_SRC += $(AES3500_SRC)
endif

if ENABLE_AES4000
DRIVER_SRC += $(AES4000_SRC)
endif

if ENABLE_VFS101
DRIVER_SRC += $(VFS101_SRC)
endif

if ENABLE_VFS301
DRIVER_SRC += $(VFS301_SRC)
endif

if ENABLE_VFS5011
DRIVER_SRC += $(VFS5011_SRC)
endif

if ENABLE_UPEKTC_IMG
DRIVER_SRC += $(UPEKTC_IMG_SRC)
endif

if ENABLE_ETES603
DRIVER_SRC += $(ETES603_SRC)
endif

if ENABLE_VFS0050
DRIVER_SRC += $(VFS0050_SRC)
endif

if ENABLE_ELAN
DRIVER_SRC += $(ELAN_SRC)
endif

if REQUIRE_PIXMAN
OTHER_SRC += pixman.c
libfprint_la_CFLAGS += $(IMAGING_CFLAGS)
libfprint_la_LIBADD += $(IMAGING_LIBS)
endif

if REQUIRE_AESLIB
OTHER_SRC += aeslib.c aeslib.h
endif

if REQUIRE_AESX660
OTHER_SRC += drivers/aesx660.c drivers/aesx660.h
endif

if REQUIRE_AES3K
OTHER_SRC += drivers/aes3k.c drivers/aes3k.h
endif

libfprint_la_SOURCES =	\
	fp_internal.h	\
	async.c		\
	core.c		\
	data.c		\
	drv.c		\
	img.c		\
	imgdev.c	\
	poll.c		\
	sync.c		\
	assembling.c	\
	assembling.h	\
	$(DRIVER_SRC)	\
	$(OTHER_SRC)	\
	$(NBIS_SRC)

pkginclude_HEADERS = fprint.h
