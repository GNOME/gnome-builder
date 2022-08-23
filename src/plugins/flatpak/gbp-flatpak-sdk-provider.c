/* gbp-flatpak-sdk-provider.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-flatpak-sdk-provider"

#include "config.h"

#include <libide-gui.h>
#include <libide-threading.h>

#include "gbp-flatpak-client.h"
#include "gbp-flatpak-sdk.h"
#include "gbp-flatpak-sdk-provider.h"

#include "ipc-flatpak-service.h"
#include "ipc-flatpak-transfer-impl.h"

struct _GbpFlatpakSdkProvider
{
  IdeSdkProvider parent_instance;
};

G_DEFINE_FINAL_TYPE (GbpFlatpakSdkProvider, gbp_flatpak_sdk_provider, IDE_TYPE_SDK_PROVIDER)

static void
gbp_flatpak_sdk_provider_runtime_added_cb (GbpFlatpakSdkProvider *self,
                                           GVariant              *runtime_variant,
                                           IpcFlatpakService     *service)
{
  g_autoptr(GbpFlatpakSdk) sdk = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_SDK_PROVIDER (self));
  g_assert (runtime_variant != NULL);
  g_assert (IPC_IS_FLATPAK_SERVICE (service));

  if ((sdk = gbp_flatpak_sdk_new_from_variant (runtime_variant)))
    ide_sdk_provider_sdk_added (IDE_SDK_PROVIDER (self), IDE_SDK (sdk));

  IDE_EXIT;
}

static void
gbp_flatpak_sdk_provider_list_runtimes_cb (GObject      *object,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
  IpcFlatpakService *service = (IpcFlatpakService *)object;
  g_autoptr(GbpFlatpakSdkProvider) self = user_data;
  g_autoptr(GVariant) runtimes = NULL;
  g_autoptr(GError) error = NULL;
  guint n_children;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_FLATPAK_SDK_PROVIDER (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IPC_IS_FLATPAK_SERVICE (service));

  if (!ipc_flatpak_service_call_list_runtimes_finish (service, &runtimes, result, &error))
    {
      g_warning ("Failed to list flatpak runtimes: %s", error->message);
      IDE_EXIT;
    }

  n_children = g_variant_n_children (runtimes);

  for (guint i = 0; i < n_children; i++)
    {
      g_autoptr(GVariant) runtime = g_variant_get_child_value (runtimes, i);
      gbp_flatpak_sdk_provider_runtime_added_cb (self, runtime, service);
    }

  IDE_EXIT;
}

static void
gbp_flatpak_sdk_provider_get_service_cb (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
  GbpFlatpakClient *client = (GbpFlatpakClient *)object;
  g_autoptr(GbpFlatpakSdkProvider) self = user_data;
  g_autoptr(IpcFlatpakService) service = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_FLATPAK_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_FLATPAK_SDK_PROVIDER (self));

  if (!(service = gbp_flatpak_client_get_service_finish (client, result, &error)))
    {
      g_warning ("Failed to access gnome-builder-flatpak, cannot integrate SDK management.");
      IDE_EXIT;
    }

  g_assert (IPC_IS_FLATPAK_SERVICE (service));

  g_signal_connect_object (service,
                           "runtime-added",
                           G_CALLBACK (gbp_flatpak_sdk_provider_runtime_added_cb),
                           self,
                           G_CONNECT_SWAPPED);

  ipc_flatpak_service_call_list_runtimes (service,
                                          NULL,
                                          gbp_flatpak_sdk_provider_list_runtimes_cb,
                                          g_object_ref (self));

  IDE_EXIT;
}

static void
gbp_flatpak_sdk_provider_update_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  IpcFlatpakService *service = (IpcFlatpakService *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IPC_IS_FLATPAK_SERVICE (service));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ipc_flatpak_service_call_install_finish (service, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
gbp_flatpak_sdk_provider_update_async (IdeSdkProvider      *provider,
                                       IdeSdk              *sdk,
                                       IdeNotification     *notif,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  GbpFlatpakSdkProvider *self = (GbpFlatpakSdkProvider *)provider;
  g_autoptr(IpcFlatpakService) service = NULL;
  g_autoptr(IpcFlatpakTransfer) transfer = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *guid = NULL;
  g_autofree char *transfer_path = NULL;
  GbpFlatpakClient *client;
  const char * refs[2] = { NULL };
  GtkWindow *window;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_SDK_PROVIDER (self));
  g_assert (GBP_IS_FLATPAK_SDK (sdk));
  g_assert (IDE_IS_NOTIFICATION (notif));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  client = gbp_flatpak_client_get_default ();
  window = gtk_application_get_active_window (GTK_APPLICATION (IDE_APPLICATION_DEFAULT));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_flatpak_sdk_provider_update_async);

  if (!(service = gbp_flatpak_client_get_service (client, cancellable, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  refs[0] = gbp_flatpak_sdk_get_id (GBP_FLATPAK_SDK (sdk));

  guid = g_dbus_generate_guid ();
  transfer_path = g_strdup_printf ("/org/gnome/Builder/Flatpak/Transfer/%s", guid);
  transfer = ipc_flatpak_transfer_impl_new_simple (window);
  if (cancellable != NULL)
    g_signal_connect_object (cancellable,
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
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  g_object_bind_property (transfer, "fraction", notif, "progress", G_BINDING_SYNC_CREATE);
  g_object_bind_property (transfer, "message", notif, "body", G_BINDING_DEFAULT);
  ide_task_set_task_data (task, g_object_ref (transfer), g_object_unref);

  ipc_flatpak_service_call_install (service,
                                    refs,
                                    FALSE,
                                    transfer_path,
                                    "",
                                    cancellable,
                                    gbp_flatpak_sdk_provider_update_cb,
                                    g_steal_pointer (&task));

  IDE_EXIT;
}

static gboolean
gbp_flatpak_sdk_provider_update_finish (IdeSdkProvider  *provider,
                                        GAsyncResult    *result,
                                        GError         **error)
{
  GError *local_error = NULL;
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_SDK_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  if (!(ret = ide_task_propagate_boolean (IDE_TASK (result), &local_error)))
    {
      g_warning ("Failed to update SDK: %s", local_error->message);
      g_propagate_error (error, local_error);
    }

  IDE_RETURN (ret);
}

static void
gbp_flatpak_sdk_provider_constructed (GObject *object)
{
  GbpFlatpakSdkProvider *self = (GbpFlatpakSdkProvider *)object;
  GbpFlatpakClient *client;

  IDE_ENTRY;

  G_OBJECT_CLASS (gbp_flatpak_sdk_provider_parent_class)->constructed (object);

  client = gbp_flatpak_client_get_default ();
  gbp_flatpak_client_get_service_async (client,
                                        NULL,
                                        gbp_flatpak_sdk_provider_get_service_cb,
                                        g_object_ref (self));

  IDE_EXIT;
}

static void
gbp_flatpak_sdk_provider_class_init (GbpFlatpakSdkProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeSdkProviderClass *sdk_provider_class = IDE_SDK_PROVIDER_CLASS (klass);

  object_class->constructed = gbp_flatpak_sdk_provider_constructed;

  sdk_provider_class->update_async = gbp_flatpak_sdk_provider_update_async;
  sdk_provider_class->update_finish = gbp_flatpak_sdk_provider_update_finish;
}

static void
gbp_flatpak_sdk_provider_init (GbpFlatpakSdkProvider *self)
{
}
