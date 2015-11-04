/* gb-application-private.h
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

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <libpeas/peas.h>

#include "ide-keybindings.h"
#include "ide-recent-projects.h"
#include "ide-worker-manager.h"

G_BEGIN_DECLS

struct _IdeApplication
{
  GtkApplication        parent_instance;

  gchar                *argv0;
  gchar                *dbus_address;
  PeasExtensionSet     *extensions;
  GtkWindowGroup       *greeter_group;
  IdeKeybindings       *keybindings;
  GtkWindow            *preferences_window;
  IdeRecentProjects    *recent_projects;
  GDateTime            *startup_time;
  gchar                *type;
  IdeWorkerManager     *worker_manager;
};

G_END_DECLS

#endif /* GB_APPLICATION_PRIVATE_H */
