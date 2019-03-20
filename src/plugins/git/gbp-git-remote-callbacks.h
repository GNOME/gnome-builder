/* gnome-builder-git.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <libgit2-glib/ggit.h>
#include <libide-core.h>

G_BEGIN_DECLS

#define GBP_TYPE_GIT_REMOTE_CALLBACKS (gbp_git_remote_callbacks_get_type())

G_DECLARE_FINAL_TYPE (GbpGitRemoteCallbacks, gbp_git_remote_callbacks, GBP, GIT_REMOTE_CALLBACKS, GgitRemoteCallbacks)

GgitRemoteCallbacks *gbp_git_remote_callbacks_new          (void);
gdouble              gbp_git_remote_callbacks_get_fraction (GbpGitRemoteCallbacks *self);
IdeNotification     *gbp_git_remote_callbacks_get_progress (GbpGitRemoteCallbacks *self);
void                 gbp_git_remote_callbacks_cancel       (GbpGitRemoteCallbacks *self);

G_END_DECLS
