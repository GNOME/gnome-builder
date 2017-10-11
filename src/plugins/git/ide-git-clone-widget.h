/* ide-git-clone-widget.h
 *
 * Copyright © 2015 Christian Hergert <chergert@redhat.com>
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

G_BEGIN_DECLS

#define IDE_TYPE_GIT_CLONE_WIDGET (ide_git_clone_widget_get_type())

G_DECLARE_FINAL_TYPE (IdeGitCloneWidget, ide_git_clone_widget, IDE, GIT_CLONE_WIDGET, GtkBin)

void     ide_git_clone_widget_clone_async  (IdeGitCloneWidget    *self,
                                            GCancellable         *cancellable,
                                            GAsyncReadyCallback   callback,
                                            gpointer              user_data);
gboolean ide_git_clone_widget_clone_finish (IdeGitCloneWidget    *self,
                                            GAsyncResult         *result,
                                            GError              **error);

G_END_DECLS
