/* ide-gutter.h
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#if !defined (IDE_SOURCEVIEW_INSIDE) && !defined (IDE_SOURCEVIEW_COMPILATION)
# error "Only <libide-sourceview.h> can be included directly."
#endif

#include <gtksourceview/gtksource.h>

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_GUTTER (ide_gutter_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_INTERFACE (IdeGutter, ide_gutter, IDE, GUTTER, GtkSourceGutterRenderer)

struct _IdeGutterInterface
{
  GTypeInterface parent_class;

  void (*style_changed) (IdeGutter *self);
};

IDE_AVAILABLE_IN_ALL
gboolean ide_gutter_get_show_line_changes          (IdeGutter *self);
IDE_AVAILABLE_IN_ALL
gboolean ide_gutter_get_show_line_numbers          (IdeGutter *self);
IDE_AVAILABLE_IN_ALL
gboolean ide_gutter_get_show_relative_line_numbers (IdeGutter *self);
IDE_AVAILABLE_IN_ALL
gboolean ide_gutter_get_show_line_diagnostics      (IdeGutter *self);
IDE_AVAILABLE_IN_47
gboolean ide_gutter_get_show_line_selection_styling(IdeGutter *self);
IDE_AVAILABLE_IN_ALL
void     ide_gutter_set_show_line_changes          (IdeGutter *self,
                                                    gboolean   show_line_changes);
IDE_AVAILABLE_IN_ALL
void     ide_gutter_set_show_line_numbers          (IdeGutter *self,
                                                    gboolean   show_line_numbers);
IDE_AVAILABLE_IN_ALL
void     ide_gutter_set_show_relative_line_numbers (IdeGutter *self,
                                                    gboolean   show_relative_line_numbers);
IDE_AVAILABLE_IN_ALL
void     ide_gutter_set_show_line_diagnostics      (IdeGutter *self,
                                                    gboolean   show_line_diagnostics);
IDE_AVAILABLE_IN_47
void     ide_gutter_set_show_line_selection_styling(IdeGutter *self,
                                                    gboolean   show_line_selection_styling);
IDE_AVAILABLE_IN_ALL
void     ide_gutter_style_changed                  (IdeGutter *self);

G_END_DECLS
