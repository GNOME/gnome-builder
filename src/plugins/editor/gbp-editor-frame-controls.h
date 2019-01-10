/* gbp-editor-frame-controls.h
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#include <gtk/gtk.h>
#include <dazzle.h>

#include <gtk/gtk.h>
#include <dazzle.h>
#include <libide-editor.h>

G_BEGIN_DECLS

#define GBP_TYPE_EDITOR_FRAME_CONTROLS (gbp_editor_frame_controls_get_type())

G_DECLARE_FINAL_TYPE (GbpEditorFrameControls, gbp_editor_frame_controls, GBP, EDITOR_FRAME_CONTROLS, GtkBox)

struct _GbpEditorFrameControls
{
  GtkBox                parent_instance;

  IdeEditorPage        *page;
  DzlBindingGroup      *buffer_bindings;
  DzlSignalGroup       *buffer_signals;

  DzlSimplePopover     *goto_line_popover;
  GtkMenuButton        *goto_line_button;
  GtkButton            *warning_button;
  DzlSimpleLabel       *line_label;
  DzlSimpleLabel       *column_label;
  GtkLabel             *range_label;

  GSimpleAction        *goto_line_action;
};

void gbp_editor_frame_controls_set_page (GbpEditorFrameControls *self,
                                         IdeEditorPage          *page);

G_END_DECLS
