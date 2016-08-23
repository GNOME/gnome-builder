/* gbp-run-workbench-addin.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#include <glib/gi18n.h>

#include "gbp-run-workbench-addin.h"

struct _GbpRunWorkbenchAddin
{
  GObject       parent_instance;

  IdeWorkbench *workbench;
};

static void workbench_addin_init_iface (IdeWorkbenchAddinInterface *iface);

G_DEFINE_TYPE_EXTENDED (GbpRunWorkbenchAddin, gbp_run_workbench_addin, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN,
                                               workbench_addin_init_iface))

static void
gbp_run_workbench_addin_run_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  IdeRunManager *run_manager = (IdeRunManager *)object;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_RUN_MANAGER (run_manager));
  g_assert (G_IS_TASK (task));

  if (!ide_run_manager_run_finish (run_manager, result, &error))
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s", error->message);
      g_task_return_error (task, error);
      IDE_GOTO (failure);
    }

  g_task_return_boolean (task, TRUE);

failure:
  IDE_EXIT;
}

static void
gbp_run_workbench_addin_run (GSimpleAction *action,
                             GVariant      *param,
                             gpointer       user_data)
{
  GbpRunWorkbenchAddin *self = user_data;
  g_autoptr(GTask) task = NULL;
  IdeRunManager *run_manager;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_RUN_WORKBENCH_ADDIN (self));

  context = ide_workbench_get_context (self->workbench);
  run_manager = ide_context_get_run_manager (context);

  task = g_task_new (self, NULL, NULL, NULL);
  g_task_set_source_tag (task, gbp_run_workbench_addin_run);

  ide_run_manager_run_async (run_manager,
                             NULL,
                             NULL,
                             gbp_run_workbench_addin_run_cb,
                             g_steal_pointer (&task));

  IDE_EXIT;
}

static void
gbp_run_workbench_addin_stop (GSimpleAction *action,
                              GVariant      *param,
                              gpointer       user_data)
{
  GbpRunWorkbenchAddin *self = user_data;
  IdeRunManager *run_manager;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_RUN_WORKBENCH_ADDIN (self));

  context = ide_workbench_get_context (self->workbench);
  run_manager = ide_context_get_run_manager (context);

  ide_run_manager_cancel (run_manager);

  IDE_EXIT;
}

static void
gbp_run_workbench_addin_load (IdeWorkbenchAddin *addin,
                              IdeWorkbench      *workbench)
{
  GbpRunWorkbenchAddin *self = (GbpRunWorkbenchAddin *)addin;
  g_autoptr(GSimpleActionGroup) group = NULL;
  IdeRunManager *run_manager;
  IdeContext *context;
  static const GActionEntry entries[] = {
    { "run", gbp_run_workbench_addin_run },
    { "stop", gbp_run_workbench_addin_stop },
  };

  g_assert (GBP_IS_RUN_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = workbench;

  context = ide_workbench_get_context (workbench);
  run_manager = ide_context_get_run_manager (context);

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group), entries, G_N_ELEMENTS (entries), self);
  gtk_widget_insert_action_group (GTK_WIDGET (workbench), "run-tools", G_ACTION_GROUP (group));

  g_object_bind_property (run_manager,
                          "busy",
                          g_action_map_lookup_action (G_ACTION_MAP (group), "run"),
                          "enabled",
                          G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

  g_object_bind_property (run_manager,
                          "busy",
                          g_action_map_lookup_action (G_ACTION_MAP (group), "stop"),
                          "enabled",
                          G_BINDING_SYNC_CREATE);
}

static void
gbp_run_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                IdeWorkbench      *workbench)
{
  GbpRunWorkbenchAddin *self = (GbpRunWorkbenchAddin *)addin;

  g_assert (GBP_IS_RUN_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));
  g_assert (workbench == self->workbench);

  self->workbench = NULL;
}

static void
workbench_addin_init_iface (IdeWorkbenchAddinInterface *iface)
{
  iface->load = gbp_run_workbench_addin_load;
  iface->unload = gbp_run_workbench_addin_unload;
}

static void
gbp_run_workbench_addin_class_init (GbpRunWorkbenchAddinClass *klass)
{
}

static void
gbp_run_workbench_addin_init (GbpRunWorkbenchAddin *self)
{
}
