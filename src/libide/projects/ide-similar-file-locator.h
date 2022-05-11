/* ide-similar-file-locator.h
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#if !defined (IDE_PROJECTS_INSIDE) && !defined (IDE_PROJECTS_COMPILATION)
# error "Only <libide-projects.h> can be included directly."
#endif

#include <libide-core.h>

#define IDE_TYPE_SIMILAR_FILE_LOCATOR (ide_similar_file_locator_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_INTERFACE (IdeSimilarFileLocator, ide_similar_file_locator, IDE, SIMILAR_FILE_LOCATOR, GObject)

struct _IdeSimilarFileLocatorInterface
{
  GTypeInterface parent_iface;

  void        (*list_async)  (IdeSimilarFileLocator  *self,
                              GFile                  *file,
                              GCancellable           *cancellable,
                              GAsyncReadyCallback     callback,
                              gpointer                user_data);
  GListModel *(*list_finish) (IdeSimilarFileLocator  *self,
                              GAsyncResult           *result,
                              GError                **error);
};

IDE_AVAILABLE_IN_ALL
void        ide_similar_file_locator_list_async  (IdeSimilarFileLocator  *self,
                                                  GFile                  *file,
                                                  GCancellable           *cancellable,
                                                  GAsyncReadyCallback     callback,
                                                  gpointer                user_data);
IDE_AVAILABLE_IN_ALL
GListModel *ide_similar_file_locator_list_finish (IdeSimilarFileLocator  *self,
                                                  GAsyncResult           *result,
                                                  GError                **error);

G_END_DECLS
