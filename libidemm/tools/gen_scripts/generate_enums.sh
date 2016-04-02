#!/bin/bash

# Note that JHBUILD_SOURCES should be defined to contain the path to the root
# of the jhbuild sources.  Also this script assumes that it resides in the
# tools/gen_scripts directory and the defs file will be placed in
# libidemm/src.

if [ -z "$JHBUILD_SOURCES" ]; then
  echo -e "JHBUILD_SOURCES must contain the path to the jhbuild sources."
  exit 1;
fi

OUT_DIR="$(dirname "$0")/../../src"

ENUM_PL="$JHBUILD_SOURCES/glibmm/tools/enum.pl"
$ENUM_PL "$JHBUILD_SOURCES"/gnome-builder/libide/*.h "$OUT_DIR/libide_enums.defs"
