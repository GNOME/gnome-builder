#!/bin/sh

flatpak_dir=$(dirname $0)
OUTPUT=$flatpak_dir/python-deps.json
SCRIPT=$flatpak_dir/flatpak-builder-tools/pip/flatpak-pip-generator

exec $SCRIPT --output=$OUTPUT jedi sphinx sphinx_rtd_theme lxml docutils

