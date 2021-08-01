/* gbp-podman-runtime-provider.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-podman-runtime-provider"

#include "config.h"

#include <libide-foundry.h>
#include <libide-threading.h>
#include <json-glib/json-glib.h>
#include <string.h>

#include "gbp-podman-runtime.h"
#include "gbp-podman-runtime-provider.h"

struct _GbpPodmanRuntimeProvider
{
  IdeObject          parent_instance;
  GCancellable      *cancellable;
  IdeRuntimeManager *manager;
  const gchar       *runtime_id;
};

static gboolean
contains_runtime (GbpPodmanRuntimeProvider *self,
                  GbpPodmanRuntime         *runtime)
{
  const char *id;
  guint n_items;

  g_assert (GBP_IS_PODMAN_RUNTIME_PROVIDER (self));
  g_assert (GBP_IS_PODMAN_RUNTIME (runtime));

  id = ide_runtime_get_id (IDE_RUNTIME (runtime));
  n_items = ide_object_get_n_children (IDE_OBJECT (self));

  for (guint i = 0; i < n_items; i++)
    {
      IdeObject *ele = ide_object_get_nth_child (IDE_OBJECT (self), i);

      if (g_strcmp0 (id, ide_runtime_get_id (IDE_RUNTIME (ele))) == 0)
        return TRUE;
    }

  return FALSE;
}

static void
gbp_podman_runtime_provider_apply_cb (JsonArray *ar,
                                      guint      index_,
                                      JsonNode  *element_node,
                                      gpointer   user_data)
{
  GbpPodmanRuntimeProvider *self = user_data;
  g_autoptr(GbpPodmanRuntime) runtime = NULL;
  JsonObject *obj;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_PODMAN_RUNTIME_PROVIDER (self));

  if (self->manager == NULL)
    return;

  if (!JSON_NODE_HOLDS_OBJECT (element_node) ||
      !(obj = json_node_get_object (element_node)))
    return;

  if ((runtime = gbp_podman_runtime_new (obj)))
    {
      if (!contains_runtime (self, runtime))
        {
          ide_object_append (IDE_OBJECT (self), IDE_OBJECT (runtime));
          ide_runtime_manager_add (self->manager, IDE_RUNTIME (runtime));
        }
    }

  IDE_EXIT;
}

static gboolean
gbp_podman_runtime_provider_apply (GbpPodmanRuntimeProvider  *self,
                                   const gchar               *json_string,
                                   GError                   **error)
{
  g_autoptr(JsonParser) parser = NULL;
  JsonArray *ar;
  JsonNode *root;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_PODMAN_RUNTIME_PROVIDER (self));
  g_assert (json_string != NULL);

  parser = json_parser_new ();

  if (!json_parser_load_from_data (parser, json_string, -1, error))
    IDE_RETURN(FALSE);

  if (!(root = json_parser_get_root (parser)) ||
      !JSON_NODE_HOLDS_ARRAY (root) ||
      !(ar = json_node_get_array (root)))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "Expected [] for root JSON node");
      IDE_RETURN(FALSE);
    }

  json_array_foreach_element (ar,
                              gbp_podman_runtime_provider_apply_cb,
                              self);

  IDE_RETURN(TRUE);
}

static void
gbp_podman_runtime_provider_load_communicate_cb (GObject      *object,
                                                 GAsyncResult *result,
                                                 gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  GbpPodmanRuntimeProvider *self;
  g_autofree gchar *stdout_buf = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);

  if (!ide_subprocess_communicate_utf8_finish (subprocess, result, &stdout_buf, NULL, &error) ||
      !gbp_podman_runtime_provider_apply (self, stdout_buf, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static gboolean
gbp_podman_runtime_provider_has_preserve_fds (GbpPodmanRuntimeProvider  *self,
                                              const gchar               *stdout_buf,
                                              GError                   **error)
{
  IDE_ENTRY;

  g_assert (GBP_IS_PODMAN_RUNTIME_PROVIDER (self));
  g_assert (stdout_buf != NULL);

  if (strstr (stdout_buf, "--preserve-fds") == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_SUPPORTED,
                   "Podman is not supported because it lacks support for --preserve-fds");
      return FALSE;
    }

  IDE_RETURN(TRUE);
}

static void
gbp_podman_runtime_provider_load_sniff_cb (GObject      *object,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  GbpPodmanRuntimeProvider *self;
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autofree gchar *stdout_buf = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  GCancellable *cancellable;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  cancellable = ide_task_get_cancellable (task);

  if (!ide_subprocess_communicate_utf8_finish (subprocess, result, &stdout_buf, NULL, &error) ||
      !gbp_podman_runtime_provider_has_preserve_fds (self, stdout_buf, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE);
  ide_subprocess_launcher_set_cwd (launcher, g_get_home_dir ());
  ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
  ide_subprocess_launcher_push_argv (launcher, "podman");
  ide_subprocess_launcher_push_argv (launcher, "ps");
  ide_subprocess_launcher_push_argv (launcher, "--all");
  ide_subprocess_launcher_push_argv (launcher, "--format=json");

  if (!(subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, &error)))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_subprocess_communicate_utf8_async (subprocess,
                                           NULL,
                                           cancellable,
                                           gbp_podman_runtime_provider_load_communicate_cb,
                                           g_steal_pointer (&task));

  IDE_EXIT;
}

static void
gbp_podman_runtime_provider_load_async (GbpPodmanRuntimeProvider *self,
                                        GCancellable             *cancellable,
                                        GAsyncReadyCallback       callback,
                                        gpointer                  user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_PODMAN_RUNTIME_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_podman_runtime_provider_load_async);

  /* First make sure that "podman exec --preserve-fds" is supported */

  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE);
  ide_subprocess_launcher_set_cwd (launcher, g_get_home_dir ());
  ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
  ide_subprocess_launcher_push_argv (launcher, "podman");
  ide_subprocess_launcher_push_argv (launcher, "exec");
  ide_subprocess_launcher_push_argv (launcher, "--help");

  if (!(subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, &error)))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_subprocess_communicate_utf8_async (subprocess,
                                           NULL,
                                           cancellable,
                                           gbp_podman_runtime_provider_load_sniff_cb,
                                           g_steal_pointer (&task));

  IDE_EXIT;
}

static void
gbp_podman_runtime_provider_load (IdeRuntimeProvider *provider,
                                  IdeRuntimeManager  *manager)
{
  GbpPodmanRuntimeProvider *self = (GbpPodmanRuntimeProvider *)provider;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_PODMAN_RUNTIME_PROVIDER (self));
  g_assert (IDE_IS_RUNTIME_MANAGER (manager));

  self->cancellable = g_cancellable_new ();
  self->manager = manager;

  /**
   * We attempt to initialize the podman provider asynchronously even if podman
   * is not configured as the runtime provider in the build configuration. This is
   * to make sure we show available runtimes in the configuration surface.
   *
   * If podman is selected as the provider for the runtime used in the build
   * configuration the _provides method will ensure that the runtime is loaded
   * before the pipeline is marked as active.
   *
   * TODO: we can probably let the bootstrap process continue with this async load
   *       when that load is not done yet before the pipeline wants us to be ready.
   */
  gbp_podman_runtime_provider_load_async (self,
                                          self->cancellable,
                                          NULL, NULL);

  IDE_EXIT;
}

static void
gbp_podman_runtime_provider_unload (IdeRuntimeProvider *provider,
                                    IdeRuntimeManager  *manager)
{
  GbpPodmanRuntimeProvider *self = (GbpPodmanRuntimeProvider *)provider;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_PODMAN_RUNTIME_PROVIDER (self));
  g_assert (IDE_IS_RUNTIME_MANAGER (manager));

  self->manager = NULL;

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  IDE_EXIT;
}

static gboolean
gbp_podman_runtime_provider_provides (IdeRuntimeProvider *provider,
                                      const gchar        *runtime_id)
{
  IDE_ENTRY;

  g_assert (GBP_IS_PODMAN_RUNTIME_PROVIDER (provider));
  g_assert (runtime_id != NULL);

  IDE_RETURN (g_str_has_prefix (runtime_id, "podman:"));
}

static void
gbp_podman_runtime_provider_bootstrap_complete (GObject      *object,
                                                GAsyncResult *result,
                                                gpointer      user_data)
{
  GbpPodmanRuntimeProvider *self = (GbpPodmanRuntimeProvider *)object;
  g_autoptr(IdeTask) task = user_data;
  IdeContext *context;
  IdeRuntime *runtime;
  IdeRuntimeManager *runtime_manager;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));
  g_assert (GBP_IS_PODMAN_RUNTIME_PROVIDER (self));

  context = ide_object_get_context (IDE_OBJECT (self));
  runtime_manager = ide_runtime_manager_from_context (context);

  runtime = ide_runtime_manager_get_runtime (runtime_manager, self->runtime_id);

  if (runtime != NULL)
    ide_task_return_pointer (task, g_object_ref (runtime), g_object_unref);
  else
    ide_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "Failed to initialize runtime for build");
}

static void
gbp_podman_runtime_provider_bootstrap_async (IdeRuntimeProvider  *provider,
                                              IdePipeline         *pipeline,
                                              GCancellable        *cancellable,
                                              GAsyncReadyCallback  callback,
                                              gpointer             user_data)
{
  GbpPodmanRuntimeProvider *self = (GbpPodmanRuntimeProvider *)provider;
  g_autoptr(IdeTask) task = NULL;
  const gchar *runtime_id;
  IdeConfig *config;

  IDE_ENTRY;

  g_assert (GBP_IS_PODMAN_RUNTIME_PROVIDER (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_release_on_propagate (task, FALSE);
  ide_task_set_source_tag (task, gbp_podman_runtime_provider_bootstrap_async);

  config = ide_pipeline_get_config (pipeline);
  runtime_id = ide_config_get_runtime_id (config);

  if (runtime_id == NULL ||
      !g_str_has_prefix (runtime_id, "podman:"))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_FOUND,
                                 "No runtime available");
      IDE_EXIT;
    }

  self->runtime_id = runtime_id;

  gbp_podman_runtime_provider_load_async (self,
                                          self->cancellable,
                                          gbp_podman_runtime_provider_bootstrap_complete,
                                          g_object_ref (task));

  IDE_EXIT;

}

static IdeRuntime *
gbp_podman_runtime_provider_bootstrap_finish (IdeRuntimeProvider  *provider,
                                               GAsyncResult        *result,
                                               GError             **error)
{
  IdeRuntime *ret;

  IDE_ENTRY;

  g_assert (GBP_IS_PODMAN_RUNTIME_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
runtime_provider_iface_init (IdeRuntimeProviderInterface *iface)
{
  iface->load = gbp_podman_runtime_provider_load;
  iface->unload = gbp_podman_runtime_provider_unload;
  iface->provides = gbp_podman_runtime_provider_provides;
  iface->bootstrap_async = gbp_podman_runtime_provider_bootstrap_async;
  iface->bootstrap_finish = gbp_podman_runtime_provider_bootstrap_finish;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpPodmanRuntimeProvider, gbp_podman_runtime_provider, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_RUNTIME_PROVIDER, runtime_provider_iface_init))

static void
gbp_podman_runtime_provider_class_init (GbpPodmanRuntimeProviderClass *klass)
{
}

static void
gbp_podman_runtime_provider_init (GbpPodmanRuntimeProvider *self)
{
}
