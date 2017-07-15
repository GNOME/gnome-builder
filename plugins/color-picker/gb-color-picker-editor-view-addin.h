/* gb-color-picker-editor-view-addin.h
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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
 */

#pragma once

#include <ide.h>

G_BEGIN_DECLS

#define GB_TYPE_COLOR_PICKER_EDITOR_VIEW_ADDIN (gb_color_picker_editor_view_addin_get_type())

G_DECLARE_FINAL_TYPE (GbColorPickerEditorViewAddin, gb_color_picker_editor_view_addin, GB, COLOR_PICKER_EDITOR_VIEW_ADDIN, GObject)

gboolean gb_color_picker_editor_view_addin_get_enabled (GbColorPickerEditorViewAddin *self);
void     gb_color_picker_editor_view_addin_set_enabled (GbColorPickerEditorViewAddin *self,
                                                        gboolean                      enabled);
void     gb_color_picker_editor_view_addin_set_color   (GbColorPickerEditorViewAddin *self,
                                                        GstyleColor                  *color);

G_END_DECLS
