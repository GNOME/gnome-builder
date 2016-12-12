#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.
olddir=`pwd`
cd $srcdir
aclocal --install -I build-aux || exit 1
autoreconf --force --install -Wno-portability || exit 1
cd $olddir
