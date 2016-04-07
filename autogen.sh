#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

(test -f $srcdir/src/main.c) || {
    echo -n "**Error**: Directory "\`$srcdir\'" does not look like the"
    echo " top-level gnome-builder directory"
    exit 1
}


touch ChangeLog
touch INSTALL

aclocal --install -I build/autotools || exit 1
glib-gettextize --force --copy || exit 1
intltoolize --force --copy --automake || exit 1
gtkdocize || exit 1

if command -v mm-common-prepare 2>/dev/null; then
    mm-common-prepare --copy --force $srcdir/libidemm
fi

autoreconf --force --install -Wno-portability || exit 1

if [ "$NOCONFIGURE" = "" ]; then
        $srcdir/configure "$@" || exit 1

        if [ "$1" = "--help" ]; then exit 0 else
                echo "Now type \`make\' to compile" || exit 1
        fi
else
        echo "Skipping configure process."
fi

set +x
