/* ide-pipeline.c
 *
 * Copyright 2016-2023 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-pipeline"

#include "config.h"

#include <string.h>

#include <glib/gi18n.h>

#include <libpeas.h>
#include <vte/vte.h>

#include <libide-core.h>
#include <libide-code.h>
#include <libide-io.h>
#include <libide-plugins.h>
#include <libide-projects.h>
#include <libide-threading.h>

#include "ide-marshal.h"

#include "ide-build-log-private.h"
#include "ide-config.h"
#include "ide-deploy-strategy.h"
#include "ide-pipeline-addin.h"
#include "ide-pipeline.h"
#include "ide-pipeline-private.h"
#include "ide-build-private.h"
#include "ide-pipeline-stage-command.h"
#include "ide-pipeline-stage-launcher.h"
#include "ide-pipeline-stage-private.h"
#include "ide-pipeline-stage.h"
#include "ide-build-system.h"
#include "ide-device-info.h"
#include "ide-device.h"
#include "ide-foundry-compat.h"
#include "ide-foundry-enums.h"
#include "ide-local-deploy-strategy.h"
#include "ide-local-device.h"
#include "ide-pty.h"
#include "ide-run-command.h"
#include "ide-run-context.h"
#include "ide-run-manager-private.h"
#include "ide-runtime.h"
#include "ide-toolchain-manager.h"
#include "ide-toolchain.h"
#include "ide-triplet.h"

G_DEFINE_QUARK (ide_build_error, ide_build_error)

/**
 * SECTION:idepipeline
 * @title: IdePipeline
 * @short_description: Pluggable build pipeline
 * @include: ide.h
 *
 * The #IdePipeline is responsible for managing the build process
 * for Builder. It consists of multiple build "phases" (see #IdePipelinePhase
 * for the individual phases). An #IdePipelineStage can be attached with
 * a priority to each phase and is the primary mechanism that plugins
 * use to perform their operations in the proper ordering.
 *
 * For example, the flatpak plugin provides its download stage as part of the
 * %IDE_PIPELINE_PHASE_DOWNLOAD phase. The autotools plugin provides stages to
 * phases such as %IDE_PIPELINE_PHASE_AUTOGEN, %IDE_PIPELINE_PHASE_CONFIGURE,
 * %IDE_PIPELINE_PHASE_BUILD, and %IDE_PIPELINE_PHASE_INSTALL.
 *
 * If you want ensure a particular phase is performed as part of a build,
 * then fall ide_pipeline_request_phase() with the phase you are
 * interested in seeing complete successfully.
 *
 * If your plugin has discovered that something has changed that invalidates a
 * given phase, use ide_pipeline_invalidate_phase() to ensure that the
 * phase is re-build the next time a requested phase of higher precedence
 * is requested.
 *
 * It can be useful to perform operations before or after a given stage (but
 * still be built as part of that stage) so the %IDE_PIPELINE_PHASE_BEFORE and
 * %IDE_PIPELINE_PHASE_AFTER flags may be xor'd with the requested phase.  If more
 * precise ordering is required, you may use the priority parameter to order
 * the operation with regards to other stages in that phase.
 *
 * Transient stages may be added to the pipeline and they will be removed after
 * the ide_pipeline_build_async() operation has completed successfully
 * or has failed. You can mark a stage as transient with
 * ide_pipeline_stage_set_transient(). This may be useful to perform operations
 * such as an "export tarball" stage which should only run once as determined
 * by the user requesting a "make dist" style operation.
 */

typedef struct
{
  guint             id;
  IdePipelinePhase  phase;
  int               priority;
  IdePipelineStage *stage;
} PipelineEntry;

typedef struct
{
  IdePipeline *self;
  GPtrArray   *addins;
} IdleLoadState;

typedef struct
{
  guint   id;
  GRegex *regex;
} ErrorFormat;

struct _IdePipeline
{
  IdeObject parent_instance;

  /*
   * A cancellable we can use to chain to all incoming requests so that
   * all tasks may be cancelled at once when _ide_pipeline_cancel()
   * is called.
   */
  GCancellable *cancellable;

  /*
   * These are our extensions to the BuildPipeline. Plugins insert
   * them and they might go about adding stages to the pipeline,
   * add error formats, or just monitor logs.
   */
  IdeExtensionSetAdapter *addins;

  /*
   * Deployment strategies help discover how to make a deployment to
   * a device which might require sending data to another system such
   * as a phone or tablet.
   */
  IdeExtensionSetAdapter *deploy_strategies;
  IdeDeployStrategy *best_strategy;
  int best_strategy_priority;

  /*
   * This is the configuration for the build. It is a snapshot of
   * the real configuration so that we do not need to synchronize
   * with the UI process for accesses.
   */
  IdeConfig *config;

  /*
   * The device we are building for. This allows components to setup
   * cross-compiling if necessary based on the architecture and system of
   * the device in question. It also allows for determining a deployment
   * strategy to get the compiled bits onto the device.
   */
  IdeDevice *device;
  IdeDeviceInfo *device_info;

  /*
   * The cached triplet for the device we're compiling for. This allows
   * plugins to avoid some classes of work when building for the same
   * system that Builder is running upon.
   */
  IdeTriplet *host_triplet;

  /*
   * The runtime we're using to build. This may be different than what
   * is specified in the IdeConfig, as the @device could alter
   * what architecture we're building for (and/or cross-compiling).
   */
  IdeRuntime *runtime;

  /*
   * The toolchain we're using to build. This may be different than what
   * is specified in the IdeConfig, as the @device could alter
   * what architecture we're building for (and/or cross-compiling).
   */
  IdeToolchain *toolchain;

  /*
   * The IdeBuildLog is a private implementation that we use to
   * log things from addins via observer callbacks.
   */
  IdeBuildLog *log;

  /*
   * These are our builddir/srcdir paths. Useful for building paths
   * by addins. We try to create a new builddir that will be unique
   * based on hashing of the configuration.
   */
  gchar *builddir;
  gchar *srcdir;

  /*
   * This is an array of PipelineEntry, which contain information we
   * need about the stage and an identifier that addins can use to
   * remove their inserted stages.
   */
  GArray *pipeline;

  /*
   * This contains the GBinding objects used to keep the "completed"
   * property of chained stages updated.
   */
  GPtrArray *chained_bindings;

  /*
   * This are used for ErrorFormat registration so that we have a
   * single place to extract "GCC-style" warnings and errors. Other
   * languages can also register these so they show up in the build
   * errors panel.
   */
  GArray *errfmts;
  gchar  *errfmt_current_dir;
  gchar  *errfmt_top_dir;
  guint   errfmt_seqnum;

  /*
   * The VtePty is used to connect to a VteTerminal. It's basically just a
   * wrapper around a PTY consumer. We then add a IdePtyIntercept to proxy
   * PTY data while allowing us to tap into the content being transmitted.
   * We can use that to run regexes against and perform additional error
   * extraction. Finally, pty_producer is the PTY device we created that
   * will get attached to stdin/stdout/stderr in our spawned subprocesses.
   * It is a producer to the PTY consumer owned by the IdePtyIntercept.
   */
  VtePty          *pty;
  IdePtyIntercept  intercept;
  IdePtyFd         pty_producer;

  /*
   * If the terminal interpreting our Pty has received a terminal
   * title update, it might set this message which we can use for
   * better build messages.
   */
  gchar *message;

  /*
   * No reference to the current stage. It is only available during
   * the asynchronous execution of the stage.
   */
  IdePipelineStage *current_stage;

  /*
   * The index of our current PipelineEntry. This should start at -1
   * to indicate that no stage is currently active.
   */
  gint position;

  /*
   * This is the requested mask to be built. It should be reset after
   * performing a build so that a followup build_async() would be
   * innocuous.
   */
  IdePipelinePhase requested_mask;

  /*
   * We queue incoming tasks in case we need for a finish task to
   * complete before our task can continue. The items in the queue
   * are DelayedTask structs with a IdeTask and the type id so we
   * can progress the task upon completion of the previous task.
   */
  GQueue task_queue;

  /*
   * We use this sequence number to give PipelineEntry instances a
   * unique identifier. The addins can use this to remove their
   * inserted build stages.
   */
  guint seqnum;

  /* We use a GSource to load addins in an idle callback so that
   * we don't block the main loop for too long. When disposing the
   * pipeline, we need to kill that operation too (since it may
   * lose access to IdeContext in the process).
   */
  guint idle_addins_load_source;

  /*
   * If we failed to build, this should be set.
   */
  guint failed : 1;

  /*
   * If we are within a build, this should be set.
   */
  guint busy : 1;

  /*
   * If we are in the middle of a clean operation.
   */
  guint in_clean : 1;

  /*
   * Precalculation if we need to look for errors on stdout. We can't rely
   * on @current_stage for this, because log entries might come in
   * asynchronously and after the processes/stage has completed.
   */
  guint errors_on_stdout : 1;

  /*
   * This is set to TRUE if the pipeline has failed initialization. That means
   * that all future operations will fail (but we can keep the object alive to
   * ensure that the manager has a valid object instance for the pipeline).
   */
  guint broken : 1;

  /*
   * This is set to TRUE when we attempt to load plugins (after the config
   * has been marked as ready).
   */
  guint loaded : 1;
};

typedef enum
{
  TASK_BUILD   = 1,
  TASK_CLEAN   = 2,
  TASK_REBUILD = 3,
} TaskType;

typedef struct
{
  /*
   * Our operation type. This will indicate one of the TaskType enum
   * which corellate to the various async functions of the pipeline.
   */
  TaskType type;

  /*
   * This is an unowned pointer to the task. Since the Operation structure is
   * the task data, we cannot reference as that would create a cycle. Instead,
   * we just rely on this becoming invalid during the task cleanup.
   */
  IdeTask *task;

  /*
   * The phase that should be met for the given pipeline operation.
   */
  IdePipelinePhase phase;

  union {
    struct {
      GPtrArray *stages;
    } clean;
    struct {
      GPtrArray *targets;
    } build;
    struct {
      GPtrArray *targets;
    } rebuild;
  };
} TaskData;

static void ide_pipeline_queue_flush  (IdePipeline         *self);
static void ide_pipeline_tick_build   (IdePipeline         *self,
                                       IdeTask             *task);
static void ide_pipeline_tick_clean   (IdePipeline         *self,
                                       IdeTask             *task);
static void ide_pipeline_tick_rebuild (IdePipeline         *self,
                                       IdeTask             *task);
static void initable_iface_init       (GInitableIface      *iface);
static void list_model_iface_init     (GListModelInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (IdePipeline, ide_pipeline, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init)
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initable_iface_init))

enum {
  PROP_0,
  PROP_BUSY,
  PROP_CONFIG,
  PROP_DEVICE,
  PROP_MESSAGE,
  PROP_PHASE,
  PROP_PTY,
  N_PROPS
};

enum {
  DIAGNOSTIC,
  STARTED,
  FINISHED,
  LOADED,
  LAUNCHER_CREATED,
  N_SIGNALS
};

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];
static const gchar *task_type_names[] = {
  NULL,
  "build",
  "clean",
  "rebuild",
};

static void
drop_caches (IdePipeline *self)
{
  g_autoptr(IdeContext) context = NULL;

  g_assert (IDE_IS_PIPELINE (self));

  /* We need to notify the run manager that it should drop any cached
   * information about the install state. This would normally be done
   * with a signal, but to simplify backporting, we can just call private
   * API between the two modules.
   */
  if ((context = ide_object_ref_context (IDE_OBJECT (self))))
    {
      IdeRunManager *run_manager = ide_run_manager_from_context (context);
      _ide_run_manager_drop_caches (run_manager);
    }
}

static void
chained_binding_clear (gpointer data)
{
  GBinding *binding = data;

  g_binding_unbind (binding);
  g_object_unref (binding);
}

static void
idle_load_state_free (gpointer data)
{
  IdleLoadState *state = data;

  g_clear_pointer (&state->addins, g_ptr_array_unref);
  g_clear_object (&state->self);
  g_slice_free (IdleLoadState, state);
}

static void
task_data_free (gpointer data)
{
  TaskData *td = data;

  if (td != NULL)
    {
      if (td->type == TASK_CLEAN)
        g_clear_pointer (&td->clean.stages, g_ptr_array_unref);
      if (td->type == TASK_BUILD)
        g_clear_pointer (&td->build.targets, g_ptr_array_unref);
      if (td->type == TASK_REBUILD)
        g_clear_pointer (&td->rebuild.targets, g_ptr_array_unref);
      td->type = 0;
      td->task = NULL;
      g_slice_free (TaskData, td);
    }
}

static TaskData *
task_data_new (IdeTask  *task,
               TaskType  type)
{
  TaskData *td;

  g_assert (IDE_IS_TASK (task));
  g_assert (type > 0);
  g_assert (type <= TASK_REBUILD);

  td = g_slice_new0 (TaskData);
  td->type = type;
  td->task = task;

  return td;
}

static void
clear_error_format (gpointer data)
{
  ErrorFormat *errfmt = data;

  errfmt->id = 0;
  g_clear_pointer (&errfmt->regex, g_regex_unref);
}

static inline const gchar *
build_phase_nick (IdePipelinePhase phase)
{
  GFlagsClass *klass = g_type_class_peek (IDE_TYPE_PIPELINE_PHASE);
  GFlagsValue *value;

  g_assert (klass != NULL);

  phase &= IDE_PIPELINE_PHASE_MASK;
  value = g_flags_get_first_value (klass, phase);

  if (value != NULL)
    return value->value_nick;

  return "unknown";
}

static IdeDiagnosticSeverity
parse_severity (const gchar *str)
{
  g_autofree gchar *lower = NULL;

  if (str == NULL)
    return IDE_DIAGNOSTIC_WARNING;

  lower = g_utf8_strdown (str, -1);

  if (strstr (lower, "fatal") != NULL)
    return IDE_DIAGNOSTIC_FATAL;

  if (strstr (lower, "error") != NULL)
    return IDE_DIAGNOSTIC_ERROR;

  if (strstr (lower, "warning") != NULL)
    return IDE_DIAGNOSTIC_WARNING;

  if (strstr (lower, "ignored") != NULL)
    return IDE_DIAGNOSTIC_IGNORED;

  if (strstr (lower, "unused") != NULL)
    return IDE_DIAGNOSTIC_UNUSED;

  if (strstr (lower, "deprecated") != NULL)
    return IDE_DIAGNOSTIC_DEPRECATED;

  if (strstr (lower, "note") != NULL)
    return IDE_DIAGNOSTIC_NOTE;

  return IDE_DIAGNOSTIC_WARNING;
}

static IdeDiagnostic *
create_diagnostic (IdePipeline *self,
                   GMatchInfo  *match_info)
{
  g_autofree gchar *filename = NULL;
  g_autofree gchar *line = NULL;
  g_autofree gchar *column = NULL;
  g_autofree gchar *message = NULL;
  g_autofree gchar *level = NULL;
  g_autoptr(GFile) file = NULL;
  g_autoptr(IdeLocation) location = NULL;
  IdeContext *context;
  struct {
    gint64 line;
    gint64 column;
    IdeDiagnosticSeverity severity;
  } parsed = { 0 };

  g_assert (IDE_IS_PIPELINE (self));
  g_assert (match_info != NULL);

  message = g_match_info_fetch_named (match_info, "message");

  /* XXX: This is a hack to ignore a common but unuseful error message.
   *      This really belongs somewhere else, but it's easier to do the
   *      check here for now. We need proper callback for ErrorRegex in
   *      the future so they can ignore it.
   */
  if (message == NULL || strncmp (message, "#warning _FORTIFY_SOURCE requires compiling with optimization", 61) == 0)
    return NULL;

  filename = g_match_info_fetch_named (match_info, "filename");
  line = g_match_info_fetch_named (match_info, "line");
  column = g_match_info_fetch_named (match_info, "column");
  level = g_match_info_fetch_named (match_info, "level");

  if (line != NULL)
    {
      parsed.line = g_ascii_strtoll (line, NULL, 10);
      if (parsed.line < 1 || parsed.line > G_MAXINT32)
        return NULL;
      parsed.line--;
    }

  if (column != NULL)
    {
      parsed.column = g_ascii_strtoll (column, NULL, 10);
      if (parsed.column < 1 || parsed.column > G_MAXINT32)
        return NULL;
      parsed.column--;
    }

  parsed.severity = parse_severity (level);

  /* Expand local user only, if we get a home-relative path */
  if (filename != NULL && strncmp (filename, "~/", 2) == 0)
    {
      gchar *expanded = ide_path_expand (filename);
      g_free (filename);
      filename = expanded;
    }

  if (!g_path_is_absolute (filename))
    {
      gchar *path;

      if (self->errfmt_current_dir != NULL)
        {
          const gchar *basedir = self->errfmt_current_dir;

          if (g_str_has_prefix (basedir, self->errfmt_top_dir))
            {
              basedir += strlen (self->errfmt_top_dir);
              if (*basedir == G_DIR_SEPARATOR)
                basedir++;
            }

          path = g_build_filename (basedir, filename, NULL);
          g_free (filename);
          filename = path;
        }
      else
        {
          path = g_build_filename (self->builddir, filename, NULL);
          g_free (filename);
          filename = path;
        }
    }

  context = ide_object_get_context (IDE_OBJECT (self));

  if (!g_path_is_absolute (filename))
    {
      g_autoptr(GFile) child = NULL;
      g_autoptr(GFile) workdir = NULL;
      gchar *path;

      workdir = ide_context_ref_workdir (context);
      child = g_file_get_child (workdir, filename);
      path = g_file_get_path (child);

      g_free (filename);
      filename = path;
    }

  file = ide_context_build_file (context, filename);
  location = ide_location_new (file, parsed.line, parsed.column);

  return ide_diagnostic_new (parsed.severity, message, location);
}

static gboolean
extract_directory_change (IdePipeline  *self,
                          const guint8 *data,
                          gsize         len)
{
  g_autofree gchar *dir = NULL;
  const guint8 *begin;

  g_assert (IDE_IS_PIPELINE (self));

  if (len == 0)
    return FALSE;

#define ENTERING_DIRECTORY_BEGIN "Entering directory '"
#define ENTERING_DIRECTORY_END   "'"

  begin = memmem (data, len, ENTERING_DIRECTORY_BEGIN, strlen (ENTERING_DIRECTORY_BEGIN));
  if (begin == NULL)
    return FALSE;

  begin += strlen (ENTERING_DIRECTORY_BEGIN);

  if (data[len - 1] != '\'')
    return FALSE;

  len = &data[len - 1] - begin;
  dir = g_strndup ((gchar *)begin, len);

  if (g_utf8_validate (dir, len, NULL))
    {
      g_free (self->errfmt_current_dir);

      if (len == 0)
        self->errfmt_current_dir = g_strdup (self->errfmt_top_dir);
      else
        self->errfmt_current_dir = g_strndup (dir, len);

      if (self->errfmt_top_dir == NULL)
        self->errfmt_top_dir = g_strdup (self->errfmt_current_dir);

      return TRUE;
    }

#undef ENTERING_DIRECTORY_BEGIN
#undef ENTERING_DIRECTORY_END

  return FALSE;
}

static void
extract_diagnostics (IdePipeline  *self,
                     const guint8 *data,
                     gsize         len)
{
  g_autofree guint8 *unescaped = NULL;
  IdeLineReader reader;
  gchar *line;
  gsize line_len;

  g_assert (IDE_IS_PIPELINE (self));
  g_assert (data != NULL);

  if (len == 0 || self->errfmts->len == 0)
    return;

  /* If we have any color escape sequences, remove them */
  if G_UNLIKELY (memchr (data, '\033', len) || memmem (data, len, "\\e", 2))
    {
      gsize out_len = 0;

      unescaped = _ide_build_utils_filter_color_codes (data, len, &out_len);
      if (out_len == 0)
        return;

      data = unescaped;
      len = out_len;
    }

  ide_line_reader_init (&reader, (gchar *)data, len);

  while (NULL != (line = ide_line_reader_next (&reader, &line_len)))
    {
      if (extract_directory_change (self, (const guint8 *)line, line_len))
        continue;

      for (guint i = 0; i < self->errfmts->len; i++)
        {
          const ErrorFormat *errfmt = &g_array_index (self->errfmts, ErrorFormat, i);
          g_autoptr(GMatchInfo) match_info = NULL;

          if (g_regex_match_full (errfmt->regex, line, line_len, 0, 0, &match_info, NULL))
            {
              g_autoptr(IdeDiagnostic) diagnostic = create_diagnostic (self, match_info);

              if (diagnostic != NULL)
                {
                  ide_pipeline_emit_diagnostic (self, diagnostic);
                  break;
                }
            }
        }
    }
}

static void
ide_pipeline_log_observer (IdeBuildLogStream  stream,
                           const gchar       *message,
                           gssize             message_len,
                           gpointer           user_data)
{
  IdePipeline *self = user_data;

  g_assert (stream == IDE_BUILD_LOG_STDOUT || stream == IDE_BUILD_LOG_STDERR);
  g_assert (IDE_IS_PIPELINE (self));
  g_assert (message != NULL);

  if (message_len < 0)
    message_len = strlen (message);

  if (self->log != NULL)
    ide_build_log_observer (stream, message, message_len, self->log);

  extract_diagnostics (self, (const guint8 *)message, message_len);
}

static void
ide_pipeline_intercept_pty_consumer_cb (const IdePtyIntercept     *intercept,
                                        const IdePtyInterceptSide *side,
                                        const guint8              *data,
                                        gsize                      len,
                                        gpointer                   user_data)
{
  IdePipeline *self = user_data;

  g_assert (intercept != NULL);
  g_assert (side != NULL);
  g_assert (data != NULL);
  g_assert (len > 0);
  g_assert (IDE_IS_PIPELINE (self));

  extract_diagnostics (self, data, len);
}

static void
ide_pipeline_release_transients (IdePipeline *self)
{
  IDE_ENTRY;

  g_assert (IDE_IS_PIPELINE (self));
  g_assert (self->pipeline != NULL);

  for (guint i = self->pipeline->len; i > 0; i--)
    {
      const PipelineEntry *entry = &g_array_index (self->pipeline, PipelineEntry, i - 1);

      g_assert (IDE_IS_PIPELINE_STAGE (entry->stage));

      if (ide_pipeline_stage_get_transient (entry->stage))
        {
          IDE_TRACE_MSG ("Releasing transient stage %s at index %u",
                         G_OBJECT_TYPE_NAME (entry->stage),
                         i - 1);
          g_array_remove_index (self->pipeline, i - 1);
          g_list_model_items_changed (G_LIST_MODEL (self), i - 1, 1, 0);
        }
    }

  IDE_EXIT;
}

static gboolean
ide_pipeline_check_ready (IdePipeline *self,
                          IdeTask     *task)
{
  g_assert (IDE_IS_PIPELINE (self));
  g_assert (IDE_IS_TASK (task));

  if (self->broken)
    {
      ide_task_return_new_error (task,
                                 IDE_BUILD_ERROR,
                                 IDE_BUILD_ERROR_BROKEN,
                                 _("The build pipeline is in a failed state"));
      return FALSE;
    }

  if (self->loaded == FALSE)
    {
      /* configuration:ready is FALSE */
      ide_task_return_new_error (task,
                                 IDE_BUILD_ERROR,
                                 IDE_BUILD_ERROR_NOT_LOADED,
                                 _("The build configuration has errors"));
      return FALSE;
    }

  return TRUE;
}

/**
 * ide_pipeline_get_phase:
 *
 * Gets the current phase that is executing. This is only useful during
 * execution of the pipeline.
 */
IdePipelinePhase
ide_pipeline_get_phase (IdePipeline *self)
{
  g_return_val_if_fail (IDE_IS_PIPELINE (self), 0);

  if (self->position < 0)
    return IDE_PIPELINE_PHASE_NONE;
  else if (self->failed)
    return IDE_PIPELINE_PHASE_FAILED;
  else if ((guint)self->position < self->pipeline->len)
    return g_array_index (self->pipeline, PipelineEntry, self->position).phase & IDE_PIPELINE_PHASE_MASK;
  else
    return IDE_PIPELINE_PHASE_FINISHED;
}

/**
 * ide_pipeline_get_config:
 *
 * Gets the #IdeConfig to use for the pipeline.
 *
 * Returns: (transfer none): An #IdeConfig
 */
IdeConfig *
ide_pipeline_get_config (IdePipeline *self)
{
  g_return_val_if_fail (IDE_IS_PIPELINE (self), NULL);

  return self->config;
}

static void
clear_pipeline_entry (gpointer data)
{
  PipelineEntry *entry = data;

  if (entry->stage != NULL)
    {
      ide_pipeline_stage_set_log_observer (entry->stage, NULL, NULL, NULL);
      g_clear_object (&entry->stage);
    }
}

static gint
pipeline_entry_compare (gconstpointer a,
                        gconstpointer b)
{
  const PipelineEntry *entry_a = a;
  const PipelineEntry *entry_b = b;
  gint ret;

  ret = (gint)(entry_a->phase & IDE_PIPELINE_PHASE_MASK)
      - (gint)(entry_b->phase & IDE_PIPELINE_PHASE_MASK);

  if (ret == 0)
    {
      gint whence_a = (entry_a->phase & IDE_PIPELINE_PHASE_WHENCE_MASK);
      gint whence_b = (entry_b->phase & IDE_PIPELINE_PHASE_WHENCE_MASK);

      if (whence_a != whence_b)
        {
          if (whence_a == IDE_PIPELINE_PHASE_BEFORE)
            return -1;

          if (whence_b == IDE_PIPELINE_PHASE_BEFORE)
            return 1;

          if (whence_a == 0)
            return -1;

          if (whence_b == 0)
            return 1;

          g_assert_not_reached ();
        }
    }

  if (ret == 0)
    ret = entry_a->priority - entry_b->priority;

  return ret;
}

static void
ide_pipeline_real_started (IdePipeline *self)
{
  IDE_ENTRY;

  g_assert (IDE_IS_PIPELINE (self));

  if (self->in_clean)
    ide_object_message (IDE_OBJECT (self), _("Pipeline started to clean build tree"));
  else
    ide_object_message (IDE_OBJECT (self), _("Pipeline started"));

  self->errors_on_stdout = FALSE;

  for (guint i = 0; i < self->pipeline->len; i++)
    {
      PipelineEntry *entry = &g_array_index (self->pipeline, PipelineEntry, i);

      if (ide_pipeline_stage_get_check_stdout (entry->stage))
        {
          self->errors_on_stdout = TRUE;
          break;
        }
    }

  IDE_EXIT;
}

static void
ide_pipeline_real_finished (IdePipeline *self,
                            gboolean     failed)
{
  IDE_ENTRY;

  g_assert (IDE_IS_PIPELINE (self));

  ide_object_message (IDE_OBJECT (self), _("Pipeline finished"));

  IDE_EXIT;
}

static void
ide_pipeline_extension_prepare (IdeExtensionSetAdapter *set,
                                PeasPluginInfo         *plugin_info,
                                GObject          *exten,
                                gpointer                user_data)
{
  IdePipeline *self = user_data;
  IdePipelineAddin *addin = (IdePipelineAddin *)exten;

  IDE_ENTRY;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_PIPELINE_ADDIN (addin));
  g_assert (IDE_IS_PIPELINE (self));

  ide_pipeline_addin_prepare (addin, self);

  IDE_EXIT;
}

static void
ide_pipeline_extension_added (IdeExtensionSetAdapter *set,
                              PeasPluginInfo         *plugin_info,
                              GObject          *exten,
                              gpointer                user_data)
{
  IdePipeline *self = user_data;
  IdePipelineAddin *addin = (IdePipelineAddin *)exten;

  IDE_ENTRY;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_PIPELINE_ADDIN (addin));
  g_assert (IDE_IS_PIPELINE (self));

  /* Mark that we loaded this addin, so we don't unload it if it
   * was never loaded (during async loading).
   */
  g_object_set_data (G_OBJECT (addin), "HAS_LOADED", GINT_TO_POINTER (1));

  ide_pipeline_addin_load (addin, self);

  IDE_EXIT;
}

static void
ide_pipeline_extension_removed (IdeExtensionSetAdapter *set,
                                PeasPluginInfo         *plugin_info,
                                GObject          *exten,
                                gpointer                user_data)
{
  IdePipeline *self = user_data;
  IdePipelineAddin *addin = (IdePipelineAddin *)exten;

  IDE_ENTRY;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_PIPELINE_ADDIN (addin));
  g_assert (IDE_IS_PIPELINE (self));

  if (g_object_get_data (G_OBJECT (addin), "HAS_LOADED"))
    ide_pipeline_addin_unload (addin, self);

  IDE_EXIT;
}

static void
ide_pipeline_always_incomplete (IdePipelineStage *stage,
                                IdePipeline      *pipeline,
                                GPtrArray        *targets,
                                GCancellable     *cancellable,
                                gpointer          user_data)
{
  IDE_ENTRY;

  g_assert (IDE_IS_PIPELINE_STAGE (stage));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (user_data == NULL);

  ide_pipeline_stage_set_completed (stage, FALSE);

  IDE_EXIT;
}

static void
register_build_commands_stage (IdePipeline *self,
                               IdeContext  *context)
{
  const gchar * const *build_commands;
  g_autofree gchar *rundir_path = NULL;
  GFile *rundir;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_PIPELINE (self));
  g_assert (IDE_IS_CONTEXT (context));
  g_assert (IDE_IS_CONFIG (self->config));

  if (!(build_commands = ide_config_get_build_commands (self->config)))
    return;

  if ((rundir = ide_config_get_build_commands_dir (self->config)))
    rundir_path = g_file_get_path (rundir);

  for (guint i = 0; build_commands[i]; i++)
    {
      g_autoptr(IdeRunCommand) run_command = NULL;
      g_autoptr(IdePipelineStage) stage = NULL;
      g_autofree char *title = NULL;

      run_command = ide_run_command_new ();
      ide_run_command_set_argv (run_command, IDE_STRV_INIT ("/bin/sh", "-c", build_commands[i]));

      if (rundir_path != NULL)
        ide_run_command_set_cwd (run_command, rundir_path);
      else
        ide_run_command_set_cwd (run_command, ide_pipeline_get_builddir (self));

      /* translators: %s is replaced with the build shell command */
      title = g_strdup_printf (_("Build (%s)"), build_commands[i]);

      stage = g_object_new (IDE_TYPE_PIPELINE_STAGE_COMMAND,
                            "build-command", run_command,
                            "name", title,
                            NULL);
      g_signal_connect (stage,
                        "query",
                        G_CALLBACK (ide_pipeline_always_incomplete),
                        NULL);
      ide_pipeline_attach (self,
                           IDE_PIPELINE_PHASE_BUILD | IDE_PIPELINE_PHASE_AFTER,
                           i,
                           stage);
    }
}

static void
register_post_install_commands_stage (IdePipeline *self,
                                      IdeContext  *context)
{
  const gchar * const *post_install_commands;

  g_assert (IDE_IS_PIPELINE (self));
  g_assert (IDE_IS_CONTEXT (context));
  g_assert (IDE_IS_CONFIG (self->config));

  if (!(post_install_commands = ide_config_get_post_install_commands (self->config)))
    return;

  for (guint i = 0; post_install_commands[i]; i++)
    {
      g_autoptr(IdeRunCommand) run_command = NULL;
      g_autoptr(IdePipelineStage) stage = NULL;
      g_autofree char *title = NULL;

      run_command = ide_run_command_new ();
      ide_run_command_set_argv (run_command, IDE_STRV_INIT ("/bin/sh", "-c", post_install_commands[i]));
      ide_run_command_set_cwd (run_command, ide_pipeline_get_builddir (self));

      /* translators: %s is replaced with the post-install shell command */
      title = g_strdup_printf (_("Post-install (%s)"), post_install_commands[i]);

      stage = g_object_new (IDE_TYPE_PIPELINE_STAGE_COMMAND,
                            "build-command", run_command,
                            "name", title,
                            NULL);
      g_signal_connect (stage,
                        "query",
                        G_CALLBACK (ide_pipeline_always_incomplete),
                        NULL);
      ide_pipeline_attach (self,
                           IDE_PIPELINE_PHASE_INSTALL | IDE_PIPELINE_PHASE_AFTER,
                           i,
                           stage);
    }
}

static void
collect_pipeline_addins (IdeExtensionSetAdapter *set,
                         PeasPluginInfo         *plugin_info,
                         GObject          *exten,
                         gpointer                user_data)
{
  GPtrArray *addins = user_data;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_PIPELINE_ADDIN (exten));
  g_assert (addins != NULL);

  g_ptr_array_add (addins, g_object_ref (exten));
}

static void
ide_pipeline_deploy_strategy_load_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  IdeDeployStrategy *strategy = (IdeDeployStrategy *)object;
  g_autoptr(IdePipeline) self = user_data;
  g_autoptr(GError) error = NULL;
  int priority = 0;

  IDE_ENTRY;

  g_assert (IDE_IS_DEPLOY_STRATEGY (strategy));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_PIPELINE (self));

  if (ide_deploy_strategy_load_finish (strategy, result, &priority, &error))
    {
      if (self->best_strategy == NULL || priority < self->best_strategy_priority)
        {
          g_set_object (&self->best_strategy, strategy);
          self->best_strategy_priority = priority;
          IDE_EXIT;
        }
    }

  IDE_EXIT;
}

static void
ide_pipeline_deploy_strategy_added_cb (IdeExtensionSetAdapter *set,
                                       PeasPluginInfo         *plugin_info,
                                       GObject          *exten,
                                       gpointer                user_data)
{
  IdePipeline *self = user_data;
  IdeDeployStrategy *strategy = (IdeDeployStrategy *)exten;

  IDE_ENTRY;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_DEPLOY_STRATEGY (strategy));
  g_assert (IDE_IS_PIPELINE (self));

  ide_deploy_strategy_load_async (strategy,
                                  self,
                                  self->cancellable,
                                  ide_pipeline_deploy_strategy_load_cb,
                                  g_object_ref (self));

  IDE_EXIT;
}

static gboolean
ide_pipeline_load_cb (IdleLoadState *state)
{
  IDE_ENTRY;

  g_assert (state != NULL);
  g_assert (IDE_IS_PIPELINE (state->self));
  g_assert (state->addins != NULL);

  /*
   * We only load a single addin per idle callback so that we can return to
   * the main loop and potentially start the next frame at a higher priority
   * than the addin loading.
   */

  if (state->addins->len > 0)
    {
      IdePipelineAddin *addin = g_ptr_array_index (state->addins, state->addins->len - 1);
      gint64 begin, end;

      /* Keep in sync with ide_pipeline_extension_added() */
      g_object_set_data (G_OBJECT (addin), "HAS_LOADED", GINT_TO_POINTER (1));

      begin = g_get_monotonic_time ();
      ide_pipeline_addin_load (addin, state->self);
      end = g_get_monotonic_time ();

      g_debug ("%s loaded in %lf seconds",
               G_OBJECT_TYPE_NAME (addin),
               (end - begin) / (gdouble)G_USEC_PER_SEC);

      g_ptr_array_remove_index (state->addins, state->addins->len - 1);

      if (state->addins->len > 0)
        IDE_RETURN (G_SOURCE_CONTINUE);
    }

  /* Now setup deployment strategies */
  g_signal_connect (state->self->deploy_strategies,
                    "extension-added",
                    G_CALLBACK (ide_pipeline_deploy_strategy_added_cb),
                    state->self);
  ide_extension_set_adapter_foreach (state->self->deploy_strategies,
                                     ide_pipeline_deploy_strategy_added_cb,
                                     state->self);

  state->self->loaded = TRUE;
  state->self->idle_addins_load_source = 0;

  IDE_TRACE_MSG ("Pipeline ready");

  g_signal_emit (state->self, signals [LOADED], 0);

  IDE_RETURN (G_SOURCE_REMOVE);
}

/**
 * ide_pipeline_load:
 *
 * This manages the loading of addins which will register their necessary build
 * stages.  We do this separately from ::constructed so that we can
 * enable/disable the pipeline as the IdeConfig:ready property changes.
 * This could happen when the device or runtime is added/removed while the
 * application is running.
 */
static void
ide_pipeline_load (IdePipeline *self)
{
  IdleLoadState *state;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_PIPELINE (self));
  g_assert (self->addins == NULL);

  /* We might have already disposed if our pipeline got discarded */
  if (!(context = ide_object_get_context (IDE_OBJECT (self))))
    IDE_EXIT;

  register_build_commands_stage (self, context);
  register_post_install_commands_stage (self, context);

  /* Setup pipeline addins */
  self->addins = ide_extension_set_adapter_new (IDE_OBJECT (self),
                                                peas_engine_get_default (),
                                                IDE_TYPE_PIPELINE_ADDIN,
                                                NULL, NULL);
  g_signal_connect (self->addins,
                    "extension-added",
                    G_CALLBACK (ide_pipeline_extension_prepare),
                    self);
  ide_extension_set_adapter_foreach (self->addins,
                                     ide_pipeline_extension_prepare,
                                     self);
  g_signal_connect_after (self->addins,
                          "extension-added",
                          G_CALLBACK (ide_pipeline_extension_added),
                          self);
  g_signal_connect (self->addins,
                    "extension-removed",
                    G_CALLBACK (ide_pipeline_extension_removed),
                    self);

  /* Create deployment strategies */
  self->deploy_strategies = ide_extension_set_adapter_new (IDE_OBJECT (self),
                                                           peas_engine_get_default (),
                                                           IDE_TYPE_DEPLOY_STRATEGY,
                                                           NULL, NULL);

  state = g_slice_new0 (IdleLoadState);
  state->self = g_object_ref (self);
  state->addins = g_ptr_array_new_with_free_func (g_object_unref);
  ide_extension_set_adapter_foreach (self->addins,
                                     collect_pipeline_addins,
                                     state->addins);
  self->idle_addins_load_source =
    g_idle_add_full (G_PRIORITY_LOW,
                     (GSourceFunc) ide_pipeline_load_cb,
                     state,
                     idle_load_state_free);

  IDE_EXIT;
}

static void
ide_pipeline_load_get_info_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  IdeDevice *device = (IdeDevice *)object;
  g_autoptr(IdePipeline) self = user_data;
  g_autoptr(IdeDeviceInfo) info = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_DEVICE (device));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_PIPELINE (self));

  if (!(info = ide_device_get_info_finish (device, result, &error)))
    {
      ide_object_warning (IDE_OBJECT (self),
                          "Failed to load device information: %s",
                          error->message);
      IDE_EXIT;
    }

  if (g_cancellable_is_cancelled (self->cancellable))
    IDE_EXIT;

  _ide_pipeline_check_toolchain (self, info);

  ide_pipeline_load (self);
}

static void
ide_pipeline_begin_load (IdePipeline *self)
{
  g_autofree char *srcdir = NULL;
  IdeBuildSystem *build_system;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_PIPELINE (self));
  g_assert (IDE_IS_DEVICE (self->device));

  /* First query the build system for the actual source directory
   * which may not be the same as the project workdir.
   */
  if ((context = ide_object_get_context (IDE_OBJECT (self))) &&
      (build_system = ide_build_system_from_context (context)) &&
      (srcdir = ide_build_system_get_srcdir (build_system)))
    g_set_str (&self->srcdir, srcdir);

  /*
   * The first thing we need to do is get some information from the
   * configured device. We want to know the arch/kernel/system triplet
   * for the device as some pipeline addins may need that. We can also
   * use that to ensure that we load the proper runtime and toolchain
   * for the device.
   *
   * We have to load this information asynchronously, as the device might
   * be remote (and we need to connect to it to get the information).
   */

  ide_device_get_info_async (self->device,
                             self->cancellable,
                             ide_pipeline_load_get_info_cb,
                             g_object_ref (self));

  IDE_EXIT;
}

/**
 * ide_pipeline_unload:
 * @self: an #IdePipeline
 *
 * This clears things up that were initialized in ide_pipeline_load().
 * This function is safe to run even if load has not been called. We will not
 * clean things up if the pipeline is currently executing (we can wait until
 * its finished or dispose/finalize to cleanup up further.
 */
static void
ide_pipeline_unload (IdePipeline *self)
{
  IDE_ENTRY;

  g_assert (IDE_IS_PIPELINE (self));

  g_clear_object (&self->best_strategy);

  ide_clear_and_destroy_object (&self->addins);
  ide_clear_and_destroy_object (&self->deploy_strategies);

  IDE_EXIT;
}

static void
ide_pipeline_notify_ready (IdePipeline *self,
                           GParamSpec  *pspec,
                           IdeConfig   *configuration)
{
  IDE_ENTRY;

  g_assert (IDE_IS_PIPELINE (self));
  g_assert (IDE_IS_CONFIG (configuration));

  /*
   * If we're being realistic, we can only really setup the build pipeline one
   * time, once the configuration is ready. So cancel all tracking after that
   * so that and just rely on the build manager to create a new pipeline when
   * the active configuration changes.
   */

  if (ide_config_get_ready (configuration))
    {
      g_signal_handlers_disconnect_by_func (configuration,
                                            G_CALLBACK (ide_pipeline_notify_ready),
                                            self);
      ide_pipeline_begin_load (self);
    }
  else
    g_debug ("Configuration not yet ready, delaying pipeline setup");

  IDE_EXIT;
}

static void
ide_pipeline_finalize (GObject *object)
{
  IdePipeline *self = (IdePipeline *)object;

  IDE_ENTRY;

  g_assert (self->task_queue.length == 0);
  g_queue_clear (&self->task_queue);

  g_clear_object (&self->cancellable);
  g_clear_object (&self->log);
  g_clear_object (&self->device);
  g_clear_object (&self->device_info);
  g_clear_object (&self->runtime);
  g_clear_object (&self->toolchain);
  g_clear_object (&self->config);
  g_clear_pointer (&self->pipeline, g_array_unref);
  g_clear_pointer (&self->srcdir, g_free);
  g_clear_pointer (&self->builddir, g_free);
  g_clear_pointer (&self->errfmts, g_array_unref);
  g_clear_pointer (&self->errfmt_top_dir, g_free);
  g_clear_pointer (&self->errfmt_current_dir, g_free);
  g_clear_pointer (&self->chained_bindings, g_ptr_array_unref);
  g_clear_pointer (&self->host_triplet, ide_triplet_unref);

  G_OBJECT_CLASS (ide_pipeline_parent_class)->finalize (object);

  IDE_EXIT;
}

static void
ide_pipeline_destroy (IdeObject *object)
{
  IdePipeline *self = IDE_PIPELINE (object);
  g_auto(IdePtyFd) fd = IDE_PTY_FD_INVALID;

  IDE_ENTRY;

  g_clear_handle_id (&self->idle_addins_load_source, g_source_remove);

  _ide_pipeline_cancel (self);

  ide_pipeline_unload (self);

  g_clear_pointer (&self->message, g_free);

  g_clear_object (&self->pty);
  fd = pty_fd_steal (&self->pty_producer);

  if (IDE_IS_PTY_INTERCEPT (&self->intercept))
    ide_pty_intercept_clear (&self->intercept);

  IDE_OBJECT_CLASS (ide_pipeline_parent_class)->destroy (object);

  IDE_EXIT;
}

static gboolean
ide_pipeline_initable_init (GInitable     *initable,
                            GCancellable  *cancellable,
                            GError       **error)
{
  IdePipeline *self = (IdePipeline *)initable;
  IdePtyFd consumer_fd;

  IDE_ENTRY;

  g_assert (IDE_IS_PIPELINE (self));
  g_assert (IDE_IS_CONFIG (self->config));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  g_debug ("initializing build pipeline with device %s",
           G_OBJECT_TYPE_NAME (self->device));

  if (self->runtime == NULL)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_FAILED,
                           "No runtime assigned to build pipeline");
      IDE_RETURN (FALSE);
    }

  /*
   * Create a PTY for subprocess launchers. PTY initialization does not
   * support cancellation, so do not pass @cancellable along to it.
   */
  if (!(self->pty = ide_pty_new_sync (error)))
    IDE_RETURN (FALSE);

  consumer_fd = vte_pty_get_fd (self->pty);

  if (!ide_pty_intercept_init (&self->intercept, consumer_fd, NULL))
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_FAILED,
                           "Failed to initialize PTY intercept");
      IDE_RETURN (FALSE);
    }

  ide_pty_intercept_set_callback (&self->intercept,
                                  &self->intercept.consumer,
                                  ide_pipeline_intercept_pty_consumer_cb,
                                  self);

  g_signal_connect_object (self->config,
                           "notify::ready",
                           G_CALLBACK (ide_pipeline_notify_ready),
                           self,
                           G_CONNECT_SWAPPED);

  ide_pipeline_notify_ready (self, NULL, self->config);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PTY]);

  IDE_RETURN (TRUE);
}

static void
initable_iface_init (GInitableIface *iface)
{
  iface->init = ide_pipeline_initable_init;
}

static void
ide_pipeline_parent_set (IdeObject *object,
                         IdeObject *parent)
{
  IdePipeline *self = IDE_PIPELINE (object);
  IdeToolchainManager *toolchain_manager;
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(GFile) workdir = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_PIPELINE (self));
  g_assert (!parent || IDE_IS_OBJECT (parent));
  g_assert (IDE_IS_CONFIG (self->config));

  if (parent == NULL)
    return;

  context = IDE_CONTEXT (ide_object_ref_root (IDE_OBJECT (self)));
  workdir = ide_context_ref_workdir (context);

  self->srcdir = g_file_get_path (workdir);

  toolchain_manager = ide_toolchain_manager_from_context (context);
  self->toolchain = ide_toolchain_manager_get_toolchain (toolchain_manager, "default");

  IDE_EXIT;
}

static void
ide_pipeline_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  IdePipeline *self = IDE_PIPELINE (object);

  switch (prop_id)
    {
    case PROP_BUSY:
      g_value_set_boolean (value, self->busy);
      break;

    case PROP_CONFIG:
      g_value_set_object (value, ide_pipeline_get_config (self));
      break;

    case PROP_MESSAGE:
      g_value_take_string (value, ide_pipeline_get_message (self));
      break;

    case PROP_PHASE:
      g_value_set_flags (value, ide_pipeline_get_phase (self));
      break;

    case PROP_PTY:
      g_value_set_object (value, ide_pipeline_get_pty (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_pipeline_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  IdePipeline *self = IDE_PIPELINE (object);

  switch (prop_id)
    {
    case PROP_CONFIG:
      self->config = g_value_dup_object (value);
      break;

    case PROP_DEVICE:
      self->device = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_pipeline_class_init (IdePipelineClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  object_class->finalize = ide_pipeline_finalize;
  object_class->get_property = ide_pipeline_get_property;
  object_class->set_property = ide_pipeline_set_property;

  i_object_class->destroy = ide_pipeline_destroy;
  i_object_class->parent_set = ide_pipeline_parent_set;

  /**
   * IdePipeline:busy:
   *
   * Gets the "busy" property. If %TRUE, the pipeline is busy executing.
   */
  properties [PROP_BUSY] =
    g_param_spec_boolean ("busy",
                          "Busy",
                          "If the pipeline is busy",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * IdePipeline:configuration:
   *
   * The configuration to use for the build pipeline.
   */
  properties [PROP_CONFIG] =
    g_param_spec_object ("config",
                         "Configuration",
                         "Configuration",
                         IDE_TYPE_CONFIG,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  /**
   * IdePipeline:device:
   *
   * The "device" property is the device we are compiling for.
   */
  properties [PROP_DEVICE] =
    g_param_spec_object ("device",
                         "Device",
                         "The device we are building for",
                         IDE_TYPE_DEVICE,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  /**
   * IdePipeline:message:
   *
   * The "message" property is descriptive text about what the the
   * pipeline is doing or it's readiness status.
   */
  properties [PROP_MESSAGE] =
    g_param_spec_string ("message",
                         "Message",
                         "The message for the build phase",
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * IdePipeline:phase:
   *
   * The current build phase during execution of the pipeline.
   */
  properties [PROP_PHASE] =
    g_param_spec_flags ("phase",
                        "Phase",
                        "The phase that is being buildd",
                        IDE_TYPE_PIPELINE_PHASE,
                        IDE_PIPELINE_PHASE_NONE,
                        (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * IdePipeline:pty:
   *
   * The "pty" property is the #VtePty that is used by build stages that
   * build subprocesses with a pseudo terminal.
   */
  properties [PROP_PTY] =
    g_param_spec_object ("pty",
                         "PTY",
                         "The PTY used by the pipeline",
                         VTE_TYPE_PTY,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * IdePipeline::diagnostic:
   * @self: An #IdePipeline
   * @diagnostic: The newly created diagnostic
   *
   * This signal is emitted when a plugin has detected a diagnostic while
   * building the pipeline.
   */
  signals [DIAGNOSTIC] =
    g_signal_new_class_handler ("diagnostic",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                NULL, NULL, NULL,
                                ide_marshal_VOID__OBJECT,
                                G_TYPE_NONE, 1, IDE_TYPE_DIAGNOSTIC);
  g_signal_set_va_marshaller (signals [DIAGNOSTIC],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__OBJECTv);

  /**
   * IdePipeline::started:
   * @self: An #IdePipeline
   * @phase: the #IdePipelinePhase for which we are advancing
   *
   * This signal is emitted when the pipeline has started executing in
   * response to ide_pipeline_build_async() being called.
   */
  signals [STARTED] =
    g_signal_new_class_handler ("started",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (ide_pipeline_real_started),
                                NULL, NULL,
                                ide_marshal_VOID__FLAGS,
                                G_TYPE_NONE, 1, IDE_TYPE_PIPELINE_PHASE);
  g_signal_set_va_marshaller (signals [STARTED],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__FLAGSv);

  /**
   * IdePipeline::finished:
   * @self: An #IdePipeline
   * @failed: If the build was a failure
   *
   * This signal is emitted when the build process has finished executing.
   * If the build failed to complete all requested stages, then @failed will
   * be set to %TRUE, otherwise %FALSE.
   */
  signals [FINISHED] =
    g_signal_new_class_handler ("finished",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (ide_pipeline_real_finished),
                                NULL, NULL,
                                ide_marshal_VOID__BOOLEAN,
                                G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
  g_signal_set_va_marshaller (signals [FINISHED],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__BOOLEANv);

  /**
   * IdePipeline::loaded:
   *
   * The "loaded" signal is emitted after the pipeline has finished
   * loading addins.
   */
  signals [LOADED] =
    g_signal_new_class_handler ("loaded",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                NULL, NULL, NULL, NULL,
                                G_TYPE_NONE, 0);

  /**
   * IdePipeline::launcher-created:
   * @self: an #IdePipeline
   * @launcher: an #IdeSubprocessLauncher
   *
   * The "launcher-created" signal is emitted when a new
   * #IdeSubprocessLauncher is created by the pipeline. This may be useful
   * to plugins that wan to modify the launcher in a consistent way for all
   * pipeline consumers.
   */
  signals [LAUNCHER_CREATED] =
    g_signal_new_class_handler ("launcher-created",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                NULL, NULL, NULL,
                                ide_marshal_VOID__OBJECT,
                                G_TYPE_NONE, 1, IDE_TYPE_SUBPROCESS_LAUNCHER);
  g_signal_set_va_marshaller (signals [LAUNCHER_CREATED],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__OBJECTv);
}

static void
ide_pipeline_init (IdePipeline *self)
{
  self->cancellable = g_cancellable_new ();

  self->position = -1;
  self->pty_producer = -1;

  self->best_strategy_priority = G_MAXINT;
  self->best_strategy = ide_local_deploy_strategy_new ();

  self->pipeline = g_array_new (FALSE, FALSE, sizeof (PipelineEntry));
  g_array_set_clear_func (self->pipeline, clear_pipeline_entry);

  self->errfmts = g_array_new (FALSE, FALSE, sizeof (ErrorFormat));
  g_array_set_clear_func (self->errfmts, clear_error_format);

  self->chained_bindings = g_ptr_array_new_with_free_func ((GDestroyNotify)chained_binding_clear);

  self->log = ide_build_log_new ();
}

static void
ide_pipeline_stage_build_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  IdePipelineStage *stage = (IdePipelineStage *)object;
  IdePipeline *self;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_PIPELINE_STAGE (stage));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  g_assert (IDE_IS_PIPELINE (self));

  if (!_ide_pipeline_stage_build_with_query_finish (stage, result, &error))
    {
      g_debug ("stage of type %s failed: %s",
               G_OBJECT_TYPE_NAME (stage),
               error->message);
      self->failed = TRUE;
      ide_task_return_error (task, g_steal_pointer (&error));
    }

  ide_pipeline_stage_set_completed (stage, !self->failed);

  g_clear_pointer (&self->chained_bindings, g_ptr_array_unref);
  self->chained_bindings = g_ptr_array_new_with_free_func (g_object_unref);

  if (self->failed == FALSE)
    ide_pipeline_tick_build (self, task);

  IDE_EXIT;
}

static void
ide_pipeline_try_chain (IdePipeline      *self,
                        IdePipelineStage *stage,
                        guint             position)
{
  g_assert (IDE_IS_PIPELINE (self));
  g_assert (IDE_IS_PIPELINE_STAGE (stage));

  for (; position < self->pipeline->len; position++)
    {
      const PipelineEntry *entry = &g_array_index (self->pipeline, PipelineEntry, position);
      gboolean chained;
      GBinding *chained_binding;

      /*
       * Ignore all future stages if they were not requested by the current
       * pipeline execution.
       */
      if (((entry->phase & IDE_PIPELINE_PHASE_MASK) & self->requested_mask) == 0)
        return;

      /* Skip past the stage if it is disabled. */
      if (ide_pipeline_stage_get_disabled (entry->stage))
        continue;

      chained = ide_pipeline_stage_chain (stage, entry->stage);

      IDE_TRACE_MSG ("Checking if %s chains to stage[%d] (%s) = %s",
                     G_OBJECT_TYPE_NAME (stage),
                     position,
                     G_OBJECT_TYPE_NAME (entry->stage),
                     chained ? "yes" : "no");

      if (!chained)
        return;

      chained_binding = g_object_bind_property (stage, "completed", entry->stage, "completed", 0);
      g_ptr_array_add (self->chained_bindings, g_object_ref (chained_binding));

      self->position = position;
    }
}

static void
complete_queued_before_phase (IdePipeline      *self,
                              IdePipelinePhase  phase)
{
  g_assert (IDE_IS_PIPELINE (self));

  phase = phase & IDE_PIPELINE_PHASE_MASK;

  for (GList *iter = self->task_queue.head; iter; iter = iter->next)
    {
      IdeTask *task;
      TaskData *task_data;

    again:
      task = iter->data;
      task_data = ide_task_get_task_data (task);

      g_assert (IDE_IS_TASK (task));
      g_assert (task_data->task == task);

      /*
       * If this task has a phase that is less-than the phase given
       * to us, we can complete the task immediately.
       */
      if (task_data->phase < phase)
        {
          GList *to_remove = iter;

          iter = iter->next;
          g_queue_delete_link (&self->task_queue, to_remove);
          ide_task_return_boolean (task, TRUE);
          g_object_unref (task);

          if (iter == NULL)
            break;

          goto again;
        }
    }
}

static void
ide_pipeline_tick_build (IdePipeline *self,
                         IdeTask     *task)
{
  GCancellable *cancellable;
  TaskData *td;

  IDE_ENTRY;

  g_assert (IDE_IS_PIPELINE (self));
  g_assert (IDE_IS_TASK (task));

  self->current_stage = NULL;

  td = ide_task_get_task_data (task);
  cancellable = ide_task_get_cancellable (task);

  g_assert (td != NULL);
  g_assert (td->type == TASK_BUILD || td->type == TASK_REBUILD);
  g_assert (td->task == task);
  g_assert (td->phase != IDE_PIPELINE_PHASE_NONE);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  /* Clear any message from the previous stage */
  _ide_pipeline_set_message (self, NULL);

  /* Clear cached directory enter/leave tracking */
  g_clear_pointer (&self->errfmt_current_dir, g_free);
  g_clear_pointer (&self->errfmt_top_dir, g_free);

  /* Short circuit now if the task was cancelled */
  if (ide_task_return_error_if_cancelled (task))
    IDE_EXIT;

  /* If we can skip walking the pipeline, go ahead and do so now. */
  if (!ide_pipeline_request_phase (self, td->phase))
    {
      ide_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  /*
   * Walk forward to the next stage requiring execution and asynchronously
   * build it. The stage may also need to perform an async ::query signal
   * delaying pipeline execution. _ide_pipeline_stage_build_with_query_async()
   * will handle all of that for us, in cause they call ide_pipeline_stage_pause()
   * during the ::query callback.
   */
  for (self->position++; (guint)self->position < self->pipeline->len; self->position++)
    {
      const PipelineEntry *entry = &g_array_index (self->pipeline, PipelineEntry, self->position);

      g_assert (entry->stage != NULL);
      g_assert (IDE_IS_PIPELINE_STAGE (entry->stage));

      /* Complete any tasks that are waiting for this to complete */
      complete_queued_before_phase (self, entry->phase);

      /* Ignore the stage if it is disabled */
      if (ide_pipeline_stage_get_disabled (entry->stage))
        continue;

      if ((entry->phase & IDE_PIPELINE_PHASE_MASK) & self->requested_mask)
        {
          GPtrArray *targets = NULL;

          self->current_stage = entry->stage;

          if (td->type == TASK_BUILD)
            targets = td->build.targets;
          else if (td->type == TASK_REBUILD)
            targets = td->rebuild.targets;

          /*
           * We might be able to chain upcoming stages to this stage and avoid
           * duplicate work. This will also advance self->position based on
           * how many stages were chained.
           */
          ide_pipeline_try_chain (self, entry->stage, self->position + 1);

          _ide_pipeline_stage_build_with_query_async (entry->stage,
                                                      self,
                                                      targets,
                                                      cancellable,
                                                      ide_pipeline_stage_build_cb,
                                                      g_object_ref (task));

          g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MESSAGE]);
          g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PHASE]);

          IDE_EXIT;
        }
    }

  ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
ide_pipeline_task_notify_completed (IdePipeline *self,
                                    GParamSpec  *pspec,
                                    IdeTask     *task)
{
  IDE_ENTRY;

  g_assert (IDE_IS_PIPELINE (self));
  g_assert (IDE_IS_TASK (task));

  IDE_TRACE_MSG ("Clearing busy bit for pipeline");

  self->current_stage = NULL;
  self->busy = FALSE;
  self->requested_mask = 0;
  self->in_clean = FALSE;

  g_clear_pointer (&self->message, g_free);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MESSAGE]);

  /*
   * XXX: How do we ensure transients are built with the part of the
   *      pipeline we care about? We might just need to ensure that :busy is
   *      FALSE before adding transients.
   */
  ide_pipeline_release_transients (self);

  g_signal_emit (self, signals [FINISHED], 0, self->failed);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BUSY]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PHASE]);

  /*
   * We might have a delayed addin unloading that needs to occur after the
   * build operation completes. If the configuration is no longer valid,
   * go ahead and unload the pipeline.
   */
  if (!ide_config_get_ready (self->config))
    ide_pipeline_unload (self);
  else
    ide_pipeline_queue_flush (self);

  IDE_EXIT;
}

/**
 * ide_pipeline_build_targets_async:
 * @self: A @IdePipeline
 * @phase: the requested build phase
 * @targets: (nullable) (element-type IdeBuildTarget): an optional array of
 *   #IdeBuildTarget for the pipeline to build.
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: a callback to build upon completion
 * @user_data: data for @callback
 *
 * Asynchronously starts the build pipeline.
 *
 * The @phase parameter should contain the #IdePipelinePhase that is
 * necessary to complete. If you simply want to trigger a generic
 * build, you probably want %IDE_PIPELINE_PHASE_BUILD. If you only
 * need to configure the project (and necessarily the dependencies
 * up to that phase) you might want %IDE_PIPELINE_PHASE_CONFIGURE.
 *
 * You may not specify %IDE_PIPELINE_PHASE_AFTER or
 * %IDE_PIPELINE_PHASE_BEFORE flags as those must always be processed
 * with the underlying phase they are attached to.
 *
 * Upon completion, @callback will be build and should call
 * ide_pipeline_build_finish() to get the status of the
 * operation.
 */
void
ide_pipeline_build_targets_async (IdePipeline         *self,
                                  IdePipelinePhase     phase,
                                  GPtrArray           *targets,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  TaskData *task_data;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_PIPELINE (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  cancellable = ide_cancellable_chain (cancellable, self->cancellable);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_pipeline_build_targets_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  if (!ide_pipeline_check_ready (self, task))
    return;

  /*
   * If the requested phase has already been met (by a previous build
   * or by an active build who has already surpassed this build phase,
   * we can return a result immediately.
   *
   * Only short circuit if we're running a build, otherwise we need to
   * touch each entry and ::query() to see if it needs execution.
   */

  if (self->busy && !self->in_clean)
    {
      if (self->position >= self->pipeline->len)
        {
          goto short_circuit;
        }
      else if (self->position >= 0)
        {
          const PipelineEntry *entry = &g_array_index (self->pipeline, PipelineEntry, self->position);

          /* This phase is past the requested phase, we can complete the
           * task immediately.
           */
          if (entry->phase > phase)
            goto short_circuit;
        }
    }

  task_data = task_data_new (task, TASK_BUILD);
  task_data->phase = 1 << g_bit_nth_msf (phase, -1);
  task_data->build.targets = _g_ptr_array_copy_objects (targets);
  ide_task_set_task_data (task, task_data, task_data_free);

  g_queue_push_tail (&self->task_queue, g_steal_pointer (&task));

  ide_pipeline_queue_flush (self);

  IDE_EXIT;

short_circuit:
  ide_task_return_boolean (task, TRUE);
  IDE_EXIT;
}

/**
 * ide_pipeline_build_targets_finish:
 * @self: An #IdePipeline
 * @result: a #GAsyncResult provided to callback
 * @error: A location for a #GError, or %NULL
 *
 * This function completes the asynchronous request to build
 * up to a particular phase and targets of the build pipeline.
 *
 * Returns: %TRUE if the build stages were built successfully
 *   up to the requested build phase provided to
 *   ide_pipeline_build_targets_async().
 */
gboolean
ide_pipeline_build_targets_finish (IdePipeline   *self,
                                   GAsyncResult  *result,
                                   GError       **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_PIPELINE (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

/**
 * ide_pipeline_build_async:
 * @self: A @IdePipeline
 * @phase: the requested build phase
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: a callback to build upon completion
 * @user_data: data for @callback
 *
 * Asynchronously starts the build pipeline.
 *
 * The @phase parameter should contain the #IdePipelinePhase that is
 * necessary to complete. If you simply want to trigger a generic
 * build, you probably want %IDE_PIPELINE_PHASE_BUILD. If you only
 * need to configure the project (and necessarily the dependencies
 * up to that phase) you might want %IDE_PIPELINE_PHASE_CONFIGURE.
 *
 * You may not specify %IDE_PIPELINE_PHASE_AFTER or
 * %IDE_PIPELINE_PHASE_BEFORE flags as those must always be processed
 * with the underlying phase they are attached to.
 *
 * Upon completion, @callback will be built and should call
 * ide_pipeline_build_finish() to get the status of the
 * operation.
 */
void
ide_pipeline_build_async (IdePipeline         *self,
                          IdePipelinePhase     phase,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          gpointer             user_data)
{
  ide_pipeline_build_targets_async (self, phase, NULL, cancellable, callback, user_data);
}

/**
 * ide_pipeline_build_finish:
 * @self: An #IdePipeline
 * @result: a #GAsyncResult provided to callback
 * @error: A location for a #GError, or %NULL
 *
 * This function completes the asynchronous request to build
 * up to a particular phase of the build pipeline.
 *
 * Returns: %TRUE if the build stages were built successfully
 *   up to the requested build phase provided to
 *   ide_pipeline_build_async().
 */
gboolean
ide_pipeline_build_finish (IdePipeline   *self,
                           GAsyncResult  *result,
                           GError       **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_PIPELINE (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static gboolean
ide_pipeline_do_flush (gpointer data)
{
  IdePipeline *self = data;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GFile) builddir = NULL;
  g_autoptr(GError) error = NULL;
  TaskData *task_data;

  IDE_ENTRY;

  g_assert (IDE_IS_PIPELINE (self));

  /*
   * If the busy bit is set, there is nothing to do right now.
   */
  if (self->busy)
    {
      IDE_TRACE_MSG ("pipeline already busy, deferring flush");
      IDE_RETURN (G_SOURCE_REMOVE);
    }

  /* Ensure our builddir is created, or else we will fail all pending tasks. */
  builddir = g_file_new_for_path (self->builddir);
  if (!g_file_make_directory_with_parents (builddir, NULL, &error) &&
      !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
    {
      IdeTask *failed_task;

      while (NULL != (failed_task = g_queue_pop_head (&self->task_queue)))
        {
          ide_task_return_error (failed_task, g_error_copy (error));
          g_object_unref (failed_task);
        }

      IDE_RETURN (G_SOURCE_REMOVE);
    }

  /*
   * Pop the next task off the queue from the head (we push to the
   * tail and we want FIFO semantics).
   */
  task = g_queue_pop_head (&self->task_queue);

  if (task == NULL)
    {
      IDE_TRACE_MSG ("No tasks to process");
      IDE_RETURN (G_SOURCE_REMOVE);
    }

  g_assert (IDE_IS_TASK (task));
  g_assert (self->busy == FALSE);

  /*
   * Now prepare the task so that when it completes we can make
   * forward progress again.
   */
  g_signal_connect_object (task,
                           "notify::completed",
                           G_CALLBACK (ide_pipeline_task_notify_completed),
                           self,
                           G_CONNECT_SWAPPED);

  /* We need access to the task data to determine how to process the task. */
  task_data = ide_task_get_task_data (task);

  g_assert (task_data != NULL);
  g_assert (task_data->type > 0);
  g_assert (task_data->type <= TASK_REBUILD);
  g_assert (IDE_IS_TASK (task_data->task));

  /*
   * If this build request could cause us to spin while we are continually
   * failing to reach the CONFIGURE stage, protect ourselves as early as we
   * can. We'll defer to a rebuild request to cause the full thing to build.
   */
  if (self->failed &&
      task_data->type == TASK_BUILD &&
      task_data->phase <= IDE_PIPELINE_PHASE_CONFIGURE)
    {
      ide_task_return_new_error (task,
                                 IDE_BUILD_ERROR,
                                 IDE_BUILD_ERROR_NEEDS_REBUILD,
                                 "The build pipeline is in a failed state and requires a rebuild");
      IDE_RETURN (G_SOURCE_REMOVE);
    }

  /*
   * Now mark the pipeline as busy to protect ourself from anything recursively
   * calling into the pipeline.
   */
  self->busy = TRUE;
  self->failed = FALSE;
  self->position = -1;
  self->in_clean = (task_data->type == TASK_CLEAN);

  /* Clear any lingering message */
  g_clear_pointer (&self->message, g_free);

  /*
   * The following logs some helpful information about the build to our
   * debug log. This is useful to allow users to debug some problems
   * with our assistance (using gnome-builder -vvv).
   */
  {
    g_autoptr(GString) str = g_string_new (NULL);
    GFlagsClass *klass;
    IdePipelinePhase phase = self->requested_mask;

    klass = g_type_class_peek (IDE_TYPE_PIPELINE_PHASE);

    for (guint i = 0; i < klass->n_values; i++)
      {
        const GFlagsValue *value = &klass->values[i];

        if (phase & value->value)
          {
            if (str->len > 0)
              g_string_append (str, ", ");
            g_string_append (str, value->value_nick);
          }
      }

    g_debug ("Executing pipeline %s stages %s with %u pipeline entries",
             task_type_names[task_data->type],
             str->str,
             self->pipeline->len);

    for (guint i = 0; i < self->pipeline->len; i++)
      {
        const PipelineEntry *entry = &g_array_index (self->pipeline, PipelineEntry, i);

        g_debug (" pipeline[%02u]: %12s: %s [%s]",
                 i,
                 build_phase_nick (entry->phase),
                 G_OBJECT_TYPE_NAME (entry->stage),
                 ide_pipeline_stage_get_completed (entry->stage) ? "completed" : "pending");
      }
  }

  /* Notify any observers that a build (of some sort) is about to start. */
  g_signal_emit (self, signals [STARTED], 0, task_data->phase);

  switch (task_data->type)
    {
    case TASK_BUILD:
      ide_pipeline_tick_build (self, task);
      break;

    case TASK_CLEAN:
      ide_pipeline_tick_clean (self, task);
      break;

    case TASK_REBUILD:
      ide_pipeline_tick_rebuild (self, task);
      break;

    default:
      g_assert_not_reached ();
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BUSY]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MESSAGE]);

  IDE_RETURN (G_SOURCE_REMOVE);
}

static void
ide_pipeline_queue_flush (IdePipeline *self)
{
  IDE_ENTRY;

  g_assert (IDE_IS_PIPELINE (self));

  g_idle_add_full (G_PRIORITY_LOW, ide_pipeline_do_flush, g_object_ref (self), g_object_unref);

  IDE_EXIT;
}

/**
 * ide_pipeline_attach:
 * @self: an #IdePipeline
 * @phase: An #IdePipelinePhase
 * @priority: an optional priority for sorting within the phase
 * @stage: An #IdePipelineStage
 *
 * Insert @stage into the pipeline as part of the phase denoted by @phase.
 *
 * If priority is non-zero, it will be used to sort the stage among other
 * stages that are part of the same phase.
 *
 * Returns: A stage_id that may be passed to ide_pipeline_detach().
 */
guint
ide_pipeline_attach (IdePipeline      *self,
                     IdePipelinePhase  phase,
                     gint              priority,
                     IdePipelineStage *stage)
{
  GFlagsClass *klass, *unref_class = NULL;
  guint ret = 0;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_PIPELINE (self), 0);
  g_return_val_if_fail (IDE_IS_PIPELINE_STAGE (stage), 0);
  g_return_val_if_fail ((phase & IDE_PIPELINE_PHASE_MASK) != IDE_PIPELINE_PHASE_NONE, 0);
  g_return_val_if_fail ((phase & IDE_PIPELINE_PHASE_WHENCE_MASK) == 0 ||
                        (phase & IDE_PIPELINE_PHASE_WHENCE_MASK) == IDE_PIPELINE_PHASE_BEFORE ||
                        (phase & IDE_PIPELINE_PHASE_WHENCE_MASK) == IDE_PIPELINE_PHASE_AFTER, 0);

  if (!(klass = g_type_class_peek (IDE_TYPE_PIPELINE_PHASE)))
    klass = unref_class = g_type_class_ref (IDE_TYPE_PIPELINE_PHASE);

  for (guint i = 0; i < klass->n_values; i++)
    {
      const GFlagsValue *value = &klass->values[i];

      if ((phase & IDE_PIPELINE_PHASE_MASK) == value->value)
        {
          PipelineEntry entry = { 0 };

          _ide_pipeline_stage_set_phase (stage, phase);

          IDE_TRACE_MSG ("Adding stage to pipeline with phase %s and priority %d",
                         value->value_nick, priority);

          entry.id = ++self->seqnum;
          entry.phase = phase;
          entry.priority = priority;
          entry.stage = g_object_ref (stage);

          g_array_append_val (self->pipeline, entry);
          g_array_sort (self->pipeline, pipeline_entry_compare);

          ret = entry.id;

          ide_pipeline_stage_set_log_observer (stage,
                                            ide_pipeline_log_observer,
                                            self,
                                            NULL);

          /*
           * We need to emit items-changed for the newly added entry, but we relied
           * on insertion sort above to get our final position. So now we need to
           * scan the pipeline for where we ended up, and then emit items-changed for
           * the new stage.
           */
          for (guint j = 0; j < self->pipeline->len; j++)
            {
              const PipelineEntry *ele = &g_array_index (self->pipeline, PipelineEntry, j);

              if (ele->id == entry.id)
                {
                  g_list_model_items_changed (G_LIST_MODEL (self), j, 0, 1);
                  break;
                }
            }

          ide_object_append (IDE_OBJECT (self), IDE_OBJECT (stage));

          IDE_GOTO (cleanup);
        }
    }

  g_warning ("No such pipeline phase %02x", phase);

cleanup:
  if (unref_class != NULL)
    g_type_class_unref (unref_class);

  IDE_RETURN (ret);
}

/**
 * ide_pipeline_attach_launcher:
 * @self: an #IdePipeline
 * @phase: An #IdePipelinePhase
 * @priority: an optional priority for sorting within the phase
 * @launcher: An #IdeSubprocessLauncher
 *
 * This creates a new stage that will spawn a process using @launcher and log
 * the output of stdin/stdout.
 *
 * It is a programmer error to modify @launcher after passing it to this
 * function.
 *
 * Returns: A stage_id that may be passed to ide_pipeline_remove().
 */
guint
ide_pipeline_attach_launcher (IdePipeline           *self,
                              IdePipelinePhase       phase,
                              gint                   priority,
                              IdeSubprocessLauncher *launcher)
{
  g_autoptr(IdePipelineStage) stage = NULL;
  IdeContext *context;

  g_return_val_if_fail (IDE_IS_PIPELINE (self), 0);
  g_return_val_if_fail ((phase & IDE_PIPELINE_PHASE_MASK) != IDE_PIPELINE_PHASE_NONE, 0);
  g_return_val_if_fail ((phase & IDE_PIPELINE_PHASE_WHENCE_MASK) == 0 ||
                        (phase & IDE_PIPELINE_PHASE_WHENCE_MASK) == IDE_PIPELINE_PHASE_BEFORE ||
                        (phase & IDE_PIPELINE_PHASE_WHENCE_MASK) == IDE_PIPELINE_PHASE_AFTER, 0);

  context = ide_object_get_context (IDE_OBJECT (self));
  stage = ide_pipeline_stage_launcher_new (context, launcher);

  return ide_pipeline_attach (self, phase, priority, stage);
}

/**
 * ide_pipeline_attach_command:
 * @self: an #IdePipeline
 * @phase: An #IdePipelinePhase
 * @priority: an optional priority for sorting within the phase
 * @run_command: An #IdeRunCommand
 *
 * This creates a new stage that will spawn a process using @run_command and
 * log the output of stdin/stdout.
 *
 * It is a programmer error to modify @run_command after passing it to this
 * function.
 *
 * Returns: A stage_id that may be passed to ide_pipeline_remove().
 */
guint
ide_pipeline_attach_command (IdePipeline      *self,
                             IdePipelinePhase  phase,
                             gint              priority,
                             IdeRunCommand    *run_command)
{
  g_autoptr(IdePipelineStage) stage = NULL;

  g_return_val_if_fail (IDE_IS_PIPELINE (self), 0);
  g_return_val_if_fail ((phase & IDE_PIPELINE_PHASE_MASK) != IDE_PIPELINE_PHASE_NONE, 0);
  g_return_val_if_fail ((phase & IDE_PIPELINE_PHASE_WHENCE_MASK) == 0 ||
                        (phase & IDE_PIPELINE_PHASE_WHENCE_MASK) == IDE_PIPELINE_PHASE_BEFORE ||
                        (phase & IDE_PIPELINE_PHASE_WHENCE_MASK) == IDE_PIPELINE_PHASE_AFTER, 0);

  stage = ide_pipeline_stage_command_new (run_command, NULL);

  return ide_pipeline_attach (self, phase, priority, stage);
}

/**
 * ide_pipeline_request_phase:
 * @self: An #IdePipeline
 * @phase: An #IdePipelinePhase
 *
 * Requests that the next execution of the pipeline will build up to @phase
 * including all stages that were previously invalidated.
 *
 * Returns: %TRUE if a stage is known to require execution.
 */
gboolean
ide_pipeline_request_phase (IdePipeline      *self,
                            IdePipelinePhase  phase)
{
  GFlagsClass *klass, *unref_class = NULL;
  gboolean ret = FALSE;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_PIPELINE (self), FALSE);
  g_return_val_if_fail ((phase & IDE_PIPELINE_PHASE_MASK) != IDE_PIPELINE_PHASE_NONE, FALSE);

  /*
   * You can only request basic phases. That does not include modifiers
   * like BEFORE, AFTER, FAILED, FINISHED.
   */
  phase &= IDE_PIPELINE_PHASE_MASK;

  if (!(klass = g_type_class_peek (IDE_TYPE_PIPELINE_PHASE)))
    klass = unref_class = g_type_class_ref (IDE_TYPE_PIPELINE_PHASE);

  for (guint i = 0; i < klass->n_values; i++)
    {
      const GFlagsValue *value = &klass->values[i];

      if ((guint)phase == value->value)
        {
          IDE_TRACE_MSG ("requesting pipeline phase %s", value->value_nick);
          /*
           * Each flag is a power of two, so we can simply subtract one
           * to get a mask of all the previous phases.
           */
          self->requested_mask |= phase | (phase - 1);
          IDE_GOTO (cleanup);
        }
    }

  g_warning ("No such phase %02x", (guint)phase);

cleanup:

  /*
   * If we have a stage in one of the requested phases, then we can let the
   * caller know that they need to run build_async() to be up to date. This
   * is useful for situations where you might want to avoid calling
   * build_async() altogether. Additionally, we want to know if there are
   * any connections to the "query" which could cause the completed state
   * to be invalidated.
   */
  for (guint i = 0; i < self->pipeline->len; i++)
    {
      const PipelineEntry *entry = &g_array_index (self->pipeline, PipelineEntry, i);

      if (!(entry->phase & self->requested_mask))
        continue;

      if (!ide_pipeline_stage_get_completed (entry->stage) ||
          _ide_pipeline_stage_has_query (entry->stage))
        {
          ret = TRUE;
          break;
        }
    }

  if (unref_class != NULL)
    g_type_class_unref (unref_class);

  IDE_RETURN (ret);
}

/**
 * ide_pipeline_get_builddir:
 * @self: An #IdePipeline
 *
 * Gets the "builddir" to be used for the build process. This is generally
 * the location that build systems will use for out-of-tree builds.
 *
 * Returns: the path of the build directory
 */
const gchar *
ide_pipeline_get_builddir (IdePipeline *self)
{
  g_return_val_if_fail (IDE_IS_PIPELINE (self), NULL);

  return self->builddir;
}

/**
 * ide_pipeline_get_srcdir:
 * @self: An #IdePipeline
 *
 * Gets the "srcdir" of the project. This is equivalent to the
 * IdeVcs:working-directory property as a string.
 *
 * Returns: the path of the source directory
 */
const gchar *
ide_pipeline_get_srcdir (IdePipeline *self)
{
  g_return_val_if_fail (IDE_IS_PIPELINE (self), NULL);

  return self->srcdir;
}

static gchar *
ide_pipeline_build_path_va_list (const gchar *prefix,
                                 const gchar *first_part,
                                 va_list     *args)
{
  g_autoptr(GPtrArray) ar = NULL;

  g_assert (prefix != NULL);
  g_assert (first_part != NULL);

  ar = g_ptr_array_new ();
  g_ptr_array_add (ar, (gchar *)prefix);
  do
    g_ptr_array_add (ar, (gchar *)first_part);
  while (NULL != (first_part = va_arg (*args, const gchar *)));
  g_ptr_array_add (ar, NULL);

  return g_build_filenamev ((gchar **)ar->pdata);
}

/**
 * ide_pipeline_build_srcdir_path:
 *
 * This is a convenience function to create a new path that starts with
 * the source directory of the project.
 *
 * This is functionally equivalent to calling g_build_filename() with the
 * working directory of the source tree.
 *
 * Returns: (transfer full): A newly allocated string.
 */
gchar *
ide_pipeline_build_srcdir_path (IdePipeline *self,
                                const gchar *first_part,
                                ...)
{
  gchar *ret;
  va_list args;

  g_return_val_if_fail (IDE_IS_PIPELINE (self), NULL);
  g_return_val_if_fail (self->srcdir != NULL, NULL);
  g_return_val_if_fail (first_part != NULL, NULL);

  va_start (args, first_part);
  ret = ide_pipeline_build_path_va_list (self->srcdir, first_part, &args);
  va_end (args);

  return ret;
}

/**
 * ide_pipeline_build_builddir_path:
 *
 * This is a convenience function to create a new path that starts with
 * the build directory for this build configuration.
 *
 * This is functionally equivalent to calling g_build_filename() with the
 * result of ide_pipeline_get_builddir() as the first parameter.
 *
 * Returns: (transfer full): A newly allocated string.
 */
gchar *
ide_pipeline_build_builddir_path (IdePipeline *self,
                                  const gchar *first_part,
                                  ...)
{
  gchar *ret;
  va_list args;

  g_return_val_if_fail (IDE_IS_PIPELINE (self), NULL);
  g_return_val_if_fail (self->builddir != NULL, NULL);
  g_return_val_if_fail (first_part != NULL, NULL);

  va_start (args, first_part);
  ret = ide_pipeline_build_path_va_list (self->builddir, first_part, &args);
  va_end (args);

  return ret;
}

/**
 * ide_pipeline_detach:
 * @self: An #IdePipeline
 * @stage_id: An identifier returned from adding a stage
 *
 * This removes the stage matching @stage_id. You are returned a @stage_id when
 * inserting a stage with functions such as ide_pipeline_attach()
 * or ide_pipeline_attach_launcher().
 *
 * Plugins should use this function to remove their stages when the plugin
 * is unloading.
 */
void
ide_pipeline_detach (IdePipeline *self,
                     guint        stage_id)
{
  g_return_if_fail (IDE_IS_PIPELINE (self));
  g_return_if_fail (self->pipeline != NULL);
  g_return_if_fail (stage_id != 0);

  for (guint i = 0; i < self->pipeline->len; i++)
    {
      const PipelineEntry *entry = &g_array_index (self->pipeline, PipelineEntry, i);

      if (entry->id == stage_id)
        {
          g_autoptr(IdePipelineStage) stage = g_object_ref (entry->stage);

          /* Remove index and notify listmodel observers */
          g_array_remove_index (self->pipeline, i);
          g_list_model_items_changed (G_LIST_MODEL (self), i, 1, 0);

          /* Wait until after notifying listmodel observers */
          ide_object_destroy (IDE_OBJECT (stage));

          break;
        }
    }
}

/**
 * ide_pipeline_invalidate_phase:
 * @self: An #IdePipeline
 * @phases: The phases to invalidate
 *
 * Invalidates the phases matching @phases flags.
 *
 * If the requested phases include the phases invalidated here, the next
 * execution of the pipeline will build these phases.
 *
 * This should be used by plugins to ensure a particular phase is re-buildd
 * upon discovering its state is no longer valid. Such an example might be
 * invalidating the %IDE_PIPELINE_PHASE_AUTOGEN phase when the an autotools
 * projects autogen.sh file has been changed.
 */
void
ide_pipeline_invalidate_phase (IdePipeline      *self,
                               IdePipelinePhase  phases)
{
  g_return_if_fail (IDE_IS_PIPELINE (self));

  for (guint i = 0; i < self->pipeline->len; i++)
    {
      const PipelineEntry *entry = &g_array_index (self->pipeline, PipelineEntry, i);

      if ((entry->phase & IDE_PIPELINE_PHASE_MASK) & phases)
        ide_pipeline_stage_set_completed (entry->stage, FALSE);
    }
}

/**
 * ide_pipeline_get_stage_by_id:
 * @self: An #IdePipeline
 * @stage_id: the identifier of the stage
 *
 * Gets the stage matching the identifier @stage_id as returned from
 * ide_pipeline_attach().
 *
 * Returns: (transfer none) (nullable): An #IdePipelineStage or %NULL if the
 *   stage could not be found.
 */
IdePipelineStage *
ide_pipeline_get_stage_by_id (IdePipeline *self,
                              guint        stage_id)
{
  g_return_val_if_fail (IDE_IS_PIPELINE (self), NULL);

  for (guint i = 0; i < self->pipeline->len; i++)
    {
      const PipelineEntry *entry = &g_array_index (self->pipeline, PipelineEntry, i);

      if (entry->id == stage_id)
        return entry->stage;
    }

  return NULL;
}

/**
 * ide_pipeline_get_runtime:
 * @self: An #IdePipeline
 *
 * A convenience function to get the runtime for a build pipeline.
 *
 * Returns: (transfer none) (nullable): An #IdeRuntime or %NULL
 */
IdeRuntime *
ide_pipeline_get_runtime (IdePipeline *self)
{
  g_return_val_if_fail (IDE_IS_PIPELINE (self), NULL);

  return self->runtime;
}

/**
 * ide_pipeline_get_toolchain:
 * @self: An #IdePipeline
 *
 * A convenience function to get the toolchain for a build pipeline.
 *
 * Returns: (transfer none): An #IdeToolchain
 */
IdeToolchain *
ide_pipeline_get_toolchain (IdePipeline *self)
{
  g_return_val_if_fail (IDE_IS_PIPELINE (self), NULL);

  return self->toolchain;
}

/**
 * ide_pipeline_create_launcher:
 * @self: An #IdePipeline
 *
 * This is a convenience function to create a new #IdeSubprocessLauncher
 * using the configuration and runtime associated with the pipeline.
 *
 * Returns: (transfer full): An #IdeSubprocessLauncher.
 */
IdeSubprocessLauncher *
ide_pipeline_create_launcher (IdePipeline  *self,
                              GError      **error)
{
  g_autoptr(IdeSubprocessLauncher) ret = NULL;
  g_autoptr(IdeRunContext) run_context = NULL;
  g_auto(GStrv) environ = NULL;
  IdeRuntime *runtime;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_PIPELINE (self), NULL);

  if (!(runtime = ide_config_get_runtime (self->config)))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "The runtime %s is missing",
                   ide_config_get_runtime_id (self->config));
      return NULL;
    }

  environ = ide_environment_get_environ (ide_config_get_environment (self->config));

  run_context = ide_run_context_new ();
  ide_runtime_prepare_to_build (runtime, self, run_context);
  ide_run_context_set_cwd (run_context, ide_pipeline_get_builddir (self));
  ide_run_context_add_environ (run_context, (const char * const *)environ);

  /* Always ignore V=1 from configurations */
  if (ide_run_context_getenv (run_context, "V") != NULL)
    ide_run_context_setenv (run_context, "V", "0");

  ret = ide_run_context_end (run_context, error);

  if (ret != NULL)
    {
      ide_subprocess_launcher_set_flags (ret,
                                         (G_SUBPROCESS_FLAGS_STDERR_PIPE |
                                          G_SUBPROCESS_FLAGS_STDOUT_PIPE));
      ide_config_apply_path (self->config, ret);

      g_signal_emit (self, signals [LAUNCHER_CREATED], 0, ret);
    }

  return g_steal_pointer (&ret);
}

void
_ide_pipeline_attach_pty_to_run_context (IdePipeline   *self,
                                         IdeRunContext *run_context)
{
  static const int fileno_mapping[] = { STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO };

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_PIPELINE (self));
  g_return_if_fail (IDE_IS_RUN_CONTEXT (run_context));

  if (self->pty_producer == -1)
    {
      IdePtyFd consumer_fd = ide_pty_intercept_get_fd (&self->intercept);
      self->pty_producer = ide_pty_intercept_create_producer (consumer_fd, TRUE);
    }

  for (guint i = 0; i < G_N_ELEMENTS (fileno_mapping); i++)
    {
      int fd = self->pty_producer > -1 ? dup (self->pty_producer) : -1;

      if (fd == -1)
        {
          ide_run_context_push_error (run_context,
                                      g_error_new_literal (G_IO_ERROR,
                                                           G_IO_ERROR_FAILED,
                                                           _("Pseudo terminal creation failed. Terminal features will be limited.")));
          IDE_EXIT;
        }

      ide_run_context_take_fd (run_context, fd, fileno_mapping[i]);
    }

  ide_run_context_setenv (run_context, "TERM", "xterm-256color");
  ide_run_context_setenv (run_context, "COLORTERM", "truecolor");

  IDE_EXIT;
}

/**
 * ide_pipeline_attach_pty:
 * @self: an #IdePipeline
 * @launcher: an #IdeSubprocessLauncher
 *
 * Attaches a PTY to stdin/stdout/stderr of the #IdeSubprocessLauncher.
 * This is useful if the application can take advantage of a PTY for
 * features like colors and other escape sequences.
 */
void
ide_pipeline_attach_pty (IdePipeline           *self,
                         IdeSubprocessLauncher *launcher)
{
  GSubprocessFlags flags;

  g_return_if_fail (IDE_IS_PIPELINE (self));
  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (launcher));

  if (self->pty_producer == -1)
    {
      IdePtyFd consumer_fd = ide_pty_intercept_get_fd (&self->intercept);
      self->pty_producer = ide_pty_intercept_create_producer (consumer_fd, TRUE);
    }

  if (self->pty_producer == -1)
    {
      ide_object_warning (self, _("Pseudo terminal creation failed. Terminal features will be limited."));
      return;
    }

  /* Turn off built in pipes if set */
  flags = ide_subprocess_launcher_get_flags (launcher);
  flags &= ~(G_SUBPROCESS_FLAGS_STDERR_PIPE |
             G_SUBPROCESS_FLAGS_STDOUT_PIPE |
             G_SUBPROCESS_FLAGS_STDIN_PIPE);
  ide_subprocess_launcher_set_flags (launcher, flags);

  /* Assign producer device */
  ide_subprocess_launcher_take_stdin_fd (launcher, dup (self->pty_producer));
  ide_subprocess_launcher_take_stdout_fd (launcher, dup (self->pty_producer));
  ide_subprocess_launcher_take_stderr_fd (launcher, dup (self->pty_producer));

  /* Ensure a terminal type is set */
  ide_subprocess_launcher_setenv (launcher, "TERM", "xterm-256color", FALSE);
}

/**
 * ide_pipeline_get_pty:
 * @self: a #IdePipeline
 *
 * Gets the #VtePty for the pipeline, if set.
 *
 * This will not be set until the pipeline has been initialized. That is not
 * guaranteed to happen at object creation time.
 *
 * Returns: (transfer none) (nullable): a #VtePty or %NULL
 */
VtePty *
ide_pipeline_get_pty (IdePipeline *self)
{
  g_return_val_if_fail (IDE_IS_PIPELINE (self), NULL);

  return self->pty;
}

guint
ide_pipeline_add_log_observer (IdePipeline         *self,
                               IdeBuildLogObserver  observer,
                               gpointer             observer_data,
                               GDestroyNotify       observer_data_destroy)
{
  g_return_val_if_fail (IDE_IS_PIPELINE (self), 0);
  g_return_val_if_fail (observer != NULL, 0);

  return ide_build_log_add_observer (self->log, observer, observer_data, observer_data_destroy);
}

gboolean
ide_pipeline_remove_log_observer (IdePipeline *self,
                                  guint        observer_id)
{
  g_return_val_if_fail (IDE_IS_PIPELINE (self), FALSE);
  g_return_val_if_fail (observer_id > 0, FALSE);

  return ide_build_log_remove_observer (self->log, observer_id);
}

void
ide_pipeline_emit_diagnostic (IdePipeline   *self,
                              IdeDiagnostic *diagnostic)
{
  g_return_if_fail (IDE_IS_PIPELINE (self));
  g_return_if_fail (diagnostic != NULL);
  g_return_if_fail (IDE_IS_MAIN_THREAD ());

  g_signal_emit (self, signals[DIAGNOSTIC], 0, diagnostic);
}

/**
 * ide_pipeline_add_error_format:
 * @self: an #IdePipeline
 * @regex: A regex to be compiled
 *
 * This can be used to add a regex that will extract errors from
 * standard output. This is similar to the "errorformat" feature
 * of vim to extract warnings from standard output.
 *
 * The regex should used named capture groups to pass information
 * to the extraction process.
 *
 * Supported group names are:
 *
 *  • filename (a string path)
 *  • line (an integer)
 *  • column (an integer)
 *  • level (a string)
 *  • message (a string)
 *
 * For example, to extract warnings from GCC you might do something
 * like the following:
 *
 *   "(?&lt;filename&gt;[a-zA-Z0-9\\-\\.\\/_]+):"
 *   "(?&lt;line&gt;\\d+):"
 *   "(?&lt;column&gt;\\d+): "
 *   "(?&lt;level&gt;[\\w\\s]+): "
 *   "(?&lt;message&gt;.*)"
 *
 * To remove the regex, use the ide_pipeline_remove_error_format()
 * function with the resulting format id returned from this function.
 *
 * The resulting format id will be &gt; 0 if successful.
 *
 * Returns: an error format id that may be passed to
 *   ide_pipeline_remove_error_format().
 */
guint
ide_pipeline_add_error_format (IdePipeline        *self,
                               const gchar        *regex,
                               GRegexCompileFlags  flags)
{
  ErrorFormat errfmt = { 0 };
  g_autoptr(GError) error = NULL;

  g_return_val_if_fail (IDE_IS_PIPELINE (self), 0);

  errfmt.regex = g_regex_new (regex, G_REGEX_OPTIMIZE | flags, 0, &error);

  if (errfmt.regex == NULL)
    {
      g_warning ("%s", error->message);
      return 0;
    }

  errfmt.id = ++self->errfmt_seqnum;

  g_array_append_val (self->errfmts, errfmt);

  return errfmt.id;
}

/**
 * ide_pipeline_remove_error_format:
 * @self: An #IdePipeline
 * @error_format_id: an identifier for the error format.
 *
 * Removes an error format that was registered with
 * ide_pipeline_add_error_format().
 *
 * Returns: %TRUE if the error format was removed.
 */
gboolean
ide_pipeline_remove_error_format (IdePipeline *self,
                                  guint        error_format_id)
{
  g_return_val_if_fail (IDE_IS_PIPELINE (self), FALSE);
  g_return_val_if_fail (error_format_id > 0, FALSE);

  for (guint i = 0; i < self->errfmts->len; i++)
    {
      const ErrorFormat *errfmt = &g_array_index (self->errfmts, ErrorFormat, i);

      if (errfmt->id == error_format_id)
        {
          g_array_remove_index (self->errfmts, i);
          return TRUE;
        }
    }

  return FALSE;
}

gboolean
ide_pipeline_get_busy (IdePipeline *self)
{
  g_return_val_if_fail (IDE_IS_PIPELINE (self), FALSE);

  return self->busy;
}

/**
 * ide_pipeline_get_message:
 * @self: An #IdePipeline
 *
 * Gets the current message for the build pipeline. This can be
 * shown to users in UI elements to signify progress in the
 * build.
 *
 * Returns: (nullable) (transfer full): A string representing the
 *   current stage of the build, or %NULL.
 */
gchar *
ide_pipeline_get_message (IdePipeline *self)
{
  IdePipelinePhase phase;
  const gchar *ret = NULL;

  g_return_val_if_fail (IDE_IS_PIPELINE (self), NULL);

  /* Use any message the Pty has given us while building. */
  if (self->busy && !ide_str_empty0 (self->message))
    return g_strdup (self->message);

  if (self->in_clean)
    return g_strdup (_("Cleaning…"));

  /* Not active, use simple messaging */
  if (self->failed)
    return g_strdup (_("Failed"));
  else if (!self->busy)
    return g_strdup (_("Ready"));

  if (self->current_stage != NULL)
    {
      const gchar *name = ide_pipeline_stage_get_name (self->current_stage);

      if (!ide_str_empty0 (name))
        return g_strdup (name);
    }

  phase = ide_pipeline_get_phase (self);

  switch (phase)
    {
    case IDE_PIPELINE_PHASE_DOWNLOADS:
      ret = _("Downloading…");
      break;

    case IDE_PIPELINE_PHASE_DEPENDENCIES:
      ret = _("Building dependencies…");
      break;

    case IDE_PIPELINE_PHASE_AUTOGEN:
      ret = _("Bootstrapping…");
      break;

    case IDE_PIPELINE_PHASE_CONFIGURE:
      ret = _("Configuring…");
      break;

    case IDE_PIPELINE_PHASE_BUILD:
      ret = _("Building…");
      break;

    case IDE_PIPELINE_PHASE_INSTALL:
      ret = _("Installing…");
      break;

    case IDE_PIPELINE_PHASE_COMMIT:
      ret = _("Committing…");
      break;

    case IDE_PIPELINE_PHASE_EXPORT:
      ret = _("Exporting…");
      break;

    case IDE_PIPELINE_PHASE_FINAL:
      ret = _("Success");
      break;

    case IDE_PIPELINE_PHASE_FINISHED:
      ret = _("Success");
      break;

    case IDE_PIPELINE_PHASE_FAILED:
      ret = _("Failed");
      break;

    case IDE_PIPELINE_PHASE_PREPARE:
      ret = _("Preparing…");
      break;

    case IDE_PIPELINE_PHASE_NONE:
      ret = _("Ready");
      break;

    case IDE_PIPELINE_PHASE_AFTER:
    case IDE_PIPELINE_PHASE_BEFORE:
    default:
      g_assert_not_reached ();
    }

  g_assert (ret != NULL);

  return g_strdup (ret);
}

/**
 * ide_pipeline_foreach_stage:
 * @self: An #IdePipeline
 * @stage_callback: (scope call): A callback for each #IdePipelineStage
 * @user_data: user data for @stage_callback
 *
 * This function will call @stage_callback for every #IdePipelineStage registered
 * in the pipeline.
 */
void
ide_pipeline_foreach_stage (IdePipeline *self,
                            GFunc        stage_callback,
                            gpointer     user_data)
{
  g_return_if_fail (IDE_IS_PIPELINE (self));
  g_return_if_fail (stage_callback != NULL);

  for (guint i = 0; i < self->pipeline->len; i++)
    {
      const PipelineEntry *entry = &g_array_index (self->pipeline, PipelineEntry, i);

      stage_callback (entry->stage, user_data);
    }
}

static void
ide_pipeline_clean_cb (GObject      *object,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  IdePipelineStage *stage = (IdePipelineStage *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdePipeline *self;
  GPtrArray *stages;
  TaskData *td;

  IDE_ENTRY;

  g_assert (IDE_IS_PIPELINE_STAGE (stage));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  td = ide_task_get_task_data (task);

  g_assert (IDE_IS_PIPELINE (self));
  g_assert (td != NULL);
  g_assert (td->type == TASK_CLEAN);
  g_assert (td->task == task);
  g_assert (td->clean.stages != NULL);

  stages = td->clean.stages;

  g_assert (stages != NULL);
  g_assert (stages->len > 0);
  g_assert (g_ptr_array_index (stages, stages->len - 1) == stage);

  if (!ide_pipeline_stage_clean_finish (stage, result, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  g_ptr_array_remove_index (stages, stages->len - 1);

  ide_pipeline_tick_clean (self, task);

  IDE_EXIT;
}

static void
ide_pipeline_tick_clean (IdePipeline *self,
                         IdeTask     *task)
{
  GCancellable *cancellable;
  GPtrArray *stages;
  TaskData *td;

  IDE_ENTRY;

  g_assert (IDE_IS_PIPELINE (self));
  g_assert (IDE_IS_TASK (task));

  td = ide_task_get_task_data (task);
  cancellable = ide_task_get_cancellable (task);

  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (td != NULL);
  g_assert (td->type == TASK_CLEAN);
  g_assert (td->task == task);
  g_assert (td->clean.stages != NULL);

  stages = td->clean.stages;

  if (stages->len != 0)
    {
      IdePipelineStage *stage = g_ptr_array_index (stages, stages->len - 1);

      self->current_stage = stage;

      ide_pipeline_stage_clean_async (stage,
                                   self,
                                   cancellable,
                                   ide_pipeline_clean_cb,
                                   g_object_ref (task));

      IDE_GOTO (notify);
    }

  ide_task_return_boolean (task, TRUE);

notify:
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MESSAGE]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PHASE]);

  IDE_EXIT;
}

void
ide_pipeline_clean_async (IdePipeline         *self,
                          IdePipelinePhase     phase,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GCancellable) local_cancellable = NULL;
  g_autoptr(GPtrArray) stages = NULL;
  IdePipelinePhase min_phase = IDE_PIPELINE_PHASE_FINAL;
  IdePipelinePhase phase_mask;
  GFlagsClass *phase_class;
  TaskData *td;

  IDE_ENTRY;

  g_assert (IDE_IS_PIPELINE (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  drop_caches (self);

  if (cancellable == NULL)
    cancellable = local_cancellable = g_cancellable_new ();

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_priority (task, G_PRIORITY_LOW);
  ide_task_set_source_tag (task, ide_pipeline_clean_async);

  if (!ide_pipeline_check_ready (self, task))
    return;

  ide_cancellable_chain (cancellable, self->cancellable);

  td = task_data_new (task, TASK_CLEAN);
  td->phase = phase;
  ide_task_set_task_data (task, td, task_data_free);

  /*
   * To clean the project, we go through each stage and call it's clean async
   * vfunc pairs if they have been set. Afterwards, we ensure their
   * IdePipelineStage:completed bit is cleared so they will run as part of the
   * next build operation.
   *
   * Also, when performing a clean we walk backwards from the last stage to the
   * present so that they can rely on things being semi-up-to-date from their
   * point of view.
   *
   * To simplify the case of walking through the affected stages, we create a
   * copy of the affected stages up front. We store them in the opposite order
   * they need to be ran so that we only have to pop the last item after
   * completing each stage. Otherwise we would additionally need a position
   * variable.
   *
   * To calculate the phases that are affected, we subtract 1 from the min
   * phase that was given to us. We then twos-compliment that and use it as our
   * mask (so only our min and higher stages are cleaned).
   */

  phase_class = g_type_class_peek (IDE_TYPE_PIPELINE_PHASE);

  for (guint i = 0; i < phase_class->n_values; i++)
    {
      const GFlagsValue *value = &phase_class->values [i];

      if (value->value & phase)
        {
          if (value->value < (guint)min_phase)
            min_phase = value->value;
        }
    }

  phase_mask = ~(min_phase - 1);

  stages = g_ptr_array_new_with_free_func (g_object_unref);

  for (guint i = 0; i < self->pipeline->len; i++)
    {
      const PipelineEntry *entry = &g_array_index (self->pipeline, PipelineEntry, i);

      if ((entry->phase & IDE_PIPELINE_PHASE_MASK) & phase_mask)
        g_ptr_array_add (stages, g_object_ref (entry->stage));
    }

  /*
   * Short-circuit if we don't have any stages to clean.
   */
  if (stages->len == 0)
    {
      ide_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  td->clean.stages = g_steal_pointer (&stages);

  g_queue_push_tail (&self->task_queue, g_steal_pointer (&task));

  ide_pipeline_queue_flush (self);

  IDE_EXIT;
}

gboolean
ide_pipeline_clean_finish (IdePipeline   *self,
                           GAsyncResult  *result,
                           GError       **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_PIPELINE (self));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static gboolean
can_remove_builddir (IdePipeline *self)
{
  g_autofree gchar *_build = NULL;
  g_autoptr(GFile) builddir = NULL;
  g_autoptr(GFile) cache = NULL;
  g_autoptr(IdeContext) context = NULL;

  g_assert (IDE_IS_PIPELINE (self));

  context = ide_object_ref_context (IDE_OBJECT (self));

  /*
   * Only remove builddir if it is in ~/.cache/ or our XDG data dirs
   * equivalent or where the user has their builds artifacts configured.
   * We don't want to accidentally remove data that might be important
   * to the user.
   *
   * However, if the build dir is our special case "_build" inside the
   * project directory, we'll allow that too.
   */

  cache = ide_context_cache_file (context, NULL);
  builddir = g_file_new_for_path (self->builddir);
  if (g_file_has_prefix (builddir, cache))
    return TRUE;

  /* If this is _build in the project tree, we will allow that too
   * since we create those sometimes.
   */
  _build = ide_context_build_filename (context, "_build", NULL);
  if (g_str_equal (_build, self->builddir) &&
      g_file_test (_build, G_FILE_TEST_IS_DIR) &&
      !g_file_test (_build, G_FILE_TEST_IS_SYMLINK))
    return TRUE;

  g_debug ("%s is not in a cache directory, will not delete it", self->builddir);

  return FALSE;
}

static void
ide_pipeline_reaper_cb (GObject      *object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  IdeDirectoryReaper *reaper = (IdeDirectoryReaper *)object;
  IdePipeline *self;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  TaskData *td;

  IDE_ENTRY;

  g_assert (IDE_IS_DIRECTORY_REAPER (reaper));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  td = ide_task_get_task_data (task);

  g_assert (td != NULL);
  g_assert (td->task == task);
  g_assert (td->type == TASK_REBUILD);

  self = ide_task_get_source_object (task);
  g_assert (IDE_IS_PIPELINE (self));

  /* Make sure our reaper completed or else we bail */
  if (!ide_directory_reaper_execute_finish (reaper, result, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  if (td->phase == IDE_PIPELINE_PHASE_NONE)
    {
      ide_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  /* Perform a build using the same task and skipping the build queue. */
  ide_pipeline_tick_build (self, task);

  IDE_EXIT;
}

static inline void
ide_pipeline_log (IdePipeline *self,
                  const char  *message)
{
  g_assert (IDE_IS_PIPELINE (self));

  if (self->log == NULL)
    return;

  ide_build_log_observer (IDE_BUILD_LOG_STDOUT, message, strlen (message), self->log);
}

static void
ide_pipeline_remove_file_cb (IdePipeline        *self,
                             GFile              *file,
                             IdeDirectoryReaper *reaper)
{
  g_autofree char *message = NULL;
  /* translators: %s is replaced with the name of the file being removed */
  message = g_strdup_printf (_("Removing %s"), g_file_peek_path (file));
  ide_pipeline_log (self, message);
}

static void
ide_pipeline_tick_rebuild (IdePipeline *self,
                           IdeTask     *task)
{
  g_autoptr(IdeDirectoryReaper) reaper = NULL;
  GCancellable *cancellable;

  IDE_ENTRY;

  g_assert (IDE_IS_PIPELINE (self));
  g_assert (IDE_IS_TASK (task));

#ifndef G_DISABLE_ASSERT
  {
    TaskData *td = ide_task_get_task_data (task);

    g_assert (td != NULL);
    g_assert (td->type == TASK_REBUILD);
    g_assert (td->task == task);
  }
#endif

  reaper = ide_directory_reaper_new ();
  g_signal_connect_object (reaper,
                           "remove-file",
                           G_CALLBACK (ide_pipeline_remove_file_cb),
                           self,
                           G_CONNECT_SWAPPED);

  /*
   * Check if we can remove the builddir. We don't want to do this if it is the
   * same as the srcdir (in-tree builds).
   */
  if (can_remove_builddir (self))
    {
      g_autoptr(GFile) builddir = g_file_new_for_path (self->builddir);

      ide_directory_reaper_add_directory (reaper, builddir, 0);
    }

  /*
   * Now let the build stages add any files they might want to reap as part of
   * the rebuild process.
   */
  for (guint i = 0; i < self->pipeline->len; i++)
    {
      const PipelineEntry *entry = &g_array_index (self->pipeline, PipelineEntry, i);

      ide_pipeline_stage_emit_reap (entry->stage, reaper);
      ide_pipeline_stage_set_completed (entry->stage, FALSE);
    }

  cancellable = ide_task_get_cancellable (task);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  ide_pipeline_log (self, _("Removing build directories…"));

  /* Now build the reaper to clean up the build files. */
  ide_directory_reaper_execute_async (reaper,
                                      cancellable,
                                      ide_pipeline_reaper_cb,
                                      g_object_ref (task));

  IDE_EXIT;
}

/**
 * ide_pipeline_rebuild_async:
 * @self: A @IdePipeline
 * @phase: the requested build phase
 * @targets: (element-type IdeBuildTarget) (nullable): an array of
 *   #IdeBuildTarget or %NULL
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: a callback to build upon completion
 * @user_data: data for @callback
 *
 * Asynchronously starts the build pipeline after cleaning any
 * existing build artifacts.
 */
void
ide_pipeline_rebuild_async (IdePipeline         *self,
                            IdePipelinePhase     phase,
                            GPtrArray           *targets,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  TaskData *td;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_PIPELINE (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail ((phase & ~IDE_PIPELINE_PHASE_MASK) == 0);

  drop_caches (self);

  cancellable = ide_cancellable_chain (cancellable, self->cancellable);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_priority (task, G_PRIORITY_LOW);
  ide_task_set_source_tag (task, ide_pipeline_rebuild_async);

  if (!ide_pipeline_check_ready (self, task))
    return;

  td = task_data_new (task, TASK_REBUILD);
  td->phase = phase;
  td->rebuild.targets = _g_ptr_array_copy_objects (targets);
  ide_task_set_task_data (task, td, task_data_free);

  g_queue_push_tail (&self->task_queue, g_steal_pointer (&task));

  ide_pipeline_queue_flush (self);

  IDE_EXIT;
}

gboolean
ide_pipeline_rebuild_finish (IdePipeline   *self,
                             GAsyncResult  *result,
                             GError       **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_PIPELINE (self));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

/**
 * ide_pipeline_get_can_export:
 * @self: a #IdePipeline
 *
 * This function is useful to discover if there are any pipeline addins
 * which implement the export phase. UI or GAction implementations may
 * want to use this value to set the enabled state of the action or
 * sensitivity of a button.
 *
 * Returns: %TRUE if there are export pipeline stages.
 */
gboolean
ide_pipeline_get_can_export (IdePipeline *self)
{
  g_return_val_if_fail (IDE_IS_PIPELINE (self), FALSE);

  if (self->broken)
    return FALSE;

  for (guint i = 0; i < self->pipeline->len; i++)
    {
      const PipelineEntry *entry = &g_array_index (self->pipeline, PipelineEntry, i);

      if ((entry->phase & IDE_PIPELINE_PHASE_EXPORT) != 0)
        return TRUE;
    }

  return FALSE;
}

void
_ide_pipeline_set_message (IdePipeline *self,
                           const gchar *message)
{
  g_return_if_fail (IDE_IS_PIPELINE (self));

  if (message != NULL)
    {
      /*
       * Special case to deal with messages coming from systems we
       * know prefix the build tooling information to the message.
       * It's easier to just do this here rather than provide some
       * sort of API for plugins to do this for us.
       */
      if (g_str_has_prefix (message, "flatpak-builder: "))
        message += strlen ("flatpak-builder: ");
      else if (g_str_has_prefix (message, "jhbuild:"))
        message += strlen ("jhbuild:");
    }

  if (g_set_str (&self->message, message))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MESSAGE]);
    }
}

void
_ide_pipeline_cancel (IdePipeline *self)
{
  g_autoptr(GCancellable) cancellable = NULL;

  g_return_if_fail (IDE_IS_PIPELINE (self));

  cancellable = g_steal_pointer (&self->cancellable);
  self->cancellable = g_cancellable_new ();
  g_cancellable_cancel (cancellable);
}

/**
 * ide_pipeline_has_configured:
 * @self: a #IdePipeline
 *
 * Checks to see if the pipeline has advanced far enough to ensure that
 * the configure stage has been reached.
 *
 * Returns: %TRUE if %IDE_PIPELINE_PHASE_CONFIGURE has been reached.
 */
gboolean
ide_pipeline_has_configured (IdePipeline *self)
{
  g_return_val_if_fail (IDE_IS_PIPELINE (self), FALSE);

  if (self->broken)
    return FALSE;

  /*
   * We need to walk from beginning towards end (instead of
   * taking a cleaner approach that would be to walk from the
   * end forward) because it's possible for some items to be
   * marked completed before they've ever been run.
   *
   * So just walk forward and we have configured if we hit
   * any phase that is CONFIGURE and has completed, or no
   * configure phases were found.
   */

  for (guint i = 0; i < self->pipeline->len; i++)
    {
      const PipelineEntry *entry = &g_array_index (self->pipeline, PipelineEntry, i);

      if ((entry->phase & IDE_PIPELINE_PHASE_MASK) < IDE_PIPELINE_PHASE_CONFIGURE)
        continue;

      if (entry->phase & IDE_PIPELINE_PHASE_CONFIGURE)
        {
          /*
           * This is a configure phase, ensure that it has been
           * completed, or we have not really configured.
           */
          if (!ide_pipeline_stage_get_completed (entry->stage))
            return FALSE;

          /*
           * Check the next pipeline entry to ensure that it too
           * has been configured.
           */
          continue;
        }

      /*
       * We've advanced past CONFIGURE, so anything at this point
       * can be considered configured.
       */

      return TRUE;
    }

  /*
   * Technically we could have a build system that only supports
   * up to configure. But I don't really care about that case. If
   * that ever happens, we need an additional check here that the
   * last pipeline entry completed.
   */

  return FALSE;
}

void
_ide_pipeline_mark_broken (IdePipeline *self)
{
  g_return_if_fail (IDE_IS_PIPELINE (self));

  self->broken = TRUE;
}

static GType
ide_pipeline_get_item_type (GListModel *model)
{
  return IDE_TYPE_PIPELINE_STAGE;
}

static guint
ide_pipeline_get_n_items (GListModel *model)
{
  IdePipeline *self = (IdePipeline *)model;

  g_assert (IDE_IS_PIPELINE (self));

  return self->pipeline != NULL ? self->pipeline->len : 0;
}

static gpointer
ide_pipeline_get_item (GListModel *model,
                       guint       position)
{
  IdePipeline *self = (IdePipeline *)model;
  const PipelineEntry *entry;

  g_assert (IDE_IS_PIPELINE (self));
  g_assert (self->pipeline != NULL);
  g_assert (position < self->pipeline->len);

  entry = &g_array_index (self->pipeline, PipelineEntry, position);

  return g_object_ref (entry->stage);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item = ide_pipeline_get_item;
  iface->get_item_type = ide_pipeline_get_item_type;
  iface->get_n_items = ide_pipeline_get_n_items;
}

/**
 * ide_pipeline_get_requested_phase:
 * @self: a #IdePipeline
 *
 * Gets the phase that has been requested. This can be useful when you want to
 * get an idea of where the build pipeline will attempt to advance.
 *
 * Returns: an #IdePipelinePhase
 */
IdePipelinePhase
ide_pipeline_get_requested_phase (IdePipeline *self)
{
  IdePipelinePhase requested;
  gint msb;

  g_return_val_if_fail (IDE_IS_PIPELINE (self), 0);

  requested = self->requested_mask & IDE_PIPELINE_PHASE_MASK;

  /* We want to return a value that is not a mask of all phases
   * that will be run, but just the most signficant phase. This
   * is represented by the most-signficant-bit after our phase
   * mask has been applied.
   */

  msb = g_bit_nth_msf (requested, -1);

  if (msb == -1)
    return IDE_PIPELINE_PHASE_NONE;

  return (IdePipelinePhase)(1 << msb);
}

void
_ide_pipeline_set_pty_size (IdePipeline *self,
                            guint        rows,
                            guint        columns)
{
  g_return_if_fail (IDE_IS_PIPELINE (self));

  if (self->pty_producer != IDE_PTY_FD_INVALID)
    ide_pty_intercept_set_size (&self->intercept, rows, columns);
}

void
_ide_pipeline_set_runtime (IdePipeline *self,
                           IdeRuntime  *runtime)
{
  g_return_if_fail (IDE_IS_PIPELINE (self));
  g_return_if_fail (!runtime || IDE_IS_RUNTIME (runtime));

  if (g_set_object (&self->runtime, runtime))
    {
      IdeBuildSystem *build_system;
      IdeContext *context;

      context = ide_object_get_context (IDE_OBJECT (self));
      build_system = ide_build_system_from_context (context);

      g_clear_pointer (&self->builddir, g_free);
      self->builddir = ide_build_system_get_builddir (build_system, self);
    }
}

void
_ide_pipeline_set_toolchain (IdePipeline  *self,
                             IdeToolchain *toolchain)
{
  g_return_if_fail (IDE_IS_PIPELINE (self));
  g_return_if_fail (!toolchain || IDE_IS_TOOLCHAIN (toolchain));

  ide_object_lock (IDE_OBJECT (self));
  if (g_set_object (&self->toolchain, toolchain))
    ide_config_set_toolchain (self->config, toolchain);
  ide_object_unlock (IDE_OBJECT (self));
}

/**
 * ide_pipeline_ref_toolchain:
 * @self: a #IdePipeline
 *
 * Thread-safe variant of ide_pipeline_get_toolchain().
 *
 * Returns: (transfer full) (nullable): an #IdeToolchain or %NULL
 */
IdeToolchain *
ide_pipeline_ref_toolchain (IdePipeline *self)
{
  IdeToolchain *ret = NULL;

  g_return_val_if_fail (IDE_IS_PIPELINE (self), NULL);

  ide_object_lock (IDE_OBJECT (self));
  g_set_object (&ret, self->toolchain);
  ide_object_unlock (IDE_OBJECT (self));

  return g_steal_pointer (&ret);
}

void
_ide_pipeline_check_toolchain (IdePipeline   *self,
                               IdeDeviceInfo *info)
{
  g_autoptr(IdeToolchain) toolchain = NULL;
  g_autoptr(IdeTriplet) toolchain_triplet = NULL;
  g_autoptr(IdeContext) context = NULL;
  IdeRuntime *runtime;
  IdeTriplet *device_triplet;
  IdeToolchainManager *manager;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_PIPELINE (self));
  g_return_if_fail (IDE_IS_DEVICE_INFO (info));

  if (ide_object_in_destruction (IDE_OBJECT (self)))
    IDE_EXIT;

  g_set_object (&self->device_info, info);

  context = ide_object_ref_context (IDE_OBJECT (self));
  g_return_if_fail (IDE_IS_CONTEXT (context));

  manager = ide_toolchain_manager_from_context (context);
  g_return_if_fail (IDE_IS_TOOLCHAIN_MANAGER (manager));

  toolchain = ide_config_get_toolchain (self->config);
  runtime = ide_config_get_runtime (self->config);
  device_triplet = ide_device_info_get_host_triplet (info);
  toolchain_triplet = ide_toolchain_get_host_triplet (toolchain);

  if (self->host_triplet != device_triplet)
    {
      g_clear_pointer (&self->host_triplet, ide_triplet_unref);
      self->host_triplet = ide_triplet_ref (device_triplet);
    }

  /* Don't try to initialize too early */
  if (ide_toolchain_manager_is_loaded (manager))
    IDE_EXIT;

  /* TODO: fallback to most compatible toolchain instead of default */

  if (toolchain == NULL ||
      g_strcmp0 (ide_triplet_get_arch (device_triplet),
                 ide_triplet_get_arch (toolchain_triplet)) != 0 ||
      !ide_runtime_supports_toolchain (runtime, toolchain))
    {
      g_autoptr(IdeToolchain) default_toolchain = NULL;

      default_toolchain = ide_toolchain_manager_get_toolchain (manager, "default");
      _ide_pipeline_set_toolchain (self, default_toolchain);
    }

  IDE_EXIT;
}

/**
 * ide_pipeline_get_device:
 * @self: a #IdePipeline
 *
 * Gets the device that the pipeline is building for.
 *
 * Returns: (transfer none): an #IdeDevice.
 */
IdeDevice *
ide_pipeline_get_device (IdePipeline *self)
{
  g_return_val_if_fail (IDE_IS_PIPELINE (self), NULL);

  return self->device;
}

/**
 * ide_pipeline_get_device_info:
 * @self: a #IdePipeline
 *
 * Gets the device info for the current device.
 *
 * Returns: (nullable) (transfer none): an #IdeDeviceInfo or %NULL
 */
IdeDeviceInfo *
ide_pipeline_get_device_info (IdePipeline *self)
{
  g_return_val_if_fail (IDE_IS_PIPELINE (self), NULL);

  return self->device_info;
}

/**
 * ide_pipeline_is_ready:
 * @self: a #IdePipeline
 *
 * Checks to see if the pipeline has been loaded. Loading may be delayed
 * due to various initialization routines that need to complete.
 *
 * Returns: %TRUE if the pipeline has loaded, otherwise %FALSE
 */
gboolean
ide_pipeline_is_ready (IdePipeline *self)
{
  g_return_val_if_fail (IDE_IS_PIPELINE (self), FALSE);

  return self->loaded;
}

/**
 * ide_pipeline_get_host_triplet:
 * @self: a #IdePipeline
 *
 * Gets the "host" triplet which specifies where the build results will run.
 *
 * This is a convenience wrapper around getting the triplet from the device
 * set for the build pipeline.
 *
 * Returns: (transfer none): an #IdeTriplet
 */
IdeTriplet *
ide_pipeline_get_host_triplet (IdePipeline *self)
{
  g_return_val_if_fail (IDE_IS_PIPELINE (self), NULL);

  return self->host_triplet;
}

/**
 * ide_pipeline_is_native:
 * @self: a #IdePipeline
 *
 * This is a helper to check if the triplet that we are compiling
 * for matches the host system. That allows some plugins to do less
 * work by avoiding some cross-compiling work.
 *
 * Returns: %FALSE if we're possibly cross-compiling, otherwise %TRUE
 */
gboolean
ide_pipeline_is_native (IdePipeline *self)
{
  g_return_val_if_fail (IDE_IS_PIPELINE (self), FALSE);

  if (self->host_triplet != NULL)
    return ide_triplet_is_system (self->host_triplet);

  return FALSE;
}

/**
 * ide_pipeline_dup_arch:
 * @self: a #IdePipeline
 *
 * Gets the name of the arch.
 *
 * Returns: (transfer full): a new string containing the arch
 *
 * Since: 44
 */
char *
ide_pipeline_dup_arch (IdePipeline *self)
{
  IdeRuntime *runtime;

  g_return_val_if_fail (IDE_IS_PIPELINE (self), NULL);

  if (self->device_info != NULL)
    {
      IdeTriplet *triplet;

      if ((triplet = ide_device_info_get_host_triplet (self->device_info)))
        return g_strdup (ide_triplet_get_arch (triplet));
    }

  if ((runtime = ide_pipeline_get_runtime (self)))
    return ide_runtime_get_arch (runtime);

  return NULL;
}

static gboolean
contains_in_runtime_with_alt_path (IdeRuntime *runtime,
                                   const char *name,
                                   const char *path)
{
  g_auto(GStrv) pathsplit = NULL;

  g_assert (IDE_IS_RUNTIME (runtime));
  g_assert (name != NULL);

  if (path == NULL)
    return FALSE;

  pathsplit = g_strsplit (path, ":", 0);

  for (guint i = 0; pathsplit[i]; i++)
    {
      g_autofree char *filename = g_build_filename (pathsplit[i], name, NULL);

      if (ide_runtime_contains_program_in_path (runtime, filename, NULL))
        return TRUE;
    }

  return FALSE;
}

/**
 * ide_pipeline_contains_program_in_path:
 * @self: a #IdePipeline
 * @name: the name of a binary
 *
 * Looks through the runtime and SDK extensions for binaries matching
 * @name that may be executed.
 *
 * Returns: %TRUE if @name was found; otherwise %FALSE
 */
gboolean
ide_pipeline_contains_program_in_path (IdePipeline  *self,
                                       const gchar  *name,
                                       GCancellable *cancellable)
{
  const char *append_path = NULL;
  const char *prepend_path = NULL;

  g_return_val_if_fail (IDE_IS_PIPELINE (self), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  if (self->config != NULL)
    {
      append_path = ide_config_get_append_path (self->config);
      prepend_path = ide_config_get_prepend_path (self->config);
    }

  if (self->runtime != NULL)
    {
      if (ide_runtime_contains_program_in_path (self->runtime, name, cancellable) ||
          contains_in_runtime_with_alt_path (self->runtime, name, prepend_path) ||
          contains_in_runtime_with_alt_path (self->runtime, name, append_path))
        return TRUE;
    }

  if (self->config != NULL)
    {
      g_autoptr(GPtrArray) ar = NULL;

      if (g_cancellable_is_cancelled (cancellable))
        return FALSE;

      ar = ide_config_get_extensions (self->config);
      IDE_PTR_ARRAY_SET_FREE_FUNC (ar, ide_object_unref_and_destroy);

      for (guint i = 0; i < ar->len; i++)
        {
          IdeRuntime *runtime = g_ptr_array_index (ar, i);

          g_assert (IDE_IS_RUNTIME (runtime));

          if (ide_runtime_contains_program_in_path (runtime, name, cancellable) ||
              contains_in_runtime_with_alt_path (runtime, name, prepend_path) ||
              contains_in_runtime_with_alt_path (runtime, name, append_path))
            return TRUE;
        }
    }

  return FALSE;
}

/**
 * ide_pipeline_get_deploy_strategy:
 * @self: a #IdePipeline
 *
 * Gets the best discovered deployment strategry.
 *
 * Returns: (transfer none) (nullable): the best deployment strategy
 *   if any are supported for the current configuration.
 */
IdeDeployStrategy *
ide_pipeline_get_deploy_strategy (IdePipeline *self)
{
  g_return_val_if_fail (IDE_IS_PIPELINE (self), NULL);
  g_return_val_if_fail (!self->best_strategy ||
                        IDE_IS_DEPLOY_STRATEGY (self->best_strategy), NULL);

  return self->best_strategy;
}

/**
 * ide_pipeline_addin_find_by_module_name:
 * @pipeline: an #IdePipeline
 * @module_name: the name of the addin module
 *
 * Finds the addin (if any) matching the plugin's @module_name.
 *
 * Returns: (transfer none) (nullable): an #IdePipelineAddin or %NULL
 */
IdePipelineAddin *
ide_pipeline_addin_find_by_module_name (IdePipeline *pipeline,
                                        const gchar *module_name)
{
  PeasPluginInfo *plugin_info;
  GObject *ret = NULL;
  PeasEngine *engine;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_PIPELINE (pipeline), NULL);
  g_return_val_if_fail (module_name != NULL, NULL);

  if (pipeline->addins == NULL)
    return NULL;

  engine = peas_engine_get_default ();

  if ((plugin_info = peas_engine_get_plugin_info (engine, module_name)))
    ret = ide_extension_set_adapter_get_extension (pipeline->addins, plugin_info);

  return IDE_PIPELINE_ADDIN (ret);
}

/**
 * ide_pipeline_prepare_run_context:
 * @self: a #IdePipeline
 * @run_context: an #IdeRunContext
 *
 * Prepares #IdeRunContext to build within the pipeline.
 *
 * You should use this to prepare a new #IdeRunContext to run within the
 * build pipeline environment before adding arguments and other settings
 * to the context.
 *
 * The runtime will be consulted to modify any commands necessary.
 */
void
ide_pipeline_prepare_run_context (IdePipeline   *self,
                                  IdeRunContext *run_context)
{
  IdeRuntime *runtime;
  IdeEnvironment *env;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_PIPELINE (self));
  g_return_if_fail (IDE_IS_RUN_CONTEXT (run_context));

  if (!(runtime = ide_pipeline_get_runtime (self)))
    {
      g_critical ("Attempt to prepare a run context before pipeline has a runtime!");
      return;
    }

  ide_runtime_prepare_to_build (runtime, self, run_context);

  ide_run_context_set_cwd (run_context, ide_pipeline_get_builddir (self));

  ide_run_context_setenv (run_context, "BUILDDIR", ide_pipeline_get_builddir (self));
  ide_run_context_setenv (run_context, "SRCDIR", ide_pipeline_get_srcdir (self));

  if ((env = ide_config_get_environment (self->config)))
    {
      guint n_items = g_list_model_get_n_items (G_LIST_MODEL (env));

      for (guint i = 0; i < n_items; i++)
        {
          g_autoptr(IdeEnvironmentVariable) envvar = g_list_model_get_item (G_LIST_MODEL (env), i);

          ide_run_context_setenv (run_context,
                                  ide_environment_variable_get_key (envvar),
                                  ide_environment_variable_get_value (envvar));
        }
    }
}

/**
 * ide_pipeline_create_run_context:
 * @self: a #IdePipeline
 * @run_command: an #IdeRunCommand
 *
 * Creates a new #IdeRunContext to run @run_command.
 *
 * This helper is generally meant to be used by pipeline stages to create
 * a run context that will execute within the pipeline to run the command
 * described in @run_command.
 *
 * The run context is first prepared using ide_pipeline_prepare_run_context()
 * after which the run command's ide_run_command_prepare_to_run() is used.
 *
 * Returns: (transfer full): an #IdeRunContext
 */
IdeRunContext *
ide_pipeline_create_run_context (IdePipeline   *self,
                                 IdeRunCommand *run_command)
{
  g_autoptr(IdeRunContext) run_context = NULL;
  IdeContext *context;

  g_return_val_if_fail (IDE_IS_PIPELINE (self), NULL);
  g_return_val_if_fail (IDE_IS_RUN_COMMAND (run_command), NULL);

  context = ide_object_get_context (IDE_OBJECT (self));
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  run_context = ide_run_context_new ();
  ide_pipeline_prepare_run_context (self, run_context);
  ide_run_command_prepare_to_run (run_command, run_context, context);

  return g_steal_pointer (&run_context);
}
