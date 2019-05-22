/* ide-build-manager.c
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

#define G_LOG_DOMAIN "ide-build-manager"

#include "config.h"

#include <dazzle.h>
#include <glib/gi18n.h>
#include <libide-code.h>
#include <libide-threading.h>
#include <libide-vcs.h>

#include "ide-build-manager.h"
#include "ide-build-private.h"
#include "ide-config-manager.h"
#include "ide-config.h"
#include "ide-device-info.h"
#include "ide-device-manager.h"
#include "ide-device.h"
#include "ide-foundry-compat.h"
#include "ide-pipeline.h"
#include "ide-run-manager.h"
#include "ide-runtime-manager.h"
#include "ide-runtime-private.h"
#include "ide-runtime.h"
#include "ide-toolchain-manager.h"
#include "ide-toolchain-private.h"

/**
 * SECTION:ide-build-manager
 * @title: IdeBuildManager
 * @short_description: Manages the active build configuration and pipeline
 *
 * The #IdeBuildManager is responsible for managing the active build pipeline
 * as well as providing common high-level actions to plugins.
 *
 * You can use various async operations such as
 * ide_build_manager_build_async(), ide_build_manager_clean_async(), or
 * ide_build_manager_rebuild_async() to build, clean, and rebuild respectively
 * without needing to track the build pipeline.
 *
 * The #IdePipeline is used to specify how and when build operations
 * should occur. Plugins attach build stages to the pipeline to perform
 * build actions.
 *
 * Since: 3.32
 */

struct _IdeBuildManager
{
  IdeObject         parent_instance;

  GCancellable     *cancellable;

  IdePipeline      *pipeline;
  GDateTime        *last_build_time;
  DzlSignalGroup   *pipeline_signals;

  gchar            *branch_name;

  GTimer           *running_time;

  guint             diagnostic_count;
  guint             error_count;
  guint             warning_count;

  guint             timer_source;

  guint             started : 1;
  guint             can_build : 1;
  guint             can_export : 1;
  guint             building : 1;
  guint             needs_rediagnose : 1;
  guint             has_configured : 1;
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
ide_build_manager_reset_info (IdeBuildManager *self)
{
  g_assert (IDE_IS_BUILD_MANAGER (self));

  g_clear_pointer (&self->last_build_time, g_date_time_unref);
  self->last_build_time = g_date_time_new_now_local ();

  self->diagnostic_count = 0;
  self->warning_count = 0;
  self->error_count = 0;

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ERROR_COUNT]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_HAS_DIAGNOSTICS]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LAST_BUILD_TIME]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_RUNNING_TIME]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_WARNING_COUNT]);
}

static void
ide_build_manager_handle_diagnostic (IdeBuildManager *self,
                                     IdeDiagnostic   *diagnostic,
                                     IdePipeline     *pipeline)
{
  IdeDiagnosticSeverity severity;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_MANAGER (self));
  g_assert (diagnostic != NULL);
  g_assert (IDE_IS_PIPELINE (pipeline));

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
  can_export = self->pipeline ? ide_pipeline_get_can_export (self->pipeline) : FALSE;

  ide_build_manager_set_action_enabled (self, "build", !busy && can_build);
  ide_build_manager_set_action_enabled (self, "cancel", busy);
  ide_build_manager_set_action_enabled (self, "clean", !busy && can_build);
  ide_build_manager_set_action_enabled (self, "export", !busy && can_build && can_export);
  ide_build_manager_set_action_enabled (self, "install", !busy && can_build);
  ide_build_manager_set_action_enabled (self, "rebuild", !busy && can_build);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BUSY]);
}

static void
ide_build_manager_notify_busy (IdeBuildManager *self,
                               GParamSpec      *pspec,
                               IdePipeline     *pipeline)
{
  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_MANAGER (self));
  g_assert (G_IS_PARAM_SPEC (pspec));
  g_assert (IDE_IS_PIPELINE (pipeline));

  ide_build_manager_update_action_enabled (self);

  IDE_EXIT;
}

static void
ide_build_manager_notify_message (IdeBuildManager *self,
                                  GParamSpec      *pspec,
                                  IdePipeline     *pipeline)
{
  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_MANAGER (self));
  g_assert (G_IS_PARAM_SPEC (pspec));
  g_assert (IDE_IS_PIPELINE (pipeline));

  if (pipeline == self->pipeline)
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MESSAGE]);

  IDE_EXIT;
}

static void
ide_build_manager_pipeline_started (IdeBuildManager  *self,
                                    IdePipelinePhase  phase,
                                    IdePipeline      *pipeline)
{
  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_MANAGER (self));
  g_assert (IDE_IS_PIPELINE (pipeline));

  self->building = TRUE;

  g_signal_emit (self, signals [BUILD_STARTED], 0, pipeline);

  IDE_EXIT;
}

static void
ide_build_manager_pipeline_finished (IdeBuildManager *self,
                                     gboolean         failed,
                                     IdePipeline     *pipeline)
{
  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_MANAGER (self));
  g_assert (IDE_IS_PIPELINE (pipeline));

  self->building = FALSE;

  if (failed)
    g_signal_emit (self, signals [BUILD_FAILED], 0, pipeline);
  else
    g_signal_emit (self, signals [BUILD_FINISHED], 0, pipeline);

  IDE_EXIT;
}

static void
ide_build_manager_ensure_toolchain_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  IdeToolchainManager *toolchain_manager = (IdeToolchainManager *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeTask) task = user_data;
  IdePipeline *pipeline;
  IdeBuildManager *self;
  GCancellable *cancellable;

  IDE_ENTRY;

  g_assert (IDE_IS_TOOLCHAIN_MANAGER (toolchain_manager));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  pipeline = ide_task_get_task_data (task);

  g_assert (IDE_IS_BUILD_MANAGER (self));
  g_assert (IDE_IS_PIPELINE (pipeline));

  if (!_ide_toolchain_manager_prepare_finish (toolchain_manager, result, &error))
    {
      g_message ("Failed to prepare toolchain: %s", error->message);
      IDE_GOTO (failure);
    }

  if (pipeline != self->pipeline)
    {
      IDE_TRACE_MSG ("pipeline is no longer active, ignoring");
      IDE_GOTO (failure);
    }

  if (ide_task_return_error_if_cancelled (task))
    IDE_GOTO (failure);

  cancellable = ide_task_get_cancellable (task);

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

  ide_task_return_boolean (task, TRUE);
  IDE_EXIT;

failure:

  if (error != NULL)
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "Failed to setup build pipeline");

  IDE_EXIT;
}

static void
ide_build_manager_ensure_runtime_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  IdeRuntimeManager *runtime_manager = (IdeRuntimeManager *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeTask) task = user_data;
  IdePipeline *pipeline;
  IdeBuildManager *self;
  IdeToolchainManager *toolchain_manager;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_RUNTIME_MANAGER (runtime_manager));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  pipeline = ide_task_get_task_data (task);

  g_assert (IDE_IS_BUILD_MANAGER (self));
  g_assert (IDE_IS_PIPELINE (pipeline));

  if (!_ide_runtime_manager_prepare_finish (runtime_manager, result, &error))
    {
      g_message ("Failed to prepare runtime: %s", error->message);
      IDE_GOTO (failure);
    }

  if (pipeline != self->pipeline)
    {
      IDE_TRACE_MSG ("pipeline is no longer active, ignoring");
      IDE_GOTO (failure);
    }

  if (ide_task_return_error_if_cancelled (task))
    IDE_GOTO (failure);

  context = ide_object_get_context (IDE_OBJECT (pipeline));
  g_assert (IDE_IS_CONTEXT (context));

  toolchain_manager = ide_toolchain_manager_from_context (context);
  g_assert (IDE_IS_TOOLCHAIN_MANAGER (toolchain_manager));

  _ide_toolchain_manager_prepare_async (toolchain_manager,
                                        pipeline,
                                        ide_task_get_cancellable (task),
                                        ide_build_manager_ensure_toolchain_cb,
                                        g_object_ref (task));

  IDE_EXIT;

failure:

  if (error != NULL)
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "Failed to setup build pipeline");

  IDE_EXIT;
}

static void
ide_build_manager_device_get_info_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  IdeDevice *device = (IdeDevice *)object;
  g_autoptr(IdeDeviceInfo) info = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeTask) task = user_data;
  IdeRuntimeManager *runtime_manager;
  IdePipeline *pipeline;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_DEVICE (device));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  pipeline = ide_task_get_task_data (task);
  g_assert (IDE_IS_PIPELINE (pipeline));

  if (ide_task_return_error_if_cancelled (task))
    IDE_EXIT;

  if (!(context = ide_object_get_context (IDE_OBJECT (pipeline))) ||
      !(runtime_manager = ide_runtime_manager_from_context (context)))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_CANCELLED,
                                 "Device was destroyed");
      IDE_EXIT;
    }

  if (!(info = ide_device_get_info_finish (device, result, &error)))
    {
      ide_context_warning (context,
                           _("Failed to get device information: %s"),
                           error->message);
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  IDE_TRACE_MSG (" Device Kind = %d", ide_device_info_get_kind (info));
  IDE_TRACE_MSG (" Device Triplet = %s",
                 ide_triplet_get_full_name (ide_device_info_get_host_triplet (info)));

  _ide_pipeline_check_toolchain (pipeline, info);

  _ide_runtime_manager_prepare_async (runtime_manager,
                                      pipeline,
                                      ide_task_get_cancellable (task),
                                      ide_build_manager_ensure_runtime_cb,
                                      g_object_ref (task));

  IDE_EXIT;
}

static void
ide_build_manager_invalidate_pipeline (IdeBuildManager *self)
{
  g_autoptr(IdeTask) task = NULL;
  IdeConfigManager *config_manager;
  IdeDeviceManager *device_manager;
  IdeRunManager *run_manager;
  IdeConfig *config;
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
   * Clear any cached build targets from the run manager.
   */
  run_manager = ide_run_manager_from_context (context);
  ide_run_manager_set_build_target (run_manager, NULL);

  /*
   * Cancel and clear our previous pipeline and associated components
   * as they are not invalide.
   */
  ide_build_manager_cancel (self);

  ide_clear_and_destroy_object (&self->pipeline);

  g_clear_pointer (&self->running_time, g_timer_destroy);

  self->diagnostic_count = 0;
  self->error_count = 0;
  self->warning_count = 0;

  /* Don't setup anything new if we're in shutdown or we haven't
   * been told we are allowed to start.
   */
  if (ide_object_in_destruction (IDE_OBJECT (context)) || !self->started)
    IDE_EXIT;

  config_manager = ide_config_manager_from_context (context);
  device_manager = ide_device_manager_from_context (context);

  config = ide_config_manager_get_current (config_manager);
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
  self->pipeline = g_object_new (IDE_TYPE_PIPELINE,
                                 "config", config,
                                 "device", device,
                                 NULL);
  ide_object_append (IDE_OBJECT (self), IDE_OBJECT (self->pipeline));
  dzl_signal_group_set_target (self->pipeline_signals, self->pipeline);

  /*
   * Create a task to manage our async pipeline initialization state.
   */
  task = ide_task_new (self, self->cancellable, NULL, NULL);
  ide_task_set_task_data (task, g_object_ref (self->pipeline), g_object_unref);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  /*
   * Next, we need to get information on the build device, which may require
   * connecting to it. So we will query that information (so we can get
   * arch/kernel/system too). We might need that when bootstrapping the
   * runtime (if it's missing) among other things.
   */
  ide_device_get_info_async (device,
                             self->cancellable,
                             ide_build_manager_device_get_info_cb,
                             g_steal_pointer (&task));

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ERROR_COUNT]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_HAS_DIAGNOSTICS]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LAST_BUILD_TIME]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MESSAGE]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_RUNNING_TIME]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_WARNING_COUNT]);

  IDE_EXIT;
}

static void
ide_build_manager_vcs_changed (IdeBuildManager *self,
                               IdeVcs          *vcs)
{
  g_autofree gchar *branch_name = NULL;

  g_assert (IDE_IS_BUILD_MANAGER (self));
  g_assert (IDE_IS_VCS (vcs));

  /* Only invalidate the pipeline if they switched branches. Ignore things
   * like opening up `git gui` or other things that could touch the index
   * without really changing things out from underneath us.
   */

  branch_name = ide_vcs_get_branch_name (vcs);

  if (!ide_str_equal0 (branch_name, self->branch_name))
    {
      g_free (self->branch_name);
      self->branch_name = g_strdup (branch_name);
      ide_build_manager_invalidate_pipeline (self);
    }
}

static gboolean
initable_init (GInitable     *initable,
               GCancellable  *cancellable,
               GError       **error)
{
  IdeBuildManager *self = (IdeBuildManager *)initable;
  IdeConfigManager *config_manager;
  IdeDeviceManager *device_manager;
  IdeContext *context;
  IdeVcs *vcs;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_MANAGER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  context = ide_object_get_context (IDE_OBJECT (self));
  config_manager = ide_config_manager_from_context (context);
  device_manager = ide_device_manager_from_context (context);
  vcs = ide_vcs_from_context (context);

  self->branch_name = ide_vcs_get_branch_name (vcs);

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

  g_signal_connect_object (vcs,
                           "changed",
                           G_CALLBACK (ide_build_manager_vcs_changed),
                           self,
                           G_CONNECT_SWAPPED);

  ide_build_manager_invalidate_pipeline (self);

  IDE_RETURN (TRUE);
}

static void
ide_build_manager_real_build_started (IdeBuildManager *self,
                                      IdePipeline     *pipeline)
{
  IdePipelinePhase phase;

  g_assert (IDE_IS_BUILD_MANAGER (self));
  g_assert (IDE_IS_PIPELINE (pipeline));

  ide_build_manager_start_timer (self);

  /*
   * When the build completes, we may want to update diagnostics for
   * files that are open. But we only want to do this if we are reaching
   * configure for the first time, or performing a real build.
   */

  phase = ide_pipeline_get_requested_phase (pipeline);
  g_assert ((phase & IDE_PIPELINE_PHASE_MASK) == phase);

  if (phase == IDE_PIPELINE_PHASE_BUILD ||
      (phase == IDE_PIPELINE_PHASE_CONFIGURE && !self->has_configured))
    {
      self->needs_rediagnose = TRUE;
      self->has_configured = TRUE;
    }
}

static void
ide_build_manager_real_build_failed (IdeBuildManager *self,
                                     IdePipeline     *pipeline)
{
  g_assert (IDE_IS_BUILD_MANAGER (self));
  g_assert (IDE_IS_PIPELINE (pipeline));

  ide_build_manager_stop_timer (self);
}

static void
ide_build_manager_real_build_finished (IdeBuildManager *self,
                                       IdePipeline     *pipeline)
{
  IdeDiagnosticsManager *diagnostics;
  IdeBufferManager *bufmgr;
  IdeContext *context;
  guint n_items;

  g_assert (IDE_IS_BUILD_MANAGER (self));
  g_assert (IDE_IS_PIPELINE (pipeline));

  ide_build_manager_stop_timer (self);

  /*
   * If this was not a full build (such as advancing to just the configure
   * phase or so), then there is nothing more to do.
   */
  if (!self->needs_rediagnose)
    return;

  /*
   * We had a successful build, so lets notify the build manager to reload
   * dianostics on loaded buffers so the user doesn't have to make a change
   * to force the update.
   */

  context = ide_object_get_context (IDE_OBJECT (self));
  diagnostics = ide_diagnostics_manager_from_context (context);
  bufmgr = ide_buffer_manager_from_context (context);
  n_items = g_list_model_get_n_items (G_LIST_MODEL (bufmgr));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(IdeBuffer) buffer = g_list_model_get_item (G_LIST_MODEL (bufmgr), i);

      ide_diagnostics_manager_rediagnose (diagnostics, buffer);
    }

  self->needs_rediagnose = FALSE;
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

  ide_clear_and_destroy_object (&self->pipeline);
  g_clear_object (&self->pipeline_signals);
  g_clear_object (&self->cancellable);
  g_clear_pointer (&self->last_build_time, g_date_time_unref);
  g_clear_pointer (&self->running_time, g_timer_destroy);
  g_clear_pointer (&self->branch_name, g_free);

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
   * Since: 3.32
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
   * Since: 3.32
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
   * Since: 3.32
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
   * Since: 3.32
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
   * Since: 3.32
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
   * Since: 3.32
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
   * Since: 3.32
   */
  properties [PROP_PIPELINE] =
    g_param_spec_object ("pipeline",
                         "Pipeline",
                         "The build pipeline",
                         IDE_TYPE_PIPELINE,
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
   * Since: 3.32
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
   * Since: 3.32
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
   * @pipeline: An #IdePipeline
   *
   * The "build-started" signal is emitted when a new build has started.
   * The build may be an incremental build. The @pipeline instance is
   * the build pipeline which is being executed.
   *
   * Since: 3.32
   */
  signals [BUILD_STARTED] =
    g_signal_new_class_handler ("build-started",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (ide_build_manager_real_build_started),
                                NULL, NULL,
                                NULL,
                                G_TYPE_NONE, 1, IDE_TYPE_PIPELINE);

  /**
   * IdeBuildManager::build-failed:
   * @self: An #IdeBuildManager
   * @pipeline: An #IdePipeline
   *
   * The "build-failed" signal is emitted when a build that was previously
   * notified via #IdeBuildManager::build-started has failed to complete
   * successfully.
   *
   * Contrast this with #IdeBuildManager::build-finished for a successful
   * build.
   *
   * Since: 3.32
   */
  signals [BUILD_FAILED] =
    g_signal_new_class_handler ("build-failed",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (ide_build_manager_real_build_failed),
                                NULL, NULL,
                                NULL,
                                G_TYPE_NONE, 1, IDE_TYPE_PIPELINE);

  /**
   * IdeBuildManager::build-finished:
   * @self: An #IdeBuildManager
   * @pipeline: An #IdePipeline
   *
   * The "build-finished" signal is emitted when a build completed
   * successfully.
   *
   * Since: 3.32
   */
  signals [BUILD_FINISHED] =
    g_signal_new_class_handler ("build-finished",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (ide_build_manager_real_build_finished),
                                NULL, NULL,
                                NULL,
                                G_TYPE_NONE, 1, IDE_TYPE_PIPELINE);
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

  ide_build_manager_build_async (self, IDE_PIPELINE_PHASE_BUILD, NULL, NULL, NULL, NULL);

  IDE_EXIT;
}

static void
ide_build_manager_action_rebuild (IdeBuildManager *self,
                                  GVariant        *param)
{
  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_MANAGER (self));

  ide_build_manager_rebuild_async (self, IDE_PIPELINE_PHASE_BUILD, NULL, NULL, NULL, NULL);

  IDE_EXIT;
}

static void
ide_build_manager_action_clean (IdeBuildManager *self,
                                GVariant        *param)
{
  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_MANAGER (self));

  ide_build_manager_clean_async (self, IDE_PIPELINE_PHASE_BUILD, NULL, NULL, NULL);

  IDE_EXIT;
}

static void
ide_build_manager_action_install (IdeBuildManager *self,
                                  GVariant        *param)
{
  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_MANAGER (self));

  ide_build_manager_build_async (self, IDE_PIPELINE_PHASE_INSTALL, NULL, NULL, NULL, NULL);

  IDE_EXIT;
}

static void
ide_build_manager_action_export (IdeBuildManager *self,
                                 GVariant        *param)
{
  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_MANAGER (self));

  ide_build_manager_build_async (self, IDE_PIPELINE_PHASE_EXPORT, NULL, NULL, NULL, NULL);

  IDE_EXIT;
}

static void
ide_build_manager_init (IdeBuildManager *self)
{
  IDE_ENTRY;

  ide_build_manager_update_action_enabled (self);

  self->cancellable = g_cancellable_new ();
  self->needs_rediagnose = TRUE;

  self->pipeline_signals = dzl_signal_group_new (IDE_TYPE_PIPELINE);

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
 * Since: 3.32
 */
gboolean
ide_build_manager_get_busy (IdeBuildManager *self)
{
  g_return_val_if_fail (IDE_IS_BUILD_MANAGER (self), FALSE);

  if G_LIKELY (self->pipeline != NULL)
    return ide_pipeline_get_busy (self->pipeline);

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
 * Since: 3.32
 */
gchar *
ide_build_manager_get_message (IdeBuildManager *self)
{
  g_return_val_if_fail (IDE_IS_BUILD_MANAGER (self), NULL);

  if G_LIKELY (self->pipeline != NULL)
    return ide_pipeline_get_message (self->pipeline);

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
 * Since: 3.32
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
 * Since: 3.32
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
 * Since: 3.32
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
    _ide_pipeline_cancel (self->pipeline);

  IDE_EXIT;
}

/**
 * ide_build_manager_get_pipeline:
 * @self: An #IdeBuildManager
 *
 * This function gets the current build pipeline. The pipeline will be
 * reloaded as build configurations change.
 *
 * Returns: (transfer none) (nullable): An #IdePipeline.
 *
 * Since: 3.32
 */
IdePipeline *
ide_build_manager_get_pipeline (IdeBuildManager *self)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_BUILD_MANAGER (self), NULL);

  return self->pipeline;
}

/**
 * ide_build_manager_ref_pipeline:
 * @self: a #IdeBuildManager
 *
 * A thread-safe variant of ide_build_manager_get_pipeline().
 *
 * Returns: (transfer full) (nullable): an #IdePipeline or %NULL
 *
 * Since: 3.32
 */
IdePipeline *
ide_build_manager_ref_pipeline (IdeBuildManager *self)
{
  IdePipeline *ret = NULL;

  g_return_val_if_fail (IDE_IS_BUILD_MANAGER (self), NULL);

  ide_object_lock (IDE_OBJECT (self));
  g_set_object (&ret, self->pipeline);
  ide_object_unlock (IDE_OBJECT (self));

  return g_steal_pointer (&ret);
}

static void
ide_build_manager_build_targets_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  IdePipeline *pipeline = (IdePipeline *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_pipeline_build_targets_finish (pipeline, result, &error))
    {
      ide_object_warning (pipeline, "%s", error->message);
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_GOTO (failure);
    }

  ide_task_return_boolean (task, TRUE);

failure:
  IDE_EXIT;
}

static void
ide_build_manager_save_all_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  IdeBufferManager *buffer_manager = (IdeBufferManager *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeBuildManager *self;
  GCancellable *cancellable;
  GPtrArray *targets;
  IdePipelinePhase phase;

  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  cancellable = ide_task_get_cancellable (task);
  targets = ide_task_get_task_data (task);

  g_assert (IDE_IS_BUILD_MANAGER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (!ide_buffer_manager_save_all_finish (buffer_manager, result, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  phase = ide_pipeline_get_requested_phase (self->pipeline);

  ide_pipeline_build_targets_async (self->pipeline,
                                    phase,
                                    targets,
                                    cancellable,
                                    ide_build_manager_build_targets_cb,
                                    g_steal_pointer (&task));

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_HAS_DIAGNOSTICS]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LAST_BUILD_TIME]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_RUNNING_TIME]);

  IDE_EXIT;
}

/**
 * ide_build_manager_build_async:
 * @self: An #IdeBuildManager
 * @phase: An #IdePipelinePhase or 0
 * @targets: (nullable) (element-type IdeBuildTarget): an array of
 *   #IdeBuildTarget to build
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: A callback to execute upon completion
 * @user_data: user data for @callback
 *
 * This function will request that @phase is completed in the underlying
 * build pipeline and execute a build. Upon completion, @callback will be
 * executed and it can determine the success or failure of the operation
 * using ide_build_manager_build_finish().
 *
 * Since: 3.32
 */
void
ide_build_manager_build_async (IdeBuildManager     *self,
                               IdePipelinePhase     phase,
                               GPtrArray           *targets,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  IdeBufferManager *buffer_manager;
  IdeContext *context;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_BUILD_MANAGER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (!g_cancellable_is_cancelled (self->cancellable));

  cancellable = dzl_cancellable_chain (cancellable, self->cancellable);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_build_manager_build_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);
  ide_task_set_return_on_cancel (task, TRUE);

  if (targets != NULL)
    ide_task_set_task_data (task, _g_ptr_array_copy_objects (targets), g_ptr_array_unref);

  if (self->pipeline == NULL ||
      self->can_build == FALSE ||
      !ide_pipeline_is_ready (self->pipeline))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_PENDING,
                                 "Cannot execute pipeline, it has not yet been prepared");
      IDE_EXIT;
    }

  if (!ide_pipeline_request_phase (self->pipeline, phase))
    {
      ide_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  /*
   * Only update our "build time" if we are advancing to IDE_PIPELINE_PHASE_BUILD,
   * we don't really care about "builds" for configure stages and less.
   */
  if ((phase & IDE_PIPELINE_PHASE_MASK) >= IDE_PIPELINE_PHASE_BUILD)
    {
      g_clear_pointer (&self->last_build_time, g_date_time_unref);
      self->last_build_time = g_date_time_new_now_local ();
      self->diagnostic_count = 0;
      self->warning_count = 0;
      self->error_count = 0;
    }

  ide_build_manager_reset_info (self);

  /*
   * If we are performing a real build (not just something like configure),
   * then we want to ensure we save all the buffers. We don't want to do this
   * on every keypress (and execute_async() could be called on every keypress)
   * for ensuring build flags are up to date.
   */
  if ((phase & IDE_PIPELINE_PHASE_MASK) >= IDE_PIPELINE_PHASE_BUILD)
    {
      context = ide_object_get_context (IDE_OBJECT (self));
      buffer_manager = ide_buffer_manager_from_context (context);
      ide_buffer_manager_save_all_async (buffer_manager,
                                         NULL,
                                         ide_build_manager_save_all_cb,
                                         g_steal_pointer (&task));
      IDE_EXIT;
    }

  ide_pipeline_build_targets_async (self->pipeline,
                                    phase,
                                    targets,
                                    cancellable,
                                    ide_build_manager_build_targets_cb,
                                    g_steal_pointer (&task));

  IDE_EXIT;
}

/**
 * ide_build_manager_build_finish:
 * @self: An #IdeBuildManager
 * @result: a #GAsyncResult
 * @error: A location for a #GError or %NULL
 *
 * Completes a request to ide_build_manager_build_async().
 *
 * Returns: %TRUE if successful, otherwise %FALSE and @error is set.
 *
 * Since: 3.32
 */
gboolean
ide_build_manager_build_finish (IdeBuildManager  *self,
                                GAsyncResult     *result,
                                GError          **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_BUILD_MANAGER (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
ide_build_manager_clean_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  IdePipeline *pipeline = (IdePipeline *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_pipeline_clean_finish (pipeline, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
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
 * See ide_pipeline_clean_async() for more information.
 *
 * Since: 3.32
 */
void
ide_build_manager_clean_async (IdeBuildManager     *self,
                               IdePipelinePhase     phase,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_BUILD_MANAGER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (!g_cancellable_is_cancelled (self->cancellable));

  cancellable = dzl_cancellable_chain (cancellable, self->cancellable);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_build_manager_clean_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);
  ide_task_set_return_on_cancel (task, TRUE);

  if (self->pipeline == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_PENDING,
                                 "Cannot execute pipeline, it has not yet been prepared");
      IDE_EXIT;
    }

  ide_build_manager_reset_info (self);

  ide_pipeline_clean_async (self->pipeline,
                            phase,
                            cancellable,
                            ide_build_manager_clean_cb,
                            g_steal_pointer (&task));

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
 * Since: 3.32
 */
gboolean
ide_build_manager_clean_finish (IdeBuildManager  *self,
                                GAsyncResult     *result,
                                GError          **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_BUILD_MANAGER (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
ide_build_manager_rebuild_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  IdePipeline *pipeline = (IdePipeline *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_pipeline_rebuild_finish (pipeline, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

/**
 * ide_build_manager_rebuild_async:
 * @self: a #IdeBuildManager
 * @phase: the build phase to rebuild to
 * @targets: (element-type IdeBuildTarget) (nullable): an array of #GPtrArray
 *   of #IdeBuildTarget or %NULL.
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: (nullable): a callback to execute upon completion, or %NULL
 * @user_data: closure data for @callback
 *
 * Asynchronously requests that the build pipeline clean and rebuild up
 * to the given phase. This may involve discarding previous build artifacts
 * to allow for the rebuild process.
 *
 * See ide_pipeline_rebuild_async() for more information.
 *
 * Since: 3.32
 */
void
ide_build_manager_rebuild_async (IdeBuildManager     *self,
                                 IdePipelinePhase        phase,
                                 GPtrArray           *targets,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_BUILD_MANAGER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (!g_cancellable_is_cancelled (self->cancellable));

  cancellable = dzl_cancellable_chain (cancellable, self->cancellable);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_build_manager_rebuild_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);
  ide_task_set_return_on_cancel (task, TRUE);

  if (self->pipeline == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_PENDING,
                                 "Cannot execute pipeline, it has not yet been prepared");
      IDE_EXIT;
    }

  ide_build_manager_reset_info (self);

  ide_pipeline_rebuild_async (self->pipeline,
                              phase,
                              targets,
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
 * Since: 3.32
 */
gboolean
ide_build_manager_rebuild_finish (IdeBuildManager  *self,
                                  GAsyncResult     *result,
                                  GError          **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_BUILD_MANAGER (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

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
 * Since: 3.32
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

/**
 * ide_build_manager_invalidate:
 * @self: a #IdeBuildManager
 *
 * Requests that the #IdeBuildManager invalidate the current pipeline and
 * setup a new pipeline.
 *
 * Since: 3.32
 */
void
ide_build_manager_invalidate (IdeBuildManager *self)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_BUILD_MANAGER (self));

  ide_build_manager_invalidate_pipeline (self);
}

guint
ide_build_manager_get_error_count (IdeBuildManager *self)
{
  g_return_val_if_fail (IDE_IS_BUILD_MANAGER (self), 0);

  return self->error_count;
}

guint
ide_build_manager_get_warning_count (IdeBuildManager *self)
{
  g_return_val_if_fail (IDE_IS_BUILD_MANAGER (self), 0);

  return self->warning_count;
}

void
_ide_build_manager_start (IdeBuildManager *self)
{
  g_return_if_fail (IDE_IS_BUILD_MANAGER (self));
  g_return_if_fail (self->started == FALSE);

  self->started = TRUE;

  ide_build_manager_invalidate (self);
}
