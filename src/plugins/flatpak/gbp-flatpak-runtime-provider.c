/* gbp-flatpak-runtime-provider.c
 *
 * Copyright 2016 Christian Hergert <chergert@redhat.com>
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
 */

#define G_LOG_DOMAIN "gbp-flatpak-runtime-provider"

#include <flatpak.h>
#include <ostree.h>
#include <string.h>

#include "util/ide-posix.h"

#include "gbp-flatpak-application-addin.h"
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
  IdeConfiguration *config;
  gchar            *runtime_id;
  gchar            *name;
  gchar            *arch;
  gchar            *branch;
  guint             count;
} BootstrapState;

struct _GbpFlatpakRuntimeProvider
{
  GObject            parent_instance;
  IdeRuntimeManager *manager;
  GPtrArray         *runtimes;
};

static void runtime_provider_iface_init         (IdeRuntimeProviderInterface *iface);
static void gbp_flatpak_runtime_provider_load   (IdeRuntimeProvider          *provider,
                                                 IdeRuntimeManager           *manager);
static void gbp_flatpak_runtime_provider_unload (IdeRuntimeProvider          *provider,
                                                 IdeRuntimeManager           *manager);

G_DEFINE_TYPE_WITH_CODE (GbpFlatpakRuntimeProvider, gbp_flatpak_runtime_provider, G_TYPE_OBJECT,
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
  return (g_strcmp0 (flatpak_ref_get_name (FLATPAK_REF (ref)),
                     gbp_flatpak_runtime_get_platform (runtime)) == 0) &&
         (g_strcmp0 (flatpak_ref_get_arch (FLATPAK_REF (ref)),
                     gbp_flatpak_runtime_get_arch (runtime)) == 0) &&
         (g_strcmp0 (flatpak_ref_get_branch (FLATPAK_REF (ref)),
                     gbp_flatpak_runtime_get_branch (runtime)) == 0);
}

static void
runtime_added_cb (GbpFlatpakRuntimeProvider  *self,
                  FlatpakInstalledRef        *ref,
                  GbpFlatpakApplicationAddin *app_addin)
{
  g_autoptr(GbpFlatpakRuntime) new_runtime = NULL;
  g_autoptr(GError) error = NULL;
  const gchar *name;
  IdeContext *context;

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
  context = ide_object_get_context (IDE_OBJECT (self->manager));
  new_runtime = gbp_flatpak_runtime_new (context, ref, NULL, &error);

  if (new_runtime == NULL)
    g_warning ("Failed to create GbpFlatpakRuntime: %s", error->message);
  else
    {
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

  dzl_set_weak_pointer (&self->manager, manager);
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

  dzl_clear_weak_pointer (&self->manager);

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
  IdeTransferManager *transfer_manager;
  InstallRuntime *install;
  GCancellable *cancellable;
  gboolean sdk_matches_runtime = FALSE;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_APPLICATION_ADDIN (app_addin));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));
  g_assert (!ide_task_get_completed (task));

  install = ide_task_get_task_data (task);
  cancellable = ide_task_get_cancellable (task);

  g_assert (install != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  transfer_manager = ide_application_get_transfer_manager (IDE_APPLICATION_DEFAULT);

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
  ide_task_set_priority (task, G_PRIORITY_LOW);

  if (!g_str_has_prefix (runtime_id, "flatpak:"))
    IDE_GOTO (unknown_runtime_id);

  if (!gbp_flatpak_split_id (runtime_id + strlen ("flatpak:"), &id, &arch, &branch))
    IDE_GOTO (unknown_runtime_id);

  install = g_slice_new0 (InstallRuntime);
  install->id = g_steal_pointer (&id);
  install->arch = g_steal_pointer (&arch);
  install->branch = g_steal_pointer (&branch);

  ide_task_set_task_data (task, install, (GDestroyNotify)install_runtime_free);

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
gbp_flatpak_runtime_provider_bootstrap_cb (GObject      *object,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
  GbpFlatpakRuntimeProvider *self;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  BootstrapState *state;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (object) ||
            IDE_IS_TRANSFER_MANAGER (object));
  g_assert (IDE_IS_TASK (task));
  g_assert (G_IS_ASYNC_RESULT (result));

  self = ide_task_get_source_object (task);
  state = ide_task_get_task_data (task);

  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (self));
  g_assert (state != NULL);
  g_assert (state->count > 0);

  state->count--;

  if (GBP_IS_FLATPAK_RUNTIME_PROVIDER (object))
    {
      if (!gbp_flatpak_runtime_provider_install_finish (IDE_RUNTIME_PROVIDER (object), result, &error))
        {
          g_warning ("Failed to install runtime: %s", error->message);
          if (!ide_task_get_completed (task))
            ide_task_return_error (task, g_steal_pointer (&error));
          return;
        }
    }
  else if (IDE_IS_TRANSFER_MANAGER (object))
    {
      if (!ide_transfer_manager_execute_finish (IDE_TRANSFER_MANAGER (object), result, &error))
        {
          g_warning ("Failed to install runtime: %s", error->message);
          if (!ide_task_get_completed (task))
            ide_task_return_error (task, g_steal_pointer (&error));
          return;
        }
    }

  if (ide_task_return_error_if_cancelled (task))
    return;

  if (state->count == 0 && !ide_task_get_completed (task))
    {
      g_autofree gchar *runtime_id = NULL;
      IdeRuntimeManager *runtime_manager;
      IdeContext *context;
      IdeRuntime *runtime;

      runtime_id = g_strdup_printf ("flatpak:%s/%s/%s",
                                    state->name,
                                    state->arch,
                                    state->branch);

      context = ide_object_get_context (IDE_OBJECT (self->manager));
      runtime_manager = ide_context_get_runtime_manager (context);
      runtime = ide_runtime_manager_get_runtime (runtime_manager, runtime_id);

      if (runtime == NULL)
        ide_task_return_new_error (task,
                                   G_IO_ERROR,
                                   G_IO_ERROR_NOT_SUPPORTED,
                                   "Falling back to default runtime lookup");
      else
        ide_task_return_pointer (task, g_object_ref (runtime), g_object_unref);
    }
}

static void
gbp_flatpak_runtime_provider_bootstrap_async (IdeRuntimeProvider  *provider,
                                              IdeBuildPipeline    *pipeline,
                                              GCancellable        *cancellable,
                                              GAsyncReadyCallback  callback,
                                              gpointer             user_data)
{
  GbpFlatpakRuntimeProvider *self = (GbpFlatpakRuntimeProvider *)provider;
  g_autofree gchar *name = NULL;
  g_autofree gchar *arch = NULL;
  g_autofree gchar *branch = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(IdeTriplet) triplet = NULL;
  IdeConfiguration *config;
  BootstrapState *state;
  const gchar *runtime_id;
  const gchar *build_arch;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (self));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_flatpak_runtime_provider_bootstrap_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  triplet = ide_build_pipeline_get_device_triplet (pipeline);
  build_arch = ide_triplet_get_cpu (triplet);
  config = ide_build_pipeline_get_configuration (pipeline);
  runtime_id = ide_configuration_get_runtime_id (config);

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

  state = g_slice_new0 (BootstrapState);
  state->config = g_object_ref (config);
  state->runtime_id = g_strdup_printf ("flatpak:%s/%s/%s", name, build_arch, branch);
  state->name = g_steal_pointer (&name);
  state->branch = g_steal_pointer (&branch);
  state->arch = g_strdup (build_arch);
  ide_task_set_task_data (task, state, bootstrap_state_free);

  if (GBP_IS_FLATPAK_MANIFEST (state->config))
    {
      g_autofree gchar *platform_id = NULL;
      IdeTransferManager *transfer_manager;
      GbpFlatpakApplicationAddin *addin;
      const gchar * const *sdk_exts;

      transfer_manager = ide_application_get_transfer_manager (IDE_APPLICATION_DEFAULT);
      addin = gbp_flatpak_application_addin_get_default ();
      sdk_exts = gbp_flatpak_manifest_get_sdk_extensions (GBP_FLATPAK_MANIFEST (state->config));

      if (sdk_exts != NULL)
        {
          for (guint i = 0; sdk_exts[i] != NULL; i++)
            {
              g_autofree gchar *ext_id = NULL;
              g_autofree gchar *ext_arch = NULL;
              g_autofree gchar *ext_branch = NULL;

              if (gbp_flatpak_split_id (sdk_exts[i], &ext_id, &ext_arch, &ext_branch))
                {
                  /* Check for runtime with the arch of the device */
                  if (!gbp_flatpak_application_addin_has_runtime (addin, ext_id, state->arch, ext_branch))
                    {
                      g_autoptr(GbpFlatpakTransfer) transfer = NULL;

                      state->count++;

                      transfer = gbp_flatpak_transfer_new (ext_id, arch, ext_branch, FALSE);
                      ide_transfer_manager_execute_async (transfer_manager,
                                                          IDE_TRANSFER (transfer),
                                                          cancellable,
                                                          gbp_flatpak_runtime_provider_bootstrap_cb,
                                                          g_object_ref (task));
                    }
                }
            }
        }
    }

  state->count++;

  gbp_flatpak_runtime_provider_install_async (IDE_RUNTIME_PROVIDER (self),
                                              state->runtime_id,
                                              cancellable,
                                              gbp_flatpak_runtime_provider_bootstrap_cb,
                                              g_object_ref (task));

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

void
gbp_flatpak_runtime_provider_locate_sdk_async (GbpFlatpakRuntimeProvider *self,
                                               const gchar               *id,
                                               const gchar               *arch,
                                               const gchar               *branch,
                                               GCancellable              *cancellable,
                                               GAsyncReadyCallback        callback,
                                               gpointer                   user_data)
{
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (self));
  g_assert (id != NULL);
  g_assert (arch != NULL);
  g_assert (branch != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_flatpak_runtime_provider_locate_sdk_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  IDE_EXIT;
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
