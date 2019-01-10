/* ide-completion-list-box-row.h
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

#include <gtk/gtk.h>

#include "ide-completion-proposal.h"

G_BEGIN_DECLS

#define IDE_TYPE_COMPLETION_LIST_BOX_ROW (ide_completion_list_box_row_get_type())

IDE_AVAILABLE_IN_3_32
G_DECLARE_FINAL_TYPE (IdeCompletionListBoxRow, ide_completion_list_box_row, IDE, COMPLETION_LIST_BOX_ROW, GtkListBoxRow)

IDE_AVAILABLE_IN_3_32
GtkWidget             *ide_completion_list_box_row_new               (void);
IDE_AVAILABLE_IN_3_32
IdeCompletionProposal *ide_completion_list_box_row_get_proposal      (IdeCompletionListBoxRow *self);
IDE_AVAILABLE_IN_3_32
void                   ide_completion_list_box_row_set_proposal      (IdeCompletionListBoxRow *self,
                                                                      IdeCompletionProposal   *proposal);
IDE_AVAILABLE_IN_3_32
void                   ide_completion_list_box_row_set_icon_name     (IdeCompletionListBoxRow *self,
                                                                      const gchar             *icon_name);
IDE_AVAILABLE_IN_3_32
void                   ide_completion_list_box_row_set_left          (IdeCompletionListBoxRow *self,
                                                                      const gchar             *left);
IDE_AVAILABLE_IN_3_32
void                   ide_completion_list_box_row_set_left_markup   (IdeCompletionListBoxRow *self,
                                                                      const gchar             *left_markup);
IDE_AVAILABLE_IN_3_32
void                   ide_completion_list_box_row_set_right         (IdeCompletionListBoxRow *self,
                                                                      const gchar             *right);
IDE_AVAILABLE_IN_3_32
void                   ide_completion_list_box_row_set_center        (IdeCompletionListBoxRow *self,
                                                                      const gchar             *center);
IDE_AVAILABLE_IN_3_32
void                   ide_completion_list_box_row_set_center_markup (IdeCompletionListBoxRow *self,
                                                                      const gchar             *center_markup);

G_END_DECLS
