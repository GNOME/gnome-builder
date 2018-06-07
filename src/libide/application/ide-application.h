/* ide-application.h
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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
#include <gtk/gtk.h>

#include "ide-version-macros.h"

#include "projects/ide-recent-projects.h"
#include "transfers/ide-transfer-manager.h"

G_BEGIN_DECLS

#define IDE_TYPE_APPLICATION    (ide_application_get_type())
#define IDE_APPLICATION_DEFAULT (IDE_APPLICATION (g_application_get_default()))
#define IDE_IS_MAIN_THREAD()    (g_thread_self() == ide_application_get_main_thread())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeApplication, ide_application, IDE, APPLICATION, DzlApplication)

typedef enum
{
  IDE_APPLICATION_MODE_PRIMARY,
  IDE_APPLICATION_MODE_WORKER,
  IDE_APPLICATION_MODE_TOOL,
  IDE_APPLICATION_MODE_TESTS,
} IdeApplicationMode;

IDE_AVAILABLE_IN_ALL
GThread            *ide_application_get_main_thread        (void);
IDE_AVAILABLE_IN_ALL
IdeApplicationMode  ide_application_get_mode               (IdeApplication       *self);
IDE_AVAILABLE_IN_ALL
IdeApplication     *ide_application_new                    (IdeApplicationMode    mode);
IDE_AVAILABLE_IN_ALL
GDateTime          *ide_application_get_started_at         (IdeApplication       *self);
IDE_AVAILABLE_IN_3_28
IdeTransferManager *ide_application_get_transfer_manager   (IdeApplication       *self);
IDE_AVAILABLE_IN_ALL
IdeRecentProjects  *ide_application_get_recent_projects    (IdeApplication       *self);
IDE_AVAILABLE_IN_ALL
void                ide_application_show_projects_window   (IdeApplication       *self);
IDE_AVAILABLE_IN_ALL
const gchar        *ide_application_get_keybindings_mode   (IdeApplication       *self);
IDE_AVAILABLE_IN_ALL
void                ide_application_get_worker_async       (IdeApplication       *self,
                                                            const gchar          *plugin_name,
                                                            GCancellable         *cancellable,
                                                            GAsyncReadyCallback   callback,
                                                            gpointer              user_data);
IDE_AVAILABLE_IN_ALL
GDBusProxy         *ide_application_get_worker_finish      (IdeApplication       *self,
                                                            GAsyncResult         *result,
                                                            GError              **error);
IDE_AVAILABLE_IN_ALL
gboolean            ide_application_open_project           (IdeApplication       *self,
                                                            GFile                *file);
IDE_AVAILABLE_IN_ALL
void                ide_application_add_reaper             (IdeApplication       *self,
                                                            DzlDirectoryReaper   *reaper);
IDE_AVAILABLE_IN_3_28
GFile              *ide_application_get_projects_directory (IdeApplication       *self);

G_END_DECLS
