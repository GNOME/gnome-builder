/* ide-completion-list-box.h
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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

#include <dazzle.h>
#include <gtk/gtk.h>

#include "ide-completion-context.h"
#include "ide-completion-proposal.h"
#include "ide-version-macros.h"

G_BEGIN_DECLS

#define IDE_TYPE_COMPLETION_LIST_BOX (ide_completion_list_box_get_type())

IDE_AVAILABLE_IN_3_30
G_DECLARE_FINAL_TYPE (IdeCompletionListBox, ide_completion_list_box, IDE, COMPLETION_LIST_BOX, DzlBin)

IDE_AVAILABLE_IN_3_30
GtkWidget             *ide_completion_list_box_new          (void);
IDE_AVAILABLE_IN_3_30
IdeCompletionContext  *ide_completion_list_box_get_context  (IdeCompletionListBox   *self);
IDE_AVAILABLE_IN_3_30
void                   ide_completion_list_box_set_context  (IdeCompletionListBox   *self,
                                                             IdeCompletionContext   *context);
IDE_AVAILABLE_IN_3_30
guint                  ide_completion_list_box_get_n_rows   (IdeCompletionListBox   *self);
IDE_AVAILABLE_IN_3_30
void                   ide_completion_list_box_set_n_rows   (IdeCompletionListBox   *self,
                                                             guint                   n_rows);
IDE_AVAILABLE_IN_3_30
IdeCompletionProposal *ide_completion_list_box_get_proposal (IdeCompletionListBox   *self);
IDE_AVAILABLE_IN_3_30
gboolean               ide_completion_list_box_get_selected (IdeCompletionListBox   *self,
                                                             IdeCompletionProvider **provider,
                                                             IdeCompletionProposal **proposal);
IDE_AVAILABLE_IN_3_30
void                   ide_completion_list_box_move_cursor  (IdeCompletionListBox   *self,
                                                             GtkMovementStep         step,
                                                             gint                    direction);

G_END_DECLS
