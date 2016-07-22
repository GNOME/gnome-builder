#!/bin/sh
# "./check-palette.sh files..." will validate files given on command line.
# "./check-palette.sh" without arguments will validate all palette files
# in the ./data/palettes directory

files=""

if [ $1 ]; then
  files=$@
else
  cd ./data/palettes
  files=*.gstyle.xml
fi

for file in $files; do
  xmllint --relaxng palette.rng --noout $file || exit 1
done
