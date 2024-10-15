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

#include <glib/gi18n.h>

#include <libpeas.h>

#include <libide-core.h>
#include <libide-code.h>
#include <libide-plugins.h>
#include <libide-threading.h>
#include <libide-vcs.h>

#include "ide-marshal.h"

#include "ide-build-manager.h"
#include "ide-build-private.h"
#include "ide-build-target.h"
#include "ide-build-target-provider.h"
#include "ide-config-manager.h"
#include "ide-config.h"
#include "ide-device-info.h"
#include "ide-device-manager.h"
#include "ide-device.h"
#include "ide-foundry-compat.h"
#include "ide-pipeline.h"
#include "ide-runtime-manager.h"
#include "ide-runtime-private.h"
#include "ide-runtime.h"
#include "ide-toolchain-manager.h"
#include "ide-toolchain-private.h"
#include "ide-triplet.h"

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
 */

struct _IdeBuildManager
{
  IdeObject         parent_instance;

  GCancellable     *cancellable;

  IdePipeline      *pipeline;
  GDateTime        *last_build_time;
  GSignalGroup     *pipeline_signals;

  IdeExtensionSetAdapter
                   *build_target_providers;

  char             *branch_name;

  /* The name of the default build target to build if no targets
   * are specified. Setting to NULL (or empty string) implies that
   * no target should be specified and therefore the build system
   * should attempt a "full build" such as you would get by running
   * `make` or `ninja`.
   */
  char             *default_build_target;

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

typedef struct
{
  IdePipeline      *pipeline;
  GPtrArray        *targets;
  char             *default_target;
  IdePipelinePhase  phase;
} BuildState;

static void initable_iface_init                           (GInitableIface  *iface);
static void ide_build_manager_set_can_build               (IdeBuildManager *self,
                                                           gboolean         can_build);
static void ide_build_manager_action_build                (IdeBuildManager *self,
                                                           GVariant        *param);
static void ide_build_manager_action_rebuild              (IdeBuildManager *self,
                                                           GVariant        *param);
static void ide_build_manager_action_cancel               (IdeBuildManager *self,
                                                           GVariant        *param);
static void ide_build_manager_action_clean                (IdeBuildManager *self,
                                                           GVariant        *param);
static void ide_build_manager_action_export               (IdeBuildManager *self,
                                                           GVariant        *param);
static void ide_build_manager_action_install              (IdeBuildManager *self,
                                                           GVariant        *param);
static void ide_build_manager_action_invalidate           (IdeBuildManager *self,
                                                           GVariant        *param);
static void ide_build_manager_action_default_build_target (IdeBuildManager *self,
                                                           GVariant        *param);

IDE_DEFINE_ACTION_GROUP (IdeBuildManager, ide_build_manager, {
  { "build", ide_build_manager_action_build },
  { "cancel", ide_build_manager_action_cancel },
  { "clean", ide_build_manager_action_clean },
  { "export", ide_build_manager_action_export },
  { "install", ide_build_manager_action_install },
  { "rebuild", ide_build_manager_action_rebuild },
  { "default-build-target", ide_build_manager_action_default_build_target, "s", "''" },
  { "invalidate", ide_build_manager_action_invalidate },
})

G_DEFINE_TYPE_EXTENDED (IdeBuildManager, ide_build_manager, IDE_TYPE_OBJECT, G_TYPE_FLAG_FINAL,
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

static void
build_state_free (BuildState *state)
{
  g_clear_pointer (&state->default_target, g_free);
  g_clear_pointer (&state->targets, g_ptr_array_unref);
  g_clear_object (&state->pipeline);
  g_slice_free (BuildState, state);
}

static void
ide_build_manager_action_invalidate (IdeBuildManager *self,
                                     GVariant        *param)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUILD_MANAGER (self));

  ide_build_manager_invalidate (self);
}

static void
ide_build_manager_action_default_build_target (IdeBuildManager *self,
                                               GVariant        *param)
{
  const char *str;

  g_assert (IDE_IS_BUILD_MANAGER (self));
  g_assert (param != NULL);
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE_STRING));

  str = g_variant_get_string (param, NULL);
  if (ide_str_empty0 (str))
    str = NULL;

  if (g_set_str (&self->default_build_target, str))
    ide_build_manager_set_action_state (self,
                                        "default-build-target",
                                        g_variant_new_string (str ? str : ""));
}

static void
ide_build_manager_rediagnose (IdeBuildManager *self)
{
  IdeDiagnosticsManager *diagnostics;
  IdeBufferManager *buffer_manager;
  IdeContext *context;
  guint n_items;

  g_assert (IDE_IS_BUILD_MANAGER (self));

  context = ide_object_get_context (IDE_OBJECT (self));
  buffer_manager = ide_buffer_manager_from_context (context);
  diagnostics = ide_diagnostics_manager_from_context (context);
  n_items = g_list_model_get_n_items (G_LIST_MODEL (buffer_manager));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(IdeBuffer) buffer = g_list_model_get_item (G_LIST_MODEL (buffer_manager), i);

      ide_diagnostics_manager_rediagnose (diagnostics, buffer);
    }
}

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

  self->timer_source = g_timeout_add_seconds (1, timer_callback, self);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_RUNNING_TIME]);

  IDE_EXIT;
}

static void
ide_build_manager_stop_timer (IdeBuildManager *self)
{
  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_MANAGER (self));

  g_clear_handle_id (&self->timer_source, g_source_remove);

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

  if (ide_pipeline_is_ready (pipeline))
    ide_build_manager_rediagnose (self);
  else
    g_signal_connect_object (pipeline,
                             "loaded",
                             G_CALLBACK (ide_build_manager_rediagnose),
                             self,
                             G_CONNECT_SWAPPED);

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
      if (error != NULL)
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
  IdeConfig *config;
  IdeContext *context;
  IdeDevice *device;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_MANAGER (self));

  if (!self->started)
    {
      IDE_TRACE_MSG ("Ignoring invalidation, project not yet loaded");
      IDE_EXIT;
    }

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
      g_clear_handle_id (&self->timer_source, g_source_remove);
      g_signal_emit (self, signals [BUILD_FAILED], 0, self->pipeline);
    }

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
  g_signal_group_set_target (self->pipeline_signals, self->pipeline);

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

  if (g_set_str (&self->branch_name, branch_name))
    ide_build_manager_invalidate_pipeline (self);
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
  g_assert (IDE_IS_BUILD_MANAGER (self));
  g_assert (IDE_IS_PIPELINE (pipeline));

  ide_build_manager_stop_timer (self);

  if (self->needs_rediagnose)
    {
      self->needs_rediagnose = FALSE;
      ide_build_manager_rediagnose (self);
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

  ide_clear_and_destroy_object (&self->build_target_providers);
  ide_clear_and_destroy_object (&self->pipeline);
  g_clear_object (&self->pipeline_signals);
  g_clear_object (&self->cancellable);
  g_clear_pointer (&self->last_build_time, g_date_time_unref);
  g_clear_pointer (&self->running_time, g_timer_destroy);
  g_clear_pointer (&self->branch_name, g_free);
  g_clear_pointer (&self->default_build_target, g_free);
  g_clear_handle_id (&self->timer_source, g_source_remove);

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
   */
  signals [BUILD_STARTED] =
    g_signal_new_class_handler ("build-started",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (ide_build_manager_real_build_started),
                                NULL, NULL,
                                ide_marshal_VOID__OBJECT,
                                G_TYPE_NONE, 1, IDE_TYPE_PIPELINE);
  g_signal_set_va_marshaller (signals [BUILD_STARTED],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__OBJECTv);

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
   */
  signals [BUILD_FAILED] =
    g_signal_new_class_handler ("build-failed",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (ide_build_manager_real_build_failed),
                                NULL, NULL,
                                ide_marshal_VOID__OBJECT,
                                G_TYPE_NONE, 1, IDE_TYPE_PIPELINE);
  g_signal_set_va_marshaller (signals [BUILD_FAILED],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__OBJECTv);

  /**
   * IdeBuildManager::build-finished:
   * @self: An #IdeBuildManager
   * @pipeline: An #IdePipeline
   *
   * The "build-finished" signal is emitted when a build completed
   * successfully.
   */
  signals [BUILD_FINISHED] =
    g_signal_new_class_handler ("build-finished",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (ide_build_manager_real_build_finished),
                                NULL, NULL,
                                ide_marshal_VOID__OBJECT,
                                G_TYPE_NONE, 1, IDE_TYPE_PIPELINE);
  g_signal_set_va_marshaller (signals [BUILD_FINISHED],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__OBJECTv);
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

  self->pipeline_signals = g_signal_group_new (IDE_TYPE_PIPELINE);

  g_signal_group_connect_object (self->pipeline_signals,
                                   "diagnostic",
                                   G_CALLBACK (ide_build_manager_handle_diagnostic),
                                   self,
                                   G_CONNECT_SWAPPED);

  g_signal_group_connect_object (self->pipeline_signals,
                                   "notify::busy",
                                   G_CALLBACK (ide_build_manager_notify_busy),
                                   self,
                                   G_CONNECT_SWAPPED);

  g_signal_group_connect_object (self->pipeline_signals,
                                   "notify::message",
                                   G_CALLBACK (ide_build_manager_notify_message),
                                   self,
                                   G_CONNECT_SWAPPED);

  g_signal_group_connect_object (self->pipeline_signals,
                                   "started",
                                   G_CALLBACK (ide_build_manager_pipeline_started),
                                   self,
                                   G_CONNECT_SWAPPED);

  g_signal_group_connect_object (self->pipeline_signals,
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
      if (!ide_error_ignore (error))
        ide_object_message (pipeline, "%s", error->message);
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_GOTO (failure);
    }

  ide_task_return_boolean (task, TRUE);

failure:
  IDE_EXIT;
}

static void
ide_build_manager_build_list_targets_cb (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
  IdeBuildManager *self = (IdeBuildManager *)object;
  g_autoptr(GListModel) targets = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  BuildState *state;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_MANAGER (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  state = ide_task_get_task_data (task);

  g_assert (state != NULL);
  g_assert (state->targets == NULL);
  g_assert (state->default_target != NULL);
  g_assert (IDE_IS_PIPELINE (state->pipeline));

  if ((targets = ide_build_manager_list_targets_finish (self, result, &error)))
    {
      guint n_items = g_list_model_get_n_items (targets);

      for (guint i = 0; i < n_items; i++)
        {
          g_autoptr(IdeBuildTarget) target = g_list_model_get_item (targets, i);
          const char *name = ide_build_target_get_name (target);

          if (g_strcmp0 (name, state->default_target) == 0)
            {
              state->targets = g_ptr_array_new_with_free_func (g_object_unref);
              g_ptr_array_add (state->targets, g_steal_pointer (&target));
              break;
            }
        }
    }

  if (error != NULL && !ide_error_ignore (error))
    g_warning ("Failed to list build targets: %s", error->message);

  ide_pipeline_build_targets_async (state->pipeline,
                                    state->phase,
                                    state->targets,
                                    ide_task_get_cancellable (task),
                                    ide_build_manager_build_targets_cb,
                                    g_object_ref (task));

  IDE_EXIT;
}

static void
ide_build_manager_build_after_save (IdeTask *task)
{
  IdeBuildManager *self;
  BuildState *state;

  IDE_ENTRY;

  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  state = ide_task_get_task_data (task);

  g_assert (IDE_IS_BUILD_MANAGER (self));
  g_assert (state != NULL);
  g_assert (IDE_IS_PIPELINE (state->pipeline));

  /* If a default build target was preferred instead of the build system
   * default then we need to go fetch that from the build target providers.
   * However, we can only do this if we are just building. Anything requiring
   * us to install means that we have to do regular builds as that will happen
   * anyway as part of the install process.
   */
  if (state->targets == NULL &&
      state->default_target != NULL &&
      state->phase < IDE_PIPELINE_PHASE_INSTALL)
    ide_build_manager_list_targets_async (self,
                                          ide_task_get_cancellable (task),
                                          ide_build_manager_build_list_targets_cb,
                                          g_object_ref (task));
  else
    ide_pipeline_build_targets_async (state->pipeline,
                                      state->phase,
                                      state->targets,
                                      ide_task_get_cancellable (task),
                                      ide_build_manager_build_targets_cb,
                                      g_object_ref (task));

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

  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_HAS_DIAGNOSTICS]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LAST_BUILD_TIME]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_RUNNING_TIME]);

  if (!ide_buffer_manager_save_all_finish (buffer_manager, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_build_manager_build_after_save (task);

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
  BuildState *state;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_BUILD_MANAGER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (!g_cancellable_is_cancelled (self->cancellable));

  cancellable = ide_cancellable_chain (cancellable, self->cancellable);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_build_manager_build_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);
  ide_task_set_return_on_cancel (task, TRUE);

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

  /* Setup our state for the build process. We try to cache everything
   * we need up front so that we don't need to deal with races between
   * asynchronous operations.
   */
  state = g_slice_new0 (BuildState);
  state->phase = phase;
  state->default_target = g_strdup (self->default_build_target);
  state->targets = targets ? _g_ptr_array_copy_objects (targets) : NULL;
  state->pipeline = g_object_ref (self->pipeline);
  ide_task_set_task_data (task, state, build_state_free);

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
      IdeContext *context = ide_object_get_context (IDE_OBJECT (self));
      IdeBufferManager *buffer_manager = ide_buffer_manager_from_context (context);

      ide_buffer_manager_save_all_async (buffer_manager,
                                         NULL,
                                         ide_build_manager_save_all_cb,
                                         g_steal_pointer (&task));
      IDE_EXIT;
    }

  ide_build_manager_build_after_save (task);

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

  cancellable = ide_cancellable_chain (cancellable, self->cancellable);

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

  cancellable = ide_cancellable_chain (cancellable, self->cancellable);

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
 */
void
ide_build_manager_invalidate (IdeBuildManager *self)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_BUILD_MANAGER (self));

  ide_build_manager_invalidate_pipeline (self);

  IDE_EXIT;
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

static void
ensure_build_target_providers (IdeBuildManager *self)
{
  g_assert (IDE_IS_BUILD_MANAGER (self));

  if (self->build_target_providers != NULL)
    return;

  self->build_target_providers =
    ide_extension_set_adapter_new (IDE_OBJECT (self),
                                   peas_engine_get_default (),
                                   IDE_TYPE_BUILD_TARGET_PROVIDER,
                                   NULL, NULL);
}

typedef struct
{
  GListStore *store;
  guint n_active;
} ListTargets;

static void
list_targets_free (ListTargets *state)
{
  g_clear_object (&state->store);
  g_slice_free (ListTargets, state);
}

static void
ide_build_manager_list_targets_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  IdeBuildTargetProvider *provider = (IdeBuildTargetProvider *)object;
  g_autoptr(GPtrArray) targets = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  ListTargets *state;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUILD_TARGET_PROVIDER (provider));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  state = ide_task_get_task_data (task);

  g_assert (state != NULL);
  g_assert (G_IS_LIST_STORE (state->store));

  targets = ide_build_target_provider_get_targets_finish (provider, result, &error);
  IDE_PTR_ARRAY_SET_FREE_FUNC (targets, g_object_unref);

  if (targets != NULL)
    {
      for (guint i = 0; i < targets->len; i++)
        {
          IdeBuildTarget *target = g_ptr_array_index (targets, i);

          g_list_store_append (state->store, target);
        }
    }

  state->n_active--;

  if (state->n_active == 0)
    {
      if (g_list_model_get_n_items (G_LIST_MODEL (state->store)) > 0)
        ide_task_return_object (task, g_steal_pointer (&state->store));
      else
        ide_task_return_new_error (task,
                                   G_IO_ERROR,
                                   G_IO_ERROR_NOT_SUPPORTED,
                                   "No build targets could be located, perhaps project needs to be configured");
    }

  IDE_EXIT;
}

static void
ide_build_manager_list_targets_foreach_cb (IdeExtensionSetAdapter *set,
                                           PeasPluginInfo         *plugin_info,
                                           GObject          *exten,
                                           gpointer                user_data)
{
  IdeBuildTargetProvider *provider = (IdeBuildTargetProvider *)exten;
  IdeTask *task = user_data;
  ListTargets *state;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (IDE_IS_BUILD_TARGET_PROVIDER (provider));
  g_assert (IDE_IS_TASK (task));

  state = ide_task_get_task_data (task);

  g_assert (state != NULL);
  g_assert (G_IS_LIST_STORE (state->store));

  state->n_active++;

  ide_build_target_provider_get_targets_async (provider,
                                               ide_task_get_cancellable (task),
                                               ide_build_manager_list_targets_cb,
                                               g_object_ref (task));

  IDE_EXIT;
}

void
ide_build_manager_list_targets_async (IdeBuildManager     *self,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  ListTargets *state;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_BUILD_MANAGER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  state = g_slice_new0 (ListTargets);
  state->store = g_list_store_new (IDE_TYPE_BUILD_TARGET);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_build_manager_list_targets_async);
  ide_task_set_task_data (task, state, list_targets_free);

  ensure_build_target_providers (self);

  ide_extension_set_adapter_foreach (self->build_target_providers,
                                     ide_build_manager_list_targets_foreach_cb,
                                     task);

  if (state->n_active == 0)
    ide_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               "No build target providers found");

  IDE_EXIT;
}

/**
 * ide_build_manager_list_targets_finish:
 * @self: a #IdeBuildManager
 * @error: a location for a #GError
 *
 * Lists available build targets.
 *
 * Completes a request to list available build targets that was started with
 * ide_build_manager_list_targets_async(). If no build targetproviders were
 * discovered or no build targets were found, this will return %NULL and @error
 * will be set to %G_IO_ERROR_NOT_SUPPORTED.
 *
 * Otherwise, a non-empty #GListModel of #IdeBuildTarget will be returned.
 *
 * Returns: (transfer full): a #GListModel of #IdeBuildTarget if successful;
 *   otherwise %NULL and @error is set.
 */
GListModel *
ide_build_manager_list_targets_finish (IdeBuildManager  *self,
                                       GAsyncResult     *result,
                                       GError          **error)
{
  GListModel *ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_BUILD_MANAGER (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  ret = ide_task_propagate_object (IDE_TASK (result), error);

  IDE_RETURN (ret);
}
