/* gbp-sysprof-workbench-addin.c
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

#define G_LOG_DOMAIN "gbp-sysprof-workbench-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-gui.h>
#include <libide-threading.h>

#include "gbp-sysprof-workbench-addin.h"

struct _GbpSysprofWorkbenchAddin
{
  GObject       parent_instance;

  IdeWorkbench *workbench;

  guint         project_loaded : 1;
  guint         run_manager_busy : 1;
};

static void gbp_sysprof_workbench_addin_run (GbpSysprofWorkbenchAddin *self,
                                             GVariant                 *param);

IDE_DEFINE_ACTION_GROUP (GbpSysprofWorkbenchAddin, gbp_sysprof_workbench_addin, {
  { "run", gbp_sysprof_workbench_addin_run },
})

static void
gbp_sysprof_workbench_addin_load (IdeWorkbenchAddin *addin,
                                  IdeWorkbench      *workbench)
{
  GbpSysprofWorkbenchAddin *self = (GbpSysprofWorkbenchAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYSPROF_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = workbench;
}

static void
update_action_enabled (GbpSysprofWorkbenchAddin *self)
{
  gboolean enabled;

  g_assert (GBP_IS_SYSPROF_WORKBENCH_ADDIN (self));

  enabled = !self->run_manager_busy && self->project_loaded;
  gbp_sysprof_workbench_addin_set_action_enabled (self, "run", enabled);
}

static void
gbp_sysprof_workbench_addin_notify_busy_cb (GbpSysprofWorkbenchAddin *self,
                                            GParamSpec               *pspec,
                                            IdeRunManager            *run_manager)
{
  g_assert (GBP_IS_SYSPROF_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_RUN_MANAGER (run_manager));

  self->run_manager_busy = ide_run_manager_get_busy (run_manager);

  update_action_enabled (self);
}

static void
gbp_sysprof_workbench_addin_project_loaded (IdeWorkbenchAddin *addin,
                                            IdeProjectInfo    *project_info)
{
  GbpSysprofWorkbenchAddin *self = (GbpSysprofWorkbenchAddin *)addin;
  IdeRunManager *run_manager;
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYSPROF_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (self->workbench));

  self->project_loaded = TRUE;

  context = ide_workbench_get_context (self->workbench);
  run_manager = ide_run_manager_from_context (context);

  g_signal_connect_object (run_manager,
                           "notify::busy",
                           G_CALLBACK (gbp_sysprof_workbench_addin_notify_busy_cb),
                           self,
                           G_CONNECT_SWAPPED);

  gbp_sysprof_workbench_addin_notify_busy_cb (self, NULL, run_manager);
}

static void
gbp_sysprof_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                    IdeWorkbench      *workbench)
{
  GbpSysprofWorkbenchAddin *self = (GbpSysprofWorkbenchAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYSPROF_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = NULL;
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = gbp_sysprof_workbench_addin_load;
  iface->unload = gbp_sysprof_workbench_addin_unload;
  iface->project_loaded = gbp_sysprof_workbench_addin_project_loaded;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpSysprofWorkbenchAddin, gbp_sysprof_workbench_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, gbp_sysprof_workbench_addin_init_action_group)
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN, workbench_addin_iface_init))

static void
gbp_sysprof_workbench_addin_class_init (GbpSysprofWorkbenchAddinClass *klass)
{
}

static void
gbp_sysprof_workbench_addin_init (GbpSysprofWorkbenchAddin *self)
{
  gbp_sysprof_workbench_addin_set_action_enabled (self, "run", FALSE);
}

static void
gbp_sysprof_workbench_addin_run (GbpSysprofWorkbenchAddin *self,
                                 GVariant                 *param)
{
  PeasPluginInfo *plugin_info;
  IdeRunManager *run_manager;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYSPROF_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (self->workbench));

  plugin_info = peas_engine_get_plugin_info (peas_engine_get_default (), "sysprof");
  context = ide_workbench_get_context (self->workbench);
  run_manager = ide_run_manager_from_context (context);

  ide_run_manager_set_run_tool_from_plugin_info (run_manager, plugin_info);
  ide_run_manager_run_async (run_manager, NULL, NULL, NULL);

  IDE_EXIT;
}
