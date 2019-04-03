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
};

static void
gbp_podman_runtime_provider_apply_cb (JsonArray *ar,
                                      guint      index_,
                                      JsonNode  *element_node,
                                      gpointer   user_data)
{
  GbpPodmanRuntimeProvider *self = user_data;
  g_autoptr(GbpPodmanRuntime) runtime = NULL;
  JsonObject *obj;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_PODMAN_RUNTIME_PROVIDER (self));

  if (self->manager == NULL)
    return;

  if (!JSON_NODE_HOLDS_OBJECT (element_node) ||
      !(obj = json_node_get_object (element_node)))
    return;

  if ((runtime = gbp_podman_runtime_new (obj)))
    {
      ide_object_append (IDE_OBJECT (self), IDE_OBJECT (runtime));
      ide_runtime_manager_add (self->manager, IDE_RUNTIME (runtime));
    }
}

static gboolean
gbp_podman_runtime_provider_apply (GbpPodmanRuntimeProvider  *self,
                                   const gchar               *json_string,
                                   GError                   **error)
{
  g_autoptr(JsonParser) parser = NULL;
  JsonArray *ar;
  JsonNode *root;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_PODMAN_RUNTIME_PROVIDER (self));
  g_assert (json_string != NULL);

  parser = json_parser_new ();

  if (!json_parser_load_from_data (parser, json_string, -1, error))
    return FALSE;

  if (!(root = json_parser_get_root (parser)) ||
      !JSON_NODE_HOLDS_ARRAY (root) ||
      !(ar = json_node_get_array (root)))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "Expected [] for root JSON node");
      return FALSE;
    }

  json_array_foreach_element (ar,
                              gbp_podman_runtime_provider_apply_cb,
                              self);

  return TRUE;
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
}

static gboolean
gbp_podman_runtime_provider_has_preserve_fds (GbpPodmanRuntimeProvider  *self,
                                              const gchar               *stdout_buf,
                                              GError                   **error)
{
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

  return TRUE;
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
}

static void
gbp_podman_runtime_provider_load (IdeRuntimeProvider *provider,
                                  IdeRuntimeManager  *manager)
{
  GbpPodmanRuntimeProvider *self = (GbpPodmanRuntimeProvider *)provider;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_PODMAN_RUNTIME_PROVIDER (self));
  g_assert (IDE_IS_RUNTIME_MANAGER (manager));

  self->cancellable = g_cancellable_new ();
  self->manager = manager;

  gbp_podman_runtime_provider_load_async (self, self->cancellable, NULL, NULL);
}

static void
gbp_podman_runtime_provider_unload (IdeRuntimeProvider *provider,
                                    IdeRuntimeManager  *manager)
{
  GbpPodmanRuntimeProvider *self = (GbpPodmanRuntimeProvider *)provider;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_PODMAN_RUNTIME_PROVIDER (self));
  g_assert (IDE_IS_RUNTIME_MANAGER (manager));

  self->manager = NULL;

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
}

static void
runtime_provider_iface_init (IdeRuntimeProviderInterface *iface)
{
  iface->load = gbp_podman_runtime_provider_load;
  iface->unload = gbp_podman_runtime_provider_unload;
}

G_DEFINE_TYPE_WITH_CODE (GbpPodmanRuntimeProvider, gbp_podman_runtime_provider, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_RUNTIME_PROVIDER, runtime_provider_iface_init))

static void
gbp_podman_runtime_provider_class_init (GbpPodmanRuntimeProviderClass *klass)
{
}

static void
gbp_podman_runtime_provider_init (GbpPodmanRuntimeProvider *self)
{
}
