#!/bin/bash

# Note that script assumes it resides in the tools/gen_scripts directory and
# the defs file will be placed in libidemm/src.

ROOT_DIR="$(dirname "$0")/../.."
GEN_DIR="$ROOT_DIR/tools/extra_defs_gen"
OUT_DIR="$ROOT_DIR/src"

"$GEN_DIR"/generate_defs_libide > "$OUT_DIR"/libide_signals.defs

