/* ide-pipeline-stage-transfer.c
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

#define G_LOG_DOMAIN "ide-pipeline-stage-transfer"

#include "config.h"

#include <libide-threading.h>
#include <glib/gi18n.h>

#include "ide-pipeline-stage-transfer.h"
#include "ide-pipeline.h"
#include "ide-transfer.h"

struct _IdePipelineStageTransfer
{
  IdePipelineStage  parent_instance;
  IdeTransfer   *transfer;
  guint          disable_when_metered : 1;
};

enum {
  PROP_0,
  PROP_TRANSFER,
  PROP_DISABLE_WHEN_METERED,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdePipelineStageTransfer, ide_pipeline_stage_transfer, IDE_TYPE_PIPELINE_STAGE)

static GParamSpec *properties [N_PROPS];

static void
ide_pipeline_stage_transfer_execute_cb (GObject      *object,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
  IdeTransferManager *transfer_manager = (IdeTransferManager *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeNotification *notif;

  g_assert (IDE_IS_TRANSFER_MANAGER (transfer_manager));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  notif = ide_task_get_task_data (task);
  g_assert (IDE_IS_NOTIFICATION (notif));

  ide_notification_withdraw (notif);

  if (!ide_transfer_manager_execute_finish (transfer_manager, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);
}

static void
ide_pipeline_stage_transfer_notify_completed_cb (IdeTask                  *task,
                                                 GParamSpec               *pspec,
                                                 IdePipelineStageTransfer *transfer)
{
  g_assert (IDE_IS_TASK (task));
  g_assert (IDE_IS_PIPELINE_STAGE_TRANSFER (transfer));

  ide_pipeline_stage_set_active (IDE_PIPELINE_STAGE (transfer), FALSE);
}

static void
ide_pipeline_stage_transfer_build_async (IdePipelineStage    *stage,
                                         IdePipeline         *pipeline,
                                         GCancellable        *cancellable,
                                         GAsyncReadyCallback  callback,
                                         gpointer             user_data)
{
  IdePipelineStageTransfer *self = (IdePipelineStageTransfer *)stage;
  g_autoptr(IdeNotification) notif = NULL;
  g_autoptr(IdeNotifications) notifs = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(IdeContext) context = NULL;
  const gchar *icon_name;

  IDE_ENTRY;

  g_assert (IDE_IS_PIPELINE_STAGE_TRANSFER (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_pipeline_stage_transfer_build_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  g_signal_connect (task,
                    "notify::completed",
                    G_CALLBACK (ide_pipeline_stage_transfer_notify_completed_cb),
                    self);

  ide_pipeline_stage_set_active (stage, TRUE);

  if (ide_transfer_get_completed (self->transfer))
    {
      ide_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  if (self->disable_when_metered)
    {
      GNetworkMonitor *monitor = g_network_monitor_get_default ();

      if (g_network_monitor_get_network_metered (monitor))
        {
          g_autoptr(GSettings) settings = g_settings_new ("org.gnome.builder.build");

          if (!g_settings_get_boolean (settings, "allow-network-when-metered"))
            {
              ide_task_return_new_error (task,
                                         IDE_TRANSFER_ERROR,
                                         IDE_TRANSFER_ERROR_CONNECTION_IS_METERED,
                                         _("Cannot build transfer while on metered connection"));
              IDE_EXIT;
            }
        }
    }

  notif = ide_notification_new ();
  ide_notification_set_has_progress (notif, TRUE);
  g_object_bind_property (self->transfer, "title", notif, "title", G_BINDING_SYNC_CREATE);
  g_object_bind_property (self->transfer, "status", notif, "body", G_BINDING_SYNC_CREATE);
  g_object_bind_property (self->transfer, "progress", notif, "progress", G_BINDING_SYNC_CREATE);

  if ((icon_name = ide_transfer_get_icon_name (self->transfer)))
    {
      g_autoptr(GIcon) icon = g_themed_icon_new (icon_name);
      ide_notification_set_icon (notif, icon);
    }

  context = ide_object_ref_context (IDE_OBJECT (self));
  notifs = ide_object_get_child_typed (IDE_OBJECT (context), IDE_TYPE_NOTIFICATIONS);
  ide_notifications_add_notification (notifs, notif);

  ide_task_set_task_data (task, g_steal_pointer (&notif), g_object_unref);

  ide_transfer_manager_execute_async (NULL,
                                      self->transfer,
                                      cancellable,
                                      ide_pipeline_stage_transfer_execute_cb,
                                      g_steal_pointer (&task));

  IDE_EXIT;
}

static gboolean
ide_pipeline_stage_transfer_build_finish (IdePipelineStage  *stage,
                                          GAsyncResult      *result,
                                          GError           **error)
{
  g_assert (IDE_IS_PIPELINE_STAGE_TRANSFER (stage));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
ide_pipeline_stage_transfer_finalize (GObject *object)
{
  IdePipelineStageTransfer *self = (IdePipelineStageTransfer *)object;

  g_clear_object (&self->transfer);

  G_OBJECT_CLASS (ide_pipeline_stage_transfer_parent_class)->finalize (object);
}

static void
ide_pipeline_stage_transfer_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  IdePipelineStageTransfer *self = (IdePipelineStageTransfer *)object;

  switch (prop_id)
    {
    case PROP_DISABLE_WHEN_METERED:
      g_value_set_boolean (value, self->disable_when_metered);
      break;

    case PROP_TRANSFER:
      g_value_set_object (value, self->transfer);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_pipeline_stage_transfer_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  IdePipelineStageTransfer *self = (IdePipelineStageTransfer *)object;

  switch (prop_id)
    {
    case PROP_DISABLE_WHEN_METERED:
      self->disable_when_metered = g_value_get_boolean (value);
      break;

    case PROP_TRANSFER:
      self->transfer = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_pipeline_stage_transfer_class_init (IdePipelineStageTransferClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdePipelineStageClass *build_stage_class = IDE_PIPELINE_STAGE_CLASS (klass);

  object_class->finalize = ide_pipeline_stage_transfer_finalize;
  object_class->get_property = ide_pipeline_stage_transfer_get_property;
  object_class->set_property = ide_pipeline_stage_transfer_set_property;

  build_stage_class->build_async = ide_pipeline_stage_transfer_build_async;
  build_stage_class->build_finish = ide_pipeline_stage_transfer_build_finish;

  properties [PROP_DISABLE_WHEN_METERED] =
    g_param_spec_boolean ("disable-when-metered",
                          "Disable when Metered",
                          "If the transfer should fail when on a metered connection",
                          TRUE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_TRANSFER] =
    g_param_spec_object ("transfer",
                         "Transfer",
                         "The transfer to perform as part of the stage",
                         IDE_TYPE_TRANSFER,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_pipeline_stage_transfer_init (IdePipelineStageTransfer *self)
{
  self->disable_when_metered = TRUE;
}
