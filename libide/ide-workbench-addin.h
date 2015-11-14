/* ide-workbench-addin.h
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
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

#ifndef IDE_WORKBENCH_ADDIN_H
#define IDE_WORKBENCH_ADDIN_H

#include "ide-uri.h"
#include "ide-workbench.h"

G_BEGIN_DECLS

#define IDE_TYPE_WORKBENCH_ADDIN (ide_workbench_addin_get_type())

G_DECLARE_INTERFACE (IdeWorkbenchAddin, ide_workbench_addin, IDE, WORKBENCH_ADDIN, GObject)

struct _IdeWorkbenchAddinInterface
{
  GTypeInterface parent;

  void     (*load)        (IdeWorkbenchAddin    *self,
                           IdeWorkbench         *workbench);
  void     (*unload)      (IdeWorkbenchAddin    *self,
                           IdeWorkbench         *workbench);
  gboolean (*can_open)    (IdeWorkbenchAddin    *self,
                           IdeUri               *uri,
                           const gchar          *content_type,
                           gint                 *priority);
  void     (*open_async)  (IdeWorkbenchAddin    *self,
                           IdeUri               *uri,
                           const gchar          *content_type,
                           GCancellable         *cancellable,
                           GAsyncReadyCallback   callback,
                           gpointer              user_data);
  gboolean (*open_finish) (IdeWorkbenchAddin    *self,
                           GAsyncResult         *result,
                           GError              **error);

};

void     ide_workbench_addin_load        (IdeWorkbenchAddin    *self,
                                          IdeWorkbench         *workbench);
void     ide_workbench_addin_unload      (IdeWorkbenchAddin    *self,
                                          IdeWorkbench         *workbench);
gboolean ide_workbench_addin_can_open    (IdeWorkbenchAddin    *self,
                                          IdeUri               *uri,
                                          const gchar          *content_type,
                                          gint                 *priority);
void     ide_workbench_addin_open_async  (IdeWorkbenchAddin    *self,
                                          IdeUri               *uri,
                                          const gchar          *content_type,
                                          GCancellable         *cancellable,
                                          GAsyncReadyCallback   callback,
                                          gpointer              user_data);
gboolean ide_workbench_addin_open_finish (IdeWorkbenchAddin    *self,
                                          GAsyncResult         *result,
                                          GError              **error);

G_END_DECLS

#endif /* IDE_WORKBENCH_ADDIN_H */
