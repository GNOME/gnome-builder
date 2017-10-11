/* ide-build-stage-transfer.c
 *
 * Copyright © 2016 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-build-stage-transfer"

#include <glib/gi18n.h>

#include "ide-context.h"
#include "ide-debug.h"

#include "application/ide-application.h"
#include "buildsystem/ide-build-stage-transfer.h"
#include "buildsystem/ide-build-pipeline.h"
#include "transfers/ide-transfer-manager.h"
#include "transfers/ide-transfer.h"

struct _IdeBuildStageTransfer
{
  IdeBuildStage  parent_instnace;
  IdeTransfer   *transfer;
  guint          disable_when_metered : 1;
};

enum {
  PROP_0,
  PROP_TRANSFER,
  PROP_DISABLE_WHEN_METERED,
  N_PROPS
};

G_DEFINE_TYPE (IdeBuildStageTransfer, ide_build_stage_transfer, IDE_TYPE_BUILD_STAGE)

static GParamSpec *properties [N_PROPS];

static void
ide_build_stage_transfer_execute_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  IdeTransferManager *transfer_manager = (IdeTransferManager *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_TRANSFER_MANAGER (transfer_manager));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!ide_transfer_manager_execute_finish (transfer_manager, result, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_boolean (task, TRUE);
}

static void
ide_build_stage_transfer_execute_async (IdeBuildStage       *stage,
                                        IdeBuildPipeline    *pipeline,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  IdeBuildStageTransfer *self = (IdeBuildStageTransfer *)stage;
  g_autoptr(GTask) task = NULL;
  IdeTransferManager *transfer_manager;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_STAGE_TRANSFER (self));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_build_stage_transfer_execute_async);

  if (ide_transfer_get_completed (self->transfer))
    {
      g_task_return_boolean (task, TRUE);
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
              g_task_return_new_error (task,
                                       IDE_TRANSFER_ERROR,
                                       IDE_TRANSFER_ERROR_CONNECTION_IS_METERED,
                                       _("Cannot execute transfer while on metered connection"));
              IDE_EXIT;
            }
        }
    }

  transfer_manager = ide_application_get_transfer_manager (IDE_APPLICATION_DEFAULT);

  ide_transfer_manager_execute_async (transfer_manager,
                                      self->transfer,
                                      cancellable,
                                      ide_build_stage_transfer_execute_cb,
                                      g_steal_pointer (&task));

  IDE_EXIT;
}

static gboolean
ide_build_stage_transfer_execute_finish (IdeBuildStage  *stage,
                                         GAsyncResult   *result,
                                         GError        **error)
{
  g_assert (IDE_IS_BUILD_STAGE_TRANSFER (stage));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
ide_build_stage_transfer_finalize (GObject *object)
{
  IdeBuildStageTransfer *self = (IdeBuildStageTransfer *)object;

  g_clear_object (&self->transfer);

  G_OBJECT_CLASS (ide_build_stage_transfer_parent_class)->finalize (object);
}

static void
ide_build_stage_transfer_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  IdeBuildStageTransfer *self = (IdeBuildStageTransfer *)object;

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
ide_build_stage_transfer_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  IdeBuildStageTransfer *self = (IdeBuildStageTransfer *)object;

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
ide_build_stage_transfer_class_init (IdeBuildStageTransferClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeBuildStageClass *build_stage_class = IDE_BUILD_STAGE_CLASS (klass);

  object_class->finalize = ide_build_stage_transfer_finalize;
  object_class->get_property = ide_build_stage_transfer_get_property;
  object_class->set_property = ide_build_stage_transfer_set_property;

  build_stage_class->execute_async = ide_build_stage_transfer_execute_async;
  build_stage_class->execute_finish = ide_build_stage_transfer_execute_finish;

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
ide_build_stage_transfer_init (IdeBuildStageTransfer *self)
{
  self->disable_when_metered = TRUE;
}
