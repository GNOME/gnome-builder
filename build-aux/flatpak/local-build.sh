#!/usr/bin/env bash

rm -f org.gnome.Builder.flatpak
rm -rf _build ; mkdir _build
rm -rf _repo ; mkdir _repo

STATE_DIR=~/.cache/gnome-builder/flatpak-builder
BRANCH=main

flatpak-builder \
    --ccache --force-clean \
    --repo=_repo --state-dir=$STATE_DIR \
    --default-branch=$BRANCH \
    _build org.gnome.Builder.json

flatpak build-bundle \
    _repo org.gnome.Builder.flatpak org.gnome.Builder $BRANCH

