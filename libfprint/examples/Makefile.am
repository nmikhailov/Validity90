AM_CFLAGS = -I$(top_srcdir)
noinst_PROGRAMS = verify_live enroll verify img_capture cpp-test

verify_live_SOURCES = verify_live.c
verify_live_LDADD = ../libfprint/libfprint.la

enroll_SOURCES = enroll.c
enroll_LDADD = ../libfprint/libfprint.la

verify_SOURCES = verify.c
verify_LDADD = ../libfprint/libfprint.la

img_capture_SOURCES = img_capture.c
img_capture_LDADD = ../libfprint/libfprint.la

cpp_test_SOURCES = cpp-test.cpp
cpp_test_LDADD = ../libfprint/libfprint.la

if BUILD_X11_EXAMPLES
noinst_PROGRAMS += img_capture_continuous

img_capture_continuous_CFLAGS = $(X_CFLAGS) $(XV_CFLAGS)
img_capture_continuous_SOURCES = img_capture_continuous.c
img_capture_continuous_LDADD = ../libfprint/libfprint.la $(X_LIBS) $(X_PRE_LIBS) $(XV_LIBS) -lX11 $(X_EXTRA_LIBS);
endif

