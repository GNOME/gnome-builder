#!/bin/bash

# Note that JHBUILD_SOURCES should be defined to contain the path to the root
# of the jhbuild sources.  Also this script assumes that it resides in the
# tools/gen_scripts directory and the XML file will be placed in
# libidemm/src.

if [ -z "$JHBUILD_SOURCES" ]; then
  echo -e "JHBUILD_SOURCES must contain path to jhbuild sources."
  exit 1;
fi

ROOT_DIR="$(dirname "$0")/.."
OUT_DIR="$ROOT_DIR/src"

shopt -s extglob # Enable extended pattern matching

PARAMS="-s $JHBUILD_SOURCES/gnome-builder/libide/"
DOCEXTRACT_TO_XML_PY="$JHBUILD_SOURCES/glibmm/tools/defs_gen/docextract_to_xml.py"
$DOCEXTRACT_TO_XML_PY --no-since $PARAMS > "$OUT_DIR/libide_docs.xml"


GEN_DIR="$ROOT_DIR/tools/extra_defs_gen"
"$GEN_DIR"/generate_defs_libide > "$OUT_DIR"/libide_signals.defs


ENUM_PL="$JHBUILD_SOURCES/glibmm/tools/enum.pl"
$ENUM_PL "$JHBUILD_SOURCES"/gnome-builder/libide/!(*private).h > "$OUT_DIR/libide_enums.defs"


H2DEF_PY="$JHBUILD_SOURCES/glibmm/tools/defs_gen/h2def.py"
$H2DEF_PY "$JHBUILD_SOURCES/gnome-builder"/libide/!(*private).h > "$OUT_DIR/libide_methods.defs"
