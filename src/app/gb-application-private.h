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

#ifndef GB_APPLICATION_PRIVATE_H
#define GB_APPLICATION_PRIVATE_H

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <ide.h>
#include <libpeas/peas.h>

#include "gb-keybindings.h"
#include "gb-preferences-window.h"

G_BEGIN_DECLS

struct _GbApplication
{
  GtkApplication       parent_instance;

  GDateTime           *started_at;
  GbKeybindings       *keybindings;
  GbPreferencesWindow *preferences_window;
  IdeRecentProjects   *recent_projects;
  GtkWindowGroup      *greeter_group;
  PeasExtensionSet    *extensions;
};


G_END_DECLS

#endif /* GB_APPLICATION_PRIVATE_H */
