/* gbp-vagrant-runtime-provider.c
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

#define G_LOG_DOMAIN "gbp-vagrant-runtime-provider"

#include "config.h"

#include <glib/gi18n.h>

#include "gbp-vagrant-runtime.h"
#include "gbp-vagrant-runtime-provider.h"

struct _GbpVagrantRuntimeProvider
{
  IdeObject parent_instance;
};

static const gchar *cmd_vagrant_status[] = { "vagrant", "status", NULL };

static void
gbp_vagrant_runtime_provider_add (GbpVagrantRuntimeProvider *self,
                                  GbpVagrantRuntime         *runtime)
{
  g_autofree gchar *display_name = NULL;
  IdeRuntimeManager *runtime_manager;
  const gchar *provider;
  const gchar *vagrant_id;
  IdeContext *context;

  g_assert (GBP_IS_VAGRANT_RUNTIME_PROVIDER (self));
  g_assert (GBP_IS_VAGRANT_RUNTIME (runtime));

  provider = gbp_vagrant_runtime_get_provider (runtime);
  vagrant_id = gbp_vagrant_runtime_get_vagrant_id (runtime);
  display_name = g_strdup_printf ("%s %s (%s)", _("Vagrant"), vagrant_id, provider);
  ide_runtime_set_display_name (IDE_RUNTIME (runtime), display_name);

  context = ide_object_get_context (IDE_OBJECT (self));
  runtime_manager = ide_runtime_manager_from_context (context);
  ide_object_append (IDE_OBJECT (self), IDE_OBJECT (runtime));
  ide_runtime_manager_add (runtime_manager, IDE_RUNTIME (runtime));
}

static void
vagrant_status_cb (GbpVagrantRuntimeProvider *self,
                   GAsyncResult              *result,
                   gpointer                   user_data)
{
  g_autoptr(GbpVagrantTable) table = NULL;
  g_autoptr(GbpVagrantRuntime) runtime = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  GbpVagrantTableIter iter;

  g_assert (GBP_IS_VAGRANT_RUNTIME_PROVIDER (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!(table = gbp_vagrant_runtime_provider_command_finish (self, result, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  gbp_vagrant_table_iter_init (&iter, table);

  while (gbp_vagrant_table_iter_next (&iter))
    {
      g_autofree gchar *id = NULL;
      g_autofree gchar *key = NULL;
      g_autofree gchar *val1 = NULL;

      id = gbp_vagrant_table_iter_get_column (&iter, 1);
      key = gbp_vagrant_table_iter_get_column (&iter, 2);
      val1 = gbp_vagrant_table_iter_get_column (&iter, 3);

      if (ide_str_empty0 (id))
        continue;

      if (runtime != NULL)
        {
          const gchar *prev_id = gbp_vagrant_runtime_get_vagrant_id (runtime);

          if (!ide_str_equal0 (id, prev_id))
            {
              gbp_vagrant_runtime_provider_add (self, runtime);
              g_clear_object (&runtime);
            }
        }
      else
        {
          g_autofree gchar *runtime_id = g_strdup_printf ("vagrant:%s", id);

          runtime = g_object_new (GBP_TYPE_VAGRANT_RUNTIME,
                                  "id", runtime_id,
                                  "category", _("Vagrant"),
                                  "name", id,
                                  "vagrant-id", id,
                                  NULL);
        }

      if (ide_str_equal0 (key, "provider-name"))
        gbp_vagrant_runtime_set_provider (runtime, val1);
      else if (ide_str_equal0 (key, "state"))
        gbp_vagrant_runtime_set_state (runtime, val1);
    }

  if (runtime != NULL)
    {
      gbp_vagrant_runtime_provider_add (self, runtime);
      g_clear_object (&runtime);
    }

  ide_task_return_boolean (task, TRUE);
}

static void
reload_find_in_ancestors_cb (GFile        *workdir,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) vagrantfile = NULL;

  g_assert (G_IS_FILE (workdir));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!(vagrantfile = ide_g_file_find_in_ancestors_finish (result, &error)))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    gbp_vagrant_runtime_provider_command_async (ide_task_get_source_object (task),
                                                (const gchar * const *)cmd_vagrant_status,
                                                ide_task_get_cancellable (task),
                                                (GAsyncReadyCallback) vagrant_status_cb,
                                                g_object_ref (task));


}

static void
gbp_vagrant_runtime_provider_reload_async (GbpVagrantRuntimeProvider *self,
                                           GCancellable              *cancellable,
                                           GAsyncReadyCallback        callback,
                                           gpointer                   user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GFile) workdir = NULL;
  IdeContext *context;

  g_return_if_fail (GBP_IS_VAGRANT_RUNTIME_PROVIDER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_vagrant_runtime_provider_reload_async);

  context = ide_object_get_context (IDE_OBJECT (self));
  workdir = ide_context_ref_workdir (context);

  ide_g_file_find_in_ancestors_async (workdir,
                                      "Vagrantfile",
                                      ide_task_get_cancellable (task),
                                      (GAsyncReadyCallback) reload_find_in_ancestors_cb,
                                      g_object_ref (task));
}

static gboolean
gbp_vagrant_runtime_provider_reload_finish (GbpVagrantRuntimeProvider  *self,
                                            GAsyncResult               *result,
                                            GError                    **error)
{
  g_return_val_if_fail (GBP_IS_VAGRANT_RUNTIME_PROVIDER (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
load_reload_cb (GbpVagrantRuntimeProvider *self,
                GAsyncResult              *result,
                gpointer                   user_data)
{
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_TASK (task));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_VAGRANT_RUNTIME_PROVIDER (self));

  if (!gbp_vagrant_runtime_provider_reload_finish (self, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);
}

static void
check_vagrant_available_cb (IdeSubprocess *subprocess,
                            GAsyncResult  *result,
                            gpointer       user_data)
{
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_subprocess_wait_check_finish (subprocess, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    gbp_vagrant_runtime_provider_reload_async (ide_task_get_source_object (task),
                                               ide_task_get_cancellable (task),
                                               (GAsyncReadyCallback) load_reload_cb,
                                               g_object_ref (task));

}

static void
gbp_vagrant_runtime_provider_load_async (GbpVagrantRuntimeProvider *self,
                                         GCancellable              *cancellable,
                                         GAsyncReadyCallback        callback,
                                         gpointer                   user_data)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_VAGRANT_RUNTIME_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_vagrant_runtime_provider_load_async);

  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_SILENCE |
                                          G_SUBPROCESS_FLAGS_STDERR_SILENCE);
  ide_subprocess_launcher_set_cwd (launcher, g_get_home_dir ());
  ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
  ide_subprocess_launcher_push_argv (launcher, "which");
  ide_subprocess_launcher_push_argv (launcher, "vagrant");

  if (!(subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, &error)))
    ide_task_return_error (task, g_steal_pointer ((&error)));
  else
    ide_subprocess_wait_check_async (subprocess,
                                     cancellable,
                                     (GAsyncReadyCallback) check_vagrant_available_cb,
                                     g_steal_pointer (&task));

}

static void
gbp_vagrant_runtime_provider_load (IdeRuntimeProvider *provider,
                                   IdeRuntimeManager  *runtime_manager)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_VAGRANT_RUNTIME_PROVIDER (provider));
  g_assert (IDE_IS_RUNTIME_MANAGER (runtime_manager));

  gbp_vagrant_runtime_provider_load_async (GBP_VAGRANT_RUNTIME_PROVIDER (provider),
                                           NULL, NULL, NULL);
}

static void
runtime_provider_iface_init (IdeRuntimeProviderInterface *iface)
{
  iface->load = gbp_vagrant_runtime_provider_load;
}

G_DEFINE_TYPE_WITH_CODE (GbpVagrantRuntimeProvider, gbp_vagrant_runtime_provider, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_RUNTIME_PROVIDER, runtime_provider_iface_init))

static void
gbp_vagrant_runtime_provider_finalize (GObject *object)
{
  G_OBJECT_CLASS (gbp_vagrant_runtime_provider_parent_class)->finalize (object);
}

static void
gbp_vagrant_runtime_provider_class_init (GbpVagrantRuntimeProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_vagrant_runtime_provider_finalize;
}

static void
gbp_vagrant_runtime_provider_init (GbpVagrantRuntimeProvider *self)
{
}

static void
gbp_vagrant_runtime_provider_command_cb (IdeSubprocess *subprocess,
                                         GAsyncResult  *result,
                                         gpointer       user_data)
{
  g_autofree gchar *stdout_buf = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_subprocess_communicate_utf8_finish (subprocess, result, &stdout_buf, NULL, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_pointer (task,
                             gbp_vagrant_table_new_take (g_steal_pointer (&stdout_buf)),
                             gbp_vagrant_table_free);
}

void
gbp_vagrant_runtime_provider_command_async (GbpVagrantRuntimeProvider *self,
                                            const gchar * const       *command,
                                            GCancellable              *cancellable,
                                            GAsyncReadyCallback        callback,
                                            gpointer                   user_data)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) workdir = NULL;
  IdeContext *context;

  g_assert (GBP_IS_VAGRANT_RUNTIME_PROVIDER (self));
  g_assert (command != NULL && command[0] != NULL);
  g_assert (g_str_equal ("vagrant", command[0]));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_vagrant_runtime_provider_command_async);

  context = ide_object_get_context (IDE_OBJECT (self));
  workdir = ide_context_ref_workdir (context);

  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                          G_SUBPROCESS_FLAGS_STDERR_SILENCE);
  ide_subprocess_launcher_set_cwd (launcher, g_file_peek_path (workdir));

  ide_subprocess_launcher_push_args (launcher, command);
  if (!g_strv_contains (command, "--machine-readable"))
    ide_subprocess_launcher_push_argv (launcher, "--machine-readable");

  if (!(subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, &error)))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_subprocess_communicate_utf8_async (subprocess,
                                           NULL,
                                           cancellable,
                                           (GAsyncReadyCallback) gbp_vagrant_runtime_provider_command_cb,
                                           g_steal_pointer (&task));
}

/**
 * gbp_vagrant_runtime_provider_command_finish:
 *
 * Returns: (transfer full): a #GbpVagrantTable or %NULL
 */
GbpVagrantTable *
gbp_vagrant_runtime_provider_command_finish (GbpVagrantRuntimeProvider  *self,
                                             GAsyncResult               *result,
                                             GError                    **error)
{
  g_return_val_if_fail (GBP_IS_VAGRANT_RUNTIME_PROVIDER (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}
