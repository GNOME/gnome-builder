/*
 * gbp-git-file-item.h
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

#include "gbp-git-commit-item.h"

G_BEGIN_DECLS

#define GBP_TYPE_GIT_FILE_ITEM (gbp_git_file_item_get_type())

G_DECLARE_FINAL_TYPE (GbpGitFileItem, gbp_git_file_item, GBP, GIT_FILE_ITEM, GbpGitCommitItem)

GFile      *gbp_git_file_item_get_file  (GbpGitFileItem *self);
const char *gbp_git_file_item_get_title (GbpGitFileItem *self);
GIcon      *gbp_git_file_item_dup_icon  (GbpGitFileItem *self);

G_END_DECLS
