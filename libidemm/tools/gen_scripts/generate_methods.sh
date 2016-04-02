#!/bin/bash

# Note that JHBUILD_SOURCES should be defined to contain the path to the root
# of the jhbuild sources.  Also this script assumes that it resides in the
# tools/gen_script directory and the defs file will be placed in
# libidemm/src.

if [ -z "$JHBUILD_SOURCES" -a ! -x "`which h2def.py`" ]; then
  echo -e "JHBUILD_SOURCES must contain the path to the jhbuild sources."
  exit 1;
fi

OUT_DIR="$(dirname "$0")/../../src"

H2DEF_PY="$JHBUILD_SOURCES/glibmm/tools/defs_gen/h2def.py"

$H2DEF_PY "$JHBUILD_SOURCES/gnome-builder"/libide/*.h > "$OUT_DIR/libide_methods.defs"

