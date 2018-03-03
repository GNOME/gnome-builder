/* ide-build-manager.c
 *
 * Copyright © 2016-2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-build-manager"

#include <dazzle.h>
#include <glib/gi18n.h>

#include "ide-context.h"
#include "ide-debug.h"

#include "buffers/ide-buffer.h"
#include "buffers/ide-buffer-manager.h"
#include "buildsystem/ide-build-manager.h"
#include "buildsystem/ide-build-pipeline.h"
#include "buildsystem/ide-build-private.h"
#include "config/ide-configuration-manager.h"
#include "config/ide-configuration.h"
#include "devices/ide-device.h"
#include "devices/ide-device-manager.h"
#include "diagnostics/ide-diagnostic.h"
#include "diagnostics/ide-diagnostics-manager.h"
#include "runtimes/ide-runtime.h"
#include "runtimes/ide-runtime-manager.h"

/**
 * SECTION:ide-build-manager
 * @title: IdeBuildManager
 * @short_description: Manages the active build configuration and pipeline
 *
 * The #IdeBuildManager is responsible for managing the active build pipeline
 * as well as providing common high-level actions to plugins.
 *
 * You can use various async operations such as
 * ide_build_manager_execute_async(), ide_build_manager_clean_async(), or
 * ide_build_manager_rebuild_async() to build, clean, and rebuild respectively
 * without needing to track the build pipeline.
 *
 * The #IdeBuildPipeline is used to specify how and when build operations
 * should occur. Plugins attach build stages to the pipeline to perform
 * build actions.
 *
 * Since: 3.22
 */

struct _IdeBuildManager
{
  IdeObject         parent_instance;

  GCancellable     *cancellable;

  IdeBuildPipeline *pipeline;
  GDateTime        *last_build_time;
  DzlSignalGroup   *pipeline_signals;

  GTimer           *running_time;

  guint             diagnostic_count;
  guint             error_count;
  guint             warning_count;

  guint             timer_source;

  guint             can_build : 1;
  guint             can_export : 1;
  guint             building : 1;
};

static void initable_iface_init              (GInitableIface  *iface);
static void ide_build_manager_set_can_build  (IdeBuildManager *self,
                                              gboolean         can_build);
static void ide_build_manager_action_build   (IdeBuildManager *self,
                                              GVariant        *param);
static void ide_build_manager_action_rebuild (IdeBuildManager *self,
                                              GVariant        *param);
static void ide_build_manager_action_cancel  (IdeBuildManager *self,
                                              GVariant        *param);
static void ide_build_manager_action_clean   (IdeBuildManager *self,
                                              GVariant        *param);
static void ide_build_manager_action_export  (IdeBuildManager *self,
                                              GVariant        *param);
static void ide_build_manager_action_install (IdeBuildManager *self,
                                              GVariant        *param);

DZL_DEFINE_ACTION_GROUP (IdeBuildManager, ide_build_manager, {
  { "build", ide_build_manager_action_build },
  { "cancel", ide_build_manager_action_cancel },
  { "clean", ide_build_manager_action_clean },
  { "export", ide_build_manager_action_export },
  { "install", ide_build_manager_action_install },
  { "rebuild", ide_build_manager_action_rebuild },
})

G_DEFINE_TYPE_EXTENDED (IdeBuildManager, ide_build_manager, IDE_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initable_iface_init)
                        G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP,
                                               ide_build_manager_init_action_group))

enum {
  PROP_0,
  PROP_BUSY,
  PROP_CAN_BUILD,
  PROP_ERROR_COUNT,
  PROP_HAS_DIAGNOSTICS,
  PROP_LAST_BUILD_TIME,
  PROP_MESSAGE,
  PROP_PIPELINE,
  PROP_RUNNING_TIME,
  PROP_WARNING_COUNT,
  N_PROPS
};

enum {
  BUILD_STARTED,
  BUILD_FINISHED,
  BUILD_FAILED,
  N_SIGNALS
};

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

static gboolean
timer_callback (gpointer data)
{
  IdeBuildManager *self = data;

  g_assert (IDE_IS_BUILD_MANAGER (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_RUNNING_TIME]);

  return G_SOURCE_CONTINUE;
}

static void
ide_build_manager_start_timer (IdeBuildManager *self)
{
  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_MANAGER (self));
  g_assert (self->timer_source == 0);

  if (self->running_time != NULL)
    g_timer_start (self->running_time);
  else
    self->running_time = g_timer_new ();

  /*
   * We use the DzlFrameSource for our timer callback because we only want to
   * update at a rate somewhat close to a typical monitor refresh rate.
   * Additionally, we want to handle drift (which that source does) so that we
   * don't constantly fall behind.
   */
  self->timer_source = g_timeout_add_seconds (1, timer_callback, self);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_RUNNING_TIME]);

  IDE_EXIT;
}

static void
ide_build_manager_stop_timer (IdeBuildManager *self)
{
  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_MANAGER (self));

  dzl_clear_source (&self->timer_source);

  if (self->running_time != NULL)
    {
      g_timer_stop (self->running_time);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_RUNNING_TIME]);
    }

  IDE_EXIT;
}

static void
ide_build_manager_handle_diagnostic (IdeBuildManager  *self,
                                     IdeDiagnostic    *diagnostic,
                                     IdeBuildPipeline *pipeline)
{
  IdeDiagnosticSeverity severity;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_MANAGER (self));
  g_assert (diagnostic != NULL);
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));

  self->diagnostic_count++;
  if (self->diagnostic_count == 1)
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_HAS_DIAGNOSTICS]);

  severity = ide_diagnostic_get_severity (diagnostic);

  if (severity == IDE_DIAGNOSTIC_WARNING)
    {
      self->warning_count++;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_WARNING_COUNT]);
    }
  else if (severity == IDE_DIAGNOSTIC_ERROR || severity == IDE_DIAGNOSTIC_FATAL)
    {
      self->error_count++;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ERROR_COUNT]);
    }

  IDE_EXIT;
}

static void
ide_build_manager_update_action_enabled (IdeBuildManager *self)
{
  gboolean busy;
  gboolean can_build;
  gboolean can_export;

  g_assert (IDE_IS_BUILD_MANAGER (self));

  busy = ide_build_manager_get_busy (self);
  can_build = ide_build_manager_get_can_build (self);
  can_export = self->pipeline ? ide_build_pipeline_get_can_export (self->pipeline) : FALSE;

  ide_build_manager_set_action_enabled (self, "build", !busy && can_build);
  ide_build_manager_set_action_enabled (self, "cancel", busy);
  ide_build_manager_set_action_enabled (self, "clean", !busy && can_build);
  ide_build_manager_set_action_enabled (self, "export", !busy && can_build && can_export);
  ide_build_manager_set_action_enabled (self, "install", !busy && can_build);
  ide_build_manager_set_action_enabled (self, "rebuild", !busy && can_build);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BUSY]);
}

static void
ide_build_manager_notify_busy (IdeBuildManager  *self,
                               GParamSpec       *pspec,
                               IdeBuildPipeline *pipeline)
{
  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_MANAGER (self));
  g_assert (G_IS_PARAM_SPEC (pspec));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));

  ide_build_manager_update_action_enabled (self);

  IDE_EXIT;
}

static void
ide_build_manager_notify_message (IdeBuildManager  *self,
                                  GParamSpec       *pspec,
                                  IdeBuildPipeline *pipeline)
{
  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_MANAGER (self));
  g_assert (G_IS_PARAM_SPEC (pspec));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));

  if (pipeline == self->pipeline)
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MESSAGE]);

  IDE_EXIT;
}

static void
ide_build_manager_pipeline_started (IdeBuildManager  *self,
                                    IdeBuildPhase     phase,
                                    IdeBuildPipeline *pipeline)
{
  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_MANAGER (self));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));

  self->building = TRUE;

  g_signal_emit (self, signals [BUILD_STARTED], 0, pipeline);

  IDE_EXIT;
}

static void
ide_build_manager_pipeline_finished (IdeBuildManager  *self,
                                     gboolean          failed,
                                     IdeBuildPipeline *pipeline)
{
  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_MANAGER (self));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));

  self->building = FALSE;

  if (failed)
    g_signal_emit (self, signals [BUILD_FAILED], 0, pipeline);
  else
    g_signal_emit (self, signals [BUILD_FINISHED], 0, pipeline);

  IDE_EXIT;
}

static void
ide_build_manager_ensure_runtime_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  IdeRuntimeManager *runtime_manager = (IdeRuntimeManager *)object;
  g_autoptr(IdeRuntime) runtime = NULL;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeBuildManager *self;
  IdeBuildPipeline *pipeline;
  GCancellable *cancellable;

  IDE_ENTRY;

  g_assert (IDE_IS_RUNTIME_MANAGER (runtime_manager));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  pipeline = g_task_get_task_data (task);

  g_assert (IDE_IS_BUILD_MANAGER (self));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));

  runtime = ide_runtime_manager_ensure_config_finish (runtime_manager, result, &error);

  if (runtime == NULL)
    {
      g_message ("Failed to locate runtime: %s", error->message);
      IDE_GOTO (failure);
    }

  if (pipeline != self->pipeline)
    {
      IDE_TRACE_MSG ("pipeline is no longer active, ignoring");
      IDE_GOTO (failure);
    }

  if (g_task_return_error_if_cancelled (task))
    IDE_GOTO (failure);

  cancellable = g_task_get_cancellable (task);

  /* This will cause plugins to load on the pipeline. */
  if (!g_initable_init (G_INITABLE (pipeline), cancellable, &error))
    {
      /* translators: %s is replaced with the error message */
      ide_object_warning (self,
                          _("Failed to initialize build pipeline: %s"),
                          error->message);
      IDE_GOTO (failure);
    }

  ide_build_manager_set_can_build (self, TRUE);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PIPELINE]);

  g_task_return_pointer (task, g_steal_pointer (&runtime), g_object_unref);

  IDE_EXIT;

failure:

  if (error != NULL)
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_new_error (task,
                             G_IO_ERROR,
                             G_IO_ERROR_FAILED,
                             "Failed to setup build pipeline");

  IDE_EXIT;
}

static void
ide_build_manager_invalidate_pipeline (IdeBuildManager *self)
{
  IdeConfigurationManager *config_manager;
  g_autoptr(GTask) task = NULL;
  IdeRuntimeManager *runtime_manager;
  IdeDeviceManager *device_manager;
  IdeConfiguration *config;
  IdeContext *context;
  IdeDevice *device;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_MANAGER (self));

  context = ide_object_get_context (IDE_OBJECT (self));

  IDE_TRACE_MSG ("Reloading pipeline due to configuration change");

  /*
   * If we are currently building, we need to synthesize the failure
   * of that build and re-setup the pipeline.
   */
  if (self->building)
    {
      g_assert (self->pipeline != NULL);

      self->building = FALSE;
      dzl_clear_source (&self->timer_source);
      g_signal_emit (self, signals [BUILD_FAILED], 0, self->pipeline);
    }

  /*
   * Cancel and clear our previous pipeline and associated components
   * as they are not invalide.
   */
  ide_build_manager_cancel (self);

  g_clear_object (&self->pipeline);

  g_clear_pointer (&self->running_time, g_timer_destroy);

  self->diagnostic_count = 0;
  self->error_count = 0;
  self->warning_count = 0;

  /* Don't setup anything new if we're in shutdown */
  if (ide_context_is_unloading (context))
    IDE_EXIT;

  config_manager = ide_context_get_configuration_manager (context);
  device_manager = ide_context_get_device_manager (context);
  runtime_manager = ide_context_get_runtime_manager (context);

  config = ide_configuration_manager_get_current (config_manager);
  device = ide_device_manager_get_device (device_manager);

  /*
   * We want to set the pipeline before connecting things using the GInitable
   * interface so that we can access the builddir from
   * IdeRuntime.create_launcher() during pipeline addin initialization.
   *
   * We will delay the initialization until after the we have ensured the
   * runtime is available (possibly installing it).
   */
  ide_build_manager_set_can_build (self, FALSE);
  self->pipeline = g_object_new (IDE_TYPE_BUILD_PIPELINE,
                                 "context", context,
                                 "configuration", config,
                                 "device", device,
                                 NULL);
  dzl_signal_group_set_target (self->pipeline_signals, self->pipeline);

  /*
   * This next part of the pipeline setup is asynchronous, as we need to
   * make sure that the pipeline's runtime is available before we setup
   * the pipeline. That could mean that we have to install it.
   *
   * We setup a cancellable so that we can cancel the setup operation in
   * case a further configuration change comes through and we need to
   * tear down the pipeline immediately.
   */
  task = g_task_new (self, self->cancellable, NULL, NULL);
  g_task_set_task_data (task, g_object_ref (self->pipeline), g_object_unref);
  g_task_set_priority (task, G_PRIORITY_LOW);
  ide_runtime_manager_ensure_config_async (runtime_manager,
                                           config,
                                           self->cancellable,
                                           ide_build_manager_ensure_runtime_cb,
                                           g_steal_pointer (&task));

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ERROR_COUNT]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_HAS_DIAGNOSTICS]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LAST_BUILD_TIME]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MESSAGE]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_RUNNING_TIME]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_WARNING_COUNT]);

  IDE_EXIT;
}

static gboolean
initable_init (GInitable     *initable,
               GCancellable  *cancellable,
               GError       **error)
{
  IdeBuildManager *self = (IdeBuildManager *)initable;
  IdeConfigurationManager *config_manager;
  IdeDeviceManager *device_manager;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_MANAGER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  context = ide_object_get_context (IDE_OBJECT (self));
  config_manager = ide_context_get_configuration_manager (context);
  device_manager = ide_context_get_device_manager (context);

  g_signal_connect_object (config_manager,
                           "invalidate",
                           G_CALLBACK (ide_build_manager_invalidate_pipeline),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (device_manager,
                           "notify::device",
                           G_CALLBACK (ide_build_manager_invalidate_pipeline),
                           self,
                           G_CONNECT_SWAPPED);

  ide_build_manager_invalidate_pipeline (self);

  IDE_RETURN (TRUE);
}

static void
ide_build_manager_real_build_started (IdeBuildManager  *self,
                                      IdeBuildPipeline *pipeline)
{
  g_assert (IDE_IS_BUILD_MANAGER (self));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));

  ide_build_manager_start_timer (self);
}

static void
ide_build_manager_real_build_failed (IdeBuildManager  *self,
                                     IdeBuildPipeline *pipeline)
{
  g_assert (IDE_IS_BUILD_MANAGER (self));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));

  ide_build_manager_stop_timer (self);
}

static void
ide_build_manager_real_build_finished (IdeBuildManager  *self,
                                       IdeBuildPipeline *pipeline)
{
  IdeDiagnosticsManager *diagnostics;
  IdeBufferManager *bufmgr;
  IdeContext *context;
  guint n_items;

  g_assert (IDE_IS_BUILD_MANAGER (self));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));

  ide_build_manager_stop_timer (self);

  /*
   * We had a successful build, so lets notify the build manager to reload
   * dianostics on loaded buffers so the user doesn't have to make a change
   * to force the update.
   */

  context = ide_object_get_context (IDE_OBJECT (self));
  diagnostics = ide_context_get_diagnostics_manager (context);
  bufmgr = ide_context_get_buffer_manager (context);
  n_items = g_list_model_get_n_items (G_LIST_MODEL (bufmgr));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(IdeBuffer) buffer = g_list_model_get_item (G_LIST_MODEL (bufmgr), i);

      ide_diagnostics_manager_rediagnose (diagnostics, buffer);
    }
}

static void
initable_iface_init (GInitableIface *iface)
{
  iface->init = initable_init;
}

static void
ide_build_manager_finalize (GObject *object)
{
  IdeBuildManager *self = (IdeBuildManager *)object;

  g_clear_object (&self->pipeline);
  g_clear_object (&self->pipeline_signals);
  g_clear_object (&self->cancellable);
  g_clear_pointer (&self->last_build_time, g_date_time_unref);
  g_clear_pointer (&self->running_time, g_timer_destroy);

  dzl_clear_source (&self->timer_source);

  G_OBJECT_CLASS (ide_build_manager_parent_class)->finalize (object);
}

static void
ide_build_manager_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  IdeBuildManager *self = IDE_BUILD_MANAGER (object);

  switch (prop_id)
    {
    case PROP_BUSY:
      g_value_set_boolean (value, ide_build_manager_get_busy (self));
      break;

    case PROP_CAN_BUILD:
      g_value_set_boolean (value, ide_build_manager_get_can_build (self));
      break;

    case PROP_MESSAGE:
      g_value_take_string (value, ide_build_manager_get_message (self));
      break;

    case PROP_LAST_BUILD_TIME:
      g_value_set_boxed (value, ide_build_manager_get_last_build_time (self));
      break;

    case PROP_RUNNING_TIME:
      g_value_set_int64 (value, ide_build_manager_get_running_time (self));
      break;

    case PROP_HAS_DIAGNOSTICS:
      g_value_set_boolean (value, self->diagnostic_count > 0);
      break;

    case PROP_ERROR_COUNT:
      g_value_set_uint (value, self->error_count);
      break;

    case PROP_WARNING_COUNT:
      g_value_set_uint (value, self->warning_count);
      break;

    case PROP_PIPELINE:
      g_value_set_object (value, self->pipeline);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_build_manager_class_init (IdeBuildManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_build_manager_finalize;
  object_class->get_property = ide_build_manager_get_property;

  /**
   * IdeBuildManager:can-build:
   *
   * Gets if the build manager can queue a build request.
   *
   * This might be false if the required runtime is not available or other
   * errors in setting up the build pipeline.
   *
   * Since: 3.22
   */
  properties [PROP_CAN_BUILD] =
    g_param_spec_boolean ("can-build",
                          "Can Build",
                          "If the manager can queue a build",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * IdeBuildManager:busy:
   *
   * The "busy" property indicates if there is currently a build
   * executing. This can be bound to UI elements to display to the
   * user that a build is active (and therefore other builds cannot
   * be activated at the moment).
   *
   * Since: 3.22
   */
  properties [PROP_BUSY] =
    g_param_spec_boolean ("busy",
                          "Busy",
                          "If a build is actively executing",
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * IdeBuildManager:error-count:
   *
   * The number of errors discovered during the build process.
   *
   * Since: 3.22
   */
  properties [PROP_ERROR_COUNT] =
    g_param_spec_uint ("error-count",
                       "Error Count",
                       "The number of errors that have been seen in the current build",
                       0, G_MAXUINT, 0,
                       (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * IdeBuildManager:has-diagnostics:
   *
   * The "has-diagnostics" property indicates that there have been
   * diagnostics found during the last execution of the build pipeline.
   *
   * Since: 3.22
   */
  properties [PROP_HAS_DIAGNOSTICS] =
    g_param_spec_boolean ("has-diagnostics",
                          "Has Diagnostics",
                          "Has Diagnostics",
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * IdeBuildManager:last-build-time:
   *
   * The "last-build-time" property contains a #GDateTime of the time
   * the last build request was submitted.
   *
   * Since: 3.22
   */
  properties [PROP_LAST_BUILD_TIME] =
    g_param_spec_boxed ("last-build-time",
                        "Last Build Time",
                        "The time of the last build request",
                        G_TYPE_DATE_TIME,
                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * IdeBuildManager:message:
   *
   * The "message" property contains a string message describing
   * the current state of the build process. This may be bound to
   * UI elements to notify the user of the buid progress.
   *
   * Since: 3.22
   */
  properties [PROP_MESSAGE] =
    g_param_spec_string ("message",
                         "Message",
                         "The current build message",
                         NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * IdeBuildManager:pipeline:
   *
   * The "pipeline" property is the build pipeline that the build manager
   * is currently managing.
   *
   * Since: 3.22
   */
  properties [PROP_PIPELINE] =
    g_param_spec_object ("pipeline",
                         "Pipeline",
                         "The build pipeline",
                         IDE_TYPE_BUILD_PIPELINE,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * IdeBuildManager:running-time:
   *
   * The "running-time" property can be bound by UI elements that
   * want to track how long the current build has taken. g_object_notify()
   * is called on a regular interval during the build so that the UI
   * elements may automatically update.
   *
   * The value of this property is a #GTimeSpan, which are 64-bit signed
   * integers with microsecond precision. See %G_USEC_PER_SEC for a constant
   * to tranform this to seconds.
   *
   * Since: 3.22
   */
  properties [PROP_RUNNING_TIME] =
    g_param_spec_int64 ("running-time",
                        "Running Time",
                        "The amount of elapsed time performing the current build",
                        0,
                        G_MAXINT64,
                        0,
                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * IdeBuildManager:warning-count:
   *
   * The "warning-count" property contains the number of warnings that have
   * been discovered in the current build request.
   *
   * Since: 3.22
   */
  properties [PROP_WARNING_COUNT] =
    g_param_spec_uint ("warning-count",
                       "Warning Count",
                       "The number of warnings that have been seen in the current build",
                       0, G_MAXUINT, 0,
                       (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * IdeBuildManager::build-started:
   * @self: An #IdeBuildManager
   * @pipeline: An #IdeBuildPipeline
   *
   * The "build-started" signal is emitted when a new build has started.
   * The build may be an incremental build. The @pipeline instance is
   * the build pipeline which is being executed.
   *
   * Since: 3.22
   */
  signals [BUILD_STARTED] =
    g_signal_new_class_handler ("build-started",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (ide_build_manager_real_build_started),
                                NULL, NULL,
                                NULL,
                                G_TYPE_NONE, 1, IDE_TYPE_BUILD_PIPELINE);

  /**
   * IdeBuildManager::build-failed:
   * @self: An #IdeBuildManager
   * @pipeline: An #IdeBuildPipeline
   *
   * The "build-failed" signal is emitted when a build that was previously
   * notified via #IdeBuildManager::build-started has failed to complete
   * successfully.
   *
   * Contrast this with #IdeBuildManager::build-finished for a successful
   * build.
   *
   * Since: 3.22
   */
  signals [BUILD_FAILED] =
    g_signal_new_class_handler ("build-failed",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (ide_build_manager_real_build_failed),
                                NULL, NULL,
                                NULL,
                                G_TYPE_NONE, 1, IDE_TYPE_BUILD_PIPELINE);

  /**
   * IdeBuildManager::build-finished:
   * @self: An #IdeBuildManager
   * @pipeline: An #IdeBuildPipeline
   *
   * The "build-finished" signal is emitted when a build completed
   * successfully.
   *
   * Since: 3.22
   */
  signals [BUILD_FINISHED] =
    g_signal_new_class_handler ("build-finished",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (ide_build_manager_real_build_finished),
                                NULL, NULL,
                                NULL,
                                G_TYPE_NONE, 1, IDE_TYPE_BUILD_PIPELINE);
}

static void
ide_build_manager_action_cancel (IdeBuildManager *self,
                                 GVariant        *param)
{
  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_MANAGER (self));

  ide_build_manager_cancel (self);

  IDE_EXIT;
}

static void
ide_build_manager_action_build (IdeBuildManager *self,
                                GVariant        *param)
{
  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_MANAGER (self));

  ide_build_manager_execute_async (self, IDE_BUILD_PHASE_BUILD, NULL, NULL, NULL);

  IDE_EXIT;
}

static void
ide_build_manager_action_rebuild (IdeBuildManager *self,
                                  GVariant        *param)
{
  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_MANAGER (self));

  ide_build_manager_rebuild_async (self, IDE_BUILD_PHASE_BUILD, NULL, NULL, NULL);

  IDE_EXIT;
}

static void
ide_build_manager_action_clean (IdeBuildManager *self,
                                GVariant        *param)
{
  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_MANAGER (self));

  ide_build_manager_clean_async (self, IDE_BUILD_PHASE_BUILD, NULL, NULL, NULL);

  IDE_EXIT;
}

static void
ide_build_manager_action_install (IdeBuildManager *self,
                                  GVariant        *param)
{
  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_MANAGER (self));

  ide_build_manager_execute_async (self, IDE_BUILD_PHASE_INSTALL, NULL, NULL, NULL);

  IDE_EXIT;
}

static void
ide_build_manager_action_export (IdeBuildManager *self,
                                 GVariant        *param)
{
  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_MANAGER (self));

  ide_build_manager_execute_async (self, IDE_BUILD_PHASE_EXPORT, NULL, NULL, NULL);

  IDE_EXIT;
}

static void
ide_build_manager_init (IdeBuildManager *self)
{
  IDE_ENTRY;

  ide_build_manager_update_action_enabled (self);

  self->cancellable = g_cancellable_new ();

  self->pipeline_signals = dzl_signal_group_new (IDE_TYPE_BUILD_PIPELINE);

  dzl_signal_group_connect_object (self->pipeline_signals,
                                   "diagnostic",
                                   G_CALLBACK (ide_build_manager_handle_diagnostic),
                                   self,
                                   G_CONNECT_SWAPPED);

  dzl_signal_group_connect_object (self->pipeline_signals,
                                   "notify::busy",
                                   G_CALLBACK (ide_build_manager_notify_busy),
                                   self,
                                   G_CONNECT_SWAPPED);

  dzl_signal_group_connect_object (self->pipeline_signals,
                                   "notify::message",
                                   G_CALLBACK (ide_build_manager_notify_message),
                                   self,
                                   G_CONNECT_SWAPPED);

  dzl_signal_group_connect_object (self->pipeline_signals,
                                   "started",
                                   G_CALLBACK (ide_build_manager_pipeline_started),
                                   self,
                                   G_CONNECT_SWAPPED);

  dzl_signal_group_connect_object (self->pipeline_signals,
                                   "finished",
                                   G_CALLBACK (ide_build_manager_pipeline_finished),
                                   self,
                                   G_CONNECT_SWAPPED);


  IDE_EXIT;
}

/**
 * ide_build_manager_get_busy:
 * @self: An #IdeBuildManager
 *
 * Gets if the #IdeBuildManager is currently busy building the project.
 *
 * See #IdeBuildManager:busy for more information.
 *
 * Since: 3.22
 */
gboolean
ide_build_manager_get_busy (IdeBuildManager *self)
{
  g_return_val_if_fail (IDE_IS_BUILD_MANAGER (self), FALSE);

  if G_LIKELY (self->pipeline != NULL)
    return ide_build_pipeline_get_busy (self->pipeline);

  return FALSE;
}

/**
 * ide_build_manager_get_message:
 * @self: An #IdeBuildManager
 *
 * This function returns the current build message as a string.
 *
 * See #IdeBuildManager:message for more information.
 *
 * Returns: (transfer full): A string containing the build message or %NULL
 *
 * Since: 3.22
 */
gchar *
ide_build_manager_get_message (IdeBuildManager *self)
{
  g_return_val_if_fail (IDE_IS_BUILD_MANAGER (self), NULL);

  if G_LIKELY (self->pipeline != NULL)
    return ide_build_pipeline_get_message (self->pipeline);

  return NULL;
}

/**
 * ide_build_manager_get_last_build_time:
 * @self: An #IdeBuildManager
 *
 * This function returns a #GDateTime of the last build request. If
 * there has not yet been a build request, this will return %NULL.
 *
 * See #IdeBuildManager:last-build-time for more information.
 *
 * Returns: (nullable) (transfer none): a #GDateTime or %NULL.
 *
 * Since: 3.22
 */
GDateTime *
ide_build_manager_get_last_build_time (IdeBuildManager *self)
{
  g_return_val_if_fail (IDE_IS_BUILD_MANAGER (self), NULL);

  return self->last_build_time;
}

/**
 * ide_build_manager_get_running_time:
 *
 * Gets the amount of elapsed time of the current build as a
 * #GTimeSpan.
 *
 * Returns: a #GTimeSpan containing the elapsed time of the build.
 *
 * Since: 3.22
 */
GTimeSpan
ide_build_manager_get_running_time (IdeBuildManager *self)
{
  g_return_val_if_fail (IDE_IS_BUILD_MANAGER (self), 0);

  if (self->running_time != NULL)
    return g_timer_elapsed (self->running_time, NULL) * G_TIME_SPAN_SECOND;

  return 0;
}

/**
 * ide_build_manager_cancel:
 * @self: An #IdeBuildManager
 *
 * This function will cancel any in-flight builds.
 *
 * You may also activate this using the "cancel" #GAction provided
 * by the #GActionGroup interface.
 *
 * Since: 3.22
 */
void
ide_build_manager_cancel (IdeBuildManager *self)
{
  g_autoptr(GCancellable) cancellable = NULL;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_BUILD_MANAGER (self));

  cancellable = g_steal_pointer (&self->cancellable);
  self->cancellable = g_cancellable_new ();

  g_debug ("Cancelling [%p] build due to user request", cancellable);

  if (!g_cancellable_is_cancelled (cancellable))
    g_cancellable_cancel (cancellable);

  if (self->pipeline != NULL)
    _ide_build_pipeline_cancel (self->pipeline);

  IDE_EXIT;
}

/**
 * ide_build_manager_get_pipeline:
 * @self: An #IdeBuildManager
 *
 * This function gets the current build pipeline. The pipeline will be
 * reloaded as build configurations change.
 *
 * Returns: (transfer none) (nullable): An #IdeBuildPipeline.
 *
 * Since: 3.22
 */
IdeBuildPipeline *
ide_build_manager_get_pipeline (IdeBuildManager *self)
{
  g_return_val_if_fail (IDE_IS_BUILD_MANAGER (self), NULL);

  return self->pipeline;
}

static void
ide_build_manager_execute_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  IdeBuildPipeline *pipeline = (IdeBuildPipeline *)object;
  IdeBuildManager *self;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  g_assert (IDE_IS_BUILD_MANAGER (self));

  if (!ide_build_pipeline_execute_finish (pipeline, result, &error))
    {
      ide_object_warning (pipeline, "%s", error->message);
      g_task_return_error (task, g_steal_pointer (&error));
      IDE_GOTO (failure);
    }

  g_task_return_boolean (task, TRUE);

failure:
  IDE_EXIT;
}

static void
ide_build_manager_save_all_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  IdeBufferManager *buffer_manager = (IdeBufferManager *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeBuildManager *self;
  GCancellable *cancellable;

  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));
  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  cancellable = g_task_get_cancellable (task);

  g_assert (IDE_IS_BUILD_MANAGER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (!ide_buffer_manager_save_all_finish (buffer_manager, result, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  ide_build_pipeline_execute_async (self->pipeline,
                                    cancellable,
                                    ide_build_manager_execute_cb,
                                    g_steal_pointer (&task));

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_HAS_DIAGNOSTICS]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LAST_BUILD_TIME]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_RUNNING_TIME]);

  IDE_EXIT;
}

/**
 * ide_build_manager_execute_async:
 * @self: An #IdeBuildManager
 * @phase: An #IdeBuildPhase or 0
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: A callback to execute upon completion
 * @user_data: user data for @callback
 *
 * This function will request that @phase is completed in the underlying
 * build pipeline and execute a build. Upon completion, @callback will be
 * executed and it can determine the success or failure of the operation
 * using ide_build_manager_execute_finish().
 *
 * Since: 3.22
 */
void
ide_build_manager_execute_async (IdeBuildManager     *self,
                                 IdeBuildPhase        phase,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  IdeBufferManager *buffer_manager;
  IdeContext *context;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_BUILD_MANAGER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (!g_cancellable_is_cancelled (self->cancellable));

  cancellable = dzl_cancellable_chain (cancellable, self->cancellable);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_build_manager_execute_async);
  g_task_set_priority (task, G_PRIORITY_LOW);
  g_task_set_return_on_cancel (task, TRUE);

  if (self->pipeline == NULL || self->can_build == FALSE)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_PENDING,
                               "Cannot execute pipeline, it has not yet been prepared");
      IDE_EXIT;
    }

  if (!ide_build_pipeline_request_phase (self->pipeline, phase))
    {
      g_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  /*
   * Only update our "build time" if we are advancing to IDE_BUILD_PHASE_BUILD,
   * we don't really care about "builds" for configure stages and less.
   */
  if ((phase & IDE_BUILD_PHASE_MASK) >= IDE_BUILD_PHASE_BUILD)
    {
      g_clear_pointer (&self->last_build_time, g_date_time_unref);
      self->last_build_time = g_date_time_new_now_local ();
      self->diagnostic_count = 0;
      self->warning_count = 0;
      self->error_count = 0;
    }

  /*
   * If we are performing a real build (not just something like configure),
   * then we want to ensure we save all the buffers. We don't want to do this
   * on every keypress (and execute_async() could be called on every keypress)
   * for ensuring build flags are up to date.
   */
  if ((phase & IDE_BUILD_PHASE_MASK) >= IDE_BUILD_PHASE_BUILD)
    {
      context = ide_object_get_context (IDE_OBJECT (self));
      buffer_manager = ide_context_get_buffer_manager (context);
      ide_buffer_manager_save_all_async (buffer_manager,
                                         cancellable,
                                         ide_build_manager_save_all_cb,
                                         g_steal_pointer (&task));
      IDE_EXIT;
    }

  ide_build_pipeline_build_async (self->pipeline,
                                  phase,
                                  cancellable,
                                  ide_build_manager_execute_cb,
                                  g_steal_pointer (&task));

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ERROR_COUNT]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_HAS_DIAGNOSTICS]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LAST_BUILD_TIME]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_RUNNING_TIME]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_WARNING_COUNT]);

  IDE_EXIT;
}

/**
 * ide_build_manager_execute_finish:
 * @self: An #IdeBuildManager
 * @result: a #GAsyncResult
 * @error: A location for a #GError or %NULL
 *
 * Completes a request to ide_build_manager_execute_async().
 *
 * Returns: %TRUE if successful, otherwise %FALSE and @error is set.
 *
 * Since: 3.22
 */
gboolean
ide_build_manager_execute_finish (IdeBuildManager  *self,
                                  GAsyncResult     *result,
                                  GError          **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_BUILD_MANAGER (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  ret = g_task_propagate_boolean (G_TASK (result), error);

  IDE_RETURN (ret);
}

static void
ide_build_manager_clean_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  IdeBuildPipeline *pipeline = (IdeBuildPipeline *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeBuildManager *self;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  g_assert (IDE_IS_BUILD_MANAGER (self));

  if (!ide_build_pipeline_clean_finish (pipeline, result, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_boolean (task, TRUE);
}

/**
 * ide_build_manager_clean_async:
 * @self: a #IdeBuildManager
 * @phase: the build phase to clean
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: (nullable): a callback to execute upon completion, or %NULL
 * @user_data: closure data for @callback
 *
 * Asynchronously requests that the build pipeline clean up to @phase.
 *
 * See ide_build_pipeline_clean_async() for more information.
 *
 * Since: 3.22
 */
void
ide_build_manager_clean_async (IdeBuildManager     *self,
                               IdeBuildPhase        phase,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_BUILD_MANAGER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (!g_cancellable_is_cancelled (self->cancellable));

  cancellable = dzl_cancellable_chain (cancellable, self->cancellable);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_build_manager_clean_async);
  g_task_set_priority (task, G_PRIORITY_LOW);
  g_task_set_return_on_cancel (task, TRUE);

  if (self->pipeline == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_PENDING,
                               "Cannot execute pipeline, it has not yet been prepared");
      IDE_EXIT;
    }

  self->diagnostic_count = 0;
  self->error_count = 0;
  self->warning_count = 0;

  ide_build_pipeline_clean_async (self->pipeline,
                                  phase,
                                  cancellable,
                                  ide_build_manager_clean_cb,
                                  g_steal_pointer (&task));

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ERROR_COUNT]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_HAS_DIAGNOSTICS]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_WARNING_COUNT]);

  IDE_EXIT;
}

/**
 * ide_build_manager_clean_finish:
 * @self: a #IdeBuildManager
 * @result: a #GAsyncResult
 * @error: a location for a #GError, or %NULL
 *
 * Completes an asynchronous request to ide_build_manager_clean_async().
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 *
 * Since: 3.22
 */
gboolean
ide_build_manager_clean_finish (IdeBuildManager  *self,
                                GAsyncResult     *result,
                                GError          **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_BUILD_MANAGER (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  ret = g_task_propagate_boolean (G_TASK (result), error);

  IDE_RETURN (ret);
}

static void
ide_build_manager_rebuild_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  IdeBuildPipeline *pipeline = (IdeBuildPipeline *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!ide_build_pipeline_rebuild_finish (pipeline, result, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

/**
 * ide_build_manager_rebuild_async:
 * @self: a #IdeBuildManager
 * @phase: the build phase to rebuild to
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: (nullable): a callback to execute upon completion, or %NULL
 * @user_data: closure data for @callback
 *
 * Asynchronously requests that the build pipeline clean and rebuild up
 * to the given phase. This may involve discarding previous build artifacts
 * to allow for the rebuild process.
 *
 * See ide_build_pipeline_rebuild_async() for more information.
 *
 * Since: 3.22
 */
void
ide_build_manager_rebuild_async (IdeBuildManager     *self,
                                 IdeBuildPhase        phase,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_BUILD_MANAGER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (!g_cancellable_is_cancelled (self->cancellable));

  cancellable = dzl_cancellable_chain (cancellable, self->cancellable);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_build_manager_rebuild_async);
  g_task_set_priority (task, G_PRIORITY_LOW);
  g_task_set_return_on_cancel (task, TRUE);

  if (self->pipeline == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_PENDING,
                               "Cannot execute pipeline, it has not yet been prepared");
      IDE_EXIT;
    }

  ide_build_pipeline_rebuild_async (self->pipeline,
                                    phase,
                                    cancellable,
                                    ide_build_manager_rebuild_cb,
                                    g_steal_pointer (&task));

  IDE_EXIT;
}

/**
 * ide_build_manager_rebuild_finish:
 * @self: a #IdeBuildManager
 * @result: a #GAsyncResult
 * @error: a location for a #GError, or %NULL
 *
 * Completes an asynchronous request to ide_build_manager_rebuild_async().
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 *
 * Since: 3.22
 */
gboolean
ide_build_manager_rebuild_finish (IdeBuildManager  *self,
                                  GAsyncResult     *result,
                                  GError          **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_BUILD_MANAGER (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  ret = g_task_propagate_boolean (G_TASK (result), error);

  IDE_RETURN (ret);
}

/**
 * ide_build_manager_get_can_build:
 * @self: a #IdeBuildManager
 *
 * Checks if the current pipeline is ready to build.
 *
 * Returns: %TRUE if a build operation can advance the pipeline.
 *
 * Since: 3.22
 */
gboolean
ide_build_manager_get_can_build (IdeBuildManager *self)
{
  g_return_val_if_fail (IDE_IS_BUILD_MANAGER (self), FALSE);

  return self->can_build;
}

static void
ide_build_manager_set_can_build (IdeBuildManager *self,
                                 gboolean         can_build)
{
  g_return_if_fail (IDE_IS_BUILD_MANAGER (self));

  can_build = !!can_build;

  if (self->can_build != can_build)
    {
      self->can_build = can_build;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CAN_BUILD]);
      ide_build_manager_update_action_enabled (self);
    }
}
