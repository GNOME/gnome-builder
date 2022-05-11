/* ide-project.h
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

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_PROJECT (ide_project_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeProject, ide_project, IDE, PROJECT, IdeObject)

IDE_AVAILABLE_IN_ALL
IdeProject *ide_project_from_context         (IdeContext           *context);
IDE_AVAILABLE_IN_ALL
void        ide_project_rename_file_async    (IdeProject           *self,
                                              GFile                *orig_file,
                                              GFile                *new_file,
                                              GCancellable         *cancellable,
                                              GAsyncReadyCallback   callback,
                                              gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gboolean    ide_project_rename_file_finish   (IdeProject           *self,
                                              GAsyncResult         *result,
                                              GError              **error);
IDE_AVAILABLE_IN_ALL
void        ide_project_trash_file_async     (IdeProject           *self,
                                              GFile                *file,
                                              GCancellable         *cancellable,
                                              GAsyncReadyCallback   callback,
                                              gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gboolean    ide_project_trash_file_finish    (IdeProject           *self,
                                              GAsyncResult         *result,
                                              GError              **error);
IDE_AVAILABLE_IN_ALL
void         ide_project_list_similar_async  (IdeProject           *self,
                                              GFile                *file,
                                              GCancellable         *cancellable,
                                              GAsyncReadyCallback   callback,
                                              gpointer              user_data);
IDE_AVAILABLE_IN_ALL
GListModel  *ide_project_list_similar_finish (IdeProject           *self,
                                              GAsyncResult         *result,
                                              GError              **error);

G_END_DECLS
