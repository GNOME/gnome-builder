/* gbp-buildsystem-workbench-addin.c
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

#define G_LOG_DOMAIN "gbp-buildsystem-workbench-addin"

#include "config.h"

#include <libide-gui.h>
#include <libide-foundry.h>
#include <libide-plugins.h>
#include <libide-threading.h>
#include <libpeas.h>

#include "gbp-buildsystem-workbench-addin.h"

struct _GbpBuildsystemWorkbenchAddin
{
  GObject       parent_instance;
  IdeWorkbench *workbench;
};

typedef struct
{
  IdeExtensionSetAdapter *set;
  GFile                  *directory;
  const gchar            *best_match;
  gint                    best_match_priority;
  guint                   n_active;
} Discovery;

static void
discovery_free (Discovery *state)
{
  g_assert (state);
  g_assert (state->n_active == 0);

  g_clear_object (&state->directory);
  ide_clear_and_destroy_object (&state->set);
  g_slice_free (Discovery, state);
}

static void
discovery_foreach_cb (IdeExtensionSetAdapter *set,
                      PeasPluginInfo         *plugin_info,
                      GObject          *exten,
                      gpointer                user_data)
{
  IdeBuildSystemDiscovery *addin = (IdeBuildSystemDiscovery *)exten;
  Discovery *state = user_data;
  g_autofree gchar *ret = NULL;
  gint priority = 0;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_BUILD_SYSTEM_DISCOVERY (addin));
  g_assert (state != NULL);

  if ((ret = ide_build_system_discovery_discover (addin,
                                                  state->directory,
                                                  NULL,
                                                  &priority,
                                                  NULL)))
    {
      if (priority < state->best_match_priority || state->best_match == NULL)
        {
          state->best_match = g_intern_string (ret);
          state->best_match_priority = priority;
        }
    }
}

static void
discovery_worker (IdeTask      *task,
                  gpointer      source_object,
                  gpointer      task_data,
                  GCancellable *cancellable)
{
  Discovery *state = task_data;

  g_assert (IDE_IS_TASK (task));
  g_assert (GBP_IS_BUILDSYSTEM_WORKBENCH_ADDIN (source_object));
  g_assert (state != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  ide_extension_set_adapter_foreach (state->set, discovery_foreach_cb, state);

  if (state->best_match != NULL)
    (ide_task_return_pointer) (task, (gpointer)state->best_match, NULL);
  else
    ide_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               "Failed to discover a build system");
}

static void
discover_async (GbpBuildsystemWorkbenchAddin *self,
                GFile                        *directory,
                const gchar                  *hint,
                GCancellable                 *cancellable,
                GAsyncReadyCallback           callback,
                gpointer                      user_data)
{
  g_autoptr(IdeTask) task = NULL;
  IdeContext *context;
  Discovery *state;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_BUILDSYSTEM_WORKBENCH_ADDIN (self));
  g_assert (G_IS_FILE (directory));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (IDE_IS_WORKBENCH (self->workbench));

  context = ide_workbench_get_context (self->workbench);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, discover_async);

  state = g_slice_new0 (Discovery);
  state->directory = g_file_dup (directory);
  state->set = ide_extension_set_adapter_new (IDE_OBJECT (context),
                                              peas_engine_get_default (),
                                              IDE_TYPE_BUILD_SYSTEM_DISCOVERY,
                                              NULL, NULL);

  /* If we have a hint here, we want to lock in this build system
   * instead of allowing another to override it. So in that case,
   * raise the priority (by lowering the value).
   */
  if (hint != NULL)
    {
      state->best_match = g_intern_string (hint);
      state->best_match_priority = G_MININT;
    }

  ide_task_set_task_data (task, state, discovery_free);
  ide_task_run_in_thread (task, discovery_worker);
}

static const gchar *
discover_finish (GbpBuildsystemWorkbenchAddin  *self,
                 GAsyncResult                  *result,
                 GError                       **error)
{
  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static void
discover_cb (GObject      *object,
             GAsyncResult *result,
             gpointer      user_data)
{
  GbpBuildsystemWorkbenchAddin *self = (GbpBuildsystemWorkbenchAddin *)object;
  g_autoptr(IdeBuildSystem) build_system = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  PeasPluginInfo *plugin_info;
  IdeProjectInfo *project_info;
  const gchar *plugin_name;
  PeasEngine *engine;
  GFile *file;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_BUILDSYSTEM_WORKBENCH_ADDIN (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!(plugin_name = discover_finish (self, result, &error)))
    {
      ide_task_return_boolean (task, TRUE);
      return;
    }

  if (self->workbench == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_CANCELLED,
                                 "Workbench was destroyed");
      return;
    }

  engine = peas_engine_get_default ();

  if (!(plugin_info = peas_engine_get_plugin_info (engine, plugin_name)) ||
      !peas_engine_provides_extension (engine, plugin_info, IDE_TYPE_BUILD_SYSTEM))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_FOUND,
                                 "Failed to locate build system plugin %s",
                                 plugin_name);
      return;
    }

  project_info = ide_task_get_task_data (task);
  g_assert (IDE_IS_PROJECT_INFO (project_info));

  file = ide_project_info_get_file (project_info);
  g_assert (G_IS_FILE (file));

  build_system = (IdeBuildSystem *)
    peas_engine_create_extension (engine,
                                  plugin_info,
                                  IDE_TYPE_BUILD_SYSTEM,
                                  "project-file", file,
                                  NULL);

  ide_workbench_set_build_system (self->workbench, build_system);

  if (G_IS_ASYNC_INITABLE (build_system))
    g_async_initable_init_async (G_ASYNC_INITABLE (build_system),
                                 G_PRIORITY_DEFAULT,
                                 NULL, NULL, NULL);
  else if (G_IS_INITABLE (build_system))
    g_initable_init (G_INITABLE (build_system), NULL, NULL);

  ide_task_return_boolean (task, TRUE);
}

static void
gbp_buildsystem_workbench_addin_load_project_async (IdeWorkbenchAddin   *addin,
                                                    IdeProjectInfo      *project_info,
                                                    GCancellable        *cancellable,
                                                    GAsyncReadyCallback  callback,
                                                    gpointer             user_data)
{
  GbpBuildsystemWorkbenchAddin *self = (GbpBuildsystemWorkbenchAddin *)addin;
  g_autoptr(IdeTask) task = NULL;
  const gchar *hint;
  GFile *directory;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_BUILDSYSTEM_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_PROJECT_INFO (project_info));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (IDE_IS_WORKBENCH (self->workbench));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_buildsystem_workbench_addin_load_project_async);
  ide_task_set_task_data (task, g_object_ref (project_info), g_object_unref);

  directory = ide_project_info_get_directory (project_info);
  g_assert (G_IS_FILE (directory));

  /* Get the hint, but ignore if it is "greeter" */
  hint = ide_project_info_get_build_system_hint (project_info);
  if (ide_str_equal0 (hint, "greeter"))
    hint = NULL;

  discover_async (self,
                  directory,
                  hint,
                  cancellable,
                  discover_cb,
                  g_steal_pointer (&task));
}

static gboolean
gbp_buildsystem_workbench_addin_load_project_finish (IdeWorkbenchAddin  *addin,
                                                     GAsyncResult       *result,
                                                     GError            **error)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_BUILDSYSTEM_WORKBENCH_ADDIN (addin));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
gbp_buildsystem_workbench_addin_load (IdeWorkbenchAddin *addin,
                                      IdeWorkbench      *workbench)
{
  GBP_BUILDSYSTEM_WORKBENCH_ADDIN (addin)->workbench = workbench;
}

static void
gbp_buildsystem_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                        IdeWorkbench      *workbench)
{
  GBP_BUILDSYSTEM_WORKBENCH_ADDIN (addin)->workbench = NULL;
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = gbp_buildsystem_workbench_addin_load;
  iface->unload = gbp_buildsystem_workbench_addin_unload;
  iface->load_project_async = gbp_buildsystem_workbench_addin_load_project_async;
  iface->load_project_finish = gbp_buildsystem_workbench_addin_load_project_finish;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpBuildsystemWorkbenchAddin,
                         gbp_buildsystem_workbench_addin,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN,
                                                workbench_addin_iface_init))

static void
gbp_buildsystem_workbench_addin_class_init (GbpBuildsystemWorkbenchAddinClass *klass)
{
}

static void
gbp_buildsystem_workbench_addin_init (GbpBuildsystemWorkbenchAddin *self)
{
}
