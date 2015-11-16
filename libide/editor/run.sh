#!/usr/bin/env bash

base="../../src/editor"

refactor.py \
	"${base}/gb-editor-frame-actions.c=ide-editor-frame-actions.c" \
	"${base}/gb-editor-frame-actions.h=ide-editor-frame-actions.h" \
	"${base}/gb-editor-frame.c=ide-editor-frame.c" \
	"${base}/gb-editor-frame.h=ide-editor-frame.h" \
	"${base}/gb-editor-frame-private.h=ide-editor-frame-private.h" \
	"${base}/gb-editor-map-bin.c=ide-editor-map-bin.c" \
	"${base}/gb-editor-map-bin.h=ide-editor-map-bin.h" \
	"${base}/gb-editor-print-operation.c=ide-editor-print-operation.c" \
	"${base}/gb-editor-print-operation.h=ide-editor-print-operation.h" \
	"${base}/gb-editor-settings-widget.c=ide-editor-settings-widget.c" \
	"${base}/gb-editor-settings-widget.h=ide-editor-settings-widget.h" \
	"${base}/gb-editor-tweak-widget.c=ide-editor-tweak-widget.c" \
	"${base}/gb-editor-tweak-widget.h=ide-editor-tweak-widget.h" \
	"${base}/gb-editor-view-actions.c=ide-editor-view-actions.c" \
	"${base}/gb-editor-view-actions.h=ide-editor-view-actions.h" \
	"${base}/gb-editor-view-addin.c=ide-editor-view-addin.c" \
	"${base}/gb-editor-view-addin.h=ide-editor-view-addin.h" \
	"${base}/gb-editor-view-addin-private.h=ide-editor-view-addin-private.h" \
	"${base}/gb-editor-view.c=ide-editor-view.c" \
	"${base}/gb-editor-view.h=ide-editor-view.h" \
	"${base}/gb-editor-view-private.h=ide-editor-view-private.h"
