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

static IdeBuildTarget *
find_best_target (GPtrArray *targets)
{
  IdeBuildTarget *ret = NULL;
  guint i;

  g_assert (targets != NULL);

  for (i = 0; i < targets->len; i++)
    {
      IdeBuildTarget *target = g_ptr_array_index (targets, i);
      g_autoptr(GFile) installdir = NULL;

      installdir = ide_build_target_get_install_directory (target);

      if (installdir == NULL)
        continue;

      if (ret == NULL)
        ret = target;

      /* TODO: Compare likelyhood of primary binary */
    }

  return ret;
}

static void
gbp_run_workbench_addin_get_build_targets_cb (GObject      *object,
                                              GAsyncResult *result,
                                              gpointer      user_data)
{
  IdeBuildSystem *build_system = (IdeBuildSystem *)object;
  GbpRunWorkbenchAddin *self;
  g_autoptr(GPtrArray) targets = NULL;
  g_autoptr(GTask) task = user_data;
  IdeBuildTarget *best_match;
  IdeRunManager *run_manager;
  GCancellable *cancellable;
  IdeContext *context;
  GError *error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_SYSTEM (build_system));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  cancellable = g_task_get_cancellable (task);

  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (GBP_IS_RUN_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (self->workbench));

  targets = ide_build_system_get_build_targets_finish (build_system, result, &error);

  if (targets == NULL)
    {
      g_task_return_error (task, error);
      IDE_EXIT;
    }

  best_match = find_best_target (targets);

  if (best_match == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "%s",
                               _("Failed to locate build target"));
      IDE_EXIT;
    }

  context = ide_workbench_get_context (self->workbench);
  run_manager = ide_context_get_run_manager (context);

  ide_run_manager_run_async (run_manager,
                             best_match,
                             cancellable,
                             gbp_run_workbench_addin_run_cb,
                             g_steal_pointer (&task));

  IDE_EXIT;
}

static void
gbp_run_workbench_addin_run (GSimpleAction *action,
                             GVariant      *param,
                             gpointer       user_data)
{
  GbpRunWorkbenchAddin *self = user_data;
  g_autoptr(GTask) task = NULL;
  IdeBuildSystem *build_system;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_RUN_WORKBENCH_ADDIN (self));

  context = ide_workbench_get_context (self->workbench);
  build_system = ide_context_get_build_system (context);

  task = g_task_new (self, NULL, NULL, NULL);
  g_task_set_source_tag (task, gbp_run_workbench_addin_run);

  ide_build_system_get_build_targets_async (build_system,
                                            NULL,
                                            gbp_run_workbench_addin_get_build_targets_cb,
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
  IdeWorkbenchHeaderBar *headerbar;
  IdeRunManager *run_manager;
  IdeContext *context;
  GtkWidget *button;
  static const GActionEntry entries[] = {
    { "run", gbp_run_workbench_addin_run },
    { "stop", gbp_run_workbench_addin_stop },
  };

  g_assert (GBP_IS_RUN_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = workbench;

  context = ide_workbench_get_context (workbench);
  run_manager = ide_context_get_run_manager (context);

  headerbar = ide_workbench_get_headerbar (workbench);

  button = g_object_new (GTK_TYPE_BUTTON,
                         "action-name", "run-tools.run",
                         "focus-on-click", FALSE,
                         "child", g_object_new (GTK_TYPE_IMAGE,
                                                "icon-name", "media-playback-start-symbolic",
                                                "visible", TRUE,
                                                NULL),
                         "tooltip-text", _("Run project"),
                         NULL);
  g_object_bind_property (run_manager, "busy", button, "visible", G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);
  ide_widget_add_style_class (button, "image-button");
  ide_workbench_header_bar_insert_right (headerbar, button, GTK_PACK_START, 0);

  button = g_object_new (GTK_TYPE_BUTTON,
                         "action-name", "run-tools.stop",
                         "focus-on-click", FALSE,
                         "child", g_object_new (GTK_TYPE_IMAGE,
                                                "icon-name", "media-playback-stop-symbolic",
                                                "visible", TRUE,
                                                NULL),
                         NULL);
  g_object_bind_property (run_manager, "busy", button, "visible", G_BINDING_SYNC_CREATE);
  ide_widget_add_style_class (button, "image-button");
  ide_workbench_header_bar_insert_right (headerbar, button, GTK_PACK_START, 0);

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
