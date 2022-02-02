/* gbp-flatpak-sdk-stage.c
 *
 * Copyright 2021 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-flatpak-sdk-stage"

#include "config.h"

#include <glib/gi18n.h>

#include "gbp-flatpak-client.h"
#include "gbp-flatpak-sdk-stage.h"
#include "ipc-flatpak-service.h"
#include "ipc-flatpak-transfer-impl.h"

struct _GbpFlatpakSdkStage
{
  IdePipelineStage parent_instance;
  char **sdks;
};

G_DEFINE_TYPE (GbpFlatpakSdkStage, gbp_flatpak_sdk_stage, IDE_TYPE_PIPELINE_STAGE)

static void
gbp_flatpak_sdk_stage_install_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  IpcFlatpakService *service = (IpcFlatpakService *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  GbpFlatpakSdkStage *self;

  IDE_ENTRY;

  g_assert (IPC_IS_FLATPAK_SERVICE (service));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);

  if (!ipc_flatpak_service_call_install_finish (service, result, &error))
    ide_object_warning (IDE_OBJECT (self), _("Failed to update SDKs: %s"), error->message);

  ide_pipeline_stage_set_active (IDE_PIPELINE_STAGE (self), FALSE);
  ide_pipeline_stage_set_completed (IDE_PIPELINE_STAGE (self), TRUE);

  ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
gbp_flatpak_sdk_stage_build_async (IdePipelineStage    *stage,
                                   IdePipeline         *pipeline,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  GbpFlatpakSdkStage *self = (GbpFlatpakSdkStage *)stage;
  g_autofree char *guid = NULL;
  g_autofree char *transfer_path = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(IpcFlatpakTransfer) transfer = NULL;
  g_autoptr(IdeNotification) notif = NULL;
  g_autoptr(GError) error = NULL;
  IpcFlatpakService *service;
  GbpFlatpakClient *client;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_SDK_STAGE (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (IDE_IS_PIPELINE (pipeline));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_flatpak_sdk_stage_build_async);

  context = ide_object_get_context (IDE_OBJECT (pipeline));
  client = gbp_flatpak_client_get_default ();

  if (!(service = gbp_flatpak_client_get_service (client, cancellable, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  guid = g_dbus_generate_guid ();
  transfer_path = g_strdup_printf ("/org/gnome/Builder/Flatpak/Transfer/%s", guid);

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
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  ide_task_set_task_data (task, g_object_ref (transfer), g_object_unref);
  ide_pipeline_stage_set_active (stage, TRUE);

  notif = ide_notification_new ();
  ide_notification_set_icon_name (notif, "builder-sdk-symbolic");
  ide_notification_set_title (notif, _("Updating Necessary SDKs"));
  ide_notification_set_body (notif, _("Builder is updating Software Development Kits necessary for building your application."));
  ide_notification_set_has_progress (notif, TRUE);
  ide_notification_set_progress_is_imprecise (notif, FALSE);

  g_signal_connect_object (task,
                           "notify::completed",
                           G_CALLBACK (ide_notification_withdraw),
                           notif,
                           G_CONNECT_SWAPPED);

  g_object_bind_property (transfer, "fraction", notif, "progress", G_BINDING_SYNC_CREATE);
  g_object_bind_property (transfer, "message", notif, "body", G_BINDING_DEFAULT);

  ide_notification_attach (notif, IDE_OBJECT (self));

  ipc_flatpak_service_call_install (service,
                                    (const char * const *)self->sdks,
                                    FALSE,
                                    transfer_path,
                                    "",
                                    ide_task_get_cancellable (task),
                                    gbp_flatpak_sdk_stage_install_cb,
                                    g_object_ref (task));

  IDE_EXIT;
}

static gboolean
gbp_flatpak_sdk_stage_build_finish (IdePipelineStage  *stage,
                                    GAsyncResult      *result,
                                    GError           **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_SDK_STAGE (stage));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
gbp_flatpak_sdk_stage_query (IdePipelineStage *stage,
                             IdePipeline      *pipeline,
                             GPtrArray        *targets,
                             GCancellable     *cancellable)
{
  ide_pipeline_stage_set_completed (stage, FALSE);
}

static void
gbp_flatpak_sdk_stage_finalize (GObject *object)
{
  GbpFlatpakSdkStage *self = (GbpFlatpakSdkStage *)object;

  g_clear_pointer (&self->sdks, g_strfreev);

  G_OBJECT_CLASS (gbp_flatpak_sdk_stage_parent_class)->finalize (object);
}

static void
gbp_flatpak_sdk_stage_class_init (GbpFlatpakSdkStageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdePipelineStageClass *stage_class = IDE_PIPELINE_STAGE_CLASS (klass);

  stage_class->build_async = gbp_flatpak_sdk_stage_build_async;
  stage_class->build_finish = gbp_flatpak_sdk_stage_build_finish;
  stage_class->query = gbp_flatpak_sdk_stage_query;

  object_class->finalize = gbp_flatpak_sdk_stage_finalize;
}

static void
gbp_flatpak_sdk_stage_init (GbpFlatpakSdkStage *self)
{
}

GbpFlatpakSdkStage *
gbp_flatpak_sdk_stage_new (const char * const *sdks)
{
  GbpFlatpakSdkStage *self;
  g_autofree char *name = NULL;

  g_return_val_if_fail (sdks != NULL, NULL);
  g_return_val_if_fail (sdks[0] != NULL, NULL);

  self = g_object_new (GBP_TYPE_FLATPAK_SDK_STAGE,
                       "name", _("Updating SDK Runtime"),
                       "transient", TRUE,
                       NULL);
  self->sdks = g_strdupv ((char **)sdks);

  return self;
}
