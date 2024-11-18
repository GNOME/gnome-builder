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
#include "gbp-flatpak-manifest.h"
#include "gbp-flatpak-runtime.h"
#include "gbp-flatpak-runtime-provider.h"
#include "gbp-flatpak-util.h"

#include "daemon/ipc-flatpak-service.h"
#include "daemon/ipc-flatpak-transfer.h"
#include "daemon/ipc-flatpak-util.h"

#include "ipc-flatpak-transfer-impl.h"

struct _GbpFlatpakRuntimeProvider
{
  IdeRuntimeProvider parent_instance;
};

G_DEFINE_FINAL_TYPE (GbpFlatpakRuntimeProvider, gbp_flatpak_runtime_provider, IDE_TYPE_RUNTIME_PROVIDER)

static gboolean
gbp_flatpak_runtime_provider_provides (IdeRuntimeProvider *provider,
                                       const char         *runtime_id)
{
  return g_str_has_prefix (runtime_id, "flatpak:");
}

static void
on_runtime_added_cb (GbpFlatpakRuntimeProvider *self,
                     GVariant                  *info,
                     IpcFlatpakService         *service)

{
  g_autoptr(GbpFlatpakRuntime) runtime = NULL;
  g_autoptr(IdeContext) context = NULL;
  const gchar *name;
  const gchar *arch;
  const gchar *branch;
  const gchar *sdk_name;
  const gchar *sdk_branch;
  const gchar *deploy_dir;
  const gchar *metadata;
  gboolean is_extension;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (self));
  g_assert (info != NULL);
  g_assert (IPC_IS_FLATPAK_SERVICE (service));
  g_assert (g_variant_is_of_type (info, RUNTIME_VARIANT_TYPE));

  if (!runtime_variant_parse (info,
                              &name, &arch, &branch,
                              &sdk_name, &sdk_branch,
                              &deploy_dir,
                              &metadata,
                              &is_extension))
    IDE_EXIT;

  /* Ignore extensions for now */
  if (is_extension)
    IDE_EXIT;

  /* Ignore things we don't want in this list */
  if (gbp_flatpak_is_ignored (name))
    IDE_EXIT;

  context = ide_object_ref_context (IDE_OBJECT (self));
  runtime = gbp_flatpak_runtime_new (name,
                                     arch,
                                     branch,
                                     sdk_name,
                                     sdk_branch,
                                     deploy_dir,
                                     metadata,
                                     is_extension);

  g_debug ("Discovered Flatpak runtime %s/%s/%s using SDK %s//%s from %s",
           name, arch, branch, sdk_name, sdk_branch, deploy_dir);

  ide_runtime_provider_add (IDE_RUNTIME_PROVIDER (self), IDE_RUNTIME (runtime));

  IDE_EXIT;
}

typedef struct _Load
{
  GbpFlatpakRuntimeProvider *self;
  DexPromise *promise;
} Load;

static Load *
load_new (GbpFlatpakRuntimeProvider *self,
          DexPromise                *promise)
{
  Load *load = g_new0 (Load, 1);

  load->self = g_object_ref (self);
  load->promise = dex_ref (promise);

  return load;
}

static void
load_free (Load *load)
{
  g_clear_object (&load->self);
  dex_clear (&load->promise);
  g_free (load);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (Load, load_free)

static void
gbp_flatpak_runtime_provider_load_list_runtimes_cb (GObject      *object,
                                                    GAsyncResult *result,
                                                    gpointer      user_data)
{
  IpcFlatpakService *service = (IpcFlatpakService *)object;
  g_autoptr(Load) load = user_data;
  g_autoptr(GVariant) runtimes = NULL;
  g_autoptr(GError) error = NULL;
  GVariantIter iter;
  GVariant *info;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IPC_IS_FLATPAK_SERVICE (service));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (load != NULL);
  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (load->self));
  g_assert (DEX_IS_PROMISE (load->promise));

  if (!ipc_flatpak_service_call_list_runtimes_finish (service, &runtimes, result, &error))
    {
      dex_promise_reject (load->promise, g_steal_pointer (&error));
      IDE_EXIT;
    }

  g_variant_iter_init (&iter, runtimes);

  while ((info = g_variant_iter_next_value (&iter)))
    {
      on_runtime_added_cb (load->self, info, service);
      g_variant_unref (info);
    }

  dex_promise_resolve_boolean (load->promise, TRUE);

  IDE_EXIT;
}

static DexFuture *
gbp_flatpak_runtime_provider_load (IdeRuntimeProvider *provider)
{
  GbpFlatpakRuntimeProvider *self = (GbpFlatpakRuntimeProvider *)provider;
  g_autoptr(IpcFlatpakService) service = NULL;
  g_autoptr(IdeContext) context = NULL;
  GbpFlatpakClient *client;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (self));

  if ((context = ide_object_ref_context (IDE_OBJECT (self))) &&
      (client = gbp_flatpak_client_get_default ()) &&
      (service = gbp_flatpak_client_get_service (client, NULL, NULL)))
    {
      g_autoptr(DexPromise) promise = dex_promise_new ();

      g_signal_connect_object (service,
                               "runtime-added",
                               G_CALLBACK (on_runtime_added_cb),
                               provider,
                               G_CONNECT_SWAPPED);

      ipc_flatpak_service_call_list_runtimes (service,
                                              NULL,
                                              gbp_flatpak_runtime_provider_load_list_runtimes_cb,
                                              load_new (self, promise));

      IDE_RETURN (DEX_FUTURE (g_steal_pointer (&promise)));
    }

  IDE_RETURN (dex_future_new_reject (G_IO_ERROR,
                                     G_IO_ERROR_FAILED,
                                     "Failed to locate flatpak service"));
}

typedef struct
{
  char               *runtime_id;
  char               *transfer_path;
  char               *sdk;
  GPtrArray          *to_install;
  IpcFlatpakTransfer *transfer;
  IpcFlatpakService  *service;
  IdeNotification    *notif;
} Bootstrap;

static void
bootstrap_free (Bootstrap *b)
{
  g_clear_pointer (&b->runtime_id, g_free);
  g_clear_pointer (&b->sdk, g_free);
  g_clear_pointer (&b->transfer_path, g_free);
  g_clear_object (&b->notif);
  g_clear_object (&b->service);
  g_clear_object (&b->transfer);
  g_clear_pointer (&b->to_install, g_ptr_array_unref);
  g_slice_free (Bootstrap, b);
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
                               G_IO_ERROR_NOT_FOUND,
                               "Failed to locate %s after installation",
                               state->runtime_id);
  else
    ide_task_return_pointer (task, g_steal_pointer (&runtime), g_object_unref);

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
  g_assert (state != NULL);
  g_assert (!state->notif || IDE_IS_NOTIFICATION (state->notif));
  g_assert (!state->service || IPC_IS_FLATPAK_SERVICE (state->service));
  g_assert (!state->transfer || IPC_IS_FLATPAK_TRANSFER (state->transfer));

  if (state->to_install->len > 0)
    {
      g_autofree char *detailed_action_name = NULL;
      g_autoptr(GIcon) icon = g_themed_icon_new ("builder-build-stop-symbolic");

      g_assert (IDE_IS_NOTIFICATION (state->notif));
      g_assert (IPC_IS_FLATPAK_SERVICE (state->service));
      g_assert (IPC_IS_FLATPAK_TRANSFER (state->transfer));

      /* Register an action that can be cancelled */
      detailed_action_name = ide_application_create_cancel_action (IDE_APPLICATION_DEFAULT,
                                                                   ide_task_get_cancellable (task));
      ide_notification_add_button (state->notif, _("_Cancel"), icon, detailed_action_name);
      ide_notification_attach (state->notif, IDE_OBJECT (self));

      ipc_flatpak_service_call_install (state->service,
                                        (const char * const *)state->to_install->pdata,
                                        TRUE,
                                        state->transfer_path,
                                        "",
                                        ide_task_get_cancellable (task),
                                        gbp_flatpak_runtime_provider_bootstrap_install_cb,
                                        g_object_ref (task));
    }
  else
    {
      if (!(runtime = find_runtime (self, state->runtime_id)))
        ide_task_return_new_error (task,
                                   G_IO_ERROR,
                                   G_IO_ERROR_NOT_FOUND,
                                   "Failed to locate runtime %s",
                                   state->runtime_id);
      else
        ide_task_return_pointer (task, g_steal_pointer (&runtime), g_object_unref);
    }

  IDE_RETURN (G_SOURCE_REMOVE);
}

static void
gbp_flatpak_runtime_provider_bootstrap (IdeTask      *task,
                                        gpointer      source_object,
                                        gpointer      task_data,
                                        GCancellable *cancellable)
{
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(IpcFlatpakService) service = NULL;
  g_autoptr(IpcFlatpakTransfer) transfer = NULL;
  g_autoptr(IdeNotification) notif = NULL;
  Bootstrap *state = task_data;
  g_autoptr(GString) debug_install = NULL;
  GbpFlatpakClient *client;

  IDE_ENTRY;

  g_assert (IDE_IS_TASK (task));
  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (source_object));
  g_assert (state != NULL);
  g_assert (state->to_install != NULL);
  g_assert (state->to_install->len > 0);

  if (!(context = ide_object_ref_context (source_object)) ||
      !(client = gbp_flatpak_client_get_default ()) ||
      !(service = gbp_flatpak_client_get_service (client, NULL, NULL)))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_CANCELLED,
                                 "Operation was cancelled");
      IDE_GOTO (failure);
    }

  debug_install = g_string_new (NULL);
  for (guint i = 0; i < state->to_install->len; i++)
    g_string_append_printf (debug_install,
                            "%s, ",
                            (char *)g_ptr_array_index(state->to_install, i));
  if (debug_install->len > 2)
    g_string_truncate (debug_install, debug_install->len-2);
  g_debug ("Needed flatpak runtimes: %s", debug_install->str);

  /* Filter out anything we can't find or is already installed */
  for (guint i = state->to_install->len; i > 0; i--)
    {
      char *id = g_ptr_array_index (state->to_install, i-1);
      gboolean is_known = FALSE;
      gint64 size = 0;

      /* If this is a plain SDK extension name, resolve it now */
      if (strchr (id, '/') == NULL)
        {
          g_autofree char *resolved = NULL;

          if (ipc_flatpak_service_call_resolve_extension_sync (service, state->sdk, id, &resolved, NULL, NULL))
            {
              G_GNUC_UNUSED g_autofree char *old = g_steal_pointer (&id);
              g_ptr_array_index (state->to_install, i-1) = id = g_steal_pointer (&resolved);
            }
        }

      /* If we're missing runtime/ (or app/) prefix, add it now */
      if (!g_str_has_prefix (id, "runtime/") && !g_str_has_prefix (id, "app/"))
        {
          g_autofree char *old = id;
          g_ptr_array_index (state->to_install, i-1) = id = g_strdup_printf ("runtime/%s", old);
        }

      /* Ignore this unless we know it can be installed from a peer */
      if (!ipc_flatpak_service_call_runtime_is_known_sync (service, id, &is_known, &size, NULL, NULL) ||
          is_known == FALSE)
        g_ptr_array_remove_index (state->to_install, i-1);
    }

  /* Now create a transfer to install the rest */
  if (state->to_install->len > 0)
    {
      g_autofree char *guid = g_dbus_generate_guid ();
      g_autofree char *transfer_path = g_strdup_printf ("/org/gnome/Builder/Flatpak/Transfer/%s", guid);
      g_autoptr(GError) error = NULL;

      notif = ide_notification_new ();
      ide_notification_set_icon_name (notif, "system-software-install-symbolic");
      ide_notification_set_title (notif, _("Installing Necessary SDKs"));
      ide_notification_set_body (notif, _("Builder is installing Software Development Kits necessary for building your application."));
      ide_notification_set_has_progress (notif, TRUE);
      ide_notification_set_progress_is_imprecise (notif, FALSE);

      g_signal_connect_object (task,
                               "notify::completed",
                               G_CALLBACK (ide_notification_withdraw),
                               notif,
                               G_CONNECT_SWAPPED);

      transfer = ipc_flatpak_transfer_impl_new (context);
      g_signal_connect_object (ide_task_get_cancellable (task),
                               "cancelled",
                               G_CALLBACK (ipc_flatpak_transfer_emit_cancel),
                               transfer,
                               G_CONNECT_SWAPPED);
      g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (transfer),
                                        g_dbus_proxy_get_connection (G_DBUS_PROXY (service)),
                                        transfer_path,
                                        &error);

      if (error != NULL)
        {
          g_warning ("Failed to register transfer object on D-Bus: %s", error->message);
          g_clear_error (&error);
        }

      g_object_bind_property (transfer, "fraction", notif, "progress", G_BINDING_SYNC_CREATE);
      g_object_bind_property (transfer, "message", notif, "body", G_BINDING_DEFAULT);

      state->service = g_object_ref (service);
      state->notif = g_object_ref (notif);
      state->transfer = g_object_ref (transfer);
      state->transfer_path = g_strdup (transfer_path);

      g_ptr_array_add (state->to_install, NULL);
    }

  g_timeout_add_full (G_PRIORITY_DEFAULT,
                      0,
                      gbp_flatpak_runtime_provider_bootstrap_complete,
                      g_object_ref (task),
                      g_object_unref);

  IDE_EXIT;

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
  g_autofree char *full_docs = NULL;
  g_autofree char *arch = NULL;
  Bootstrap *state;
  IdeConfig *config;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  arch = ide_pipeline_dup_arch (pipeline);
  config = ide_pipeline_get_config (pipeline);

  state = g_slice_new0 (Bootstrap);
  state->runtime_id = g_strdup (ide_config_get_runtime_id (config));
  state->to_install = g_ptr_array_new_with_free_func (g_free);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_flatpak_runtime_provider_bootstrap_async);
  ide_task_set_task_data (task, state, bootstrap_free);
  ide_task_set_return_on_cancel (task, FALSE);
  ide_task_set_release_on_propagate (task, FALSE);

  /* Collect all of the runtimes that could be needed */
  if (GBP_IS_FLATPAK_MANIFEST (config))
    {
      const char *sdk = gbp_flatpak_manifest_get_sdk (GBP_FLATPAK_MANIFEST (config));
      const char *platform = gbp_flatpak_manifest_get_platform (GBP_FLATPAK_MANIFEST (config));
      const char *branch = gbp_flatpak_manifest_get_branch (GBP_FLATPAK_MANIFEST (config));
      const char * const *extensions = gbp_flatpak_manifest_get_sdk_extensions (GBP_FLATPAK_MANIFEST (config));
      const char *base = gbp_flatpak_manifest_get_base (GBP_FLATPAK_MANIFEST (config));
      const char *base_version = gbp_flatpak_manifest_get_base_version (GBP_FLATPAK_MANIFEST (config));

      if (sdk == NULL)
        sdk = platform;

      if (branch == NULL)
        branch = "master";

      full_sdk = g_strdup_printf ("runtime/%s/%s/%s", sdk, arch, branch);
      full_platform = g_strdup_printf ("runtime/%s/%s/%s", platform, arch, branch);
      full_docs = g_strdup_printf ("runtime/%s.Docs/%s/%s", sdk, arch, branch);

      g_ptr_array_add (state->to_install, g_strdup (full_sdk));
      if (g_strcmp0 (full_sdk, full_platform) != 0)
        g_ptr_array_add (state->to_install, g_strdup (full_platform));

      if (base != NULL && base_version != NULL)
        g_ptr_array_add (state->to_install,
                         g_strdup_printf ("app/%s/%s/%s",
                                          base,
                                          arch,
                                          base_version));

      if (extensions != NULL)
        {
          for (guint i = 0; extensions[i]; i++)
            g_ptr_array_add (state->to_install, g_strdup (extensions[i]));
        }

      g_ptr_array_add (state->to_install, g_strdup (full_docs));
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

  state->sdk = g_strdup (full_sdk);

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

static void
gbp_flatpak_runtime_provider_bootstrap_cb (GObject      *object,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
  GbpFlatpakRuntimeProvider *self = (GbpFlatpakRuntimeProvider *)object;
  g_autoptr(DexPromise) promise = user_data;
  g_autoptr(IdeRuntime) runtime = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (DEX_IS_PROMISE (promise));

  if (!(runtime = gbp_flatpak_runtime_provider_bootstrap_finish (IDE_RUNTIME_PROVIDER (self), result, &error)))
    dex_promise_reject (promise, g_error_copy (error));
  else
    dex_promise_resolve_object (promise, g_steal_pointer (&runtime));

  IDE_EXIT;
}

static DexFuture *
gbp_flatpak_runtime_provider_bootstrap_runtime (IdeRuntimeProvider *provider,
                                                IdePipeline        *pipeline)
{
  g_autoptr(DexPromise) promise = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_RUNTIME_PROVIDER (provider));
  g_assert (IDE_IS_PIPELINE (pipeline));

  /* This is just a wrapper until bootstrap_async() is turned into
   * a DexFuture-based implementation. To do that well, we really need
   * to have a gdbus-codegen that can generate DexFuture or improve how
   * we can wrap calls w/ DexAsyncPair.
   */

  promise = dex_promise_new_cancellable ();
  gbp_flatpak_runtime_provider_bootstrap_async (provider,
                                                pipeline,
                                                dex_promise_get_cancellable (promise),
                                                gbp_flatpak_runtime_provider_bootstrap_cb,
                                                dex_ref (promise));

  IDE_RETURN (DEX_FUTURE (g_steal_pointer (&promise)));
}

static void
gbp_flatpak_runtime_provider_class_init (GbpFlatpakRuntimeProviderClass *klass)
{
  IdeRuntimeProviderClass *runtime_provider_class = IDE_RUNTIME_PROVIDER_CLASS (klass);

  runtime_provider_class->load = gbp_flatpak_runtime_provider_load;
  runtime_provider_class->bootstrap_runtime = gbp_flatpak_runtime_provider_bootstrap_runtime;
  runtime_provider_class->provides = gbp_flatpak_runtime_provider_provides;
}

static void
gbp_flatpak_runtime_provider_init (GbpFlatpakRuntimeProvider *self)
{
}
