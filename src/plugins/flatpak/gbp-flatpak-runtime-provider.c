/* gbp-flatpak-runtime-provider.c
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-flatpak-runtime-provider"

#include <flatpak.h>
#include <ostree.h>
#include <string.h>

#include <libide-gui.h>

#include "ide-gui-private.h"

#include "gbp-flatpak-application-addin.h"
#include "gbp-flatpak-install-dialog.h"
#include "gbp-flatpak-manifest.h"
#include "gbp-flatpak-runtime.h"
#include "gbp-flatpak-runtime-provider.h"
#include "gbp-flatpak-transfer.h"
#include "gbp-flatpak-util.h"

typedef struct
{
  gchar *id;
  gchar *arch;
  gchar *branch;
  gchar *sdk_id;
  gchar *sdk_arch;
  gchar *sdk_branch;
  guint  op_count : 2;
  guint  failed : 1;
} InstallRuntime;

typedef struct
{
  IdeConfig *config;
  gchar     *runtime_id;
  gchar     *name;
  gchar     *arch;
  gchar     *branch;
  gint       count;
} BootstrapState;

struct _GbpFlatpakRuntimeProvider
{
  IdeObject          parent_instance;
  IdeRuntimeManager *manager;
  GPtrArray         *runtimes;
};

static void runtime_provider_iface_init         (IdeRuntimeProviderInterface *iface);
static void gbp_flatpak_runtime_provider_load   (IdeRuntimeProvider          *provider,
                                                 IdeRuntimeManager           *manager);
static void gbp_flatpak_runtime_provider_unload (IdeRuntimeProvider          *provider,
                                                 IdeRuntimeManager           *manager);

G_DEFINE_TYPE_WITH_CODE (GbpFlatpakRuntimeProvider, gbp_flatpak_runtime_provider, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_RUNTIME_PROVIDER, runtime_provider_iface_init))

static void
bootstrap_state_free (gpointer data)
{
  BootstrapState *state = data;

  g_assert (state != NULL);

  g_clear_object (&state->config);
  g_clear_pointer (&state->runtime_id, g_free);
  g_clear_pointer (&state->name, g_free);
  g_clear_pointer (&state->arch, g_free);
  g_clear_pointer (&state->branch, g_free);
  g_slice_free (BootstrapState, state);
}

static void
install_runtime_free (gpointer data)
{
  InstallRuntime *install = data;

  g_clear_pointer (&install->id, g_free);
  g_clear_pointer (&install->arch, g_free);
  g_clear_pointer (&install->branch, g_free);
  g_clear_pointer (&install->sdk_id, g_free);
  g_clear_pointer (&install->sdk_arch, g_free);
  g_clear_pointer (&install->sdk_branch, g_free);

  g_slice_free (InstallRuntime, install);
}

static gboolean
is_same_runtime (GbpFlatpakRuntime   *runtime,
                 FlatpakInstalledRef *ref)
{
  g_autofree gchar *arch = NULL;

  return (g_strcmp0 (flatpak_ref_get_name (FLATPAK_REF (ref)),
                     gbp_flatpak_runtime_get_platform (runtime)) == 0) &&
         (g_strcmp0 (flatpak_ref_get_arch (FLATPAK_REF (ref)),
                     arch = ide_runtime_get_arch (IDE_RUNTIME (runtime))) == 0) &&
         (g_strcmp0 (flatpak_ref_get_branch (FLATPAK_REF (ref)),
                     gbp_flatpak_runtime_get_branch (runtime)) == 0);
}

static void
monitor_transfer (GbpFlatpakRuntimeProvider *self,
                  GbpFlatpakTransfer        *transfer)
{
  g_autoptr(IdeNotification) notif = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (self));
  g_assert (GBP_IS_FLATPAK_TRANSFER (transfer));

  notif = ide_transfer_create_notification (IDE_TRANSFER (transfer));
  ide_notification_attach (notif, IDE_OBJECT (self));
}

static void
runtime_added_cb (GbpFlatpakRuntimeProvider  *self,
                  FlatpakInstalledRef        *ref,
                  GbpFlatpakApplicationAddin *app_addin)
{
  g_autoptr(GbpFlatpakRuntime) new_runtime = NULL;
  g_autoptr(GError) error = NULL;
  const gchar *name;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_APPLICATION_ADDIN (app_addin));
  g_assert (FLATPAK_IS_INSTALLED_REF (ref));
  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (self));
  g_assert (IDE_IS_RUNTIME_MANAGER (self->manager));

  name = flatpak_ref_get_name (FLATPAK_REF (ref));
  if (gbp_flatpak_is_ignored (name))
    IDE_EXIT;

  for (guint i = 0; i < self->runtimes->len; i++)
    {
      GbpFlatpakRuntime *runtime = g_ptr_array_index (self->runtimes, i);

      /*
       * If this is the same as a previous runtime, there is no sense in
       * doing anything about this because our runtime objects don't hold
       * anything as private state that would matter.
       */
      if (is_same_runtime (runtime, ref))
        IDE_EXIT;
    }

  /*
   * We didn't already have this runtime, so go ahead and just
   * add it now (and keep a copy so we can find it later).
   */
  new_runtime = gbp_flatpak_runtime_new (ref, FALSE, NULL, &error);

  if (new_runtime == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED))
        g_warning ("Failed to create GbpFlatpakRuntime: %s", error->message);
    }
  else
    {
      ide_object_append (IDE_OBJECT (self), IDE_OBJECT (new_runtime));
      ide_runtime_manager_add (self->manager, IDE_RUNTIME (new_runtime));
      g_ptr_array_add (self->runtimes, g_steal_pointer (&new_runtime));
    }

  IDE_EXIT;
}

static void
gbp_flatpak_runtime_provider_load (IdeRuntimeProvider *provider,
                                   IdeRuntimeManager  *manager)
{
  GbpFlatpakRuntimeProvider *self = (GbpFlatpakRuntimeProvider *)provider;
  GbpFlatpakApplicationAddin *app_addin = gbp_flatpak_application_addin_get_default ();
  g_autoptr(GPtrArray) refs = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (self));
  g_assert (IDE_IS_RUNTIME_MANAGER (manager));

  g_set_weak_pointer (&self->manager, manager);
  self->runtimes = g_ptr_array_new_with_free_func (g_object_unref);
  refs = gbp_flatpak_application_addin_get_runtimes (app_addin);

  g_signal_connect_object (app_addin,
                           "runtime-added",
                           G_CALLBACK (runtime_added_cb),
                           self,
                           G_CONNECT_SWAPPED);

  for (guint i = 0; i < refs->len; i++)
    {
      FlatpakInstalledRef *ref = g_ptr_array_index (refs, i);
      runtime_added_cb (self, ref, app_addin);
    }

  IDE_EXIT;
}

static void
gbp_flatpak_runtime_provider_unload (IdeRuntimeProvider *provider,
                                     IdeRuntimeManager  *manager)
{
  GbpFlatpakRuntimeProvider *self = (GbpFlatpakRuntimeProvider *)provider;
  GbpFlatpakApplicationAddin *app_addin = gbp_flatpak_application_addin_get_default ();

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (self));
  g_assert (IDE_IS_RUNTIME_MANAGER (manager));

  if (app_addin != NULL)
    g_signal_handlers_disconnect_by_func (app_addin,
                                          G_CALLBACK (runtime_added_cb),
                                          self);

  if (self->runtimes != NULL)
    {
      for (guint i= 0; i < self->runtimes->len; i++)
        {
          IdeRuntime *runtime = g_ptr_array_index (self->runtimes, i);

          ide_runtime_manager_remove (manager, runtime);
        }
    }

  g_clear_pointer (&self->runtimes, g_ptr_array_unref);

  g_clear_weak_pointer (&self->manager);

  IDE_EXIT;
}

static void
gbp_flatpak_runtime_provider_class_init (GbpFlatpakRuntimeProviderClass *klass)
{
}

static void
gbp_flatpak_runtime_provider_init (GbpFlatpakRuntimeProvider *self)
{
}

static gboolean
gbp_flatpak_runtime_provider_can_install (IdeRuntimeProvider *provider,
                                          const gchar        *runtime_id)
{
  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (provider));
  g_assert (runtime_id != NULL);

  return g_str_has_prefix (runtime_id, "flatpak:");
}

static void
gbp_flatpak_runtime_provider_install_cb (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
  IdeTransferManager *transfer_manager = (IdeTransferManager *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  InstallRuntime *install;

  IDE_ENTRY;

  g_assert (IDE_IS_TRANSFER_MANAGER (transfer_manager));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  install = ide_task_get_task_data (task);

  if (!ide_transfer_manager_execute_finish (transfer_manager, result, &error))
    {
      if (!install->failed)
        {
          ide_task_return_error (task, g_steal_pointer (&error));
          install->failed = TRUE;
        }
    }

  install->op_count--;

  if (install->op_count == 0 && !install->failed)
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
gbp_flatpak_runtime_provider_install_docs_cb (GObject      *object,
                                              GAsyncResult *result,
                                              gpointer      user_data)
{
  IdeTransferManager *transfer_manager = (IdeTransferManager *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  InstallRuntime *install;

  IDE_ENTRY;

  g_assert (IDE_IS_TRANSFER_MANAGER (transfer_manager));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  install = ide_task_get_task_data (task);

  /* This error is not fatal */
  if (!ide_transfer_manager_execute_finish (transfer_manager, result, &error))
    g_warning ("Failed to install docs: %s", error->message);

  install->op_count--;

  if (install->op_count == 0 && !install->failed)
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
gbp_flatpak_runtime_provider_locate_sdk_cb (GObject      *object,
                                            GAsyncResult *result,
                                            gpointer      user_data)
{
  GbpFlatpakApplicationAddin *app_addin = (GbpFlatpakApplicationAddin *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *docs_id = NULL;
  GbpFlatpakRuntimeProvider *self;
  IdeTransferManager *transfer_manager;
  InstallRuntime *install;
  GCancellable *cancellable;
  gboolean sdk_matches_runtime = FALSE;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_APPLICATION_ADDIN (app_addin));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));
  g_assert (!ide_task_get_completed (task));

  self = ide_task_get_source_object (task);
  install = ide_task_get_task_data (task);
  cancellable = ide_task_get_cancellable (task);

  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (self));
  g_assert (install != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  transfer_manager = ide_transfer_manager_get_default ();

  if (!gbp_flatpak_application_addin_locate_sdk_finish (app_addin,
                                                        result,
                                                        &install->sdk_id,
                                                        &install->sdk_arch,
                                                        &install->sdk_branch,
                                                        &error))
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
        {
          ide_task_return_error (task, g_steal_pointer (&error));
          IDE_EXIT;
        }
    }

  install->op_count = 3;

  /* Make sure the Platform runtime is installed */
  if (gbp_flatpak_application_addin_has_runtime (app_addin,
                                                 install->id,
                                                 install->arch,
                                                 install->branch))
    install->op_count--;
  else
    {
      g_autoptr(GbpFlatpakTransfer) transfer = NULL;

      transfer = gbp_flatpak_transfer_new (install->id,
                                           install->arch,
                                           install->branch,
                                           FALSE);
      monitor_transfer (self, transfer);
      ide_transfer_manager_execute_async (transfer_manager,
                                          IDE_TRANSFER (transfer),
                                          cancellable,
                                          gbp_flatpak_runtime_provider_install_cb,
                                          g_object_ref (task));
    }

  /* Now make sure the SDK is installed */
  sdk_matches_runtime = (g_strcmp0 (install->sdk_id, install->id) == 0 &&
                         g_strcmp0 (install->sdk_arch, install->arch) == 0 &&
                         g_strcmp0 (install->sdk_branch, install->branch) == 0);
  if (sdk_matches_runtime || install->sdk_id == NULL ||
      gbp_flatpak_application_addin_has_runtime (app_addin,
                                                 install->sdk_id,
                                                 install->sdk_arch,
                                                 install->sdk_branch))
    install->op_count--;
  else
    {
      g_autoptr(GbpFlatpakTransfer) transfer = NULL;

      transfer = gbp_flatpak_transfer_new (install->sdk_id,
                                           install->sdk_arch,
                                           install->sdk_branch,
                                           FALSE);
      monitor_transfer (self, transfer);
      ide_transfer_manager_execute_async (transfer_manager,
                                          IDE_TRANSFER (transfer),
                                          cancellable,
                                          gbp_flatpak_runtime_provider_install_cb,
                                          g_object_ref (task));
    }

  /* If there is a .Docs runtime for the SDK, install that too */
  if (install->sdk_id == NULL ||
      !(docs_id = g_strdup_printf ("%s.Docs", install->sdk_id)) ||
      gbp_flatpak_application_addin_has_runtime (app_addin,
                                                 docs_id,
                                                 install->arch,
                                                 install->branch))
    install->op_count--;
  else
    {
      g_autoptr(GbpFlatpakTransfer) transfer = NULL;

      transfer = gbp_flatpak_transfer_new (docs_id,
                                           install->arch,
                                           install->branch,
                                           FALSE);
      monitor_transfer (self, transfer);
      ide_transfer_manager_execute_async (transfer_manager,
                                          IDE_TRANSFER (transfer),
                                          cancellable,
                                          gbp_flatpak_runtime_provider_install_docs_cb,
                                          g_object_ref (task));
    }

  /* Complete the task now if everything is done */
  if (install->op_count == 0)
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
gbp_flatpak_runtime_provider_install_async (IdeRuntimeProvider  *provider,
                                            const gchar         *runtime_id,
                                            GCancellable        *cancellable,
                                            GAsyncReadyCallback  callback,
                                            gpointer             user_data)
{
  GbpFlatpakRuntimeProvider *self = (GbpFlatpakRuntimeProvider *)provider;
  g_autoptr(IdeTask) task = NULL;
  g_autofree gchar *id = NULL;
  g_autofree gchar *arch = NULL;
  g_autofree gchar *branch = NULL;
  InstallRuntime *install;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (self));
  g_assert (runtime_id != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  /*
   * The process here is to first locate the SDK for the runtime, and then
   * to submit transfers for both the runtime and the SDK if they are not
   * already installed (this is done from the callback). Since we will have
   * two async operations in flight, we need to keep track of when both are
   * done before completing the operation.
   */

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_flatpak_runtime_provider_install_async);

  if (!g_str_has_prefix (runtime_id, "flatpak:"))
    IDE_GOTO (unknown_runtime_id);

  if (!gbp_flatpak_split_id (runtime_id + strlen ("flatpak:"), &id, &arch, &branch))
    IDE_GOTO (unknown_runtime_id);

  install = g_slice_new0 (InstallRuntime);
  install->id = g_steal_pointer (&id);
  install->arch = g_steal_pointer (&arch);
  install->branch = g_steal_pointer (&branch);

  ide_task_set_task_data (task, install, install_runtime_free);

  gbp_flatpak_application_addin_locate_sdk_async (gbp_flatpak_application_addin_get_default (),
                                                  install->id,
                                                  install->arch,
                                                  install->branch,
                                                  cancellable,
                                                  gbp_flatpak_runtime_provider_locate_sdk_cb,
                                                  g_steal_pointer (&task));

  IDE_EXIT;

unknown_runtime_id:
  ide_task_return_new_error (task,
                             G_IO_ERROR,
                             G_IO_ERROR_NOT_SUPPORTED,
                             "Unknown runtime_id %s",
                             runtime_id);

  IDE_EXIT;
}

static gboolean
gbp_flatpak_runtime_provider_install_finish (IdeRuntimeProvider  *provider,
                                             GAsyncResult        *result,
                                             GError             **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
gbp_flatpak_runtime_provider_bootstrap_complete (IdeTask *task)
{
  IdeRuntimeManager *runtime_manager;
  BootstrapState *state;
  IdeContext *context;
  IdeRuntime *runtime;
  IdeObject *object;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TASK (task));

  state = ide_task_get_task_data (task);

  g_assert (state != NULL);
  g_assert (state->count == 0);

  object = ide_task_get_source_object (task);
  context = ide_object_get_context (object);
  runtime_manager = ide_runtime_manager_from_context (context);
  runtime = ide_runtime_manager_get_runtime (runtime_manager, state->runtime_id);

  if (runtime != NULL)
    ide_task_return_pointer (task, g_object_ref (runtime), g_object_unref);
  else
    ide_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "Failed to install runtime for build");
}

static void
gbp_flatpak_runtime_provider_bootstrap_install_cb (GObject      *object,
                                                   GAsyncResult *result,
                                                   gpointer      user_data)
{
  IdeTransferManager *transfer_manager = (IdeTransferManager *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  BootstrapState *state;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TRANSFER_MANAGER (transfer_manager));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  state = ide_task_get_task_data (task);

  g_assert (state != NULL);
  g_assert (state->count > 0);

  state->count--;

  /* We might still be able to find the runtime if the transfer fails */
  ide_transfer_manager_execute_finish (transfer_manager, result, &error);

  if (error != NULL)
    g_debug ("Transfer failed: %s", error->message);

  if (!ide_task_had_error (task) && state->count == 0)
    gbp_flatpak_runtime_provider_bootstrap_complete (task);
}

static void
gbp_flatpak_runtime_provider_bootstrap_cb (GObject      *object,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
  GbpFlatpakInstallDialog *dialog = (GbpFlatpakInstallDialog *)object;
  GbpFlatpakRuntimeProvider *self;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_auto(GStrv) runtimes = NULL;
  IdeTransferManager *transfer_manager;
  BootstrapState *state;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_FLATPAK_INSTALL_DIALOG (dialog));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (ide_task_had_error (task))
    return;

  self = ide_task_get_source_object (task);
  state = ide_task_get_task_data (task);

  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (self));
  g_assert (state != NULL);

  if (!gbp_flatpak_install_dialog_run_finish (dialog, result, &error))
    {
      gbp_flatpak_runtime_provider_bootstrap_complete (task);
      return;
    }

  runtimes = gbp_flatpak_install_dialog_get_runtimes (dialog);
  transfer_manager = ide_transfer_manager_get_default ();

  for (guint i = 0; runtimes[i]; i++)
    {
      g_autofree gchar *name = NULL;
      g_autofree gchar *arch = NULL;
      g_autofree gchar *branch = NULL;

      if (gbp_flatpak_split_id (runtimes[i], &name, &arch, &branch))
        {
          g_autoptr(GbpFlatpakTransfer) transfer = NULL;
          g_autoptr(IdeNotification) notif = NULL;

          state->count++;

          transfer = gbp_flatpak_transfer_new (name, arch, branch, FALSE);
          notif = ide_transfer_create_notification (IDE_TRANSFER (transfer));
          ide_notification_attach (notif, IDE_OBJECT (self));

          ide_transfer_manager_execute_async (transfer_manager,
                                              IDE_TRANSFER (transfer),
                                              ide_task_get_cancellable (task),
                                              gbp_flatpak_runtime_provider_bootstrap_install_cb,
                                              g_object_ref (task));
        }
    }

  if (state->count == 0)
    gbp_flatpak_runtime_provider_bootstrap_complete (task);
}

static void
gbp_flatpak_runtime_provider_bootstrap_async (IdeRuntimeProvider  *provider,
                                              IdePipeline         *pipeline,
                                              GCancellable        *cancellable,
                                              GAsyncReadyCallback  callback,
                                              gpointer             user_data)
{
  GbpFlatpakRuntimeProvider *self = (GbpFlatpakRuntimeProvider *)provider;
  g_autofree gchar *name = NULL;
  g_autofree gchar *arch = NULL;
  g_autofree gchar *branch = NULL;
  g_autofree gchar *docs_id = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_auto(GStrv) runtimes = NULL;
  GbpFlatpakApplicationAddin *addin;
  GbpFlatpakInstallDialog *dialog;
  BootstrapState *state;
  IdeWorkbench *workbench;
  IdeWorkspace *workspace;
  const gchar *runtime_id;
  const gchar *build_arch;
  IdeContext *context;
  IdeConfig *config;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_release_on_propagate (task, FALSE);
  ide_task_set_source_tag (task, gbp_flatpak_runtime_provider_bootstrap_async);

  context = ide_object_get_context (IDE_OBJECT (self));
  build_arch = ide_pipeline_get_arch (pipeline);
  config = ide_pipeline_get_config (pipeline);
  runtime_id = ide_config_get_runtime_id (config);

  if (runtime_id == NULL ||
      !g_str_has_prefix (runtime_id, "flatpak:") ||
      !gbp_flatpak_split_id (runtime_id + strlen ("flatpak:"), &name, &arch, &branch))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_FOUND,
                                 "No runtime available");
      IDE_EXIT;
    }

  /* Create dialog to potentially query user if we are allowed to install */
  workbench = _ide_workbench_from_context (context);
  workspace = ide_workbench_get_current_workspace (workbench);
  dialog = gbp_flatpak_install_dialog_new (GTK_WINDOW (workspace));
  gtk_window_group_add_window (GTK_WINDOW_GROUP (workbench), GTK_WINDOW (dialog));

  /* Create state for async op */
  state = g_slice_new0 (BootstrapState);
  state->config = g_object_ref (config);
  state->runtime_id = g_strdup_printf ("flatpak:%s/%s/%s", name, build_arch, branch);
  state->name = g_steal_pointer (&name);
  state->branch = g_steal_pointer (&branch);
  state->arch = g_strdup (build_arch);
  ide_task_set_task_data (task, state, bootstrap_state_free);

  addin = gbp_flatpak_application_addin_get_default ();

  /* Add all the runtimes the manifest needs */
  if (GBP_IS_FLATPAK_MANIFEST (state->config))
    {
      g_auto(GStrv) all = NULL;

      all = gbp_flatpak_manifest_get_runtimes (GBP_FLATPAK_MANIFEST (state->config),
                                               state->arch);

      if (all != NULL)
        {
          for (guint i = 0; all[i]; i++)
            {
              g_autofree gchar *item_name = NULL;
              g_autofree gchar *item_arch = NULL;
              g_autofree gchar *item_branch = NULL;

              if (gbp_flatpak_split_id (all[i], &item_name, &item_arch, &item_branch))
                {
                  if (!gbp_flatpak_application_addin_has_runtime (addin, item_name, item_arch, item_branch))
                    gbp_flatpak_install_dialog_add_runtime (dialog, all[i]);
                }
            }
        }
    }

  /* Add runtime specifically (in case no manifest is set) */
  if (!gbp_flatpak_application_addin_has_runtime (addin, state->name, state->arch, state->branch))
    gbp_flatpak_install_dialog_add_runtime_full (dialog, state->name, state->arch, state->branch);

  runtimes = gbp_flatpak_install_dialog_get_runtimes (dialog);

  if (runtimes == NULL || runtimes[0] == NULL)
    {
      gtk_widget_destroy (GTK_WIDGET (dialog));
      gbp_flatpak_runtime_provider_bootstrap_complete (task);
    }
  else
    {
      /* Do not propagate cancellable to this operation or we risk cancelling
       * in-flight operations that the user is expecting to complete.
       */
      gbp_flatpak_install_dialog_run_async (dialog,
                                            NULL,
                                            gbp_flatpak_runtime_provider_bootstrap_cb,
                                            g_object_ref (task));
    }

  IDE_EXIT;
}

static IdeRuntime *
gbp_flatpak_runtime_provider_bootstrap_finish (IdeRuntimeProvider  *provider,
                                               GAsyncResult        *result,
                                               GError             **error)
{
  IdeRuntime *ret;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
runtime_provider_iface_init (IdeRuntimeProviderInterface *iface)
{
  iface->load = gbp_flatpak_runtime_provider_load;
  iface->unload = gbp_flatpak_runtime_provider_unload;
  iface->can_install = gbp_flatpak_runtime_provider_can_install;
  iface->install_async = gbp_flatpak_runtime_provider_install_async;
  iface->install_finish = gbp_flatpak_runtime_provider_install_finish;
  iface->bootstrap_async = gbp_flatpak_runtime_provider_bootstrap_async;
  iface->bootstrap_finish = gbp_flatpak_runtime_provider_bootstrap_finish;
}
