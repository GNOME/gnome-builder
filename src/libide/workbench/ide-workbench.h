/* ide-workbench.h
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

#pragma once

#include <dazzle.h>

#include "ide-context.h"

#include "util/ide-uri.h"
#include "workbench/ide-perspective.h"
#include "workbench/ide-workbench-header-bar.h"
#include "workbench/ide-workbench-message.h"

G_BEGIN_DECLS

typedef enum
{
  IDE_WORKBENCH_OPEN_FLAGS_NONE       = 0,
  IDE_WORKBENCH_OPEN_FLAGS_BACKGROUND = 1 << 0,
  IDE_WORKBENCH_OPEN_FLAGS_NO_VIEW    = 1 << 1,
} IdeWorkbenchOpenFlags;

#define IDE_TYPE_WORKBENCH (ide_workbench_get_type())

G_DECLARE_FINAL_TYPE (IdeWorkbench, ide_workbench, IDE, WORKBENCH, DzlApplicationWindow)

void                   ide_workbench_open_project_async           (IdeWorkbench           *self,
                                                                   GFile                  *file_or_directory,
                                                                   GCancellable           *cancellable,
                                                                   GAsyncReadyCallback     callback,
                                                                   gpointer                user_data);
gboolean               ide_workbench_open_project_finish          (IdeWorkbench           *self,
                                                                   GAsyncResult           *result,
                                                                   GError                **error);
void                   ide_workbench_open_uri_async               (IdeWorkbench           *self,
                                                                   IdeUri                 *uri,
                                                                   const gchar            *hint,
                                                                   IdeWorkbenchOpenFlags   flags,
                                                                   GCancellable           *cancellable,
                                                                   GAsyncReadyCallback     callback,
                                                                   gpointer                user_data);
gboolean               ide_workbench_open_uri_finish              (IdeWorkbench           *self,
                                                                   GAsyncResult           *result,
                                                                   GError                **error);
void                   ide_workbench_open_files_async             (IdeWorkbench           *self,
                                                                   GFile                 **files,
                                                                   guint                   n_files,
                                                                   const gchar            *hint,
                                                                   IdeWorkbenchOpenFlags   flags,
                                                                   GCancellable           *cancellable,
                                                                   GAsyncReadyCallback     callback,
                                                                   gpointer                user_data);
gboolean               ide_workbench_open_files_finish            (IdeWorkbench           *self,
                                                                   GAsyncResult           *result,
                                                                   GError                **error);
void                   ide_workbench_save_all_async               (IdeWorkbench           *self,
                                                                   GCancellable           *cancellable,
                                                                   GAsyncReadyCallback     callback,
                                                                   gpointer                user_data);
gboolean               ide_workbench_save_all_finish              (IdeWorkbench           *self,
                                                                   GAsyncResult           *result,
                                                                   GError                **error);
void                   ide_workbench_focus                        (IdeWorkbench           *self,
                                                                   GtkWidget              *widget);
void                   ide_workbench_close                        (IdeWorkbench           *self);
IdeContext            *ide_workbench_get_context                  (IdeWorkbench           *self);
void                   ide_workbench_add_perspective              (IdeWorkbench           *self,
                                                                   IdePerspective         *perspective);
void                   ide_workbench_remove_perspective           (IdeWorkbench           *self,
                                                                   IdePerspective         *perspective);
IdePerspective        *ide_workbench_get_perspective_by_name      (IdeWorkbench           *self,
                                                                   const gchar            *name);
IdePerspective        *ide_workbench_get_visible_perspective      (IdeWorkbench           *self);
void                   ide_workbench_set_visible_perspective      (IdeWorkbench           *self,
                                                                   IdePerspective         *perspective);
const gchar           *ide_workbench_get_visible_perspective_name (IdeWorkbench           *self);
void                   ide_workbench_set_visible_perspective_name (IdeWorkbench           *self,
                                                                   const gchar            *name);
void                   ide_workbench_views_foreach                (IdeWorkbench           *self,
                                                                   GtkCallback             callback,
                                                                   gpointer                user_data);
IdeWorkbenchHeaderBar *ide_workbench_get_headerbar                (IdeWorkbench           *self);
void                   ide_workbench_push_message                 (IdeWorkbench           *self,
                                                                   IdeWorkbenchMessage    *message);
gboolean               ide_workbench_pop_message                  (IdeWorkbench           *self,
                                                                   const gchar            *message_id);
gboolean               ide_workbench_get_focus_mode               (IdeWorkbench           *self);
void                   ide_workbench_set_focus_mode               (IdeWorkbench           *self,
                                                                   gboolean                focus_mode);

G_END_DECLS
