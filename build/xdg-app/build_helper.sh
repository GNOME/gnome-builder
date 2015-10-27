#!/bin/bash

set -o nounset
set -o errexit

if [ "$#" -nt 3 ]; then
    echo "Wrong args"
fi

OPT="$1"
CONFIGURE_ARGS="$2"

contains() {
  for word in $1; do
    [[ "$word" = "$2" ]] && return 0
  done
  return 1
}

if contains "$OPT" stdcpp11; then
    export CXXFLAGS="-O2 -std=c++11"
fi

if ! contains "$OPT" noautogen ; then
    ./autogen.sh $CONFIGURE_ARGS
fi
if contains "$OPT" force; then
    libtoolize --force
    aclocal
    autoheader
    automake --force-missing --add-missing --foreign
    autoconf
fi
if contains "$OPT" configure ||  contains "$OPT" noautogen; then
    ./configure $CONFIGURE_ARGS
fi
make -j`nproc`
make install
