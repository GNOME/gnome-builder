/* gbp-update-dependencies-workbench-addin.c
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

#define G_LOG_DOMAIN "gbp-update-dependencies-workbench-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <libpeas.h>

#include <libide-foundry.h>
#include <libide-gui.h>

#include "gbp-update-dependencies-workbench-addin.h"

struct _GbpUpdateDependenciesWorkbenchAddin
{
  GObject       parent_instance;
  IdeWorkbench *workbench;
};

typedef struct
{
  IdeNotification *notif;
  gint n_active;
} UpdateDependencies;

static void
update_dependencies_free (UpdateDependencies *ud)
{
  if (ud->notif != NULL)
    {
      ide_notification_withdraw (ud->notif);
      ide_clear_and_destroy_object (&ud->notif);
    }

  g_slice_free (UpdateDependencies, ud);
}

static void
update_dependencies_cb (GObject      *object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  IdeDependencyUpdater *updater = (IdeDependencyUpdater *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  UpdateDependencies *ud;
  IdeContext *context;

  g_assert (IDE_IS_DEPENDENCY_UPDATER (updater));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  context = ide_object_get_context (IDE_OBJECT (updater));

  if (!ide_dependency_updater_update_finish (updater, result, &error))
    {
      if (!ide_error_ignore (error))
        ide_context_warning (context, "%s", error->message);
    }

  ide_object_destroy (IDE_OBJECT (updater));

  ud = ide_task_get_task_data (task);
  ud->n_active--;

  if (ud->n_active == 0)
    ide_task_return_boolean (task, TRUE);
}

static void
update_dependencies_foreach_cb (PeasExtensionSet *set,
                                PeasPluginInfo   *plugin_info,
                                GObject    *exten,
                                gpointer          user_data)
{
  IdeDependencyUpdater *updater = (IdeDependencyUpdater *)exten;
  GbpUpdateDependenciesWorkbenchAddin *self;
  UpdateDependencies *ud;
  IdeContext *context;
  IdeTask *task = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_DEPENDENCY_UPDATER (updater));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  context = ide_workbench_get_context (self->workbench);
  ide_object_append (IDE_OBJECT (context), IDE_OBJECT (updater));

  ud = ide_task_get_task_data (task);
  ud->n_active++;

  ide_dependency_updater_update_async (updater,
                                       NULL,
                                       update_dependencies_cb,
                                       g_object_ref (task));
}

static void
update_action (GbpUpdateDependenciesWorkbenchAddin *self,
               GVariant                            *param)
{
  g_autoptr(PeasExtensionSet) set = NULL;
  g_autoptr(IdeNotification) notif = NULL;
  g_autoptr(IdeTask) task = NULL;
  UpdateDependencies *state;
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_UPDATE_DEPENDENCIES_WORKBENCH_ADDIN (self));

  context = ide_workbench_get_context (self->workbench);

  notif = ide_notification_new ();
  ide_notification_set_title (notif, _("Updating Dependencies…"));
  ide_notification_set_body (notif, _("Builder is updating your project’s configured dependencies."));
  ide_notification_set_icon_name (notif, "software-update-available-symbolic");
  ide_notification_set_has_progress (notif, TRUE);
  ide_notification_set_progress_is_imprecise (notif, TRUE);
  ide_notification_attach (notif, IDE_OBJECT (context));

  state = g_slice_new0 (UpdateDependencies);
  state->n_active = 0;
  state->notif = g_object_ref (notif);

  task = ide_task_new (self, NULL, NULL, NULL);
  ide_task_set_source_tag (task, update_action);
  ide_task_set_task_data (task, state, update_dependencies_free);

  set = peas_extension_set_new (peas_engine_get_default (),
                                IDE_TYPE_DEPENDENCY_UPDATER,
                                NULL);

  peas_extension_set_foreach (set, update_dependencies_foreach_cb, task);

  if (state->n_active == 0)
    ide_task_return_boolean (task, TRUE);
}

IDE_DEFINE_ACTION_GROUP (GbpUpdateDependenciesWorkbenchAddin, gbp_update_dependencies_workbench_addin, {
  { "update", update_action },
})

static void
gbp_update_dependencies_workbench_addin_project_loaded (IdeWorkbenchAddin *addin,
                                                        IdeProjectInfo    *project_info)
{
  GbpUpdateDependenciesWorkbenchAddin *self = GBP_UPDATE_DEPENDENCIES_WORKBENCH_ADDIN (addin);
  gbp_update_dependencies_workbench_addin_set_action_enabled (self, "update", TRUE);
}

static void
gbp_update_dependencies_workbench_addin_load (IdeWorkbenchAddin *addin,
                                              IdeWorkbench      *workbench)
{
  GBP_UPDATE_DEPENDENCIES_WORKBENCH_ADDIN (addin)->workbench = workbench;
}

static void
gbp_update_dependencies_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                                IdeWorkbench      *workbench)
{
  GBP_UPDATE_DEPENDENCIES_WORKBENCH_ADDIN (addin)->workbench = NULL;
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->project_loaded = gbp_update_dependencies_workbench_addin_project_loaded;
  iface->load = gbp_update_dependencies_workbench_addin_load;
  iface->unload = gbp_update_dependencies_workbench_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpUpdateDependenciesWorkbenchAddin, gbp_update_dependencies_workbench_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, gbp_update_dependencies_workbench_addin_init_action_group)
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN, workbench_addin_iface_init))

static void
gbp_update_dependencies_workbench_addin_class_init (GbpUpdateDependenciesWorkbenchAddinClass *klass)
{
}

static void
gbp_update_dependencies_workbench_addin_init (GbpUpdateDependenciesWorkbenchAddin *self)
{
  gbp_update_dependencies_workbench_addin_set_action_enabled (self, "update", FALSE);
}
