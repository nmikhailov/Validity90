#!/bin/sh

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

olddir="`pwd`"

cd "$srcdir"

libtoolize --copy --force || exit 1
aclocal || exit 1
autoheader || exit 1
autoconf || exit 1
automake -a -c || exit 1
cd "$olddir"

if test -z "$NOCONFIGURE"; then
    $srcdir/configure --enable-maintainer-mode --enable-examples-build \
	--enable-x11-examples-build --enable-debug-log $*
fi
