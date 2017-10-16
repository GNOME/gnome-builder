/* ide-build-pipeline.c
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

#define G_LOG_DOMAIN "ide-build-pipeline"

#include <glib/gi18n.h>
#include <dazzle.h>
#include <libpeas/peas.h>
#include <string.h>

#include "ide-context.h"
#include "ide-debug.h"
#include "ide-enums.h"
#include "ide-macros.h"

#include "application/ide-application.h"
#include "buildsystem/ide-build-log.h"
#include "buildsystem/ide-build-log-private.h"
#include "buildsystem/ide-build-pipeline.h"
#include "buildsystem/ide-build-pipeline-addin.h"
#include "buildsystem/ide-build-stage.h"
#include "buildsystem/ide-build-stage-launcher.h"
#include "buildsystem/ide-build-stage-private.h"
#include "buildsystem/ide-build-system.h"
#include "buildsystem/ide-build-utils.h"
#include "diagnostics/ide-diagnostic.h"
#include "diagnostics/ide-source-location.h"
#include "diagnostics/ide-source-range.h"
#include "plugins/ide-extension-util.h"
#include "projects/ide-project.h"
#include "runtimes/ide-runtime.h"
#include "vcs/ide-vcs.h"

DZL_DEFINE_COUNTER (Instances, "Pipeline", "N Pipelines", "Number of Pipeline instances")

/**
 * SECTION:idebuildpipeline
 * @title: IdeBuildPipeline
 * @short_description: Pluggable build pipeline
 * @include: ide.h
 *
 * The #IdeBuildPipeline is responsible for managing the build process
 * for Builder. It consists of multiple build "phases" (see #IdeBuildPhase
 * for the individual phases). An #IdeBuildStage can be attached with
 * a priority to each phase and is the primary mechanism that plugins
 * use to perform their operations in the proper ordering.
 *
 * For example, the flatpak plugin provides its download stage as part of the
 * %IDE_BUILD_PHASE_DOWNLOAD phase. The autotools plugin provides stages to
 * phases such as %IDE_BUILD_PHASE_AUTOGEN, %IDE_BUILD_PHASE_CONFIGURE,
 * %IDE_BUILD_PHASE_BUILD, and %IDE_BUILD_PHASE_INSTALL.
 *
 * If you want ensure a particular phase is performed as part of a build,
 * then fall ide_build_pipeline_request_phase() with the phase you are
 * interested in seeing complete successfully.
 *
 * If your plugin has discovered that something has changed that invalidates a
 * given phase, use ide_build_pipeline_invalidate_phase() to ensure that the
 * phase is re-executed the next time a requested phase of higher precidence
 * is requested.
 *
 * It can be useful to perform operations before or after a given stage (but
 * still be executed as part of that stage) so the %IDE_BUILD_PHASE_BEFORE and
 * %IDE_BUILD_PHASE_AFTER flags may be xor'd with the requested phase.  If more
 * precise ordering is required, you may use the priority parameter to order
 * the operation with regards to other stages in that phase.
 *
 * Transient stages may be added to the pipeline and they will be removed after
 * the ide_build_pipeline_execute_async() operation has completed successfully
 * or has failed. You can mark a stage as trandient with
 * ide_build_stage_set_transient(). This may be useful to perform operations
 * such as an "export tarball" stage which should only run once as determined
 * by the user requesting a "make dist" style operation.
 */

typedef struct
{
  guint          id;
  IdeBuildPhase  phase;
  gint           priority;
  IdeBuildStage *stage;
} PipelineEntry;

typedef struct
{
  guint   id;
  GRegex *regex;
} ErrorFormat;

struct _IdeBuildPipeline
{
  IdeObject         parent_instance;

  /*
   * These are our extensions to the BuildPipeline. Plugins insert
   * them and they might go about adding stages to the pipeline,
   * add error formats, or just monitor logs.
   */
  PeasExtensionSet *addins;

  /*
   * This is the configuration for the build. It is a snapshot of
   * the real configuration so that we do not need to synchronize
   * with the UI process for accesses.
   */
  IdeConfiguration *configuration;

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
   * No reference to the current stage. It is only available during
   * the asynchronous execution of the stage.
   */
  IdeBuildStage *current_stage;

  /*
   * The index of our current PipelineEntry. This should start at -1
   * to indicate that no stage is currently active.
   */
  gint position;

  /*
   * This is the requested mask to be built. It should be reset after
   * performing a build so that a followup execute_async() would be
   * innocuous.
   */
  IdeBuildPhase requested_mask;

  /*
   * We queue incoming tasks in case we need for a finish task to
   * complete before our task can continue. The items in the queue
   * are DelayedTask structs with a GTask and the type id so we
   * can progress the task upon completion of the previous task.
   */
  GQueue task_queue;

  /*
   * We use this sequence number to give PipelineEntry instances a
   * unique identifier. The addins can use this to remove their
   * inserted build stages.
   */
  guint seqnum;

  /*
   * If we failed to build, this should be set.
   */
  guint failed : 1;

  /*
   * If we are within a built, this should be set.
   */
  guint busy : 1;

  /*
   * If we are in the middle of a clean operation.
   */
  guint in_clean : 1;

  /*
   * Precalculation if we need to look for errors on stdout. We can't rely
   * on @current_stage for this, becase log entries might come in
   * asynchronously and after the processes/stage has completed.
   */
  guint errors_on_stdout : 1;
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
  GTask *task;

  /*
   * The phase that should be met for the given pipeline operation.
   */
  IdeBuildPhase phase;

  union {
    struct {
      GPtrArray *stages;
    } clean;
  };
} TaskData;

static void ide_build_pipeline_queue_flush  (IdeBuildPipeline *self);
static void ide_build_pipeline_tick_execute (IdeBuildPipeline *self,
                                             GTask            *task);
static void ide_build_pipeline_tick_clean   (IdeBuildPipeline *self,
                                             GTask            *task);
static void ide_build_pipeline_tick_rebuild (IdeBuildPipeline *self,
                                             GTask            *task);
static void initable_iface_init             (GInitableIface   *iface);

G_DEFINE_TYPE_EXTENDED (IdeBuildPipeline, ide_build_pipeline, IDE_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initable_iface_init))

enum {
  PROP_0,
  PROP_BUSY,
  PROP_CONFIGURATION,
  PROP_MESSAGE,
  PROP_PHASE,
  N_PROPS
};

enum {
  DIAGNOSTIC,
  STARTED,
  FINISHED,
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
chained_binding_clear (gpointer data)
{
  GBinding *binding = data;

  g_binding_unbind (binding);
  g_object_unref (binding);
}

static void
task_data_free (gpointer data)
{
  TaskData *td = data;

  if (td != NULL)
    {
      if (td->type == TASK_CLEAN)
        g_clear_pointer (&td->clean.stages, g_ptr_array_unref);
      td->type = 0;
      td->task = NULL;
      g_slice_free (TaskData, td);
    }
}

static TaskData *
task_data_new (GTask    *task,
               TaskType  type)
{
  TaskData *td;

  g_assert (G_IS_TASK (task));
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
build_phase_nick (IdeBuildPhase phase)
{
  GFlagsClass *klass = g_type_class_peek (IDE_TYPE_BUILD_PHASE);
  GFlagsValue *value;

  g_assert (klass != NULL);

  phase &= IDE_BUILD_PHASE_MASK;
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

  if (strstr (lower, "deprecated") != NULL)
    return IDE_DIAGNOSTIC_DEPRECATED;

  if (strstr (lower, "note") != NULL)
    return IDE_DIAGNOSTIC_NOTE;

  return IDE_DIAGNOSTIC_WARNING;
}

static IdeDiagnostic *
create_diagnostic (IdeBuildPipeline *self,
                   GMatchInfo       *match_info)
{
  g_autofree gchar *filename = NULL;
  g_autofree gchar *line = NULL;
  g_autofree gchar *column = NULL;
  g_autofree gchar *message = NULL;
  g_autofree gchar *level = NULL;
  g_autoptr(IdeFile) file = NULL;
  g_autoptr(IdeSourceLocation) location = NULL;
  IdeContext *context;
  struct {
    gint64 line;
    gint64 column;
    IdeDiagnosticSeverity severity;
  } parsed = { 0 };

  g_assert (IDE_IS_BUILD_PIPELINE (self));
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
      IdeVcs *vcs;
      GFile *workdir;
      gchar *path;

      vcs = ide_context_get_vcs (context);
      workdir = ide_vcs_get_working_directory (vcs);

      child = g_file_get_child (workdir, filename);
      path = g_file_get_path (child);

      g_free (filename);
      filename = path;
    }

  file = ide_file_new_for_path (context, filename);
  location = ide_source_location_new (file, parsed.line, parsed.column, 0);

  return ide_diagnostic_new (parsed.severity, message, location);
}

static void
ide_build_pipeline_log_observer (IdeBuildLogStream  stream,
                                 const gchar       *message,
                                 gssize             message_len,
                                 gpointer           user_data)
{
  IdeBuildPipeline *self = user_data;
  g_autofree gchar *filtered_message = NULL;
  const gchar *enterdir;

  g_assert (stream == IDE_BUILD_LOG_STDOUT || stream == IDE_BUILD_LOG_STDERR);
  g_assert (IDE_IS_BUILD_PIPELINE (self));
  g_assert (message != NULL);

#define ENTERING_DIRECTORY_BEGIN "Entering directory '"
#define ENTERING_DIRECTORY_END   "'"

  if (message_len < 0)
    message_len = strlen (message);

  if (self->log != NULL)
    ide_build_log_observer (stream, message, message_len, self->log);

  filtered_message = ide_build_utils_color_codes_filtering (message);

  if (stream == IDE_BUILD_LOG_STDOUT)
    {
      /*
       * This expects LANG=C, which is defined in the autotools Builder.
       * Not the most ideal decoupling of logic, but we don't have a whole
       * lot to work with here.
       */
      if (NULL != (enterdir = strstr (filtered_message, ENTERING_DIRECTORY_BEGIN)) &&
          g_str_has_suffix (enterdir, ENTERING_DIRECTORY_END))
        {
          gssize len;

          enterdir += IDE_LITERAL_LENGTH (ENTERING_DIRECTORY_BEGIN);

          /* Translate to relative paths for out-of-tree builds */
          if (g_str_has_prefix (enterdir, self->builddir))
            {
              enterdir += strlen (self->builddir);
              if (*enterdir == G_DIR_SEPARATOR)
                enterdir++;
            }

          len = strlen (enterdir) - IDE_LITERAL_LENGTH (ENTERING_DIRECTORY_END);

          if (len > 0)
            {
              g_free (self->errfmt_current_dir);
              self->errfmt_current_dir = g_strndup (enterdir, len);
              if (self->errfmt_top_dir == NULL)
                self->errfmt_top_dir = g_strndup (enterdir, len);
            }

          return;
        }
    }

  /*
   * Unfortunately, some build engines such as Ninja refuse to pass errors on
   * stderr like the tooling they abstract. So we must parse stdout in addition
   * to stderr to extract errors.
   */
  if (stream == IDE_BUILD_LOG_STDERR || self->errors_on_stdout)
    {
      for (guint i = 0; i < self->errfmts->len; i++)
        {
          const ErrorFormat *errfmt = &g_array_index (self->errfmts, ErrorFormat, i);
          g_autoptr(GMatchInfo) match_info = NULL;

          if (g_regex_match (errfmt->regex, filtered_message, 0, &match_info))
            {
              g_autoptr(IdeDiagnostic) diagnostic = create_diagnostic (self, match_info);

              if (diagnostic != NULL)
                {
                  ide_build_pipeline_emit_diagnostic (self, diagnostic);
                  return;
                }
            }
        }
    }

#undef ENTERING_DIRECTORY_BEGIN
#undef ENTERING_DIRECTORY_END
}

static void
ide_build_pipeline_release_transients (IdeBuildPipeline *self)
{
  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_PIPELINE (self));
  g_assert (self->pipeline != NULL);

  for (guint i = self->pipeline->len; i > 0; i--)
    {
      const PipelineEntry *entry = &g_array_index (self->pipeline, PipelineEntry, i - 1);

      g_assert (IDE_IS_BUILD_STAGE (entry->stage));

      if (ide_build_stage_get_transient (entry->stage))
        {
          IDE_TRACE_MSG ("Releasing transient stage %s at index %u",
                         G_OBJECT_TYPE_NAME (entry->stage),
                         i - 1);
          g_array_remove_index (self->pipeline, i);
        }
    }

  IDE_EXIT;
}

/**
 * ide_build_pipeline_get_phase:
 *
 * Gets the current phase that is executing. This is only useful during
 * execution of the pipeline.
 */
IdeBuildPhase
ide_build_pipeline_get_phase (IdeBuildPipeline *self)
{
  g_return_val_if_fail (IDE_IS_BUILD_PIPELINE (self), 0);

  if (self->position < 0)
    return IDE_BUILD_PHASE_NONE;
  else if (self->failed)
    return IDE_BUILD_PHASE_FAILED;
  else if ((guint)self->position < self->pipeline->len)
    return g_array_index (self->pipeline, PipelineEntry, self->position).phase & IDE_BUILD_PHASE_MASK;
  else
    return IDE_BUILD_PHASE_FINISHED;
}

/**
 * ide_build_pipeline_get_configuration:
 *
 * Gets the #IdeConfiguration to use for the pipeline.
 *
 * Returns: (transfer none): An #IdeConfiguration
 */
IdeConfiguration *
ide_build_pipeline_get_configuration (IdeBuildPipeline *self)
{
  g_return_val_if_fail (IDE_IS_BUILD_PIPELINE (self), NULL);

  return self->configuration;
}

static void
clear_pipeline_entry (gpointer data)
{
  PipelineEntry *entry = data;

  if (entry->stage != NULL)
    {
      ide_build_stage_set_log_observer (entry->stage, NULL, NULL, NULL);
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

  ret = (gint)(entry_a->phase & IDE_BUILD_PHASE_MASK)
      - (gint)(entry_b->phase & IDE_BUILD_PHASE_MASK);

  if (ret == 0)
    {
      gint whence_a = (entry_a->phase & IDE_BUILD_PHASE_WHENCE_MASK);
      gint whence_b = (entry_b->phase & IDE_BUILD_PHASE_WHENCE_MASK);

      if (whence_a != whence_b)
        {
          if (whence_a == IDE_BUILD_PHASE_BEFORE)
            return -1;

          if (whence_b == IDE_BUILD_PHASE_BEFORE)
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
ide_build_pipeline_real_started (IdeBuildPipeline *self)
{
  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_PIPELINE (self));

  self->errors_on_stdout = FALSE;

  for (guint i = 0; i < self->pipeline->len; i++)
    {
      PipelineEntry *entry = &g_array_index (self->pipeline, PipelineEntry, i);

      if (ide_build_stage_get_check_stdout (entry->stage))
        {
          self->errors_on_stdout = TRUE;
          break;
        }
    }

  IDE_EXIT;
}

static void
ide_build_pipeline_real_finished (IdeBuildPipeline *self,
                                  gboolean          failed)
{
  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_PIPELINE (self));

  IDE_EXIT;
}

static void
ide_build_pipeline_extension_added (PeasExtensionSet *set,
                                    PeasPluginInfo   *plugin_info,
                                    PeasExtension    *exten,
                                    gpointer          user_data)
{
  IdeBuildPipeline *self = user_data;
  IdeBuildPipelineAddin *addin = (IdeBuildPipelineAddin *)exten;

  IDE_ENTRY;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_BUILD_PIPELINE_ADDIN (addin));
  g_assert (IDE_IS_BUILD_PIPELINE (self));

  ide_build_pipeline_addin_load (addin, self);

  IDE_EXIT;
}

static void
ide_build_pipeline_extension_removed (PeasExtensionSet *set,
                                      PeasPluginInfo   *plugin_info,
                                      PeasExtension    *exten,
                                      gpointer          user_data)
{
  IdeBuildPipeline *self = user_data;
  IdeBuildPipelineAddin *addin = (IdeBuildPipelineAddin *)exten;

  IDE_ENTRY;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_BUILD_PIPELINE_ADDIN (addin));
  g_assert (IDE_IS_BUILD_PIPELINE (self));

  ide_build_pipeline_addin_unload (addin, self);

  IDE_EXIT;
}

static void
register_build_commands_stage (IdeBuildPipeline *self,
                               IdeContext       *context)
{
  g_autoptr(GError) error = NULL;
  const gchar * const *build_commands;

  g_assert (IDE_IS_BUILD_PIPELINE (self));
  g_assert (IDE_IS_CONTEXT (context));
  g_assert (IDE_IS_CONFIGURATION (self->configuration));

  build_commands = ide_configuration_get_build_commands (self->configuration);
  if (build_commands == NULL)
    return;
  for (guint i = 0; build_commands[i]; i++)
    {
      g_autoptr(IdeSubprocessLauncher) launcher = NULL;
      g_autoptr(IdeBuildStage) stage = NULL;

      if (NULL == (launcher = ide_build_pipeline_create_launcher (self, &error)))
        {
          g_warning ("%s", error->message);
          return;
        }

      ide_subprocess_launcher_push_argv (launcher, "/bin/sh");
      ide_subprocess_launcher_push_argv (launcher, "-c");
      ide_subprocess_launcher_push_argv (launcher, build_commands[i]);

      stage = g_object_new (IDE_TYPE_BUILD_STAGE_LAUNCHER,
                            "context", context,
                            "launcher", launcher,
                            NULL);

      ide_build_pipeline_connect (self,
                                  IDE_BUILD_PHASE_BUILD | IDE_BUILD_PHASE_AFTER,
                                  i,
                                  stage);
    }
}

static void
register_post_install_commands_stage (IdeBuildPipeline *self,
                                      IdeContext       *context)
{
  g_autoptr(GError) error = NULL;
  const gchar * const *post_install_commands;

  g_assert (IDE_IS_BUILD_PIPELINE (self));
  g_assert (IDE_IS_CONTEXT (context));
  g_assert (IDE_IS_CONFIGURATION (self->configuration));

  post_install_commands = ide_configuration_get_post_install_commands (self->configuration);
  if (post_install_commands == NULL)
    return;
  for (guint i = 0; post_install_commands[i]; i++)
    {
      g_autoptr(IdeSubprocessLauncher) launcher = NULL;
      g_autoptr(IdeBuildStage) stage = NULL;

      if (NULL == (launcher = ide_build_pipeline_create_launcher (self, &error)))
        {
          g_warning ("%s", error->message);
          return;
        }

      ide_subprocess_launcher_push_argv (launcher, "/bin/sh");
      ide_subprocess_launcher_push_argv (launcher, "-c");
      ide_subprocess_launcher_push_argv (launcher, post_install_commands[i]);

      stage = g_object_new (IDE_TYPE_BUILD_STAGE_LAUNCHER,
                            "context", context,
                            "launcher", launcher,
                            NULL);

      ide_build_pipeline_connect (self,
                                  IDE_BUILD_PHASE_INSTALL | IDE_BUILD_PHASE_AFTER,
                                  i,
                                  stage);
    }
}

/**
 * ide_build_pipeline_load:
 *
 * This manages the loading of addins which will register their necessary build
 * stages.  We do this separately from ::constructed so that we can
 * enable/disable the pipeline as the IdeConfiguration:ready property changes.
 * This could happen when the device or runtime is added/removed while the
 * application is running.
 */
static void
ide_build_pipeline_load (IdeBuildPipeline *self)
{
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_PIPELINE (self));
  g_assert (self->addins == NULL);

  context = ide_object_get_context (IDE_OBJECT (self));

  register_build_commands_stage (self, context);
  register_post_install_commands_stage (self, context);

  self->addins = ide_extension_set_new (peas_engine_get_default (),
                                        IDE_TYPE_BUILD_PIPELINE_ADDIN,
                                        "context", context,
                                        NULL);

  g_signal_connect (self->addins,
                    "extension-added",
                    G_CALLBACK (ide_build_pipeline_extension_added),
                    self);

  g_signal_connect (self->addins,
                    "extension-removed",
                    G_CALLBACK (ide_build_pipeline_extension_removed),
                    self);

  peas_extension_set_foreach (self->addins,
                              ide_build_pipeline_extension_added,
                              self);

  IDE_EXIT;
}

/*
 * ide_build_pipeline_unload:
 *
 * This clears things up that were initialized in ide_build_pipeline_load. This
 * function is safe to run even if load has not been called. We will not clean
 * things up if the pipeline is currently executing (we can wait until its
 * finished or dispose/finalize to cleanup up further.
 */
static void
ide_build_pipeline_unload (IdeBuildPipeline *self)
{
  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_PIPELINE (self));

  g_clear_object (&self->addins);

  IDE_EXIT;
}

static void
ide_build_pipeline_notify_ready (IdeBuildPipeline *self,
                                 GParamSpec       *pspec,
                                 IdeConfiguration *configuration)
{
  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_PIPELINE (self));
  g_assert (IDE_IS_CONFIGURATION (configuration));

  if (ide_configuration_get_ready (configuration))
    ide_build_pipeline_load (self);
  else
    ide_build_pipeline_unload (self);

  IDE_EXIT;
}

static void
ide_build_pipeline_finalize (GObject *object)
{
  IdeBuildPipeline *self = (IdeBuildPipeline *)object;

  IDE_ENTRY;

  g_assert (self->task_queue.length == 0);
  g_queue_clear (&self->task_queue);

  g_clear_object (&self->log);
  g_clear_object (&self->configuration);
  g_clear_pointer (&self->pipeline, g_array_unref);
  g_clear_pointer (&self->srcdir, g_free);
  g_clear_pointer (&self->builddir, g_free);
  g_clear_pointer (&self->errfmts, g_array_unref);
  g_clear_pointer (&self->errfmt_top_dir, g_free);
  g_clear_pointer (&self->errfmt_current_dir, g_free);
  g_clear_pointer (&self->chained_bindings, g_ptr_array_free);

  G_OBJECT_CLASS (ide_build_pipeline_parent_class)->finalize (object);

  DZL_COUNTER_DEC (Instances);

  IDE_EXIT;
}

static void
ide_build_pipeline_dispose (GObject *object)
{
  IdeBuildPipeline *self = IDE_BUILD_PIPELINE (object);

  IDE_ENTRY;

  ide_build_pipeline_unload (self);

  G_OBJECT_CLASS (ide_build_pipeline_parent_class)->dispose (object);

  IDE_EXIT;
}

static gboolean
ide_build_pipeline_initable_init (GInitable     *initable,
                                  GCancellable  *cancellable,
                                  GError       **error)
{
  IdeBuildPipeline *self = (IdeBuildPipeline *)initable;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_PIPELINE (self));
  g_assert (IDE_IS_CONFIGURATION (self->configuration));

  g_signal_connect_object (self->configuration,
                           "notify::ready",
                           G_CALLBACK (ide_build_pipeline_notify_ready),
                           self,
                           G_CONNECT_SWAPPED);

  ide_build_pipeline_notify_ready (self, NULL, self->configuration);

  IDE_RETURN (TRUE);
}

static void
initable_iface_init (GInitableIface *iface)
{
  iface->init = ide_build_pipeline_initable_init;
}

static void
ide_build_pipeline_constructed (GObject *object)
{
  IdeBuildPipeline *self = IDE_BUILD_PIPELINE (object);
  IdeBuildSystem *build_system;
  IdeContext *context;
  IdeVcs *vcs;
  GFile *workdir;

  IDE_ENTRY;

  G_OBJECT_CLASS (ide_build_pipeline_parent_class)->constructed (object);

  g_assert (IDE_IS_CONFIGURATION (self->configuration));

  context = ide_object_get_context (IDE_OBJECT (self));
  build_system = ide_context_get_build_system (context);
  vcs = ide_context_get_vcs (context);
  workdir = ide_vcs_get_working_directory (vcs);

  self->srcdir = g_file_get_path (workdir);
  self->builddir = ide_build_system_get_builddir (build_system, self->configuration);

  IDE_EXIT;
}

static void
ide_build_pipeline_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  IdeBuildPipeline *self = IDE_BUILD_PIPELINE (object);

  switch (prop_id)
    {
    case PROP_BUSY:
      g_value_set_boolean (value, self->busy);
      break;

    case PROP_CONFIGURATION:
      g_value_set_object (value, ide_build_pipeline_get_configuration (self));
      break;

    case PROP_MESSAGE:
      g_value_set_string (value, ide_build_pipeline_get_message (self));
      break;

    case PROP_PHASE:
      g_value_set_flags (value, ide_build_pipeline_get_phase (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_build_pipeline_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  IdeBuildPipeline *self = IDE_BUILD_PIPELINE (object);

  switch (prop_id)
    {
    case PROP_CONFIGURATION:
      self->configuration = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_build_pipeline_class_init (IdeBuildPipelineClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ide_build_pipeline_constructed;
  object_class->dispose = ide_build_pipeline_dispose;
  object_class->finalize = ide_build_pipeline_finalize;
  object_class->get_property = ide_build_pipeline_get_property;
  object_class->set_property = ide_build_pipeline_set_property;

  /**
   * IdeBuildPipeline:busy:
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
   * IdeBuildPipeline:configuration:
   *
   * The configuration to use for the build pipeline.
   */
  properties [PROP_CONFIGURATION] =
    g_param_spec_object ("configuration",
                         "Configuration",
                         "Configuration",
                         IDE_TYPE_CONFIGURATION,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_MESSAGE] =
    g_param_spec_string ("message",
                         "Message",
                         "The message for the build phase",
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * IdeBuildPipeline:phase:
   *
   * The current build phase during execution of the pipeline.
   */
  properties [PROP_PHASE] =
    g_param_spec_flags ("phase",
                        "Phase",
                        "The phase that is being executed",
                        IDE_TYPE_BUILD_PHASE,
                        IDE_BUILD_PHASE_NONE,
                        (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * IdeBuildPipeline::diagnostic:
   * @self: An #IdeBuildPipeline
   * @diagnostic: The newly created diagnostic
   *
   * This signal is emitted when a plugin has detected a diagnostic while
   * building the pipeline.
   */
  signals [DIAGNOSTIC] =
    g_signal_new_class_handler ("diagnostic",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                NULL, NULL, NULL, NULL,
                                G_TYPE_NONE, 1, IDE_TYPE_DIAGNOSTIC);

  /**
   * IdeBuildPipeline::started:
   * @self: An #IdeBuildPipeline
   * @phase: the #IdeBuildPhase for which we are advancing
   *
   * This signal is emitted when the pipeline has started executing in
   * response to ide_build_pipeline_execute_async() being called.
   */
  signals [STARTED] =
    g_signal_new_class_handler ("started",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (ide_build_pipeline_real_started),
                                NULL, NULL, NULL,
                                G_TYPE_NONE, 1, IDE_TYPE_BUILD_PHASE);

  /**
   * IdeBuildPipeline::finished:
   * @self: An #IdeBuildPipeline
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
                                G_CALLBACK (ide_build_pipeline_real_finished),
                                NULL, NULL, NULL,
                                G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
}

static void
ide_build_pipeline_init (IdeBuildPipeline *self)
{
  DZL_COUNTER_INC (Instances);

  self->position = -1;

  self->pipeline = g_array_new (FALSE, FALSE, sizeof (PipelineEntry));
  g_array_set_clear_func (self->pipeline, clear_pipeline_entry);

  self->errfmts = g_array_new (FALSE, FALSE, sizeof (ErrorFormat));
  g_array_set_clear_func (self->errfmts, clear_error_format);

  self->chained_bindings = g_ptr_array_new_with_free_func ((GDestroyNotify)chained_binding_clear);

  self->log = ide_build_log_new ();
}

static void
ide_build_pipeline_stage_execute_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  IdeBuildStage *stage = (IdeBuildStage *)object;
  IdeBuildPipeline *self;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_STAGE (stage));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  g_assert (IDE_IS_BUILD_PIPELINE (self));

  if (!_ide_build_stage_execute_with_query_finish (stage, result, &error))
    {
      g_debug ("stage of type %s failed: %s",
               G_OBJECT_TYPE_NAME (stage),
               error->message);
      self->failed = TRUE;
      g_task_return_error (task, g_steal_pointer (&error));
    }

  ide_build_stage_set_completed (stage, !self->failed);

  g_clear_pointer (&self->chained_bindings, g_ptr_array_free);
  self->chained_bindings = g_ptr_array_new_with_free_func (g_object_unref);

  if (self->failed == FALSE)
    ide_build_pipeline_tick_execute (self, task);

  IDE_EXIT;
}

static void
ide_build_pipeline_try_chain (IdeBuildPipeline *self,
                              IdeBuildStage    *stage,
                              guint             position)
{
  g_assert (IDE_IS_BUILD_PIPELINE (self));
  g_assert (IDE_IS_BUILD_STAGE (stage));

  for (; position < self->pipeline->len; position++)
    {
      const PipelineEntry *entry = &g_array_index (self->pipeline, PipelineEntry, position);
      gboolean chained;
      GBinding *chained_binding;

      /*
       * Ignore all future stages if they were not requested by the current
       * pipeline execution.
       */
      if (((entry->phase & IDE_BUILD_PHASE_MASK) & self->requested_mask) == 0)
        return;

      /* Skip past the stage if it is disabled. */
      if (ide_build_stage_get_disabled (entry->stage))
        continue;

      chained = ide_build_stage_chain (stage, entry->stage);

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
ide_build_pipeline_tick_execute (IdeBuildPipeline *self,
                                 GTask            *task)
{
  GCancellable *cancellable;
  TaskData *td;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_PIPELINE (self));
  g_assert (G_IS_TASK (task));

  self->current_stage = NULL;

  td = g_task_get_task_data (task);
  cancellable = g_task_get_cancellable (task);

  g_assert (td != NULL);
  g_assert (td->type == TASK_BUILD || td->type == TASK_REBUILD);
  g_assert (td->task == task);
  g_assert (td->phase != IDE_BUILD_PHASE_NONE);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  /* Short circuit now if the task was cancelled */
  if (g_task_return_error_if_cancelled (task))
    IDE_EXIT;

  /* If we can skip walking the pipeline, go ahead and do so now. */
  if (!ide_build_pipeline_request_phase (self, td->phase))
    {
      g_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  /*
   * Walk forward to the next stage requiring execution and asynchronously
   * execute it. The stage may also need to perform an async ::query signal
   * delaying pipeline execution. _ide_build_stage_execute_with_query_async()
   * will handle all of that for us, in cause they call ide_build_stage_pause()
   * during the ::query callback.
   */
  for (self->position++; (guint)self->position < self->pipeline->len; self->position++)
    {
      const PipelineEntry *entry = &g_array_index (self->pipeline, PipelineEntry, self->position);

      g_assert (entry->stage != NULL);
      g_assert (IDE_IS_BUILD_STAGE (entry->stage));

      /* Ignore the stage if it is disabled */
      if (ide_build_stage_get_disabled (entry->stage))
        continue;

      if ((entry->phase & IDE_BUILD_PHASE_MASK) & self->requested_mask)
        {
          self->current_stage = entry->stage;

          /*
           * We might be able to chain upcoming stages to this stage and avoid
           * duplicate work. This will also advance self->position based on
           * how many stages were chained.
           */
          ide_build_pipeline_try_chain (self, entry->stage, self->position + 1);

          _ide_build_stage_execute_with_query_async (entry->stage,
                                                     self,
                                                     cancellable,
                                                     ide_build_pipeline_stage_execute_cb,
                                                     g_object_ref (task));

          g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MESSAGE]);
          g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PHASE]);

          IDE_EXIT;
        }
    }

  g_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
ide_build_pipeline_task_notify_completed (IdeBuildPipeline *self,
                                          GParamSpec       *pspec,
                                          GTask            *task)
{
  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_PIPELINE (self));
  g_assert (G_IS_TASK (task));

  IDE_TRACE_MSG ("Clearing busy bit for pipeline");

  self->current_stage = NULL;
  self->busy = FALSE;
  self->requested_mask = 0;
  self->in_clean = FALSE;

  /*
   * XXX: How do we ensure transients are executed with the part of the
   *      pipeline we care about? We might just need to ensure that :busy is
   *      FALSE before adding transients.
   */
  ide_build_pipeline_release_transients (self);

  g_signal_emit (self, signals [FINISHED], 0, self->failed);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BUSY]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PHASE]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MESSAGE]);

  /*
   * We might have a delayed addin unloading that needs to occur after the
   * build operation completes. If the configuration is no longer valid,
   * go ahead and unload the pipeline.
   */
  if (!ide_configuration_get_ready (self->configuration))
    ide_build_pipeline_unload (self);
  else
    ide_build_pipeline_queue_flush (self);

  IDE_EXIT;
}

/**
 * ide_build_pipeline_execute_async:
 * @self: A @IdeBuildPipeline
 * @cancellable: (nullable): A #GCancellable or %NULL
 * @callback: a callback to execute upon completion
 * @user_data: data for @callback
 *
 * Asynchronously starts the build pipeline.
 *
 * Any phase that has been invalidated up to the requested phase
 * will be executed until a stage has failed.
 *
 * Upon completion, @callback will be executed and should call
 * ide_build_pipeline_execute_finish() to get the status of the
 * operation.
 */
void
ide_build_pipeline_execute_async (IdeBuildPipeline    *self,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  g_autoptr(GFile) builddir = NULL;
  g_autoptr(GError) error = NULL;
  TaskData *task_data;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_BUILD_PIPELINE (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_build_pipeline_execute_async);
  g_task_set_priority (task, G_PRIORITY_LOW);

  if (self->requested_mask == IDE_BUILD_PHASE_NONE)
    {
      g_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  /*
   * XXX: Maybe we should allow a phase to be provided with execute
   *      now for symmetry with the others. Also, rename to build_async()?
   */

  task_data = task_data_new (task, TASK_BUILD);
  task_data->phase = 1 << g_bit_nth_msf (self->requested_mask, -1);
  g_task_set_task_data (task, task_data, task_data_free);

  g_queue_push_tail (&self->task_queue, g_steal_pointer (&task));

  ide_build_pipeline_queue_flush (self);
}

static gboolean
ide_build_pipeline_do_flush (gpointer data)
{
  IdeBuildPipeline *self = data;
  g_autoptr(GTask) task = NULL;
  g_autoptr(GFile) builddir = NULL;
  g_autoptr(GError) error = NULL;
  TaskData *task_data;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_PIPELINE (self));

  /*
   * If the busy bit is set, there is nothing to do right now.
   */
  if (self->busy)
    {
      IDE_TRACE_MSG ("pipeline already busy, defering flush");
      IDE_RETURN (G_SOURCE_REMOVE);
    }

  /* Ensure our builddir is created, or else we will fail all pending tasks. */
  builddir = g_file_new_for_path (self->builddir);
  if (!g_file_make_directory_with_parents (builddir, NULL, &error) &&
      !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
    {
      GTask *failed_task;

      while (NULL != (failed_task = g_queue_pop_head (&self->task_queue)))
        {
          g_task_return_error (failed_task, g_error_copy (error));
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

  g_assert (G_IS_TASK (task));
  g_assert (self->busy == FALSE);

  /*
   * Now prepare the task so that when it completes we can make
   * forward progress again.
   */
  g_signal_connect_object (task,
                           "notify::completed",
                           G_CALLBACK (ide_build_pipeline_task_notify_completed),
                           self,
                           G_CONNECT_SWAPPED);

  /* We need access to the task data to determine how to process the task. */
  task_data = g_task_get_task_data (task);

  g_assert (task_data != NULL);
  g_assert (task_data->type > 0);
  g_assert (task_data->type <= TASK_REBUILD);
  g_assert (G_IS_TASK (task_data->task));

  /*
   * If this build request could cause us to spin while we are continually
   * failing to reach the CONFIGURE stage, protect ourselves as early as we
   * can. We'll defer to a rebuild request to cause the full thing to build.
   */
  if (self->failed &&
      task_data->type == TASK_BUILD &&
      task_data->phase <= IDE_BUILD_PHASE_CONFIGURE)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
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

  /*
   * The following logs some helpful information about the build to our
   * debug log. This is useful to allow users to debug some problems
   * with our assistance (using gnome-builder -vvv).
   */
  {
    g_autoptr(GString) str = g_string_new (NULL);
    GFlagsClass *klass;
    IdeBuildPhase phase = self->requested_mask;

    klass = g_type_class_peek (IDE_TYPE_BUILD_PHASE);

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

        g_debug (" pipeline[%u]: %12s: %s [%s]",
                 i,
                 build_phase_nick (entry->phase),
                 G_OBJECT_TYPE_NAME (entry->stage),
                 ide_build_stage_get_completed (entry->stage) ? "completed" : "pending");
      }
  }

  /* Notify any observers that a build (of some sort) is about to start. */
  g_signal_emit (self, signals [STARTED], 0, task_data->phase);

  switch (task_data->type)
    {
    case TASK_BUILD:
      ide_build_pipeline_tick_execute (self, task);
      break;

    case TASK_CLEAN:
      ide_build_pipeline_tick_clean (self, task);
      break;

    case TASK_REBUILD:
      ide_build_pipeline_tick_rebuild (self, task);
      break;

    default:
      g_assert_not_reached ();
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BUSY]);

  IDE_RETURN (G_SOURCE_REMOVE);
}

static void
ide_build_pipeline_queue_flush (IdeBuildPipeline *self)
{
  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_PIPELINE (self));

  gdk_threads_add_idle_full (G_PRIORITY_LOW,
                             ide_build_pipeline_do_flush,
                             g_object_ref (self),
                             g_object_unref);

  IDE_EXIT;
}

/**
 * ide_build_pipeline_execute_finish:
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 */
gboolean
ide_build_pipeline_execute_finish (IdeBuildPipeline  *self,
                                   GAsyncResult      *result,
                                   GError           **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_BUILD_PIPELINE (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  ret = g_task_propagate_boolean (G_TASK (result), error);

  IDE_RETURN (ret);
}

/**
 * ide_build_pipeline_connect:
 * @self: A #IdeBuildPipeline
 * @phase: An #IdeBuildPhase
 * @priority: an optional priority for sorting within the phase
 * @stage: An #IdeBuildStage
 *
 * Insert @stage into the pipeline as part of the phase denoted by @phase.
 *
 * If priority is non-zero, it will be used to sort the stage among other
 * stages that are part of the same phase.
 *
 * Returns: A stage_id that may be passed to ide_build_pipeline_disconnect().
 */
guint
ide_build_pipeline_connect (IdeBuildPipeline *self,
                            IdeBuildPhase     phase,
                            gint              priority,
                            IdeBuildStage    *stage)
{
  GFlagsClass *klass;
  guint ret = 0;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_BUILD_PIPELINE (self), 0);
  g_return_val_if_fail (IDE_IS_BUILD_STAGE (stage), 0);
  g_return_val_if_fail ((phase & IDE_BUILD_PHASE_MASK) != IDE_BUILD_PHASE_NONE, 0);
  g_return_val_if_fail ((phase & IDE_BUILD_PHASE_WHENCE_MASK) == 0 ||
                        (phase & IDE_BUILD_PHASE_WHENCE_MASK) == IDE_BUILD_PHASE_BEFORE ||
                        (phase & IDE_BUILD_PHASE_WHENCE_MASK) == IDE_BUILD_PHASE_AFTER, 0);

  klass = g_type_class_ref (IDE_TYPE_BUILD_PHASE);

  for (guint i = 0; i < klass->n_values; i++)
    {
      const GFlagsValue *value = &klass->values[i];

      if ((phase & IDE_BUILD_PHASE_MASK) == value->value)
        {
          PipelineEntry entry = { 0 };

          IDE_TRACE_MSG ("Adding stage to pipeline with phase %s and priority %d",
                         value->value_nick, priority);

          entry.id = ++self->seqnum;
          entry.phase = phase;
          entry.priority = priority;
          entry.stage = g_object_ref (stage);

          g_array_append_val (self->pipeline, entry);
          g_array_sort (self->pipeline, pipeline_entry_compare);

          ret = entry.id;

          ide_build_stage_set_log_observer (stage,
                                            ide_build_pipeline_log_observer,
                                            self,
                                            NULL);

          IDE_GOTO (cleanup);
        }
    }

  g_warning ("No such pipeline phase %02x", phase);

cleanup:
  g_type_class_unref (klass);

  IDE_RETURN (ret);
}

/**
 * ide_build_pipeline_connect_launcher:
 * @self: A #IdeBuildPipeline
 * @phase: An #IdeBuildPhase
 * @priority: an optional priority for sorting within the phase
 * @launcher: An #IdeSubprocessLauncher
 *
 * This creates a new stage that will spawn a process using @launcher and log
 * the output of stdin/stdout.
 *
 * It is a programmer error to modify @launcher after passing it to this
 * function.
 *
 * Returns: A stage_id that may be passed to ide_build_pipeline_remove().
 */
guint
ide_build_pipeline_connect_launcher (IdeBuildPipeline      *self,
                                     IdeBuildPhase          phase,
                                     gint                   priority,
                                     IdeSubprocessLauncher *launcher)
{
  g_autoptr(IdeBuildStage) stage = NULL;
  IdeContext *context;

  g_return_val_if_fail (IDE_IS_BUILD_PIPELINE (self), 0);
  g_return_val_if_fail ((phase & IDE_BUILD_PHASE_MASK) != IDE_BUILD_PHASE_NONE, 0);
  g_return_val_if_fail ((phase & IDE_BUILD_PHASE_WHENCE_MASK) == 0 ||
                        (phase & IDE_BUILD_PHASE_WHENCE_MASK) == IDE_BUILD_PHASE_BEFORE ||
                        (phase & IDE_BUILD_PHASE_WHENCE_MASK) == IDE_BUILD_PHASE_AFTER, 0);

  context = ide_object_get_context (IDE_OBJECT (self));
  stage = ide_build_stage_launcher_new (context, launcher);

  return ide_build_pipeline_connect (self, phase, priority, stage);
}

/**
 * ide_build_pipeline_request_phase:
 * @self: An #IdeBuildPipeline
 * @phase: An #IdeBuildPhase
 *
 * Requests that the next execution of the pipeline will build up to @phase
 * including all stages that were previously invalidated.
 *
 * Returns: %TRUE if a stage is known to require execution.
 */
gboolean
ide_build_pipeline_request_phase (IdeBuildPipeline *self,
                                  IdeBuildPhase     phase)
{
  GFlagsClass *klass;
  gboolean ret = FALSE;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_BUILD_PIPELINE (self), FALSE);
  g_return_val_if_fail ((phase & IDE_BUILD_PHASE_MASK) != IDE_BUILD_PHASE_NONE, FALSE);

  /*
   * You can only request basic phases. That does not include modifiers
   * like BEFORE, AFTER, FAILED, FINISHED.
   */
  phase &= IDE_BUILD_PHASE_MASK;

  klass = g_type_class_ref (IDE_TYPE_BUILD_PHASE);

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
   * caller know that they need to run execute_async() to be up to date. This
   * is useful for situations where you might want to avoid calling
   * execute_async() altogether. Additionally, we want to know if there are
   * any connections to the "query" which could cause the completed state
   * to be invalidated.
   */
  for (guint i = 0; i < self->pipeline->len; i++)
    {
      const PipelineEntry *entry = &g_array_index (self->pipeline, PipelineEntry, i);

      if (!(entry->phase & self->requested_mask))
        continue;

      if (!ide_build_stage_get_completed (entry->stage) ||
          _ide_build_stage_has_query (entry->stage))
        {
          ret = TRUE;
          break;
        }
    }

  g_type_class_unref (klass);

  IDE_RETURN (ret);
}

/**
 * ide_build_pipeline_get_builddir:
 * @self: An #IdeBuildPipeline
 *
 * Gets the "builddir" to be used for the build process. This is generally
 * the location that build systems will use for out-of-tree builds.
 *
 * Returns: the path of the build directory
 */
const gchar *
ide_build_pipeline_get_builddir (IdeBuildPipeline *self)
{
  g_return_val_if_fail (IDE_IS_BUILD_PIPELINE (self), NULL);

  return self->builddir;
}

/**
 * ide_build_pipeline_get_srcdir:
 * @self: An #IdeBuildPipeline
 *
 * Gets the "srcdir" of the project. This is equivalent to the
 * IdeVcs:working-directory property as a string.
 *
 * Returns: the path of the source directory
 */
const gchar *
ide_build_pipeline_get_srcdir (IdeBuildPipeline *self)
{
  g_return_val_if_fail (IDE_IS_BUILD_PIPELINE (self), NULL);

  return self->srcdir;
}

static gchar *
ide_build_pipeline_build_path_va_list (const gchar *prefix,
                                       const gchar *first_part,
                                       va_list      args)
{
  g_autoptr(GPtrArray) ar = NULL;

  g_assert (prefix != NULL);
  g_assert (first_part != NULL);

  ar = g_ptr_array_new ();
  g_ptr_array_add (ar, (gchar *)prefix);
  do
    g_ptr_array_add (ar, (gchar *)first_part);
  while (NULL != (first_part = va_arg (args, const gchar *)));
  g_ptr_array_add (ar, NULL);

  return g_build_filenamev ((gchar **)ar->pdata);
}

/**
 * ide_build_pipeline_build_srcdir_path:
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
ide_build_pipeline_build_srcdir_path (IdeBuildPipeline *self,
                                      const gchar      *first_part,
                                      ...)
{
  gchar *ret;
  va_list args;

  g_return_val_if_fail (IDE_IS_BUILD_PIPELINE (self), NULL);
  g_return_val_if_fail (first_part != NULL, NULL);

  va_start (args, first_part);
  ret = ide_build_pipeline_build_path_va_list (self->srcdir, first_part, args);
  va_end (args);

  return ret;
}

/**
 * ide_build_pipeline_build_builddir_path:
 *
 * This is a convenience function to create a new path that starts with
 * the build directory for this build configuration.
 *
 * This is functionally equivalent to calling g_build_filename() with the
 * result of ide_build_pipeline_get_builddir() as the first parameter.
 *
 * Returns: (transfer full): A newly allocated string.
 */
gchar *
ide_build_pipeline_build_builddir_path (IdeBuildPipeline *self,
                                        const gchar      *first_part,
                                        ...)
{
  gchar *ret;
  va_list args;

  g_return_val_if_fail (IDE_IS_BUILD_PIPELINE (self), NULL);
  g_return_val_if_fail (first_part != NULL, NULL);

  va_start (args, first_part);
  ret = ide_build_pipeline_build_path_va_list (self->builddir, first_part, args);
  va_end (args);

  return ret;
}

/**
 * ide_build_pipeline_disconnect:
 * @self: An #IdeBuildPipeline
 * @stage_id: An identifier returned from adding a stage
 *
 * This removes the stage matching @stage_id. You are returned a @stage_id when
 * inserting a stage with functions such as ide_build_pipeline_connect()
 * or ide_build_pipeline_connect_launcher().
 *
 * Plugins should use this function to remove their stages when the plugin
 * is unloading.
 */
void
ide_build_pipeline_disconnect (IdeBuildPipeline *self,
                               guint             stage_id)
{
  g_return_if_fail (IDE_IS_BUILD_PIPELINE (self));
  g_return_if_fail (self->pipeline != NULL);
  g_return_if_fail (stage_id != 0);

  for (guint i = 0; i < self->pipeline->len; i++)
    {
      const PipelineEntry *entry = &g_array_index (self->pipeline, PipelineEntry, i);

      if (entry->id == stage_id)
        {
          g_array_remove_index (self->pipeline, i);
          break;
        }
    }
}

/**
 * ide_build_pipeline_invalidate_phase:
 * @self: An #IdeBuildPipeline
 * @phases: The phases to invalidate
 *
 * Invalidates the phases matching @phases flags.
 *
 * If the requested phases include the phases invalidated here, the next
 * execution of the pipeline will execute thse phases.
 *
 * This should be used by plugins to ensure a particular phase is re-executed
 * upon discovering its state is no longer valid. Such an example might be
 * invalidating the %IDE_BUILD_PHASE_AUTOGEN phase when the an autotools
 * projects autogen.sh file has been changed.
 */
void
ide_build_pipeline_invalidate_phase (IdeBuildPipeline *self,
                                     IdeBuildPhase     phases)
{
  g_return_if_fail (IDE_IS_BUILD_PIPELINE (self));

  for (guint i = 0; i < self->pipeline->len; i++)
    {
      const PipelineEntry *entry = &g_array_index (self->pipeline, PipelineEntry, i);

      if ((entry->phase & IDE_BUILD_PHASE_MASK) & phases)
        ide_build_stage_set_completed (entry->stage, FALSE);
    }
}

/**
 * ide_build_pipeline_get_stage_by_id:
 * @self: An #IdeBuildPipeline
 * @stage_id: the identfier of the stage
 *
 * Gets the stage matching the identifier @stage_id as returned from
 * ide_build_pipeline_connect().
 *
 * Returns: (transfer none) (nullable): An #IdeBuildStage or %NULL if the
 *   stage could not be found.
 */
IdeBuildStage *
ide_build_pipeline_get_stage_by_id (IdeBuildPipeline *self,
                                    guint             stage_id)
{
  g_return_val_if_fail (IDE_IS_BUILD_PIPELINE (self), NULL);

  for (guint i = 0; i < self->pipeline->len; i++)
    {
      const PipelineEntry *entry = &g_array_index (self->pipeline, PipelineEntry, i);

      if (entry->id == stage_id)
        return entry->stage;
    }

  return NULL;
}

/**
 * ide_build_pipeline_get_runtime:
 * @self: An #IdeBuildPipeline
 *
 * A convenience function to get the runtime for a build pipeline.
 *
 * Returns: (transfer none) (nullable): An #IdeRuntime or %NULL
 *
 * Since: 3.28
 */
IdeRuntime *
ide_build_pipeline_get_runtime (IdeBuildPipeline *self)
{
  g_return_val_if_fail (IDE_IS_BUILD_PIPELINE (self), NULL);

  return ide_configuration_get_runtime (self->configuration);
}

/**
 * ide_build_pipeline_create_launcher:
 * @self: An #IdeBuildPipeline
 *
 * This is a convenience function to create a new #IdeSubprocessLauncher
 * using the configuration and runtime associated with the pipeline.
 *
 * Returns: (transfer full): An #IdeSubprocessLauncher.
 */
IdeSubprocessLauncher *
ide_build_pipeline_create_launcher (IdeBuildPipeline  *self,
                                    GError           **error)
{
  g_autoptr(IdeSubprocessLauncher) ret = NULL;
  IdeRuntime *runtime;

  g_return_val_if_fail (IDE_IS_BUILD_PIPELINE (self), NULL);

  runtime = ide_configuration_get_runtime (self->configuration);

  if (runtime == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "The runtime %s is missing",
                   ide_configuration_get_runtime_id (self->configuration));
      return NULL;
    }

  ret = ide_runtime_create_launcher (runtime, error);

  if (ret != NULL)
    {
      IdeEnvironment *env = ide_configuration_get_environment (self->configuration);

      ide_subprocess_launcher_set_clear_env (ret, TRUE);
      ide_subprocess_launcher_overlay_environment (ret, env);
      /* Always ignore V=1 from configurations */
      ide_subprocess_launcher_setenv (ret, "V", "0", TRUE);
      ide_subprocess_launcher_set_cwd (ret, ide_build_pipeline_get_builddir (self));
      ide_subprocess_launcher_set_flags (ret,
                                         (G_SUBPROCESS_FLAGS_STDERR_PIPE |
                                          G_SUBPROCESS_FLAGS_STDOUT_PIPE));
    }

  return g_steal_pointer (&ret);
}

guint
ide_build_pipeline_add_log_observer (IdeBuildPipeline    *self,
                                     IdeBuildLogObserver  observer,
                                     gpointer             observer_data,
                                     GDestroyNotify       observer_data_destroy)
{
  g_return_val_if_fail (IDE_IS_BUILD_PIPELINE (self), 0);
  g_return_val_if_fail (observer != NULL, 0);

  return ide_build_log_add_observer (self->log, observer, observer_data, observer_data_destroy);

}

gboolean
ide_build_pipeline_remove_log_observer (IdeBuildPipeline *self,
                                        guint             observer_id)
{
  g_return_val_if_fail (IDE_IS_BUILD_PIPELINE (self), FALSE);
  g_return_val_if_fail (observer_id > 0, FALSE);

  return ide_build_log_remove_observer (self->log, observer_id);
}

void
ide_build_pipeline_emit_diagnostic (IdeBuildPipeline *self,
                                    IdeDiagnostic    *diagnostic)
{
  g_return_if_fail (IDE_IS_BUILD_PIPELINE (self));
  g_return_if_fail (diagnostic != NULL);
  g_return_if_fail (IDE_IS_MAIN_THREAD ());

  g_signal_emit (self, signals[DIAGNOSTIC], 0, diagnostic);
}

/**
 * ide_build_pipeline_add_error_format:
 * @self: A #IdeBuildPipeline
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
 *   "(?<filename>[a-zA-Z0-9\\-\\.\\/_]+):"
 *   "(?<line>\\d+):"
 *   "(?<column>\\d+): "
 *   "(?<level>[\\w\\s]+): "
 *   "(?<message>.*)"
 *
 * To remove the regex, use the ide_build_pipeline_remove_error_format()
 * function with the resulting format id returned from this function.
 *
 * The resulting format id will be > 0 if successful.
 *
 * Returns: an error format id that may be passed to
 *   ide_build_pipeline_remove_error_format().
 */
guint
ide_build_pipeline_add_error_format (IdeBuildPipeline   *self,
                                     const gchar        *regex,
                                     GRegexCompileFlags  flags)
{
  ErrorFormat errfmt = { 0 };
  g_autoptr(GError) error = NULL;

  g_return_val_if_fail (IDE_IS_BUILD_PIPELINE (self), 0);

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
 * ide_build_pipeline_remove_error_format:
 * @self: An #IdeBuildPipeline
 * @error_format_id: an identifier for the error format.
 *
 * Removes an error format that was registered with
 * ide_build_pipeline_add_error_format().
 *
 * Returns: %TRUE if the error format was removed.
 */
gboolean
ide_build_pipeline_remove_error_format (IdeBuildPipeline *self,
                                        guint             error_format_id)
{
  g_return_val_if_fail (IDE_IS_BUILD_PIPELINE (self), FALSE);
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
ide_build_pipeline_get_busy (IdeBuildPipeline *self)
{
  g_return_val_if_fail (IDE_IS_BUILD_PIPELINE (self), FALSE);

  return self->busy;
}

/**
 * ide_build_pipeline_get_message:
 * @self: An #IdeBuildPipeline
 *
 * Gets the current message for the build pipeline. This can be
 * shown to users in UI elements to signify progress in the
 * build.
 *
 * Returns: (nullable) (transfer full): A string representing the
 *   current stage of the build, or %NULL.
 */
gchar *
ide_build_pipeline_get_message (IdeBuildPipeline *self)
{
  IdeBuildPhase phase;
  const gchar *ret = NULL;

  g_return_val_if_fail (IDE_IS_BUILD_PIPELINE (self), NULL);

  if (self->in_clean)
    return g_strdup (_("Cleaning…"));

  if (self->current_stage != NULL)
    {
      const gchar *name = ide_build_stage_get_name (self->current_stage);

      if (!ide_str_empty0 (name))
        return g_strdup (name);
    }

  phase = ide_build_pipeline_get_phase (self);

  switch (phase)
    {
    case IDE_BUILD_PHASE_DOWNLOADS:
      ret = _("Downloading…");
      break;

    case IDE_BUILD_PHASE_DEPENDENCIES:
      ret = _("Building dependencies…");
      break;

    case IDE_BUILD_PHASE_AUTOGEN:
      ret = _("Bootstrapping…");
      break;

    case IDE_BUILD_PHASE_CONFIGURE:
      ret = _("Configuring…");
      break;

    case IDE_BUILD_PHASE_BUILD:
      ret = _("Building…");
      break;

    case IDE_BUILD_PHASE_INSTALL:
      ret = _("Installing…");
      break;

    case IDE_BUILD_PHASE_EXPORT:
      ret = _("Exporting…");
      break;

    case IDE_BUILD_PHASE_FINAL:
      ret = _("Success");
      break;

    case IDE_BUILD_PHASE_FINISHED:
      ret = _("Success");
      break;

    case IDE_BUILD_PHASE_FAILED:
      ret = _("Failed");
      break;

    case IDE_BUILD_PHASE_PREPARE:
      ret = _("Preparing…");
      break;

    case IDE_BUILD_PHASE_NONE:
      ret = _("Ready");
      break;

    case IDE_BUILD_PHASE_AFTER:
    case IDE_BUILD_PHASE_BEFORE:
    default:
      g_assert_not_reached ();
    }

  return g_strdup (ret);
}

/**
 * ide_build_pipeline_foreach_stage:
 * @self: An #IdeBuildPipeline
 * @stage_callback: (scope call): A callback for each #IdePipelineStage
 * @user_data: user data for @stage_callback
 *
 * This function will call @stage_callback for every #IdeBuildStage registered
 * in the pipeline.
 */
void
ide_build_pipeline_foreach_stage (IdeBuildPipeline *self,
                                  GFunc             stage_callback,
                                  gpointer          user_data)
{
  g_return_if_fail (IDE_IS_BUILD_PIPELINE (self));
  g_return_if_fail (stage_callback != NULL);

  for (guint i = 0; i < self->pipeline->len; i++)
    {
      const PipelineEntry *entry = &g_array_index (self->pipeline, PipelineEntry, i);

      stage_callback (entry->stage, user_data);
    }
}

static void
ide_build_pipeline_clean_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  IdeBuildStage *stage = (IdeBuildStage *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeBuildPipeline *self;
  GPtrArray *stages;
  TaskData *td;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_STAGE (stage));
  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  td = g_task_get_task_data (task);

  g_assert (IDE_IS_BUILD_PIPELINE (self));
  g_assert (td != NULL);
  g_assert (td->type == TASK_CLEAN);
  g_assert (td->task == task);
  g_assert (td->clean.stages != NULL);

  stages = td->clean.stages;

  g_assert (stages != NULL);
  g_assert (stages->len > 0);
  g_assert (g_ptr_array_index (stages, stages->len - 1) == stage);

  if (!ide_build_stage_clean_finish (stage, result, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  g_ptr_array_remove_index (stages, stages->len - 1);

  ide_build_pipeline_tick_clean (self, task);

  IDE_EXIT;
}

static void
ide_build_pipeline_tick_clean (IdeBuildPipeline *self,
                               GTask            *task)
{
  GCancellable *cancellable;
  GPtrArray *stages;
  TaskData *td;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_PIPELINE (self));
  g_assert (G_IS_TASK (task));

  td = g_task_get_task_data (task);
  cancellable = g_task_get_cancellable (task);

  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (td != NULL);
  g_assert (td->type == TASK_CLEAN);
  g_assert (td->task == task);
  g_assert (td->clean.stages != NULL);

  stages = td->clean.stages;

  if (stages->len != 0)
    {
      IdeBuildStage *stage = g_ptr_array_index (stages, stages->len - 1);

      self->current_stage = stage;

      ide_build_stage_clean_async (stage,
                                   self,
                                   cancellable,
                                   ide_build_pipeline_clean_cb,
                                   g_object_ref (task));

      IDE_GOTO (notify);
    }

  g_task_return_boolean (task, TRUE);

notify:
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MESSAGE]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PHASE]);

  IDE_EXIT;
}

void
ide_build_pipeline_clean_async (IdeBuildPipeline    *self,
                                IdeBuildPhase        phase,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  g_autoptr(GPtrArray) stages = NULL;
  IdeBuildPhase min_phase = IDE_BUILD_PHASE_FINAL;
  IdeBuildPhase phase_mask;
  GFlagsClass *phase_class;
  TaskData *td;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_PIPELINE (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_build_pipeline_clean_async);

  td = task_data_new (task, TASK_CLEAN);
  td->phase = phase;
  g_task_set_task_data (task, td, task_data_free);

  /*
   * To clean the project, we go through each stage and call it's clean async
   * vfunc pairs if they have been set. Afterwards, we ensure their
   * IdeBuildStage:completed bit is cleared so they will run as part of the
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

  phase_class = g_type_class_peek (IDE_TYPE_BUILD_PHASE);

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

      if ((entry->phase & IDE_BUILD_PHASE_MASK) & phase_mask)
        g_ptr_array_add (stages, g_object_ref (entry->stage));
    }

  /*
   * Short-circuit if we don't have any stages to clean.
   */
  if (stages->len == 0)
    {
      g_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  td->clean.stages = g_steal_pointer (&stages);

  g_queue_push_tail (&self->task_queue, g_steal_pointer (&task));

  ide_build_pipeline_queue_flush (self);

  IDE_EXIT;
}

gboolean
ide_build_pipeline_clean_finish (IdeBuildPipeline  *self,
                                 GAsyncResult      *result,
                                 GError           **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_PIPELINE (self));
  g_assert (G_IS_TASK (result));

  ret = g_task_propagate_boolean (G_TASK (result), error);

  IDE_RETURN (ret);
}

static gboolean
can_remove_builddir (IdeBuildPipeline *self)
{
  g_autoptr(GFile) builddir = NULL;
  g_autoptr(GFile) cache = NULL;

  g_assert (IDE_IS_BUILD_PIPELINE (self));

  /*
   * Only remove builddir if it is in ~/.cache/ or our XDG data dirs
   * equivalent. We don't want to accidentally remove data that might
   * be important to the user.
   */

  cache = g_file_new_for_path (g_get_user_cache_dir ());
  builddir = g_file_new_for_path (self->builddir);

  return g_file_has_prefix (builddir, cache);
}

static void
ide_build_pipeline_reaper_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  DzlDirectoryReaper *reaper = (DzlDirectoryReaper *)object;
  IdeBuildPipeline *self;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  GCancellable *cancellable;
  TaskData *td;

  IDE_ENTRY;

  g_assert (DZL_IS_DIRECTORY_REAPER (reaper));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  td = g_task_get_task_data (task);

  g_assert (td != NULL);
  g_assert (td->task == task);
  g_assert (td->type == TASK_REBUILD);

  cancellable = g_task_get_cancellable (task);
  self = g_task_get_source_object (task);

  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (IDE_IS_BUILD_PIPELINE (self));

  /* Make sure our reaper completed or else we bail */
  if (!dzl_directory_reaper_execute_finish (reaper, result, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  if (td->phase == IDE_BUILD_PHASE_NONE)
    {
      g_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  /* Perform a build using the same task and skipping the build queue. */
  ide_build_pipeline_tick_execute (self, task);

  IDE_EXIT;
}

static void
ide_build_pipeline_tick_rebuild (IdeBuildPipeline *self,
                                 GTask            *task)
{
  g_autoptr(DzlDirectoryReaper) reaper = NULL;
  GCancellable *cancellable;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_PIPELINE (self));
  g_assert (G_IS_TASK (task));

#ifndef G_DISABLE_ASSERT
  {
    TaskData *td = g_task_get_task_data (task);

    g_assert (td != NULL);
    g_assert (td->type == TASK_REBUILD);
    g_assert (td->task == task);
  }
#endif

  reaper = dzl_directory_reaper_new ();

  /*
   * Check if we can remove the builddir. We don't want to do this if it is the
   * same as the srcdir (in-tree builds).
   */
  if (can_remove_builddir (self))
    {
      g_autoptr(GFile) builddir = g_file_new_for_path (self->builddir);

      dzl_directory_reaper_add_directory (reaper, builddir, 0);
    }

  /*
   * Now let the build stages add any files they might want to reap as part of
   * the rebuild process.
   */
  for (guint i = 0; i < self->pipeline->len; i++)
    {
      const PipelineEntry *entry = &g_array_index (self->pipeline, PipelineEntry, i);

      ide_build_stage_emit_reap (entry->stage, reaper);
      ide_build_stage_set_completed (entry->stage, FALSE);
    }

  cancellable = g_task_get_cancellable (task);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  /* Now execute the reaper to clean up the build files. */
  dzl_directory_reaper_execute_async (reaper,
                                      cancellable,
                                      ide_build_pipeline_reaper_cb,
                                      g_object_ref (task));

  IDE_EXIT;
}

void
ide_build_pipeline_rebuild_async (IdeBuildPipeline    *self,
                                  IdeBuildPhase        phase,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  TaskData *td;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_BUILD_PIPELINE (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail ((phase & ~IDE_BUILD_PHASE_MASK) == 0);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_build_pipeline_rebuild_async);

  td = task_data_new (task, TASK_REBUILD);
  td->phase = phase;
  g_task_set_task_data (task, td, task_data_free);

  g_queue_push_tail (&self->task_queue, g_steal_pointer (&task));

  ide_build_pipeline_queue_flush (self);

  IDE_EXIT;
}

gboolean
ide_build_pipeline_rebuild_finish (IdeBuildPipeline  *self,
                                   GAsyncResult      *result,
                                   GError           **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_PIPELINE (self));
  g_assert (G_IS_TASK (result));

  ret = g_task_propagate_boolean (G_TASK (result), error);

  IDE_RETURN (ret);
}

/**
 * ide_build_pipeline_get_can_export:
 * @self: a #IdeBuildPipeline
 *
 * This function is useful to discover if there are any pipeline addins
 * which implement the export phase. UI or GAction implementations may
 * want to use this value to set the enabled state of the action or
 * sensitivity of a button.
 *
 * Returns: %TRUE if there are export pipeline stages.
 */
gboolean
ide_build_pipeline_get_can_export (IdeBuildPipeline *self)
{
  g_return_val_if_fail (IDE_IS_BUILD_PIPELINE (self), FALSE);

  for (guint i = 0; i < self->pipeline->len; i++)
    {
      const PipelineEntry *entry = &g_array_index (self->pipeline, PipelineEntry, i);

      if ((entry->phase & IDE_BUILD_PHASE_EXPORT) != 0)
        return TRUE;
    }

  return FALSE;
}
