/* ide-application-private.h
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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
#include <gio/gio.h>
#include <libpeas/peas.h>

#include "application/ide-application.h"
#include "gsettings/ide-language-defaults.h"
#include "keybindings/ide-keybindings.h"
#include "projects/ide-recent-projects.h"
#include "workers/ide-worker-manager.h"

G_BEGIN_DECLS

struct _IdeApplication
{
  DzlApplication       parent_instance;

  IdeApplicationMode   mode;

  PeasExtensionSet    *addins;
  gchar               *dbus_address;

  PeasPluginInfo      *tool;
  gchar              **tool_arguments;

  IdeTransferManager  *transfer_manager;

  PeasPluginInfo      *worker;
  IdeWorkerManager    *worker_manager;

  IdeKeybindings      *keybindings;

  IdeRecentProjects   *recent_projects;

  GDateTime           *started_at;

  GHashTable          *plugin_css;
  GHashTable          *plugin_gresources;

  GList               *test_funcs;

  GHashTable          *plugin_settings;

  GPtrArray           *reapers;
};

void     ide_application_discover_plugins           (IdeApplication        *self) G_GNUC_INTERNAL;
void     ide_application_load_plugins               (IdeApplication        *self) G_GNUC_INTERNAL;
void     ide_application_load_addins                (IdeApplication        *self) G_GNUC_INTERNAL;
void     ide_application_init_plugin_accessories    (IdeApplication        *self) G_GNUC_INTERNAL;
gboolean ide_application_local_command_line         (GApplication          *application,
                                                     gchar               ***arguments,
                                                     gint                  *exit_status) G_GNUC_INTERNAL;
void     ide_application_run_tests                  (IdeApplication        *self);
void     ide_application_open_async                 (IdeApplication        *self,
                                                     GFile                **files,
                                                     gint                   n_files,
                                                     const gchar           *hint,
                                                     GCancellable          *cancellable,
                                                     GAsyncReadyCallback    callback,
                                                     gpointer               user_data);
gboolean ide_application_open_finish                (IdeApplication        *self,
                                                     GAsyncResult          *reuslt,
                                                     GError               **error);
void     _ide_application_init_shortcuts            (IdeApplication        *self);
void     _ide_application_set_mode                  (IdeApplication        *self,
                                                     IdeApplicationMode     mode);

G_END_DECLS
