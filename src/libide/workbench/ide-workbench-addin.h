/* ide-workbench-addin.h
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

#include "ide-version-macros.h"

#include "util/ide-uri.h"
#include "workbench/ide-perspective.h"
#include "workbench/ide-workbench.h"

G_BEGIN_DECLS

#define IDE_TYPE_WORKBENCH_ADDIN (ide_workbench_addin_get_type())

G_DECLARE_INTERFACE (IdeWorkbenchAddin, ide_workbench_addin, IDE, WORKBENCH_ADDIN, GObject)

struct _IdeWorkbenchAddinInterface
{
  GTypeInterface parent;

  gchar    *(*get_id)          (IdeWorkbenchAddin      *self);
  void      (*load)            (IdeWorkbenchAddin      *self,
                                IdeWorkbench           *workbench);
  void      (*unload)          (IdeWorkbenchAddin      *self,
                                IdeWorkbench           *workbench);
  gboolean  (*can_open)        (IdeWorkbenchAddin      *self,
                                IdeUri                 *uri,
                                const gchar            *content_type,
                                gint                   *priority);
  void      (*open_async)      (IdeWorkbenchAddin      *self,
                                IdeUri                 *uri,
                                const gchar            *content_type,
                                IdeWorkbenchOpenFlags   flags,
                                GCancellable           *cancellable,
                                GAsyncReadyCallback     callback,
                                gpointer                user_data);
  gboolean  (*open_finish)     (IdeWorkbenchAddin      *self,
                                GAsyncResult           *result,
                                GError                **error);
  void      (*perspective_set) (IdeWorkbenchAddin      *self,
                                IdePerspective         *perspective);
};

IDE_AVAILABLE_IN_ALL
IdeWorkbenchAddin *ide_workbench_addin_find_by_module_name (IdeWorkbench *workbench,
                                                            const gchar  *addin_name);

IDE_AVAILABLE_IN_ALL
gchar    *ide_workbench_addin_get_id          (IdeWorkbenchAddin      *self);
IDE_AVAILABLE_IN_ALL
void      ide_workbench_addin_load            (IdeWorkbenchAddin      *self,
                                               IdeWorkbench           *workbench);
IDE_AVAILABLE_IN_ALL
void      ide_workbench_addin_unload          (IdeWorkbenchAddin      *self,
                                               IdeWorkbench           *workbench);
IDE_AVAILABLE_IN_ALL
gboolean  ide_workbench_addin_can_open        (IdeWorkbenchAddin      *self,
                                               IdeUri                 *uri,
                                               const gchar            *content_type,
                                               gint                   *priority);
IDE_AVAILABLE_IN_ALL
void      ide_workbench_addin_open_async      (IdeWorkbenchAddin      *self,
                                               IdeUri                 *uri,
                                               const gchar            *content_type,
                                               IdeWorkbenchOpenFlags   flags,
                                               GCancellable           *cancellable,
                                               GAsyncReadyCallback     callback,
                                               gpointer                user_data);
IDE_AVAILABLE_IN_ALL
gboolean  ide_workbench_addin_open_finish     (IdeWorkbenchAddin      *self,
                                               GAsyncResult           *result,
                                               GError                **error);
IDE_AVAILABLE_IN_ALL
void      ide_workbench_addin_perspective_set (IdeWorkbenchAddin      *self,
                                               IdePerspective         *perspective);

G_END_DECLS
