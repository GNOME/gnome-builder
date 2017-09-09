/* gbp-flatpak-runtime-provider.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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
#include "gbp-flatpak-runtime.h"
#include "gbp-flatpak-runtime-provider.h"
#include "gbp-flatpak-transfer.h"

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

struct _GbpFlatpakRuntimeProvider
{
  GObject            parent_instance;
  IdeRuntimeManager *manager;
  GPtrArray         *runtimes;
};

static void runtime_provider_iface_init (IdeRuntimeProviderInterface *);

G_DEFINE_TYPE_EXTENDED (GbpFlatpakRuntimeProvider, gbp_flatpak_runtime_provider, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_RUNTIME_PROVIDER,
                                               runtime_provider_iface_init))

static void gbp_flatpak_runtime_provider_load (IdeRuntimeProvider *provider, IdeRuntimeManager *manager);
static void gbp_flatpak_runtime_provider_unload (IdeRuntimeProvider *provider, IdeRuntimeManager *manager);

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
  IdeContext *context;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_APPLICATION_ADDIN (app_addin));
  g_assert (FLATPAK_IS_INSTALLED_REF (ref));
  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (self));
  g_assert (IDE_IS_RUNTIME_MANAGER (self->manager));

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

  ide_set_weak_pointer (&self->manager, manager);
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

  ide_clear_weak_pointer (&self->manager);

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
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  InstallRuntime *install;

  IDE_ENTRY;

  g_assert (IDE_IS_TRANSFER_MANAGER (transfer_manager));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  install = g_task_get_task_data (task);

  if (!ide_transfer_manager_execute_finish (transfer_manager, result, &error))
    {
      if (!install->failed)
        {
          g_task_return_error (task, g_steal_pointer (&error));
          install->failed = TRUE;
        }
    }

  install->op_count--;

  if (install->op_count == 0 && !install->failed)
    g_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
gbp_flatpak_runtime_provider_locate_sdk_cb (GObject      *object,
                                            GAsyncResult *result,
                                            gpointer      user_data)
{
  GbpFlatpakApplicationAddin *app_addin = (GbpFlatpakApplicationAddin *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  GbpFlatpakRuntimeProvider *self;
  IdeTransferManager *transfer_manager;
  InstallRuntime *install;
  GCancellable *cancellable;
  IdeContext *context;
  gboolean sdk_matches_runtime = FALSE;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_APPLICATION_ADDIN (app_addin));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));
  g_assert (!g_task_get_completed (task));

  self = g_task_get_source_object (task);
  install = g_task_get_task_data (task);
  cancellable = g_task_get_cancellable (task);

  g_assert (install != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (self != NULL);
  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (self));

  context = ide_object_get_context (IDE_OBJECT (self->manager));
  transfer_manager = ide_context_get_transfer_manager (context);

  if (!gbp_flatpak_application_addin_locate_sdk_finish (app_addin,
                                                        result,
                                                        &install->sdk_id,
                                                        &install->sdk_arch,
                                                        &install->sdk_branch,
                                                        &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  install->op_count = 2;

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

  sdk_matches_runtime = (g_strcmp0 (install->sdk_id, install->id) == 0 &&
                         g_strcmp0 (install->sdk_arch, install->arch) == 0 &&
                         g_strcmp0 (install->sdk_branch, install->branch) == 0);
  if (sdk_matches_runtime ||
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

  if (install->op_count == 0)
    g_task_return_boolean (task, TRUE);

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
  g_autoptr(GTask) task = NULL;
  g_autofree gchar *delimited = NULL;
  g_auto(GStrv) parts = NULL;
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

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gbp_flatpak_runtime_provider_install_async);
  g_task_set_priority (task, G_PRIORITY_LOW);

  if (!g_str_has_prefix (runtime_id, "flatpak:"))
    IDE_GOTO (unknown_runtime_id);
  delimited = g_strdelimit (g_strdup (runtime_id), ":/", ':');
  parts = g_strsplit (delimited, ":", 0);
  if (g_strv_length (parts) != 4)
    IDE_GOTO (unknown_runtime_id);

  install = g_slice_new0 (InstallRuntime);
  install->id = g_strdup (parts[1]);
  install->arch = g_strdup (parts[2]);
  install->branch = g_strdup (parts[3]);

  g_task_set_task_data (task, install, (GDestroyNotify)install_runtime_free);

  gbp_flatpak_application_addin_locate_sdk_async (gbp_flatpak_application_addin_get_default (),
                                                  install->id,
                                                  install->arch,
                                                  install->branch,
                                                  cancellable,
                                                  gbp_flatpak_runtime_provider_locate_sdk_cb,
                                                  g_steal_pointer (&task));

  IDE_EXIT;

unknown_runtime_id:
  g_task_return_new_error (task,
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
  g_assert (G_IS_TASK (result));

  ret = g_task_propagate_boolean (G_TASK (result), error);

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
  g_autoptr(GTask) task = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (self));
  g_assert (id != NULL);
  g_assert (arch != NULL);
  g_assert (branch != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gbp_flatpak_runtime_provider_locate_sdk_async);
  g_task_set_priority (task, G_PRIORITY_LOW);

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
}
