/* ide-rename-provider.h
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#ifndef IDE_RENAME_PROVIDER_H
#define IDE_RENAME_PROVIDER_H

#include "ide-object.h"

#include "projects/ide-project-edit.h"

G_BEGIN_DECLS

#define IDE_TYPE_RENAME_PROVIDER (ide_rename_provider_get_type())

G_DECLARE_INTERFACE (IdeRenameProvider, ide_rename_provider, IDE, RENAME_PROVIDER, IdeObject)

struct _IdeRenameProviderInterface
{
  GTypeInterface parent_iface;

  void     (*rename_async)  (IdeRenameProvider    *self,
                             IdeSourceLocation    *location,
                             const gchar          *new_name,
                             GCancellable         *cancellable,
                             GAsyncReadyCallback   callback,
                             gpointer              user_data);
  gboolean (*rename_finish) (IdeRenameProvider    *self,
                             GAsyncResult         *result,
                             GPtrArray           **edits,
                             GError              **error);
  void     (*load)          (IdeRenameProvider    *self);
};

void      ide_rename_provider_load          (IdeRenameProvider     *self);
void      ide_rename_provider_rename_async  (IdeRenameProvider     *self,
                                             IdeSourceLocation     *location,
                                             const gchar           *new_name,
                                             GCancellable          *cancellable,
                                             GAsyncReadyCallback    callback,
                                             gpointer               user_data);
gboolean  ide_rename_provider_rename_finish (IdeRenameProvider     *self,
                                             GAsyncResult          *result,
                                             GPtrArray            **edits,
                                             GError               **error);

G_END_DECLS

#endif /* IDE_RENAME_PROVIDER_H */
