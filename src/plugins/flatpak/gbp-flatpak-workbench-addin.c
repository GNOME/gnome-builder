/* gbp-flatpak-workbench-addin.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-flatpak-workbench-addin"

#include "config.h"

#define G_SETTINGS_ENABLE_BACKEND
#include <gio/gsettingsbackend.h>

#include <libide-foundry.h>

#include "gbp-flatpak-manifest.h"
#include "gbp-flatpak-workbench-addin.h"

struct _GbpFlatpakWorkbenchAddin
{
  GObject        parent_instance;
  IdeWorkbench  *workbench;
  IdeRunManager *run_manager;
  gulong         run_signal_handler;
};

static void
gbp_flatpak_workbench_addin_run_cb (GbpFlatpakWorkbenchAddin *self,
                                    IdeRunContext            *run_context,
                                    IdeRunManager            *run_manager)
{
  g_autoptr(GSettingsBackend) backend = NULL;
  g_autoptr(GSettings) gtk_settings = NULL;
  g_autofree char *filename = NULL;
  IdeConfigManager *config_manager;
  IdeContext *context;
  const char *app_id;
  IdeConfig *config;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_RUN_CONTEXT (run_context));
  g_assert (IDE_IS_RUN_MANAGER (run_manager));

  /* This function will overwrite various settings in the application that
   * are very useful for developing applications from Builder. It modifies
   * the GLib GKeyFile-based settings file.
   */

  if (!(context = ide_object_get_context (IDE_OBJECT (run_manager))) ||
      !(config_manager = ide_config_manager_from_context (context)) ||
      !(config = ide_config_manager_get_current (config_manager)) ||
      !GBP_IS_FLATPAK_MANIFEST (config) ||
      !(app_id = ide_config_get_app_id (config)))
    IDE_EXIT;

  filename = g_build_filename (g_get_home_dir (), ".var", "app", app_id, "config", "glib-2.0", "settings", "keyfile", NULL);
  backend = g_keyfile_settings_backend_new (filename, "/", NULL);
  gtk_settings = g_settings_new_with_backend ("org.gtk.Settings.Debug", backend);

  g_settings_set_boolean (gtk_settings, "enable-inspector-keybinding", TRUE);

  IDE_EXIT;
}

static void
gbp_flatpak_workbench_addin_project_loaded (IdeWorkbenchAddin *addin,
                                            IdeProjectInfo    *project_info)
{
  GbpFlatpakWorkbenchAddin *self = (GbpFlatpakWorkbenchAddin *)addin;
  IdeRunManager *run_manager;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (self->workbench));
  g_assert (IDE_IS_PROJECT_INFO (project_info));

  context = ide_workbench_get_context (self->workbench);
  run_manager = ide_run_manager_from_context (context);

  g_set_object (&self->run_manager, run_manager);

  self->run_signal_handler =
    g_signal_connect_object (run_manager,
                             "run",
                             G_CALLBACK (gbp_flatpak_workbench_addin_run_cb),
                             self,
                             G_CONNECT_SWAPPED);

  IDE_EXIT;
}

static void
gbp_flatpak_workbench_addin_load (IdeWorkbenchAddin *addin,
                                  IdeWorkbench      *workbench)
{
  GbpFlatpakWorkbenchAddin *self = (GbpFlatpakWorkbenchAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = workbench;

  IDE_EXIT;
}

static void
gbp_flatpak_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                    IdeWorkbench      *workbench)
{
  GbpFlatpakWorkbenchAddin *self = (GbpFlatpakWorkbenchAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  if (self->run_manager != NULL)
    {
      g_clear_signal_handler (&self->run_signal_handler, self->run_manager);
      g_clear_object (&self->run_manager);
    }

  self->workbench = NULL;

  IDE_EXIT;
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = gbp_flatpak_workbench_addin_load;
  iface->project_loaded = gbp_flatpak_workbench_addin_project_loaded;
  iface->unload = gbp_flatpak_workbench_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpFlatpakWorkbenchAddin, gbp_flatpak_workbench_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN, workbench_addin_iface_init))

static void
gbp_flatpak_workbench_addin_class_init (GbpFlatpakWorkbenchAddinClass *klass)
{
}

static void
gbp_flatpak_workbench_addin_init (GbpFlatpakWorkbenchAddin *self)
{
}
