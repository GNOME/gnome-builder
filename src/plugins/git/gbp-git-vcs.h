/* gbp-git-vcs.h
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

#include <libgit2-glib/ggit.h>
#include <libide-vcs.h>

G_BEGIN_DECLS

#define GBP_TYPE_GIT_VCS (gbp_git_vcs_get_type())

G_DECLARE_FINAL_TYPE (GbpGitVcs, gbp_git_vcs, GBP, GIT_VCS, IdeObject)

GFile          *gbp_git_vcs_get_location   (GbpGitVcs            *self);
GgitRepository *gbp_git_vcs_get_repository (GbpGitVcs            *self);
void            gbp_git_vcs_reload_async   (GbpGitVcs            *self,
                                            GCancellable         *cancellable,
                                            GAsyncReadyCallback   callback,
                                            gpointer              user_data);
gboolean        gbp_git_vcs_reload_finish  (GbpGitVcs            *self,
                                            GAsyncResult         *result,
                                            GError              **error);

G_END_DECLS
