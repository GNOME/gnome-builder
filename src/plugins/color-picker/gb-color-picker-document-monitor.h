/* gb-color-picker-document-monitor.h
 *
 * Copyright 2016 sebastien lafargue <slafargue@gnome.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <glib-object.h>
#include <libide-editor.h>

#include "gstyle-color.h"

G_BEGIN_DECLS

#define GB_TYPE_COLOR_PICKER_DOCUMENT_MONITOR (gb_color_picker_document_monitor_get_type())

G_DECLARE_FINAL_TYPE (GbColorPickerDocumentMonitor, gb_color_picker_document_monitor, GB, COLOR_PICKER_DOCUMENT_MONITOR, GObject)

GbColorPickerDocumentMonitor *gb_color_picker_document_monitor_new                        (IdeBuffer                    *buffer);
IdeBuffer                    *gb_color_picker_document_monitor_get_buffer                 (GbColorPickerDocumentMonitor *self);
void                          gb_color_picker_document_monitor_set_buffer                 (GbColorPickerDocumentMonitor *self,
                                                                                           IdeBuffer                    *buffer);

void                          gb_color_picker_document_monitor_set_color_tag_at_cursor    (GbColorPickerDocumentMonitor *self,
                                                                                           GstyleColor                  *color);
void                          gb_color_picker_document_monitor_queue_colorize             (GbColorPickerDocumentMonitor *self,
                                                                                           const GtkTextIter            *begin,
                                                                                           const GtkTextIter            *end);
void                          gb_color_picker_document_monitor_queue_uncolorize           (GbColorPickerDocumentMonitor *self,
                                                                                           const GtkTextIter            *begin,
                                                                                           const GtkTextIter            *end);

G_END_DECLS
