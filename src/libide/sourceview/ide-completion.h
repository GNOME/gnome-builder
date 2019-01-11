/* ide-completion.h
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

#include <libide-core.h>
#include <gtk/gtk.h>

#include "ide-completion-types.h"
#include "ide-source-view.h"

G_BEGIN_DECLS

#define IDE_TYPE_COMPLETION (ide_completion_get_type())

IDE_AVAILABLE_IN_3_32
G_DECLARE_FINAL_TYPE (IdeCompletion, ide_completion, IDE, COMPLETION, GObject)

IDE_AVAILABLE_IN_3_32
IdeCompletionDisplay *ide_completion_get_display         (IdeCompletion         *self);
IDE_AVAILABLE_IN_3_32
GtkSourceView        *ide_completion_get_view            (IdeCompletion         *self);
IDE_AVAILABLE_IN_3_32
GtkTextBuffer        *ide_completion_get_buffer          (IdeCompletion         *self);
IDE_AVAILABLE_IN_3_32
void                  ide_completion_block_interactive   (IdeCompletion         *self);
IDE_AVAILABLE_IN_3_32
void                  ide_completion_unblock_interactive (IdeCompletion         *self);
IDE_AVAILABLE_IN_3_32
void                  ide_completion_add_provider        (IdeCompletion         *self,
                                                          IdeCompletionProvider *provider);
IDE_AVAILABLE_IN_3_32
void                  ide_completion_remove_provider     (IdeCompletion         *self,
                                                          IdeCompletionProvider *provider);
IDE_AVAILABLE_IN_3_32
guint                 ide_completion_get_n_rows          (IdeCompletion         *self);
IDE_AVAILABLE_IN_3_32
void                  ide_completion_set_n_rows          (IdeCompletion         *self,
                                                          guint                  n_rows);
IDE_AVAILABLE_IN_3_32
void                  ide_completion_hide                (IdeCompletion         *self);
IDE_AVAILABLE_IN_3_32
void                  ide_completion_show                (IdeCompletion         *self);
IDE_AVAILABLE_IN_3_32
void                  ide_completion_cancel              (IdeCompletion         *self);
IDE_AVAILABLE_IN_3_32
gboolean              ide_completion_is_visible          (IdeCompletion         *self);
IDE_AVAILABLE_IN_3_32
void                  ide_completion_move_cursor         (IdeCompletion         *self,
                                                          GtkMovementStep        step,
                                                          gint                   direction);
IDE_AVAILABLE_IN_3_32
gboolean              ide_completion_fuzzy_match         (const gchar           *haystack,
                                                          const gchar           *casefold_needle,
                                                          guint                 *priority);
IDE_AVAILABLE_IN_3_32
gchar                *ide_completion_fuzzy_highlight     (const gchar           *haystack,
                                                          const gchar           *casefold_query);

G_END_DECLS
