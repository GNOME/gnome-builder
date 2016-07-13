#!/bin/sh

RE2C="re2c"
MIN_VERSION="0.13.7"
MIN_MAJOR_VERSION="0"
MIN_MINOR_VERSION="13"
MIN_MICRO_VERSION="7"

BASE_DIR=$(dirname "$0")

if [ ! hash $RE2C 2>/dev/null ]; then
  echo "Can't find re2c program. You need to install it in a version >= $version"
  exit 1
fi

CURRENT_VERSION=$(echo `re2c -v`| cut -f2 -d " ")
CURRENT_MAJOR_VERSION=$(echo "$CURRENT_VERSION" | cut -f1 -d.)
CURRENT_MINOR_VERSION=$(echo "$CURRENT_VERSION" | cut -f2 -d.)
CURRENT_MICRO_VERSION=$(echo "$CURRENT_VERSION" | cut -f3 -d.)

if [ $CURRENT_MICRO_VERSION="" ]; then
  CURRENT_MICRO_VERSION="0"
fi

echo "re2c current version found: $CURRENT_VERSION"

if [ $CURRENT_MAJOR_VERSION -lt $MIN_MAJOR_VERSION ]; then
  if [ $CURRENT_MINOR_VERSION -lt $MIN_MINOR_VERSION ]; then
    if [ $CURRENT_MICRO_VERSION -lt $MIN_MICRO_VERSION ]; then
      echo "You need a re2c version of at least $MIN_VERSION"
      exit 1
    fi
  fi
fi

`re2c ${BASE_DIR}/gstyle-colorlexer.re -b -s -8 -i -o ${BASE_DIR}/gstyle-colorlexer.c`

echo "gstyle-colorlexer.re compiled in gstyle-colorlexer.c"
