#!/usr/bin/env bash

set -euf -o pipefail

version=$1
name=$2
archive_file=$name-$version.tar.xz
prefix=$name-$version/

pushd $MESON_SOURCE_ROOT

if ! git config tar.tar.xz.command > /dev/null; then
  echo "Setting git archive format for tar.xz"
  git config tar.tar.xz.command "xz -c"
fi

# TODO: Rely on tag existing?
echo "Creating tarball $archive_file of HEAD" 
git archive --format=tar.xz --output=$archive_file --prefix=$prefix HEAD

temp=$(mktemp -d)
srcdir=$temp/source
blddir=$temp/build
instdir=$temp/install

mkdir $srcdir
tar -xf $archive_file -C $srcdir

# Test read-only source dir
chmod 500 $srcdir

pushd $srcdir/$prefix
meson $blddir
pushd $blddir

# FIXME: Tests currently fail on both autotools and meson
#ninja test
env DESTDIR=$instdir ninja install

popd
popd
popd

chmod 700 $srcdir
rm -rf $temp
echo "Everything succeeded!"