#!/bin/sh

#
# Run this to generate all of the initial configure scripts and Makefiles.
#

# Switch to the source directory to initialize autoconf.
olddir=`pwd`
test -n "${srcdir}" || srcdir=`dirname "$0"`
test -n "${srcdir}" || srcdir=.
cd "${srcdir}"

# Load all of our autogen scripts for various features.
for script in build/autotools/autogen.d/*.sh; do
    . "${script}"
done

# Discover the location of autoconf.
AUTORECONF=`which autoreconf`
if test -z "${AUTORECONF}"; then
    echo "*** No autoreconf found, please install it ***"
    exit 1
fi

# Generate m4 sub-configure scripts for inclusion by configure.ac.
for group in ac am config lt output print; do
    for mode in pre post; do
        FILE="build/autotools/autoconf.d/${mode}-${group}.m4"
        cat build/autotools/autoconf.d/*.${mode}-${group} 2>/dev/null> ${FILE}
    done
done

# Run autoconf to build configure.
intltoolize --automake --copy
autoreconf --force --install --verbose -I build/autotools ${ACLOCAL_FLAGS} || exit $?

# Remove that pesky autom4te.cache
rm -rf autom4te.cache

# Return to the original directory.
cd "$olddir"

# Unless NOCONFIGURE is set, run configure too.
test -n "$NOCONFIGURE" || "$srcdir/configure" "$@"
