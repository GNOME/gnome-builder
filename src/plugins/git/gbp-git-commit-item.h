/*
 * gbp-git-commit-item.h
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

#include <glib-object.h>

G_BEGIN_DECLS

#define GBP_TYPE_GIT_COMMIT_ITEM (gbp_git_commit_item_get_type())

G_DECLARE_DERIVABLE_TYPE (GbpGitCommitItem, gbp_git_commit_item, GBP, GIT_COMMIT_ITEM, GObject)

struct _GbpGitCommitItemClass
{
  GObjectClass parent_class;
};

const char *gbp_git_commit_item_get_title     (GbpGitCommitItem *self);
void        gbp_git_commit_item_set_title     (GbpGitCommitItem *self,
                                               const char       *title);
const char *gbp_git_commit_item_get_icon_name (GbpGitCommitItem *self);
void        gbp_git_commit_item_set_icon_name (GbpGitCommitItem *self,
                                               const char       *icon_name);

G_END_DECLS
