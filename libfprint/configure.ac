AC_INIT([libfprint], [0.7.0])
AM_INIT_AUTOMAKE([1.11 no-dist-gzip dist-xz check-news subdir-objects])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([libfprint/core.c])
AC_CONFIG_HEADERS([config.h])

# Enable silent build when available (Automake 1.11)
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AC_PREREQ([2.50])
AC_PROG_CC
AC_PROG_LIBTOOL
AC_C_INLINE
AM_PROG_CC_C_O
AC_PROG_CXX
AC_DEFINE([_GNU_SOURCE], [], [Use GNU extensions])

# Library versioning
lt_major="0"
lt_revision="0"
lt_age="0"
AC_SUBST(lt_major)
AC_SUBST(lt_revision)
AC_SUBST(lt_age)

all_drivers="upekts upektc upeksonly vcom5s uru4000 fdu2000 aes1610 aes1660 aes2501 aes2550 aes2660 aes3500 aes4000 vfs101 vfs301 vfs5011 upektc_img etes603 vfs0050 elan"

require_imaging='no'
require_aeslib='no'
require_aesX660='no'
require_aes3k='no'
enable_upeke2='no'
enable_upekts='no'
enable_upektc='no'
enable_upeksonly='no'
enable_vcom5s='no'
enable_uru4000='no'
enable_fdu2000='no'
enable_aes1610='no'
enable_aes1660='no'
enable_aes2501='no'
enable_aes2550='no'
enable_aes2660='no'
enable_aes3500='no'
enable_aes4000='no'
enable_vfs101='no'
enable_vfs301='no'
enable_vfs5011='no'
enable_upektc_img='no'
enable_etes603='no'
enable_vfs0050='no'
enable_elan='no'

AC_ARG_WITH([drivers],[AS_HELP_STRING([--with-drivers],
	[List of drivers to enable])],
	[drivers="$withval"],
	[drivers="$all_drivers"])

if test "x$drivers" = "xall" ; then
	drivers=$all_drivers
fi

for driver in `echo ${drivers} | sed -e 's/,/ /g' -e 's/,$//g'`; do
	case ${driver} in
		upekts)
			AC_DEFINE([ENABLE_UPEKTS], [], [Build UPEK TouchStrip driver])
			enable_upekts="yes"
		;;
		upeke2)
			AC_DEFINE([ENABLE_UPEKE2], [], [Build UPEK Eikon 2])
			enable_upeke2="yes"
		;;
		upektc)
			AC_DEFINE([ENABLE_UPEKTC], [], [Build UPEK TouchChip driver])
			enable_upektc="yes"
		;;
		upeksonly)
			AC_DEFINE([ENABLE_UPEKSONLY], [], [Build UPEK TouchStrip sensor-only driver])
			enable_upeksonly="yes"
		;;
		uru4000)
			AC_DEFINE([ENABLE_URU4000], [], [Build Digital Persona U.are.U 4000 driver])
			enable_uru4000="yes"
		;;
		fdu2000)
			AC_DEFINE([ENABLE_FDU2000], [], [Build Secugen FDU 2000 driver])
			enable_fdu2000="no"
			# Driver not ported
		;;
		vcom5s)
			AC_DEFINE([ENABLE_VCOM5S], [], [Build Veridicom 5thSense driver])
			enable_vcom5s="yes"
		;;
		aes2501)
			AC_DEFINE([ENABLE_AES2501], [], [Build AuthenTec AES2501 driver])
			require_aeslib="yes"
			enable_aes2501="yes"
		;;
		aes2550)
			AC_DEFINE([ENABLE_AES2550], [], [Build AuthenTec AES2550/AES2810 driver])
			require_aeslib="yes"
			enable_aes2550="yes"
		;;
		aes1610)
			AC_DEFINE([ENABLE_AES1610], [], [Build AuthenTec AES1610 driver])
			require_aeslib="yes"
			enable_aes1610="yes"
		;;
		aes1660)
			AC_DEFINE([ENABLE_AES1660], [], [Build AuthenTec AES1660 driver])
			require_aeslib="yes"
			require_aesX660="yes"
			enable_aes1660="yes"
		;;
		aes2660)
			AC_DEFINE([ENABLE_AES2660], [], [Build AuthenTec AES1660 driver])
			require_aeslib="yes"
			require_aesX660="yes"
			enable_aes2660="yes"
		;;
		aes3500)
			AC_DEFINE([ENABLE_AES3500], [], [Build AuthenTec AES3500 driver])
			require_aeslib="yes"
			require_imaging="yes"
			require_aes3k="yes"
			enable_aes3500="yes"
		;;
		aes4000)
			AC_DEFINE([ENABLE_AES4000], [], [Build AuthenTec AES4000 driver])
			require_aeslib="yes"
			require_imaging="yes"
			require_aes3k="yes"
			enable_aes4000="yes"
		;;
		vfs101)
			AC_DEFINE([ENABLE_VFS101], [], [Build Validity VFS101 driver])
			enable_vfs101="yes"
		;;
		vfs301)
			AC_DEFINE([ENABLE_VFS301], [], [Build Validity VFS301/VFS300 driver])
			enable_vfs301="yes"
		;;
		vfs5011)
			AC_DEFINE([ENABLE_VFS5011], [], [Build Validity VFS5011 driver])
			enable_vfs5011="yes"
		;;
		upektc_img)
			AC_DEFINE([ENABLE_UPEKTC_IMG], [], [Build Upek TouchChip Fingerprint Coprocessor driver])
			enable_upektc_img="yes"
		;;
		etes603)
			AC_DEFINE([ENABLE_ETES603], [], [Build EgisTec ES603 driver])
			enable_etes603="yes"
		;;
		vfs0050)
			AC_DEFINE([ENABLE_VFS0050], [], [Build Validity VFS0050 driver])
			enable_vfs0050="yes"
		;;
		elan)
			AC_DEFINE([ENABLE_ELAN], [], [Build Elan driver])
			enable_elan="yes"
		;;
	esac
done

AM_CONDITIONAL([ENABLE_UPEKTS], [test "$enable_upekts" = "yes"])
AM_CONDITIONAL([ENABLE_UPEKE2], [test "$enable_upeke2" = "yes"])
AM_CONDITIONAL([ENABLE_UPEKTC], [test "$enable_upektc" = "yes"])
AM_CONDITIONAL([ENABLE_UPEKSONLY], [test "$enable_upeksonly" = "yes"])
AM_CONDITIONAL([ENABLE_VCOM5S], [test "$enable_vcom5s" = "yes"])
AM_CONDITIONAL([ENABLE_URU4000], [test "$enable_uru4000" = "yes"])
AM_CONDITIONAL([ENABLE_FDU2000], [test "$enable_fdu2000" = "yes"])
AM_CONDITIONAL([ENABLE_AES1610], [test "$enable_aes1610" = "yes"])
AM_CONDITIONAL([ENABLE_AES1660], [test "$enable_aes1660" = "yes"])
AM_CONDITIONAL([ENABLE_AES2501], [test "$enable_aes2501" = "yes"])
AM_CONDITIONAL([ENABLE_AES2550], [test "$enable_aes2550" = "yes"])
AM_CONDITIONAL([ENABLE_AES2660], [test "$enable_aes2660" = "yes"])
AM_CONDITIONAL([ENABLE_AES3500], [test "$enable_aes3500" = "yes"])
AM_CONDITIONAL([ENABLE_AES4000], [test "$enable_aes4000" = "yes"])
AM_CONDITIONAL([REQUIRE_AESLIB], [test "$require_aeslib" = "yes"])
AM_CONDITIONAL([REQUIRE_AESX660], [test "$require_aesX660" = "yes"])
AM_CONDITIONAL([REQUIRE_AES3K], [test "$require_aes3k" = "yes"])
AM_CONDITIONAL([ENABLE_VFS101], [test "$enable_vfs101" = "yes"])
AM_CONDITIONAL([ENABLE_VFS301], [test "$enable_vfs301" = "yes"])
AM_CONDITIONAL([ENABLE_VFS5011], [test "$enable_vfs5011" = "yes"])
AM_CONDITIONAL([ENABLE_UPEKTC_IMG], [test "$enable_upektc_img" = "yes"])
AM_CONDITIONAL([ENABLE_ETES603], [test "$enable_etes603" = "yes"])
AM_CONDITIONAL([ENABLE_VFS0050], [test "$enable_vfs0050" = "yes"])
AM_CONDITIONAL([ENABLE_ELAN], [test "$enable_elan" = "yes"])


PKG_CHECK_MODULES(LIBUSB, [libusb-1.0 >= 0.9.1])
AC_SUBST(LIBUSB_CFLAGS)
AC_SUBST(LIBUSB_LIBS)

# check for OpenSSL's libcrypto
PKG_CHECK_MODULES(CRYPTO, nss)
AC_SUBST(CRYPTO_CFLAGS)
AC_SUBST(CRYPTO_LIBS)

PKG_CHECK_MODULES(GLIB, [glib-2.0 >= 2.28])
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

pixman_found=no

AC_ARG_ENABLE(udev-rules,
	AC_HELP_STRING([--enable-udev-rules],[Update the udev rules]),
	[case "${enableval}" in
	yes) ENABLE_UDEV_RULES=yes ;;
	no) ENABLE_UDEV_RULES=no ;;
	*) AC_MSG_ERROR(bad value ${enableval} for --enable-udev-rules) ;;
	esac],
	[ENABLE_UDEV_RULES=yes]) dnl Default value
AM_CONDITIONAL(ENABLE_UDEV_RULES, test x$ENABLE_UDEV_RULES = "xyes")

if test $ENABLE_UDEV_RULES = no && test -d .git/ ; then
	AC_MSG_ERROR(--disable-udev-rules can only be used when building from tarballs)
fi

AC_ARG_WITH(udev-rules-dir,
	    AS_HELP_STRING([--with-udev-rules-dir=DIR],[Installation path for udev rules @<:@auto@:>@]),
	    [ac_with_udev_rules_dir=$withval],
	    [ac_with_udev_rules_dir=""])

if test "${ac_with_udev_rules_dir}" = ""; then
	ac_with_udev_rules_dir=`$PKG_CONFIG --variable=udevdir udev`/rules.d
fi
AC_MSG_NOTICE([installing udev rules in ${ac_with_udev_rules_dir}])
AC_SUBST([udev_rulesdir],[${ac_with_udev_rules_dir}])

if test "$require_imaging" = "yes"; then
	PKG_CHECK_MODULES(IMAGING, pixman-1, [pixman_found=yes], [pixman_found=no])
	if test "$pixman_found" != "yes"; then
		AC_MSG_ERROR([pixman is required for imaging support])
	fi
fi

AM_CONDITIONAL([REQUIRE_PIXMAN], [test "$pixman_found" = "yes"])
AC_SUBST(IMAGING_CFLAGS)
AC_SUBST(IMAGING_LIBS)

# Examples build
AC_ARG_ENABLE([examples-build], [AS_HELP_STRING([--enable-examples-build],
	[build example applications (default n)])],
	[build_examples=$enableval],
	[build_examples='no'])
AM_CONDITIONAL([BUILD_EXAMPLES], [test "x$build_examples" != "xno"])

# Examples build
AC_ARG_ENABLE([x11-examples-build], [AS_HELP_STRING([--enable-x11-examples-build],
	[build X11 example applications (default n)])],
	[build_x11_examples=$enableval],
	[build_x11_examples='no'])
AM_CONDITIONAL([BUILD_X11_EXAMPLES], [test "x$build_x11_examples" != "xno"])


if test "x$build_x11_examples" != "xno"; then
	# check for Xv extensions
	# imported from Coriander
	AC_DEFUN([AC_CHECK_XV],[
		AC_SUBST(XV_CFLAGS)
		AC_SUBST(XV_LIBS)
		AC_MSG_CHECKING(for Xv extensions)
		AC_TRY_COMPILE([
		#include <X11/Xlib.h>
		#include <X11/extensions/Xvlib.h>],[
		int main(void) { (void) XvGetPortAttribute(0, 0, 0, 0); return 0; }
		],xv=yes,xv=no);
		AC_MSG_RESULT($xv)
		if test x$xv = xyes; then
	       XV_LIBS="-lXv -lXext"
	       XV_CFLAGS=""
		   AC_DEFINE(HAVE_XV,1,[defined if XV video overlay is available])
		else
			AC_MSG_ERROR([XV is required for X11 examples])
		fi
	])
	AC_CHECK_XV
fi

# Message logging
AC_ARG_ENABLE([log], [AS_HELP_STRING([--disable-log], [disable all logging])],
	[log_enabled=$enableval],
	[log_enabled='yes'])
if test "x$log_enabled" != "xno"; then
	AC_DEFINE([ENABLE_LOGGING], 1, [Message logging])
fi

AC_ARG_ENABLE([debug-log], [AS_HELP_STRING([--enable-debug-log],
	[enable debug logging (default n)])],
	[debug_log_enabled=$enableval],
	[debug_log_enabled='no'])
if test "x$debug_log_enabled" != "xno"; then
	AC_DEFINE([ENABLE_DEBUG_LOGGING], 1, [Debug message logging])
fi

# Restore gnu89 inline semantics on gcc 4.3 and newer
saved_cflags="$CFLAGS"
CFLAGS="$CFLAGS -fgnu89-inline"
AC_COMPILE_IFELSE([AC_LANG_SOURCE([[]])], inline_cflags="-fgnu89-inline", inline_cflags="")
CFLAGS="$saved_cflags"

AC_DEFINE([API_EXPORTED], [__attribute__((visibility("default")))], [Default visibility])
AM_CFLAGS="-std=gnu99 $inline_cflags -Wall -Wundef -Wunused -Wstrict-prototypes -Werror-implicit-function-declaration -Wno-pointer-sign -Wshadow"
AC_SUBST(AM_CFLAGS)

if test "$require_imaging" = "yes"; then
	if test x$pixman_found != no; then
		AC_MSG_NOTICE([** Using pixman for imaging])
	fi
else
	AC_MSG_NOTICE([   Imaging support disabled])
fi

if test x$enable_upekts != xno ; then
	AC_MSG_NOTICE([** upekts driver enabled])
else
	AC_MSG_NOTICE([   upekts driver disabled])
fi
if test x$enable_upeke2 != xno ; then
	AC_MSG_NOTICE([** upeke2 driver enabled])
else
	AC_MSG_NOTICE([   upeke2 driver disabled (handled by upektc_img driver)])
fi
if test x$enable_upektc != xno ; then
	AC_MSG_NOTICE([** upektc driver enabled])
else
	AC_MSG_NOTICE([   upektc driver disabled])
fi
if test x$enable_upeksonly != xno ; then
	AC_MSG_NOTICE([** upeksonly driver enabled])
else
	AC_MSG_NOTICE([   upeksonly driver disabled])
fi
if test x$enable_vcom5s != xno ; then
	AC_MSG_NOTICE([** vcom5s driver enabled])
else
	AC_MSG_NOTICE([   vcom5s driver disabled])
fi
if test x$enable_uru4000 != xno ; then
	AC_MSG_NOTICE([** uru4000 driver enabled])
else
	AC_MSG_NOTICE([   uru4000 driver disabled])
fi
if test x$enable_fdu2000 != xno ; then
	AC_MSG_NOTICE([** fdu2000 driver enabled])
else
	AC_MSG_NOTICE([   fdu2000 driver disabled (not ported)])
fi
if test x$enable_aes1610 != xno ; then
	AC_MSG_NOTICE([** aes1610 driver enabled])
else
	AC_MSG_NOTICE([   aes1610 driver disabled])
fi
if test x$enable_aes1660 != xno ; then
	AC_MSG_NOTICE([** aes1660 driver enabled])
else
	AC_MSG_NOTICE([   aes1660 driver disabled])
fi
if test x$enable_aes2501 != xno ; then
	AC_MSG_NOTICE([** aes2501 driver enabled])
else
	AC_MSG_NOTICE([   aes2501 driver disabled])
fi
if test x$enable_aes2550 != xno ; then
	AC_MSG_NOTICE([** aes2550/aes2810 driver enabled])
else
	AC_MSG_NOTICE([   aes2550/aes2810 driver disabled])
fi
if test x$enable_aes2660 != xno ; then
	AC_MSG_NOTICE([** aes2660 driver enabled])
else
	AC_MSG_NOTICE([   aes2660 driver disabled])
fi
if test x$enable_aes3500 != xno ; then
	AC_MSG_NOTICE([** aes3500 driver enabled])
else
	AC_MSG_NOTICE([   aes3500 driver disabled])
fi
if test x$enable_aes4000 != xno ; then
	AC_MSG_NOTICE([** aes4000 driver enabled])
else
	AC_MSG_NOTICE([   aes4000 driver disabled])
fi
if test x$enable_vfs101 != xno ; then
	AC_MSG_NOTICE([** vfs101 driver enabled])
else
	AC_MSG_NOTICE([   vfs101 driver disabled])
fi
if test x$enable_vfs301 != xno ; then
	AC_MSG_NOTICE([** vfs301 driver enabled])
else
	AC_MSG_NOTICE([   vfs301 driver disabled])
fi
if test x$enable_vfs5011 != xno ; then
	AC_MSG_NOTICE([** vfs5011 driver enabled])
else
	AC_MSG_NOTICE([   vfs5011 driver disabled])
fi
if test x$enable_upektc_img != xno ; then
	AC_MSG_NOTICE([** upektc_img driver enabled])
else
	AC_MSG_NOTICE([   upektc_img driver disabled])
fi
if test x$enable_etes603 != xno ; then
	AC_MSG_NOTICE([** etes603 driver enabled])
else
	AC_MSG_NOTICE([   etes603 driver disabled])
fi
if test x$enable_vfs0050 != xno ; then
	AC_MSG_NOTICE([** vfs0050 driver enabled])
else
	AC_MSG_NOTICE([   vfs0050 driver disabled])
fi
if test x$enable_elan != xno ; then
	AC_MSG_NOTICE([** elan driver enabled])
else
	AC_MSG_NOTICE([   elan driver disabled])
fi
if test x$require_aeslib != xno ; then
	AC_MSG_NOTICE([** aeslib helper functions enabled])
else
	AC_MSG_NOTICE([   aeslib helper functions disabled])
fi
if test x$require_aesX660 != xno ; then
	AC_MSG_NOTICE([** aesX660 common routines enabled])
else
	AC_MSG_NOTICE([   aesX660 common routines disabled])
fi
if test x$require_aes3k != xno ; then
	AC_MSG_NOTICE([** aes3k common routines enabled])
else
	AC_MSG_NOTICE([   aes3k common routines disabled])
fi

AC_CONFIG_FILES([libfprint.pc] [Makefile] [libfprint/Makefile] [examples/Makefile] [doc/Makefile])
AC_OUTPUT

