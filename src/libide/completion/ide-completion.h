/* ide-completion.h
 *
 * Copyright © 2018 Christian Hergert <chergert@redhat.com>
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

#include <gtk/gtk.h>

#include "ide-version-macros.h"

#include "completion/ide-completion-types.h"
#include "sourceview/ide-source-view.h"

G_BEGIN_DECLS

#define IDE_TYPE_COMPLETION (ide_completion_get_type())

typedef enum
{
  IDE_COMPLETION_INTERACTIVE,
  IDE_COMPLETION_USER_REQUESTED,
  IDE_COMPLETION_TRIGGERED,
} IdeCompletionActivation;

IDE_AVAILABLE_IN_3_30
G_DECLARE_FINAL_TYPE (IdeCompletion, ide_completion, IDE, COMPLETION, GObject)

IDE_AVAILABLE_IN_3_30
IdeCompletionDisplay *ide_completion_get_display         (IdeCompletion *self);
IDE_AVAILABLE_IN_3_30
GtkSourceView        *ide_completion_get_view            (IdeCompletion         *self);
IDE_AVAILABLE_IN_3_30
GtkTextBuffer        *ide_completion_get_buffer          (IdeCompletion         *self);
IDE_AVAILABLE_IN_3_30
void                  ide_completion_block_interactive   (IdeCompletion         *self);
IDE_AVAILABLE_IN_3_30
void                  ide_completion_unblock_interactive (IdeCompletion         *self);
IDE_AVAILABLE_IN_3_30
void                  ide_completion_add_provider        (IdeCompletion         *self,
                                                          IdeCompletionProvider *provider);
IDE_AVAILABLE_IN_3_30
void                  ide_completion_remove_provider     (IdeCompletion         *self,
                                                          IdeCompletionProvider *provider);
IDE_AVAILABLE_IN_3_30
guint                 ide_completion_get_n_rows          (IdeCompletion         *self);
IDE_AVAILABLE_IN_3_30
void                  ide_completion_set_n_rows          (IdeCompletion         *self,
                                                          guint                  n_rows);
IDE_AVAILABLE_IN_3_30
void                  ide_completion_hide                (IdeCompletion         *self);
IDE_AVAILABLE_IN_3_30
void                  ide_completion_show                (IdeCompletion         *self);
IDE_AVAILABLE_IN_3_30
void                  ide_completion_cancel              (IdeCompletion         *self);
IDE_AVAILABLE_IN_3_30
gboolean              ide_completion_is_visible          (IdeCompletion         *self);
IDE_AVAILABLE_IN_3_30
void                  ide_completion_move_cursor         (IdeCompletion         *self,
                                                          GtkMovementStep        step,
                                                          gint                   direction);

G_END_DECLS
