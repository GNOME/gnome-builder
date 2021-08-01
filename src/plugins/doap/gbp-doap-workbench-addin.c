/* gbp-doap-workbench-addin.c
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

#define G_LOG_DOMAIN "gbp-doap-workbench-addin"

#include "config.h"

#include <libide-gui.h>
#include <libide-projects.h>
#include <libide-threading.h>

#include "gbp-doap-workbench-addin.h"

struct _GbpDoapWorkbenchAddin
{
  GObject     parent_instance;
  IdeContext *context;
};

static void
gbp_doap_workbench_addin_find_doap_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  GFile *file = (GFile *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GPtrArray) found = NULL;
  g_autoptr(GError) error = NULL;
  IdeProjectInfo *project_info;
  GCancellable *cancellable;
  GbpDoapWorkbenchAddin *self;

  g_assert (G_IS_FILE (file));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!(found = ide_g_file_find_finish (file, result, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  IDE_PTR_ARRAY_SET_FREE_FUNC (found, g_object_unref);

  self = ide_task_get_source_object (task);
  cancellable = ide_task_get_cancellable (task);
  project_info = ide_task_get_task_data (task);

  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (IDE_IS_PROJECT_INFO (project_info));

  for (guint i = 0; i < found->len; i++)
    {
      GFile *doap_file = g_ptr_array_index (found, 0);
      g_autoptr(IdeDoap) doap = ide_doap_new ();

      g_assert (G_IS_FILE (doap_file));

      g_debug ("Trying doap file %s for project information",
               g_file_peek_path (doap_file));

      if (ide_doap_load_from_file (doap, doap_file, cancellable, NULL))
        {
          const gchar *name = ide_doap_get_name (doap);

          if (!ide_str_empty0 (name))
            {
              ide_project_info_set_name (project_info, name);
              ide_context_set_title (self->context, name);
            }

          ide_project_info_set_doap (project_info, doap);

          break;
        }
    }

  ide_task_return_boolean (task, TRUE);
}

static void
gbp_doap_workbench_addin_load_project_async (IdeWorkbenchAddin   *addin,
                                             IdeProjectInfo      *project_info,
                                             GCancellable        *cancellable,
                                             GAsyncReadyCallback  callback,
                                             gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  GFile *directory;

  g_assert (GBP_IS_DOAP_WORKBENCH_ADDIN (addin));
  g_assert (IDE_IS_PROJECT_INFO (project_info));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (addin, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_doap_workbench_addin_load_project_async);
  ide_task_set_task_data (task, g_object_ref (project_info), g_object_unref);

  if (!(directory = ide_project_info_get_directory (project_info)))
    {
      ide_task_return_boolean (task, TRUE);
      return;
    }

  ide_g_file_find_with_depth_async (directory,
                                    "*.doap",
                                    1,
                                    cancellable,
                                    gbp_doap_workbench_addin_find_doap_cb,
                                    g_steal_pointer (&task));
}

static gboolean
gbp_doap_workbench_addin_load_project_finish (IdeWorkbenchAddin  *addin,
                                              GAsyncResult       *result,
                                              GError            **error)
{
  g_assert (GBP_IS_DOAP_WORKBENCH_ADDIN (addin));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
gbp_doap_workbench_addin_load (IdeWorkbenchAddin *addin,
                               IdeWorkbench      *workbench)
{
  GBP_DOAP_WORKBENCH_ADDIN (addin)->context = ide_workbench_get_context (workbench);
}

static void
gbp_doap_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                 IdeWorkbench      *workbench)
{
  GBP_DOAP_WORKBENCH_ADDIN (addin)->context = NULL;
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load_project_async = gbp_doap_workbench_addin_load_project_async;
  iface->load_project_finish = gbp_doap_workbench_addin_load_project_finish;
  iface->load = gbp_doap_workbench_addin_load;
  iface->unload = gbp_doap_workbench_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpDoapWorkbenchAddin, gbp_doap_workbench_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN,
                                                workbench_addin_iface_init))

static void
gbp_doap_workbench_addin_class_init (GbpDoapWorkbenchAddinClass *klass)
{
}

static void
gbp_doap_workbench_addin_init (GbpDoapWorkbenchAddin *self)
{
}
