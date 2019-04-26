#!/usr/bin/env bash

rm -f org.gnome.Builder.flatpak
rm -rf _build ; mkdir _build
rm -rf _repo ; mkdir _repo

STATE_DIR=~/.cache/gnome-builder/flatpak-builder
BRANCH=master

flatpak-builder --ccache --state-dir=$STATE_DIR --force-clean _build org.gnome.Builder.json --repo=_repo --default-branch=$BRANCH
flatpak build-bundle _repo org.gnome.Builder.flatpak org.gnome.Builder $BRANCH

