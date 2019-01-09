/* gbp-rename-file-popover.h
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#include <libide-tree.h>

G_BEGIN_DECLS

#define GBP_TYPE_RENAME_FILE_POPOVER (gbp_rename_file_popover_get_type())

G_DECLARE_FINAL_TYPE (GbpRenameFilePopover, gbp_rename_file_popover, GBP, RENAME_FILE_POPOVER, GtkPopover)

GFile *gbp_rename_file_popover_get_file       (GbpRenameFilePopover  *self);
void   gbp_rename_file_popover_display_async  (GbpRenameFilePopover  *self,
                                               IdeTree               *tree,
                                               IdeTreeNode           *node,
                                               GCancellable          *cancellable,
                                               GAsyncReadyCallback    callback,
                                               gpointer               user_data);
GFile *gbp_rename_file_popover_display_finish (GbpRenameFilePopover  *self,
                                               GAsyncResult          *result,
                                               GError               **error);

G_END_DECLS
