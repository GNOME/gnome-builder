/* ide-diagnostic-tool.c
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

#define G_LOG_DOMAIN "ide-diagnostic-tool"

#include "config.h"

#include <libide-threading.h>

#include "ide-build-manager.h"
#include "ide-diagnostic-tool.h"
#include "ide-pipeline.h"
#include "ide-run-context.h"
#include "ide-runtime.h"
#include "ide-runtime-manager.h"

#define DEFAULT_FLAGS (G_SUBPROCESS_FLAGS_STDIN_PIPE|G_SUBPROCESS_FLAGS_STDOUT_PIPE|G_SUBPROCESS_FLAGS_STDERR_PIPE)

typedef struct
{
  char *program_name;
  char *bundled_program_path;
  char *local_program_path;
  GSubprocessFlags flags;
} IdeDiagnosticToolPrivate;

typedef struct
{
  GBytes *stdin_bytes;
  GFile  *file;
} DiagnoseState;

static void diagnostic_provider_iface_init (IdeDiagnosticProviderInterface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (IdeDiagnosticTool, ide_diagnostic_tool, IDE_TYPE_OBJECT,
                                  G_ADD_PRIVATE (IdeDiagnosticTool)
                                  G_IMPLEMENT_INTERFACE (IDE_TYPE_DIAGNOSTIC_PROVIDER,
                                                         diagnostic_provider_iface_init))

enum {
  PROP_0,
  PROP_PROGRAM_NAME,
  PROP_BUNDLED_PROGRAM_PATH,
  PROP_LOCAL_PROGRAM_PATH,
  PROP_SUBPROCESS_FLAGS,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
diagnose_state_free (DiagnoseState *state)
{
  IDE_ENTRY;

  g_assert (state != NULL);

  g_clear_pointer (&state->stdin_bytes, g_bytes_unref);
  g_clear_object (&state->file);
  g_slice_free (DiagnoseState, state);

  IDE_EXIT;
}

static GBytes *
ide_diagnostic_tool_real_get_stdin_bytes (IdeDiagnosticTool *self,
                                          GFile             *file,
                                          GBytes            *contents,
                                          const char        *language_id)
{
  IDE_ENTRY;

  g_assert (IDE_IS_DIAGNOSTIC_TOOL (self));
  g_assert (!file || G_IS_FILE (file));
  g_assert (file != NULL || contents != NULL);

  if (contents != NULL)
    IDE_RETURN (g_bytes_ref (contents));

  IDE_RETURN (NULL);
}

static gboolean
ide_diagnostic_tool_real_prepare_run_context (IdeDiagnosticTool  *self,
                                              IdeRunContext      *run_context,
                                              GFile              *file,
                                              GBytes             *contents,
                                              const char         *language_id,
                                              GError            **error)
{
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(GFile) workdir = NULL;
  const char *srcdir = NULL;
  const char *local_program_path;
  const char *bundled_program_path;
  const char *program_name;
  g_autofree char *program_path = NULL;
  g_autofree char *found = NULL;
  IdeRuntimeManager *runtime_manager;
  IdeRuntime *host = NULL;
  IdePipeline *pipeline = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_DIAGNOSTIC_TOOL (self));

  if (!(context = ide_object_ref_context (IDE_OBJECT (self))))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_CANCELLED,
                   "Context lost, cancelling request");
      IDE_RETURN (FALSE);
    }

  workdir = ide_context_ref_workdir (context);
  srcdir = g_file_peek_path (workdir);

  local_program_path = ide_diagnostic_tool_get_local_program_path (self);
  bundled_program_path = ide_diagnostic_tool_get_bundled_program_path (self);
  program_name = ide_diagnostic_tool_get_program_name (self);

  if (program_name == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_CANCELLED,
                   "No program-name provided for diagnostic tool");
      IDE_RETURN (FALSE);
    }

  if (ide_context_has_project (context))
    {
      IdeBuildManager *build_manager = ide_build_manager_from_context (context);
      pipeline = ide_build_manager_get_pipeline (build_manager);
      runtime_manager = ide_runtime_manager_from_context (context);
      host = ide_runtime_manager_get_runtime (runtime_manager, "host");
    }

  if (pipeline != NULL)
    srcdir = ide_pipeline_get_srcdir (pipeline);

  if (local_program_path != NULL)
    {
      g_autofree char *local_program = g_build_filename (srcdir, local_program_path, NULL);
      if (g_file_test (local_program, G_FILE_TEST_IS_EXECUTABLE))
        program_path = g_steal_pointer (&local_program);
    }

  if (pipeline != NULL &&
      ((program_path != NULL && ide_pipeline_contains_program_in_path (pipeline, program_path, NULL)) ||
      ide_pipeline_contains_program_in_path (pipeline, program_name, NULL)))
    {
      ide_pipeline_prepare_run_context (pipeline, run_context);
      IDE_GOTO (setup_launcher);
    }

  if (host != NULL)
    {
      /* Now try on the host using the "host" runtime which can do
       * a better job of discovering the program on the host and
       * take into account if the user has something modifying the
       * shell like .bashrc.
       */
      if (program_path != NULL ||
          ide_runtime_contains_program_in_path (host, program_name, NULL))
        {
          ide_runtime_prepare_to_build (host, pipeline, run_context);
          IDE_GOTO (setup_launcher);
        }
    }
  else if (program_path != NULL)
    {
      ide_run_context_push_host (run_context);
      IDE_GOTO (setup_launcher);
    }

  if (bundled_program_path != NULL && ide_is_flatpak ())
    program_path = g_strdup (bundled_program_path);

  /* See if Builder itself has bundled the program */
  if (program_path || (found = g_find_program_in_path (program_name)))
    IDE_GOTO (setup_launcher);

  g_set_error (error,
               G_IO_ERROR,
               G_IO_ERROR_NOT_FOUND,
               "Failed to locate program \"%s\"",
               program_name);

  IDE_RETURN (FALSE);

setup_launcher:
  ide_run_context_append_argv (run_context, program_path ? program_path : program_name);
  ide_run_context_set_cwd (run_context, srcdir);

  IDE_RETURN (TRUE);
}

static gboolean
ide_diagnostic_tool_real_can_diagnose (IdeDiagnosticTool *self,
                                       GFile             *file,
                                       GBytes            *bytes,
                                       const char        *language_id)
{
  IDE_ENTRY;

  g_assert (IDE_IS_DIAGNOSTIC_TOOL (self));
  g_assert (!file || G_IS_FILE (file));
  g_assert (file != NULL || bytes != NULL);

  IDE_RETURN (TRUE);
}

static void
ide_diagnostic_tool_constructed (GObject *object)
{
  IdeDiagnosticTool *self = (IdeDiagnosticTool *)object;

  G_OBJECT_CLASS (ide_diagnostic_tool_parent_class)->constructed (object);

  if (!IDE_DIAGNOSTIC_TOOL_GET_CLASS (self)->populate_diagnostics)
    g_critical ("%s inherits from IdeDiagnosticTool but does not implement populate_diagnostics(). This will not work.",
                G_OBJECT_TYPE_NAME (self));

  IDE_TRACE_MSG ("Created diagnostic tool %s",
                 G_OBJECT_TYPE_NAME (object));
}

static void
ide_diagnostic_tool_finalize (GObject *object)
{
  IdeDiagnosticTool *self = (IdeDiagnosticTool *)object;
  IdeDiagnosticToolPrivate *priv = ide_diagnostic_tool_get_instance_private (self);

  IDE_ENTRY;

  IDE_TRACE_MSG ("Finalizing diagnostic tool %s",
                 G_OBJECT_TYPE_NAME (self));

  g_clear_pointer (&priv->program_name, g_free);
  g_clear_pointer (&priv->bundled_program_path, g_free);
  g_clear_pointer (&priv->local_program_path, g_free);

  G_OBJECT_CLASS (ide_diagnostic_tool_parent_class)->finalize (object);

  IDE_EXIT;
}

static void
ide_diagnostic_tool_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  IdeDiagnosticTool *self = IDE_DIAGNOSTIC_TOOL (object);

  switch (prop_id)
    {
    case PROP_PROGRAM_NAME:
      g_value_set_string (value, ide_diagnostic_tool_get_program_name (self));
      break;

    case PROP_BUNDLED_PROGRAM_PATH:
      g_value_set_string (value, ide_diagnostic_tool_get_bundled_program_path (self));
      break;

    case PROP_LOCAL_PROGRAM_PATH:
      g_value_set_string (value, ide_diagnostic_tool_get_local_program_path (self));
      break;

    case PROP_SUBPROCESS_FLAGS:
      g_value_set_flags (value, ide_diagnostic_tool_get_subprocess_flags (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_diagnostic_tool_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  IdeDiagnosticTool *self = IDE_DIAGNOSTIC_TOOL (object);

  switch (prop_id)
    {
    case PROP_PROGRAM_NAME:
      ide_diagnostic_tool_set_program_name (self, g_value_get_string (value));
      break;

    case PROP_BUNDLED_PROGRAM_PATH:
      ide_diagnostic_tool_set_bundled_program_path (self, g_value_get_string (value));
      break;

    case PROP_LOCAL_PROGRAM_PATH:
      ide_diagnostic_tool_set_local_program_path (self, g_value_get_string (value));
      break;

    case PROP_SUBPROCESS_FLAGS:
      ide_diagnostic_tool_set_subprocess_flags (self, g_value_get_flags (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_diagnostic_tool_class_init (IdeDiagnosticToolClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ide_diagnostic_tool_constructed;
  object_class->finalize = ide_diagnostic_tool_finalize;
  object_class->get_property = ide_diagnostic_tool_get_property;
  object_class->set_property = ide_diagnostic_tool_set_property;

  klass->get_stdin_bytes = ide_diagnostic_tool_real_get_stdin_bytes;
  klass->can_diagnose = ide_diagnostic_tool_real_can_diagnose;
  klass->prepare_run_context = ide_diagnostic_tool_real_prepare_run_context;

  /**
   * IdeDiagnosticTool:program-name:
   *
   * The "program-name" property contains the name of the executable to
   * locate within the build container, host system, or within Builder's
   * own runtime container.
   */
  properties [PROP_PROGRAM_NAME] =
    g_param_spec_string ("program-name",
                         "Program Name",
                         "The name of the program executable to locate",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_BUNDLED_PROGRAM_PATH] =
    g_param_spec_string ("bundled-program-path",
                         "Bundled Program Path",
                         "The path of the bundled program",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_LOCAL_PROGRAM_PATH] =
    g_param_spec_string ("local-program-path",
                         "Local Program Path",
                         "The path of the program inside active project",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SUBPROCESS_FLAGS] =
    g_param_spec_flags ("subprocess-flags", NULL, NULL,
                        G_TYPE_SUBPROCESS_FLAGS,
                        DEFAULT_FLAGS,
                        (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_diagnostic_tool_init (IdeDiagnosticTool *self)
{
  IdeDiagnosticToolPrivate *priv = ide_diagnostic_tool_get_instance_private (self);

  priv->flags = DEFAULT_FLAGS;
}

static void
ide_diagnostic_tool_communicate_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  g_autoptr(IdeDiagnostics) diagnostics = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autofree char *stdout_buf = NULL;
  g_autofree char *stderr_buf = NULL;
  IdeDiagnosticTool *self;
  DiagnoseState *state;

  IDE_ENTRY;

  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  state = ide_task_get_task_data(task);

  g_assert (IDE_IS_DIAGNOSTIC_TOOL (self));
  g_assert (state != NULL);
  g_assert (!state->file || G_IS_FILE (state->file));
  g_assert (state->file != NULL || state->stdin_bytes != NULL);

  IDE_TRACE_MSG ("Completing diagnose of %s",
                 G_OBJECT_TYPE_NAME (self));

  if (!ide_subprocess_communicate_utf8_finish (subprocess, result, &stdout_buf, &stderr_buf, &error))
    {
      g_assert (error != NULL);
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  diagnostics = ide_diagnostics_new ();

  if (IDE_DIAGNOSTIC_TOOL_GET_CLASS (self)->populate_diagnostics)
    IDE_DIAGNOSTIC_TOOL_GET_CLASS (self)->populate_diagnostics (self, diagnostics, state->file, stdout_buf, stderr_buf);

  ide_task_return_object (task, g_steal_pointer (&diagnostics));

  IDE_EXIT;
}

static void
ide_diagnostic_tool_diagnose_async (IdeDiagnosticProvider *provider,
                                    GFile                 *file,
                                    GBytes                *contents,
                                    const gchar           *lang_id,
                                    GCancellable          *cancellable,
                                    GAsyncReadyCallback    callback,
                                    gpointer               user_data)
{
  IdeDiagnosticTool *self = (IdeDiagnosticTool *)provider;
  IdeDiagnosticToolPrivate *priv = ide_diagnostic_tool_get_instance_private (self);
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeRunContext) run_context = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GError) error = NULL;
  DiagnoseState *state;
  const char *stdin_data;

  IDE_ENTRY;

  g_assert (IDE_IS_DIAGNOSTIC_TOOL (self));
  g_assert (G_IS_FILE (file) || contents != NULL);
  g_assert (!file || G_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_TRACE_MSG ("Diagnosing %s...", G_OBJECT_TYPE_NAME (provider));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_diagnostic_tool_diagnose_async);

  if (!IDE_DIAGNOSTIC_TOOL_GET_CLASS (self)->can_diagnose (self, file, contents, lang_id))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "Not supported");
      IDE_EXIT;
    }

  state = g_slice_new0 (DiagnoseState);
  state->file = file ? g_object_ref (file) : NULL;
  state->stdin_bytes = IDE_DIAGNOSTIC_TOOL_GET_CLASS (self)->get_stdin_bytes (self, file, contents, lang_id);
  ide_task_set_task_data (task, state, diagnose_state_free);

  if (priv->program_name == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "Program name must be set before diagnosing");
      IDE_EXIT;
    }

  run_context = ide_run_context_new ();

  if (!IDE_DIAGNOSTIC_TOOL_GET_CLASS (self)->prepare_run_context (self, run_context, file, contents, lang_id, &error) ||
      !(launcher = ide_run_context_end (run_context, &error)))
    {
      g_assert (error != NULL);

      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  ide_subprocess_launcher_set_flags (launcher, priv->flags);

  if (!(subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, &error)))
    {
      g_assert (error != NULL);

      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  if (state->stdin_bytes != NULL)
    stdin_data = (char *)g_bytes_get_data (state->stdin_bytes, NULL);
  else
    stdin_data = NULL;

  ide_subprocess_communicate_utf8_async (subprocess,
                                         stdin_data,
                                         cancellable,
                                         ide_diagnostic_tool_communicate_cb,
                                         g_steal_pointer (&task));

  IDE_EXIT;
}

static IdeDiagnostics *
ide_diagnostic_tool_diagnose_finish (IdeDiagnosticProvider  *provider,
                                     GAsyncResult           *result,
                                     GError                **error)
{
  IdeDiagnostics *ret;

  IDE_ENTRY;

  g_assert (IDE_IS_DIAGNOSTIC_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_object (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
diagnostic_provider_iface_init (IdeDiagnosticProviderInterface *iface)
{
  iface->diagnose_async = ide_diagnostic_tool_diagnose_async;
  iface->diagnose_finish = ide_diagnostic_tool_diagnose_finish;
}

const char *
ide_diagnostic_tool_get_program_name (IdeDiagnosticTool *self)
{
  IdeDiagnosticToolPrivate *priv = ide_diagnostic_tool_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DIAGNOSTIC_TOOL (self), NULL);

  return priv->program_name;
}

void
ide_diagnostic_tool_set_program_name (IdeDiagnosticTool *self,
                                      const char        *program_name)
{
  IdeDiagnosticToolPrivate *priv = ide_diagnostic_tool_get_instance_private (self);

  g_return_if_fail (IDE_IS_DIAGNOSTIC_TOOL (self));

  if (g_set_str (&priv->program_name, program_name))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PROGRAM_NAME]);
    }
}

const char *
ide_diagnostic_tool_get_bundled_program_path (IdeDiagnosticTool *self)
{
  IdeDiagnosticToolPrivate *priv = ide_diagnostic_tool_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DIAGNOSTIC_TOOL (self), NULL);

  return priv->bundled_program_path;
}

void
ide_diagnostic_tool_set_bundled_program_path (IdeDiagnosticTool *self,
                                              const char        *path)
{
  IdeDiagnosticToolPrivate *priv = ide_diagnostic_tool_get_instance_private (self);

  g_return_if_fail (IDE_IS_DIAGNOSTIC_TOOL (self));

  if (g_set_str (&priv->bundled_program_path, path))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BUNDLED_PROGRAM_PATH]);
    }
}

const char *
ide_diagnostic_tool_get_local_program_path (IdeDiagnosticTool *self)
{
  IdeDiagnosticToolPrivate *priv = ide_diagnostic_tool_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DIAGNOSTIC_TOOL (self), NULL);

  return priv->local_program_path;
}

void
ide_diagnostic_tool_set_local_program_path (IdeDiagnosticTool *self,
                                            const char        *path)
{
  IdeDiagnosticToolPrivate *priv = ide_diagnostic_tool_get_instance_private (self);

  g_return_if_fail (IDE_IS_DIAGNOSTIC_TOOL (self));

  if (g_set_str (&priv->local_program_path, path))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LOCAL_PROGRAM_PATH]);
    }
}

GSubprocessFlags
ide_diagnostic_tool_get_subprocess_flags (IdeDiagnosticTool *self)
{
  IdeDiagnosticToolPrivate *priv = ide_diagnostic_tool_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DIAGNOSTIC_TOOL (self), 0);

  return priv->flags;
}

void
ide_diagnostic_tool_set_subprocess_flags (IdeDiagnosticTool *self,
                                          GSubprocessFlags   subprocess_flags)
{
  IdeDiagnosticToolPrivate *priv = ide_diagnostic_tool_get_instance_private (self);

  g_return_if_fail (IDE_IS_DIAGNOSTIC_TOOL (self));

  if (priv->flags != subprocess_flags)
    {
      priv->flags = subprocess_flags;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SUBPROCESS_FLAGS]);
    }
}
