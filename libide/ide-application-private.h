/* ide-application-private.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#ifndef IDE_APPLICATION_PRIVATE_H
#define IDE_APPLICATION_PRIVATE_H

#include <gio/gio.h>
#include <libpeas/peas.h>

#include "ide-application.h"
#include "ide-keybindings.h"
#include "ide-recent-projects.h"
#include "ide-worker-manager.h"

G_BEGIN_DECLS

struct _IdeApplication
{
  GtkApplication       parent_instance;

  IdeApplicationMode   mode;

  PeasExtensionSet    *addins;
  gchar               *dbus_address;

  PeasPluginInfo      *tool;
  gchar              **tool_arguments;

  PeasPluginInfo      *worker;

  IdeWorkerManager    *worker_manager;

  IdeKeybindings      *keybindings;

  IdeRecentProjects   *recent_projects;

  GDateTime           *started_at;
};

void     ide_application_discover_plugins   (IdeApplication   *self) G_GNUC_INTERNAL;
void     ide_application_load_plugins       (IdeApplication   *self) G_GNUC_INTERNAL;
void     ide_application_load_addins        (IdeApplication   *self) G_GNUC_INTERNAL;
gboolean ide_application_local_command_line (GApplication     *application,
                                             gchar          ***arguments,
                                             gint             *exit_status) G_GNUC_INTERNAL;

G_END_DECLS

#endif /* IDE_APPLICATION_PRIVATE_H */
