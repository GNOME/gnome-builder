/* ide-application-private.h
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#include <dazzle.h>
#include <libpeas/peas.h>

#include "ide-application.h"
#include "ide-keybindings.h"
#include "ide-worker-manager.h"

G_BEGIN_DECLS

struct _IdeApplication
{
  DzlApplication parent_instance;

  /* Array of all of our IdeWorkebench instances (loaded projects and
   * their application windows).
   */
  GPtrArray *workbenches;

  /* We keep a hashtable of GSettings for each of the loaded plugins
   * so that we can keep track if they are manually disabled using
   * the org.gnome.builder.plugin gschema.
   */
  GHashTable *plugin_settings;

  /* Addins which are created and destroyed with the application. We
   * create them in ::startup() (after early stage operations have
   * completed) and destroy them in ::shutdown().
   */
  PeasExtensionSet *addins;

  /* D-Bus Proxy used to track color settings (Night Light) */
  GDBusProxy *color_proxy;

  /* org.gnome.Builder GSettings object to avoid creating a bunch
   * of them (and ensuring it lives long enough to trigger signals
   * for various keys.
   */
  GSettings *settings;

  /* Tracks changes to plugins and updates the available keybindings
   * to ensure they are loaded correctly (including .css files).
   */
  IdeKeybindings *keybindings;

  /* We need to track the GResource files that were manually loaded for
   * plugins on disk (generally Python plugins that need resources). That
   * way we can remove them when the plugin is unloaded.
   */
  GHashTable *plugin_gresources;

  /* We need to stash the unmodified argv for the application somewhere
   * so that we can pass it to a remote instance. Otherwise we lose
   * the ability by cmdline-addins to determine if any options were
   * delivered to the program.
   */
  gchar **argv;

  /* The time the application was started */
  GDateTime *started_at;

  /* Multi-process worker manager */
  IdeWorkerManager *worker_manager;

  /* Our type of process (optionally set to "worker" */
  gchar *type;

  /* The single plugin to load within a worker */
  gchar *plugin;

  /* The dbus-address for worker mode */
  gchar *dbus_address;

  /* Sets the type of workspace to create when creating the next workspace
   * (such as when processing command line arguments).
   */
  GType workspace_type;

  /* If we've detected we lost network access */
  GNetworkMonitor *network_monitor;
  guint has_network : 1;
};

IdeApplication *_ide_application_new                      (gboolean                 standalone,
                                                           const gchar             *type,
                                                           const gchar             *plugin,
                                                           const gchar             *dbus_address);
void            _ide_application_init_color               (IdeApplication          *self);
void            _ide_application_init_actions             (IdeApplication          *self);
void            _ide_application_init_shortcuts           (IdeApplication          *self);
void            _ide_application_load_addins              (IdeApplication          *self);
void            _ide_application_unload_addins            (IdeApplication          *self);
void            _ide_application_load_plugin              (IdeApplication          *self,
                                                           PeasPluginInfo          *plugin_info);
void            _ide_application_add_option_entries       (IdeApplication          *self);
void            _ide_application_load_plugins_for_startup (IdeApplication          *self);
void            _ide_application_load_plugins             (IdeApplication          *self);
void            _ide_application_command_line             (IdeApplication          *self,
                                                           GApplicationCommandLine *cmdline);

G_END_DECLS
