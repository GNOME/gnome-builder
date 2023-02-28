/* gbp-sessionui-workbench-addin.c
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

#define G_LOG_DOMAIN "gbp-sessionui-workbench-addin"

#include "config.h"

#include <libide-gui.h>
#include <libide-threading.h>

#include "ide-workbench-private.h"

#include "gbp-sessionui-workbench-addin.h"

struct _GbpSessionuiWorkbenchAddin
{
  GObject       parent_instance;
  IdeWorkbench *workbench;
  IdeSession   *session;
};

typedef struct
{
  GFile    *file;
  GVariant *variant;
} SaveState;

static void
save_state_free (gpointer data)
{
  SaveState *ss = data;

  g_clear_object (&ss->file);
  g_clear_pointer (&ss->variant, g_variant_unref);
  g_free (ss);
}

static SaveState *
save_state_new (GFile    *file,
                GVariant *variant)
{
  SaveState *ss;

  ss = g_new0 (SaveState, 1);
  ss->file = g_object_ref (file);
  ss->variant = g_variant_ref_sink (variant);

  return ss;
}

static void
save_state_worker (IdeTask      *task,
                   gpointer      source_object,
                   gpointer      task_data,
                   GCancellable *cancellable)
{
  SaveState *ss = task_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) directory = NULL;
  gconstpointer buffer;
  gsize length;

  IDE_ENTRY;

  g_assert (!IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SESSIONUI_WORKBENCH_ADDIN (source_object));
  g_assert (ss != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  directory = g_file_get_parent (ss->file);
  buffer = g_variant_get_data (ss->variant);
  length = g_variant_get_size (ss->variant);

  g_assert (G_IS_FILE (directory));

  if (!g_file_make_directory_with_parents (directory, cancellable, &error))
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
        {
          ide_task_return_error (task, g_steal_pointer (&error));
          IDE_EXIT;
        }
    }

  if (!g_file_replace_contents (ss->file, buffer, length, NULL, FALSE, 0, NULL, cancellable, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
gbp_sessionui_workbench_addin_load (IdeWorkbenchAddin *addin,
                                    IdeWorkbench      *workbench)
{
  GbpSessionuiWorkbenchAddin *self = (GbpSessionuiWorkbenchAddin *)addin;

  g_assert (GBP_IS_SESSIONUI_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = workbench;
}

static void
gbp_sessionui_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                      IdeWorkbench      *workbench)
{
  GbpSessionuiWorkbenchAddin *self = (GbpSessionuiWorkbenchAddin *)addin;

  g_assert (GBP_IS_SESSIONUI_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  g_clear_object (&self->session);

  self->workbench = NULL;
}

static void
gbp_sessionui_workbench_addin_save_session (IdeWorkbenchAddin *addin,
                                            IdeSession        *session)
{
  GbpSessionuiWorkbenchAddin *self = (GbpSessionuiWorkbenchAddin *)addin;

  g_assert (GBP_IS_SESSIONUI_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_SESSION (session));

  g_set_object (&self->session, session);

  /* TODO: Collect workspaces that are created (specifically secondary workspaces). */
  /* TODO: Collect pages within workspace grids */
  /* TODO: Collect panes and their position within workspace panels */
  /* TODO: Add ide_page_save_state(IdePage*, IdeSession*) */
  /* TODO: Add ide_pane_save_state(IdePage*, IdeSession*) */
  /* TODO: Add hooks to move a PanelWidget upon adding to dock */
}

static void
gbp_sessionui_workbench_addin_restore_session (IdeWorkbenchAddin *addin,
                                               IdeSession        *session)
{
  GbpSessionuiWorkbenchAddin *self = (GbpSessionuiWorkbenchAddin *)addin;

  g_assert (GBP_IS_SESSIONUI_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_SESSION (session));

}

static void
gbp_sessionui_workbench_addin_load_state_cb (GObject      *object,
                                             GAsyncResult *result,
                                             gpointer      user_data)
{
  GFile *file = (GFile *)object;
  GbpSessionuiWorkbenchAddin *self;
  g_autoptr(IdeSession) session = NULL;
  g_autoptr(GVariant) sessionv = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (G_IS_FILE (file));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);

  g_assert (GBP_IS_SESSIONUI_WORKBENCH_ADDIN (self));

  if (!(bytes = g_file_load_bytes_finish (file, result, NULL, &error)))
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
        ide_task_return_boolean (task, TRUE);
      else
        ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  if (!(sessionv = g_variant_new_from_bytes (G_VARIANT_TYPE_VARDICT, bytes, FALSE)))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVALID_DATA,
                                 "Failed to reate GVariant from state");
      IDE_EXIT;
    }

  if (!(session = ide_session_new_from_variant (sessionv, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  if (self->workbench != NULL)
    _ide_workbench_set_session (self->workbench, session);

  ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
gbp_sessionui_workbench_addin_load_project_async (IdeWorkbenchAddin   *addin,
                                                  IdeProjectInfo      *project_info,
                                                  GCancellable        *cancellable,
                                                  GAsyncReadyCallback  callback,
                                                  gpointer             user_data)
{
  GbpSessionuiWorkbenchAddin *self = (GbpSessionuiWorkbenchAddin *)addin;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GFile) file = NULL;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (GBP_IS_SESSIONUI_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_PROJECT_INFO (project_info));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (IDE_IS_WORKBENCH (self->workbench));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_sessionui_workbench_addin_load_project_async);

  context = ide_workbench_get_context (self->workbench);
  file = ide_context_cache_file (context, "session.gvariant", NULL);

  g_file_load_bytes_async (file,
                           cancellable,
                           gbp_sessionui_workbench_addin_load_state_cb,
                           g_steal_pointer (&task));

  IDE_EXIT;
}

static gboolean
gbp_sessionui_workbench_addin_load_project_finish (IdeWorkbenchAddin  *addin,
                                                   GAsyncResult       *result,
                                                   GError            **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (GBP_IS_SESSIONUI_WORKBENCH_ADDIN (addin));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
gbp_sessionui_workbench_addin_unload_project_async (IdeWorkbenchAddin   *addin,
                                                    IdeProjectInfo      *project_info,
                                                    GCancellable        *cancellable,
                                                    GAsyncReadyCallback  callback,
                                                    gpointer             user_data)
{
  GbpSessionuiWorkbenchAddin *self = (GbpSessionuiWorkbenchAddin *)addin;
  g_autoptr(GVariant) variant = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GFile) file = NULL;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (GBP_IS_SESSIONUI_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_PROJECT_INFO (project_info));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (IDE_IS_SESSION (self->session));
  g_assert (IDE_IS_WORKBENCH (self->workbench));

  context = ide_workbench_get_context (self->workbench);
  file = ide_context_cache_file (context, "session.gvariant", NULL);
  variant = ide_session_to_variant (self->session);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_sessionui_workbench_addin_unload_project_async);
  ide_task_set_task_data (task, save_state_new (file, variant), save_state_free);
  ide_task_run_in_thread (task, save_state_worker);

  IDE_EXIT;
}

static gboolean
gbp_sessionui_workbench_addin_unload_project_finish (IdeWorkbenchAddin  *addin,
                                                     GAsyncResult       *result,
                                                     GError            **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (GBP_IS_SESSIONUI_WORKBENCH_ADDIN (addin));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = gbp_sessionui_workbench_addin_load;
  iface->unload = gbp_sessionui_workbench_addin_unload;
  iface->save_session = gbp_sessionui_workbench_addin_save_session;
  iface->restore_session = gbp_sessionui_workbench_addin_restore_session;
  iface->load_project_async = gbp_sessionui_workbench_addin_load_project_async;
  iface->load_project_finish = gbp_sessionui_workbench_addin_load_project_finish;
  iface->unload_project_async = gbp_sessionui_workbench_addin_unload_project_async;
  iface->unload_project_finish = gbp_sessionui_workbench_addin_unload_project_finish;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpSessionuiWorkbenchAddin, gbp_sessionui_workbench_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN, workbench_addin_iface_init))

static void
gbp_sessionui_workbench_addin_class_init (GbpSessionuiWorkbenchAddinClass *klass)
{
}

static void
gbp_sessionui_workbench_addin_init (GbpSessionuiWorkbenchAddin *self)
{
}
