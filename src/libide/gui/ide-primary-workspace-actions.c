/* ide-primary-workspace-actions.c
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

#define G_LOG_DOMAIN "ide-primary-workspace-actions"

#include "config.h"

#include <glib/gi18n.h>
#include <libide-foundry.h>
#include <libpeas/peas.h>

#include "ide-gui-global.h"
#include "ide-gui-private.h"
#include "ide-primary-workspace.h"

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
  IdePrimaryWorkspace *self;
  UpdateDependencies *ud;
  IdeContext *context;

  g_assert (IDE_IS_DEPENDENCY_UPDATER (updater));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  context = ide_widget_get_context (GTK_WIDGET (self));

  if (!ide_dependency_updater_update_finish (updater, result, &error))
    ide_context_warning (context, "%s", error->message);

  ide_object_destroy (IDE_OBJECT (updater));

  ud = ide_task_get_task_data (task);
  ud->n_active--;

  if (ud->n_active == 0)
    ide_task_return_boolean (task, TRUE);
}

static void
ide_primary_workspace_actions_update_dependencies_cb (PeasExtensionSet *set,
                                                      PeasPluginInfo   *plugin_info,
                                                      PeasExtension    *exten,
                                                      gpointer          user_data)
{
  IdeDependencyUpdater *updater = (IdeDependencyUpdater *)exten;
  IdePrimaryWorkspace *self;
  UpdateDependencies *ud;
  IdeContext *context;
  IdeTask *task = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_DEPENDENCY_UPDATER (updater));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);

  context = ide_widget_get_context (GTK_WIDGET (self));
  ide_object_append (IDE_OBJECT (context), IDE_OBJECT (updater));

  ud = ide_task_get_task_data (task);
  ud->n_active++;

  ide_dependency_updater_update_async (updater,
                                       NULL,
                                       update_dependencies_cb,
                                       g_object_ref (task));
}

static void
ide_primary_workspace_actions_update_dependencies (GSimpleAction *action,
                                                   GVariant      *param,
                                                   gpointer       user_data)
{
  IdePrimaryWorkspace *self = user_data;
  g_autoptr(PeasExtensionSet) set = NULL;
  g_autoptr(IdeNotification) notif = NULL;
  g_autoptr(IdeTask) task = NULL;
  UpdateDependencies *state;
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_PRIMARY_WORKSPACE (self));

  context = ide_widget_get_context (GTK_WIDGET (self));

  notif = ide_notification_new ();
  ide_notification_set_title (notif, _("Updating Dependencies…"));
  ide_notification_set_body (notif, _("Builder is updating your projects configured dependencies."));
  ide_notification_set_icon_name (notif, "software-update-available-symbolic");
  ide_notification_set_has_progress (notif, TRUE);
  ide_notification_set_progress_is_imprecise (notif, TRUE);
  ide_notification_attach (notif, IDE_OBJECT (context));

  state = g_slice_new0 (UpdateDependencies);
  state->n_active = 0;
  state->notif = g_object_ref (notif);

  task = ide_task_new (self, NULL, NULL, NULL);
  ide_task_set_source_tag (task, ide_primary_workspace_actions_update_dependencies);
  ide_task_set_task_data (task, state, update_dependencies_free);

  set = peas_extension_set_new (peas_engine_get_default (),
                                IDE_TYPE_DEPENDENCY_UPDATER,
                                NULL);
  peas_extension_set_foreach (set,
                              ide_primary_workspace_actions_update_dependencies_cb,
                              task);
  if (state->n_active == 0)
    ide_task_return_boolean (task, TRUE);
}

static const GActionEntry actions[] = {
  { "update-dependencies", ide_primary_workspace_actions_update_dependencies },
};

void
_ide_primary_workspace_init_actions (IdePrimaryWorkspace *self)
{
  g_assert (IDE_IS_PRIMARY_WORKSPACE (self));

  g_action_map_add_action_entries (G_ACTION_MAP (self),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   self);
}
