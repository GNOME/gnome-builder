/* gbp-flatpak-runtime-provider.h
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

#include "config.h"

#include <glib/gi18n.h>
#include <libide-foundry.h>
#include <libide-gui.h>

#include "gbp-flatpak-client.h"
#include "gbp-flatpak-install-dialog.h"
#include "gbp-flatpak-manifest.h"
#include "gbp-flatpak-runtime.h"
#include "gbp-flatpak-runtime-provider.h"
#include "gbp-flatpak-util.h"

#include "daemon/ipc-flatpak-service.h"
#include "daemon/ipc-flatpak-transfer.h"
#include "daemon/ipc-flatpak-util.h"

struct _GbpFlatpakRuntimeProvider
{
  IdeObject parent_instance;
  GPtrArray *runtimes;
};

static void runtime_provider_iface_init (IdeRuntimeProviderInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GbpFlatpakRuntimeProvider, gbp_flatpak_runtime_provider, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_RUNTIME_PROVIDER, runtime_provider_iface_init))

static void
gbp_flatpak_runtime_provider_dispose (GObject *object)
{
  GbpFlatpakRuntimeProvider *self = (GbpFlatpakRuntimeProvider *)object;

  g_clear_pointer (&self->runtimes, g_ptr_array_unref);

  G_OBJECT_CLASS (gbp_flatpak_runtime_provider_parent_class)->dispose (object);
}

static void
gbp_flatpak_runtime_provider_class_init (GbpFlatpakRuntimeProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gbp_flatpak_runtime_provider_dispose;
}

static void
gbp_flatpak_runtime_provider_init (GbpFlatpakRuntimeProvider *self)
{
  self->runtimes = g_ptr_array_new_with_free_func (g_object_unref);
}

static void
on_runtime_added_cb (GbpFlatpakRuntimeProvider *self,
                     GVariant                  *info,
                     IpcFlatpakService         *service)

{
  g_autoptr(GbpFlatpakRuntime) runtime = NULL;
  g_autoptr(IdeContext) context = NULL;
  IdeRuntimeManager *manager;
  const gchar *name;
  const gchar *arch;
  const gchar *branch;
  const gchar *sdk_name;
  const gchar *sdk_branch;
  const gchar *deploy_dir;
  const gchar *metadata;
  gboolean is_extension;

  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (self));
  g_assert (info != NULL);
  g_assert (IPC_IS_FLATPAK_SERVICE (service));
  g_assert (g_variant_is_of_type (info, RUNTIME_VARIANT_TYPE));

  if (self->runtimes == NULL)
    return;

  if (!runtime_variant_parse (info,
                              &name, &arch, &branch,
                              &sdk_name, &sdk_branch,
                              &deploy_dir,
                              &metadata,
                              &is_extension))
    return;

  /* Ignore extensions for now */
  if (is_extension)
    return;

  context = ide_object_ref_context (IDE_OBJECT (self));
  manager = ide_runtime_manager_from_context (context);
  runtime = gbp_flatpak_runtime_new (name,
                                     arch,
                                     branch,
                                     sdk_name,
                                     sdk_branch,
                                     deploy_dir,
                                     metadata,
                                     is_extension);
  g_ptr_array_add (self->runtimes, g_object_ref (runtime));
  ide_object_append (IDE_OBJECT (self), IDE_OBJECT (runtime));
  ide_runtime_manager_add (manager, IDE_RUNTIME (runtime));
}

static void
gbp_flatpak_runtime_provider_load_list_runtimes_cb (GObject      *object,
                                                    GAsyncResult *result,
                                                    gpointer      user_data)
{
  IpcFlatpakService *service = (IpcFlatpakService *)object;
  g_autoptr(GbpFlatpakRuntimeProvider) self = user_data;
  g_autoptr(GVariant) runtimes = NULL;
  g_autoptr(GError) error = NULL;
  GVariantIter iter;
  GVariant *info;

  g_assert (IPC_IS_FLATPAK_SERVICE (service));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (self));

  if (!ipc_flatpak_service_call_list_runtimes_finish (service, &runtimes, result, &error))
    {
      g_warning ("Failed to list flatpak runtimes: %s", error->message);
      return;
    }

  g_variant_iter_init (&iter, runtimes);
  while ((info = g_variant_iter_next_value (&iter)))
    {
      on_runtime_added_cb (self, info, service);
      g_variant_unref (info);
    }
}

static void
gbp_flatpak_runtime_provider_load (IdeRuntimeProvider *provider,
                                   IdeRuntimeManager  *manager)
{
  g_autoptr(GbpFlatpakClient) client = NULL;
  g_autoptr(IpcFlatpakService) service = NULL;
  g_autoptr(IdeContext) context = NULL;

  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (provider));
  g_assert (IDE_IS_RUNTIME_MANAGER (manager));

  if ((context = ide_object_ref_context (IDE_OBJECT (provider))) &&
      (client = gbp_flatpak_client_ensure (context)) &&
      (service = gbp_flatpak_client_get_service (client, NULL, NULL)))
    {
      g_signal_connect_object (service,
                               "runtime-added",
                               G_CALLBACK (on_runtime_added_cb),
                               provider,
                               G_CONNECT_SWAPPED);
      ipc_flatpak_service_call_list_runtimes (service,
                                              NULL,
                                              gbp_flatpak_runtime_provider_load_list_runtimes_cb,
                                              g_object_ref (provider));
    }
}

static void
gbp_flatpak_runtime_provider_unload (IdeRuntimeProvider *provider,
                                     IdeRuntimeManager  *manager)
{
  GbpFlatpakRuntimeProvider *self = (GbpFlatpakRuntimeProvider *)provider;

  g_assert (IDE_IS_RUNTIME_PROVIDER (provider));
  g_assert (IDE_IS_RUNTIME_MANAGER (manager));

  if (self->runtimes == NULL || self->runtimes->len == 0)
    return;

  for (guint i = 0; i < self->runtimes->len; i++)
    ide_runtime_manager_remove (manager, g_ptr_array_index (self->runtimes, i));
  g_ptr_array_remove_range (self->runtimes, 0, self->runtimes->len);
}

typedef struct
{
  GbpFlatpakRuntimeProvider *self;
  GDBusMethodInvocation *invocation;
  IpcFlatpakTransfer *transfer;
} Confirm;

typedef struct
{
  char               *runtime_id;
  char               *transfer_path;
  GPtrArray          *to_install;
  IpcFlatpakTransfer *transfer;
  IpcFlatpakService  *service;
  IdeNotification    *notif;
} Bootstrap;

static void
bootstrap_free (Bootstrap *b)
{
  g_clear_pointer (&b->runtime_id, g_free);
  g_clear_pointer (&b->transfer_path, g_free);
  g_clear_object (&b->notif);
  g_clear_object (&b->service);
  g_clear_object (&b->transfer);
  g_clear_pointer (&b->to_install, g_ptr_array_unref);
  g_slice_free (Bootstrap, b);
}

static void
gbp_flatpak_runtime_provider_handle_confirm_cb (GObject      *object,
                                                GAsyncResult *result,
                                                gpointer      user_data)
{
  GbpFlatpakInstallDialog *dialog = (GbpFlatpakInstallDialog *)object;
  Confirm *state = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (GBP_IS_FLATPAK_INSTALL_DIALOG (dialog));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (state != NULL);
  g_assert (G_IS_DBUS_METHOD_INVOCATION (state->invocation));
  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (state->self));
  g_assert (IPC_IS_FLATPAK_TRANSFER (state->transfer));

  if (!gbp_flatpak_install_dialog_run_finish (dialog, result, &error))
    g_dbus_method_invocation_return_error (g_steal_pointer (&state->invocation),
                                           G_DBUS_ERROR,
                                           G_DBUS_ERROR_FAILED,
                                           "Unconfirmed request");
  else
    ipc_flatpak_transfer_complete_confirm (state->transfer,
                                           g_steal_pointer (&state->invocation));

  g_clear_object (&state->invocation);
  g_clear_object (&state->transfer);
  g_clear_object (&state->self);
  g_slice_free (Confirm, state);
}

static gboolean
gbp_flatpak_runtime_provider_handle_confirm (GbpFlatpakRuntimeProvider *self,
                                             GDBusMethodInvocation     *invocation,
                                             const char * const        *refs,
                                             IpcFlatpakTransfer        *transfer)
{
  g_autoptr(IdeContext) context = NULL;
  GbpFlatpakInstallDialog *dialog;
  IdeWorkbench *workbench;
  IdeWorkspace *workspace;
  Confirm *state;

  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (refs != NULL);
  g_assert (IPC_IS_FLATPAK_TRANSFER (transfer));

  context = ide_object_ref_context (IDE_OBJECT (self));
  workbench = ide_workbench_from_context (context);
  workspace = ide_workbench_get_current_workspace (workbench);
  dialog = gbp_flatpak_install_dialog_new (GTK_WINDOW (workspace));

  for (guint i = 0; refs[i]; i++)
    gbp_flatpak_install_dialog_add_runtime (dialog, refs[i]);

  if (gbp_flatpak_install_dialog_is_empty (dialog))
    {
      gtk_widget_destroy (GTK_WIDGET (dialog));
      ipc_flatpak_transfer_complete_confirm (transfer, g_steal_pointer (&invocation));
      return TRUE;
    }

  state = g_slice_new0 (Confirm);
  state->self = g_object_ref (self);
  state->transfer = g_object_ref (transfer);
  state->invocation = g_object_ref (invocation);

  gbp_flatpak_install_dialog_run_async (dialog,
                                        NULL,
                                        gbp_flatpak_runtime_provider_handle_confirm_cb,
                                        state);

  return TRUE;
}

static IdeRuntime *
find_runtime (GbpFlatpakRuntimeProvider *self,
              const char                *runtime_id)
{
  IdeRuntimeManager *manager;
  IdeContext *context;
  IdeRuntime *runtime;

  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (self));
  g_assert (runtime_id != NULL);

  context = ide_object_get_context (IDE_OBJECT (self));
  manager = ide_runtime_manager_from_context (context);
  runtime = ide_runtime_manager_get_runtime (manager, runtime_id);

  return runtime ? g_object_ref (runtime) : NULL;
}

static void
gbp_flatpak_runtime_provider_bootstrap_install_cb (GObject      *object,
                                                   GAsyncResult *result,
                                                   gpointer      user_data)
{
  GbpFlatpakRuntimeProvider *self;
  IpcFlatpakService *service = (IpcFlatpakService *)object;
  g_autoptr(IdeRuntime) runtime = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  Bootstrap *state;

  IDE_ENTRY;

  g_assert (IPC_IS_FLATPAK_SERVICE (service));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  state = ide_task_get_task_data (task);
  self = ide_task_get_source_object (task);

  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (self));
  g_assert (state != NULL);
  g_assert (IDE_IS_NOTIFICATION (state->notif));

  if (!ipc_flatpak_service_call_install_finish (service, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else if (!(runtime = find_runtime (self, state->runtime_id)))
    ide_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_CANCELLED,
                               "Operation was cancelled");
  else
    ide_task_return_pointer (task, g_steal_pointer (&runtime), g_object_unref);

  ide_notification_withdraw (state->notif);

  IDE_EXIT;
}

static gboolean
gbp_flatpak_runtime_provider_bootstrap_complete (gpointer data)
{
  GbpFlatpakRuntimeProvider *self;
  g_autoptr(IdeRuntime) runtime = NULL;
  IdeTask *task = data;
  Bootstrap *state;

  IDE_ENTRY;

  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  state = ide_task_get_task_data (task);

  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (self));
  g_assert (state->to_install != NULL);
  g_assert (IPC_IS_FLATPAK_SERVICE (state->service));
  g_assert (state->transfer_path != NULL);
  g_assert (IDE_IS_NOTIFICATION (state->notif));

  ide_notification_attach (state->notif, IDE_OBJECT (self));

  ipc_flatpak_service_call_install (state->service,
                                    (const char * const *)state->to_install->pdata,
                                    state->transfer_path,
                                    "",
                                    ide_task_get_cancellable (task),
                                    gbp_flatpak_runtime_provider_bootstrap_install_cb,
                                    g_object_ref (task));

  IDE_RETURN (G_SOURCE_REMOVE);
}

static void
gbp_flatpak_runtime_provider_bootstrap (IdeTask      *task,
                                        gpointer      source_object,
                                        gpointer      task_data,
                                        GCancellable *cancellable)
{
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(GbpFlatpakClient) client = NULL;
  g_autoptr(IpcFlatpakService) service = NULL;
  g_autoptr(IpcFlatpakTransfer) transfer = NULL;
  g_autoptr(IdeNotification) notif = NULL;
  Bootstrap *state = task_data;

  IDE_ENTRY;

  g_assert (IDE_IS_TASK (task));
  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (source_object));
  g_assert (state != NULL);
  g_assert (state->to_install != NULL);
  g_assert (state->to_install->len > 0);

  if (!(context = ide_object_ref_context (source_object)) ||
      !(client = gbp_flatpak_client_ensure (context)) ||
      !(service = gbp_flatpak_client_get_service (client, NULL, NULL)))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_CANCELLED,
                                 "Operation was cancelled");
      IDE_GOTO (failure);
    }

  /* Filter out anything we can't find or is already installed */
  for (guint i = state->to_install->len; i > 0; i--)
    {
      const char *id = g_ptr_array_index (state->to_install, i-1);
      gboolean is_known = FALSE;
      gint64 size = 0;

      if (!ipc_flatpak_service_call_runtime_is_known_sync (service, id, &is_known, &size, NULL, NULL) ||
          is_known == TRUE)
        g_ptr_array_remove_index (state->to_install, i-1);
    }

  /* Now create a transfer to install the rest */
  if (state->to_install->len > 0)
    {
      g_autofree char *guid = g_dbus_generate_guid ();
      g_autofree char *transfer_path = g_strdup_printf ("/org/gnome/Builder/Flatpak/Transfer/%s", guid);
      g_autoptr(GError) error = NULL;

      notif = ide_notification_new ();
      ide_notification_set_icon_name (notif, "system-software-install");
      ide_notification_set_title (notif, _("Installing Necessary SDKs"));
      ide_notification_set_body (notif, _("Builder is installing Software Development Kits necessary for building your application."));
      ide_notification_set_has_progress (notif, TRUE);
      ide_notification_set_progress_is_imprecise (notif, FALSE);

      transfer = ipc_flatpak_transfer_skeleton_new ();
      g_signal_connect_object (transfer,
                               "handle-confirm",
                               G_CALLBACK (gbp_flatpak_runtime_provider_handle_confirm),
                               source_object,
                               G_CONNECT_SWAPPED);
      g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (transfer),
                                        g_dbus_proxy_get_connection (G_DBUS_PROXY (service)),
                                        transfer_path,
                                        NULL);

      g_object_bind_property (transfer, "fraction", notif, "progress", G_BINDING_SYNC_CREATE);
      g_object_bind_property (transfer, "message", notif, "body", G_BINDING_DEFAULT);

      state->service = g_object_ref (service);
      state->notif = g_object_ref (notif);
      state->transfer_path = g_strdup (transfer_path);

      g_ptr_array_add (state->to_install, NULL);
    }

  g_timeout_add_full (G_PRIORITY_DEFAULT,
                      0,
                      gbp_flatpak_runtime_provider_bootstrap_complete,
                      g_object_ref (task),
                      g_object_unref);

failure:

  if (transfer != NULL)
    g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (transfer));

  IDE_EXIT;
}

static void
gbp_flatpak_runtime_provider_bootstrap_async (IdeRuntimeProvider  *provider,
                                              IdePipeline         *pipeline,
                                              GCancellable        *cancellable,
                                              GAsyncReadyCallback  callback,
                                              gpointer             user_data)
{
  GbpFlatpakRuntimeProvider *self = (GbpFlatpakRuntimeProvider *)provider;
  g_autoptr(IdeTask) task = NULL;
  g_autofree char *full_sdk = NULL;
  g_autofree char *full_platform = NULL;
  const char *arch;
  Bootstrap *state;
  IdeConfig *config;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  arch = ide_pipeline_get_arch (pipeline);
  config = ide_pipeline_get_config (pipeline);

  state = g_slice_new0 (Bootstrap);
  state->runtime_id = g_strdup (ide_config_get_runtime_id (config));
  state->to_install = g_ptr_array_new_with_free_func (g_free);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_flatpak_runtime_provider_bootstrap_async);
  ide_task_set_task_data (task, state, bootstrap_free);

  /* Collect all of the runtimes that could be needed */
  if (GBP_IS_FLATPAK_MANIFEST (config))
    {
      const char *sdk = gbp_flatpak_manifest_get_sdk (GBP_FLATPAK_MANIFEST (config));
      const char *platform = gbp_flatpak_manifest_get_platform (GBP_FLATPAK_MANIFEST (config));
      const char *branch = gbp_flatpak_manifest_get_branch (GBP_FLATPAK_MANIFEST (config));
      const char * const *extensions = gbp_flatpak_manifest_get_sdk_extensions (GBP_FLATPAK_MANIFEST (config));

      if (sdk == NULL)
        sdk = platform;

      if (branch == NULL)
        branch = "master";

      full_sdk = g_strdup_printf ("runtime/%s/%s/%s", sdk, arch, branch);
      full_platform = g_strdup_printf ("runtime/%s/%s/%s", platform, arch, branch);

      g_ptr_array_add (state->to_install, g_strdup (full_sdk));
      if (g_strcmp0 (full_sdk, full_platform) != 0)
        g_ptr_array_add (state->to_install, g_strdup (full_platform));

      if (extensions != NULL)
        {
          for (guint i = 0; extensions[i]; i++)
            g_ptr_array_add (state->to_install, g_strdup_printf ("runtime/%s", extensions[i]));
        }
    }
  else
    {
      const char *runtime_id = ide_config_get_runtime_id (config);

      if (g_str_has_prefix (runtime_id, "flatpak:"))
        {
          g_autofree char *resolved_id = NULL;
          g_autofree char *resolved_arch = NULL;
          g_autofree char *resolved_branch = NULL;

          if (gbp_flatpak_split_id (runtime_id + strlen ("flatpak:"),
                                    &resolved_id,
                                    &resolved_arch,
                                    &resolved_branch))
            {
              full_sdk = g_strdup_printf ("runtime/%s/%s/%s",
                                          resolved_id,
                                          arch,
                                          resolved_branch ?: "master");
              g_ptr_array_add (state->to_install, g_strdup (full_sdk));
            }
        }
    }

  if (state->to_install->len == 0)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "No runtime provided to bootstrap");
      g_warn_if_reached ();
      IDE_EXIT;
    }

  ide_task_run_in_thread (task, gbp_flatpak_runtime_provider_bootstrap);

  IDE_EXIT;
}

static IdeRuntime *
gbp_flatpak_runtime_provider_bootstrap_finish (IdeRuntimeProvider  *provider,
                                               GAsyncResult        *result,
                                               GError             **error)
{
  IdeRuntime *ret;

  IDE_ENTRY;

  g_return_val_if_fail (GBP_IS_FLATPAK_RUNTIME_PROVIDER (provider), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static gboolean
gbp_flatpak_runtime_provider_provides (IdeRuntimeProvider *provider,
                                       const char         *runtime_id)
{
  return g_str_has_prefix (runtime_id, "flatpak:");
}

static void
runtime_provider_iface_init (IdeRuntimeProviderInterface *iface)
{
  iface->load = gbp_flatpak_runtime_provider_load;
  iface->unload = gbp_flatpak_runtime_provider_unload;
  iface->bootstrap_async = gbp_flatpak_runtime_provider_bootstrap_async;
  iface->bootstrap_finish = gbp_flatpak_runtime_provider_bootstrap_finish;
  iface->provides = gbp_flatpak_runtime_provider_provides;
}
