#!/bin/bash

# Note that JHBUILD_SOURCES should be defined to contain the path to the root
# of the jhbuild sources.  Also this script assumes that it resides in the
# tools/gen_scripts directory and the XML file will be placed in
# libidemm/src.

if [ -z "$JHBUILD_SOURCES" ]; then
  echo -e "JHBUILD_SOURCES must contain path to jhbuild sources."
  exit 1;
fi

OUT_DIR="$(dirname "$0")/../../src"

PARAMS="-s $JHBUILD_SOURCES/gnome-builder/libide/"

DOCEXTRACT_TO_XML_PY="$JHBUILD_SOURCES/glibmm/tools/defs_gen/docextract_to_xml.py"
$DOCEXTRACT_TO_XML_PY --no-since $PARAMS > "$OUT_DIR/libide_docs.xml"
