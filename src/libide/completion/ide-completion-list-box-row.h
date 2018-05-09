/* ide-completion-list-box-row.h
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

#include <gtk/gtk.h>

#include "ide-completion-proposal.h"
#include "ide-version-macros.h"

G_BEGIN_DECLS

#define IDE_TYPE_COMPLETION_LIST_BOX_ROW (ide_completion_list_box_row_get_type())

IDE_AVAILABLE_IN_3_30
G_DECLARE_FINAL_TYPE (IdeCompletionListBoxRow, ide_completion_list_box_row, IDE, COMPLETION_LIST_BOX_ROW, GtkListBoxRow)

IDE_AVAILABLE_IN_3_30
GtkWidget             *ide_completion_list_box_row_new               (void);
IDE_AVAILABLE_IN_3_30
IdeCompletionProposal *ide_completion_list_box_row_get_proposal      (IdeCompletionListBoxRow *self);
IDE_AVAILABLE_IN_3_30
void                   ide_completion_list_box_row_set_proposal      (IdeCompletionListBoxRow *self,
                                                                      IdeCompletionProposal   *proposal);
IDE_AVAILABLE_IN_3_30
void                   ide_completion_list_box_row_set_icon_name     (IdeCompletionListBoxRow *self,
                                                                      const gchar             *icon_name);
IDE_AVAILABLE_IN_3_30
void                   ide_completion_list_box_row_set_left          (IdeCompletionListBoxRow *self,
                                                                      const gchar             *left);
IDE_AVAILABLE_IN_3_30
void                   ide_completion_list_box_row_set_right         (IdeCompletionListBoxRow *self,
                                                                      const gchar             *right);
IDE_AVAILABLE_IN_3_30
void                   ide_completion_list_box_row_set_center        (IdeCompletionListBoxRow *self,
                                                                      const gchar             *center);
IDE_AVAILABLE_IN_3_30
void                   ide_completion_list_box_row_set_center_markup (IdeCompletionListBoxRow *self,
                                                                      const gchar             *center_markup);

G_END_DECLS
