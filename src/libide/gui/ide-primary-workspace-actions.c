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

#include <libide-foundry.h>
#include <libpeas/peas.h>

#include "ide-gui-global.h"
#include "ide-gui-private.h"
#include "ide-primary-workspace.h"

static void
update_dependencies_cb (GObject      *object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  IdeDependencyUpdater *updater = (IdeDependencyUpdater *)object;
  g_autoptr(IdePrimaryWorkspace) self = user_data;
  g_autoptr(GError) error = NULL;
  IdeContext *context;

  g_assert (IDE_IS_DEPENDENCY_UPDATER (updater));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_PRIMARY_WORKSPACE (self));

  context = ide_widget_get_context (GTK_WIDGET (self));

  if (!ide_dependency_updater_update_finish (updater, result, &error))
    ide_context_warning (context, "%s", error->message);

  ide_object_destroy (IDE_OBJECT (updater));
}

static void
ide_primary_workspace_actions_update_dependencies_cb (PeasExtensionSet *set,
                                                      PeasPluginInfo   *plugin_info,
                                                      PeasExtension    *exten,
                                                      gpointer          user_data)
{
  IdeDependencyUpdater *updater = (IdeDependencyUpdater *)exten;
  IdePrimaryWorkspace *self = user_data;
  IdeContext *context;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_DEPENDENCY_UPDATER (updater));
  g_assert (IDE_IS_PRIMARY_WORKSPACE (self));

  context = ide_widget_get_context (GTK_WIDGET (self));
  ide_object_append (IDE_OBJECT (context), IDE_OBJECT (updater));

  ide_dependency_updater_update_async (updater,
                                       NULL,
                                       update_dependencies_cb,
                                       g_object_ref (self));
}

static void
ide_primary_workspace_actions_update_dependencies (GSimpleAction *action,
                                                   GVariant      *param,
                                                   gpointer       user_data)
{
  IdePrimaryWorkspace *self = user_data;
  g_autoptr(PeasExtensionSet) set = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_PRIMARY_WORKSPACE (self));

  set = peas_extension_set_new (peas_engine_get_default (),
                                IDE_TYPE_DEPENDENCY_UPDATER,
                                NULL);
  peas_extension_set_foreach (set, ide_primary_workspace_actions_update_dependencies_cb, self);
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
