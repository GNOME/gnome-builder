/* gbp-ctags-workbench-addin.c
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

#define G_LOG_DOMAIN "gbp-ctags-workbench-addin"

#include "config.h"

#include <libide-gui.h>

#include "gbp-ctags-workbench-addin.h"
#include "ide-ctags-service.h"

struct _GbpCtagsWorkbenchAddin
{
  GObject       parent_instance;
  IdeWorkbench *workbench;
};

static void
gbp_ctags_workbench_addin_load_project_async (IdeWorkbenchAddin   *addin,
                                              IdeProjectInfo      *project_info,
                                              GCancellable        *cancellable,
                                              GAsyncReadyCallback  callback,
                                              gpointer             user_data)
{
  GbpCtagsWorkbenchAddin *self = (GbpCtagsWorkbenchAddin *)addin;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(IdeCtagsService) service = NULL;
  IdeContext *context;

  g_assert (GBP_IS_CTAGS_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_PROJECT_INFO (project_info));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (addin, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_ctags_workbench_addin_load_project_async);

  /* We don't load the ctags service until a project is loaded so that
   * we have a stable workdir to use.
   */
  context = ide_workbench_get_context (self->workbench);
  service = ide_object_ensure_child_typed (IDE_OBJECT (context), IDE_TYPE_CTAGS_SERVICE);

  ide_task_return_boolean (task, TRUE);
}

static gboolean
gbp_ctags_workbench_addin_load_project_finish (IdeWorkbenchAddin  *addin,
                                               GAsyncResult       *result,
                                               GError            **error)
{
  g_assert (GBP_IS_CTAGS_WORKBENCH_ADDIN (addin));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
gbp_ctags_workbench_addin_load (IdeWorkbenchAddin *addin,
                                IdeWorkbench      *workbench)
{
  GbpCtagsWorkbenchAddin *self = (GbpCtagsWorkbenchAddin *)addin;

  g_assert (GBP_IS_CTAGS_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = workbench;
}

static void
gbp_ctags_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                  IdeWorkbench      *workbench)
{
  GbpCtagsWorkbenchAddin *self = (GbpCtagsWorkbenchAddin *)addin;

  g_assert (GBP_IS_CTAGS_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = NULL;
}

static void
pause_ctags_cb (GSimpleAction *action,
                GVariant      *param,
                gpointer       user_data)
{
  GbpCtagsWorkbenchAddin *self = user_data;
  IdeContext *context;
  IdeCtagsService *service;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_CTAGS_WORKBENCH_ADDIN (self));
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE_BOOLEAN));

  if ((context = ide_workbench_get_context (self->workbench)) &&
      (service = ide_context_peek_child_typed (context, IDE_TYPE_CTAGS_SERVICE)))
    {
      gboolean val;

      if (g_variant_get_boolean (param))
        ide_ctags_service_pause (service);
      else
        ide_ctags_service_unpause (service);

      /* Re-fetch the value incase we were out-of-sync */
      g_object_get (service, "paused", &val, NULL);
      g_simple_action_set_state (action, g_variant_new_boolean (val));
    }
}

static const GActionEntry actions[] = {
  { "pause-ctags", NULL, NULL, "false", pause_ctags_cb },
};

static void
gbp_ctags_workbench_addin_workspace_added (IdeWorkbenchAddin *addin,
                                           IdeWorkspace      *workspace)
{
  GbpCtagsWorkbenchAddin *self = (GbpCtagsWorkbenchAddin *)addin;

  g_assert (GBP_IS_CTAGS_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  g_action_map_add_action_entries (G_ACTION_MAP (workspace),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   self);
}

static void
gbp_ctags_workbench_addin_workspace_removed (IdeWorkbenchAddin *addin,
                                             IdeWorkspace      *workspace)
{
  g_assert (GBP_IS_CTAGS_WORKBENCH_ADDIN (addin));
  g_assert (IDE_IS_WORKSPACE (workspace));

  for (guint i = 0; i < G_N_ELEMENTS (actions); i++)
    g_action_map_remove_action (G_ACTION_MAP (workspace), actions[i].name);
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = gbp_ctags_workbench_addin_load;
  iface->unload = gbp_ctags_workbench_addin_unload;
  iface->load_project_async = gbp_ctags_workbench_addin_load_project_async;
  iface->load_project_finish = gbp_ctags_workbench_addin_load_project_finish;
  iface->workspace_added = gbp_ctags_workbench_addin_workspace_added;
  iface->workspace_removed = gbp_ctags_workbench_addin_workspace_removed;
}

G_DEFINE_TYPE_WITH_CODE (GbpCtagsWorkbenchAddin, gbp_ctags_workbench_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN,
                                                workbench_addin_iface_init))

static void
gbp_ctags_workbench_addin_class_init (GbpCtagsWorkbenchAddinClass *klass)
{
}

static void
gbp_ctags_workbench_addin_init (GbpCtagsWorkbenchAddin *self)
{
}
