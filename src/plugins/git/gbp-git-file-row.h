/*
 * gbp-git-file-row.h
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include "gbp-git-file-item.h"

G_BEGIN_DECLS

#define GBP_TYPE_GIT_FILE_ROW (gbp_git_file_row_get_type())

G_DECLARE_FINAL_TYPE (GbpGitFileRow, gbp_git_file_row, GBP, GIT_FILE_ROW, GtkWidget)

GtkWidget      *gbp_git_file_row_new      (void);
void            gbp_git_file_row_set_item (GbpGitFileRow  *self,
                                           GbpGitFileItem *item);
GbpGitFileItem *gbp_git_file_row_get_item (GbpGitFileRow  *self);

G_END_DECLS
