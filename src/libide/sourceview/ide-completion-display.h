/* ide-completion-display.h
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

#include "ide-completion-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_COMPLETION_DISPLAY (ide_completion_display_get_type())

IDE_AVAILABLE_IN_3_32
G_DECLARE_INTERFACE (IdeCompletionDisplay, ide_completion_display, IDE, COMPLETION_DISPLAY, GtkWidget)

struct _IdeCompletionDisplayInterface
{
  GTypeInterface parent_iface;

  void     (*set_context)     (IdeCompletionDisplay       *self,
                               IdeCompletionContext       *context);
  gboolean (*key_press_event) (IdeCompletionDisplay       *self,
                               const GdkEventKey          *key);
  void     (*attach)          (IdeCompletionDisplay       *self,
                               GtkSourceView              *view);
  void     (*set_font_desc)   (IdeCompletionDisplay       *self,
                               const PangoFontDescription *font_desc);
  void     (*set_n_rows)      (IdeCompletionDisplay       *self,
                               guint                       n_rows);
  void     (*move_cursor)     (IdeCompletionDisplay       *self,
                               GtkMovementStep             step,
                               gint                        count);
};

IDE_AVAILABLE_IN_3_32
void     ide_completion_display_attach          (IdeCompletionDisplay *self,
                                                 GtkSourceView        *view);
IDE_AVAILABLE_IN_3_32
void     ide_completion_display_set_context     (IdeCompletionDisplay *self,
                                                 IdeCompletionContext *context);
IDE_AVAILABLE_IN_3_32
gboolean ide_completion_display_key_press_event (IdeCompletionDisplay *self,
                                                 const GdkEventKey    *key);
IDE_AVAILABLE_IN_3_32
void     ide_completion_display_set_n_rows      (IdeCompletionDisplay *self,
                                                 guint                 n_rows);
IDE_AVAILABLE_IN_3_32
void     ide_completion_display_move_cursor     (IdeCompletionDisplay *self,
                                                 GtkMovementStep       step,
                                                 gint                  count);

G_END_DECLS
