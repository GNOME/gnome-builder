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
#include "ide-runtime.h"
#include "ide-runtime-manager.h"

typedef struct
{
  char *program_name;
} IdeDiagnosticToolPrivate;

static void diagnostic_provider_iface_init (IdeDiagnosticProviderInterface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (IdeDiagnosticTool, ide_diagnostic_tool, IDE_TYPE_OBJECT,
                                  G_ADD_PRIVATE (IdeDiagnosticTool)
                                  G_IMPLEMENT_INTERFACE (IDE_TYPE_DIAGNOSTIC_PROVIDER,
                                                         diagnostic_provider_iface_init))

enum {
  PROP_0,
  PROP_PROGRAM_NAME,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static GBytes *
ide_diagnostic_tool_real_get_stdin_bytes (IdeDiagnosticTool *self,
                                          GFile             *file,
                                          GBytes            *contents,
                                          const char        *language_id)
{
  if (contents != NULL)
    return g_bytes_ref (contents);
  return NULL;
}

static void
ide_diagnostic_tool_real_configure_launcher (IdeDiagnosticTool     *self,
                                             IdeSubprocessLauncher *launcher)
{
  g_assert (IDE_IS_DIAGNOSTIC_TOOL (self));
  g_assert (IDE_IS_SUBPROCESS_LAUNCHER (launcher));
}

static IdeSubprocessLauncher *
ide_diagnostic_tool_real_create_launcher (IdeDiagnosticTool  *self,
                                          const char         *program_name,
                                          GError            **error)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(GFile) workdir = NULL;
  const char *srcdir = NULL;
  IdeRuntimeManager *runtime_manager;
  IdeRuntime *host;

  IDE_ENTRY;

  g_assert (IDE_IS_DIAGNOSTIC_TOOL (self));
  g_assert (program_name != NULL);

  if (!(context = ide_object_ref_context (IDE_OBJECT (self))))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_CANCELLED,
                   "Context lost, cancelling request");
      IDE_RETURN (NULL);
    }

  workdir = ide_context_ref_workdir (context);
  srcdir = g_file_peek_path (workdir);

  if (ide_context_has_project (context))
    {
      IdeBuildManager *build_manager = ide_build_manager_from_context (context);
      IdePipeline *pipeline = ide_build_manager_get_pipeline (build_manager);

      if (pipeline != NULL)
        {
          srcdir = ide_pipeline_get_srcdir (pipeline);

          if (ide_pipeline_contains_program_in_path (pipeline, program_name, NULL))
            {
              if ((launcher = ide_pipeline_create_launcher (pipeline, NULL)))
                goto setup_launcher;
            }
        }

      /* Now try on the host using the "host" runtime which can do
       * a better job of discovering the program on the host and
       * take into account if the user has something modifying the
       * shell like .bashrc.
       */
      runtime_manager = ide_runtime_manager_from_context (context);
      host = ide_runtime_manager_get_runtime (runtime_manager, "host");
      if (ide_runtime_contains_program_in_path (host, program_name, NULL))
        {
          launcher = ide_runtime_create_launcher (host, NULL);
          goto setup_launcher;
        }
    }

  /* See if Builder itself has bundled the program */
  if (g_find_program_in_path (program_name))
    {
      launcher = ide_subprocess_launcher_new (0);
      goto setup_launcher;
    }

  g_set_error (error,
               G_IO_ERROR,
               G_IO_ERROR_NOT_FOUND,
               "Failed to locate program \"%s\"",
               program_name);

  IDE_RETURN (NULL);

setup_launcher:
  ide_subprocess_launcher_push_argv (launcher, program_name);
  ide_subprocess_launcher_set_cwd (launcher, srcdir);
  ide_subprocess_launcher_set_flags (launcher,
                                     (G_SUBPROCESS_FLAGS_STDIN_PIPE |
                                      G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                      G_SUBPROCESS_FLAGS_STDERR_PIPE));

  IDE_DIAGNOSTIC_TOOL_GET_CLASS (self)->configure_launcher (self, launcher);

  IDE_RETURN (g_steal_pointer (&launcher));
}

static void
ide_diagnostic_tool_constructed (GObject *object)
{
  IdeDiagnosticTool *self = (IdeDiagnosticTool *)object;

  G_OBJECT_CLASS (ide_diagnostic_tool_parent_class)->constructed (object);

  if (IDE_DIAGNOSTIC_TOOL_GET_CLASS (self)->populate_diagnostics)
    g_critical ("%s inherits from IdeDiagnosticTool but does not implement populate_diagnostics(). This will not work.",
                G_OBJECT_TYPE_NAME (self));
}

static void
ide_diagnostic_tool_finalize (GObject *object)
{
  IdeDiagnosticTool *self = (IdeDiagnosticTool *)object;
  IdeDiagnosticToolPrivate *priv = ide_diagnostic_tool_get_instance_private (self);

  g_clear_pointer (&priv->program_name, g_free);

  G_OBJECT_CLASS (ide_diagnostic_tool_parent_class)->finalize (object);
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

  klass->create_launcher = ide_diagnostic_tool_real_create_launcher;
  klass->configure_launcher = ide_diagnostic_tool_real_configure_launcher;
  klass->get_stdin_bytes = ide_diagnostic_tool_real_get_stdin_bytes;

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

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_diagnostic_tool_init (IdeDiagnosticTool *self)
{
}

static IdeSubprocessLauncher *
ide_diagnostic_tool_create_launcher (IdeDiagnosticTool  *self,
                                     const char         *program_name,
                                     GError            **error)
{
  g_assert (IDE_IS_DIAGNOSTIC_TOOL (self));
  g_assert (program_name != NULL);

  return IDE_DIAGNOSTIC_TOOL_GET_CLASS (self)->create_launcher (self, program_name, error);
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
  GFile *file;

  IDE_ENTRY;

  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_subprocess_communicate_utf8_finish (subprocess, result, &stdout_buf, &stderr_buf, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  self = ide_task_get_source_object (task);
  diagnostics = ide_diagnostics_new ();
  file = ide_task_get_task_data(task);

  g_assert (!file || G_IS_FILE (file));

  if (IDE_DIAGNOSTIC_TOOL_GET_CLASS (self)->populate_diagnostics != NULL)
    IDE_DIAGNOSTIC_TOOL_GET_CLASS (self)->populate_diagnostics (self, diagnostics, file, stdout_buf, stderr_buf);

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
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GBytes) stdin_bytes = NULL;
  const char *stdin_data = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_DIAGNOSTIC_TOOL (self));
  g_assert (G_IS_FILE (file) || contents != NULL);
  g_assert (!file || G_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_diagnostic_tool_diagnose_async);

  if (priv->program_name == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "Program name must be set before diagnosing");
      IDE_EXIT;
    }

  if (!(launcher = ide_diagnostic_tool_create_launcher (self, priv->program_name, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  if (!(subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  if ((stdin_bytes = IDE_DIAGNOSTIC_TOOL_GET_CLASS (self)->get_stdin_bytes (self, file, contents, lang_id)))
    stdin_data = g_bytes_get_data (stdin_bytes, NULL);

  ide_task_set_task_data (task, g_object_ref(file), g_object_unref);

  ide_subprocess_communicate_utf8_async (subprocess,
                                         stdin_data,
                                         cancellable,
                                         ide_diagnostic_tool_communicate_cb,
                                         g_object_ref (task));

  IDE_EXIT;
}

static IdeDiagnostics *
ide_diagnostic_tool_diagnose_finish (IdeDiagnosticProvider  *provider,
                                     GAsyncResult           *result,
                                     GError                **error)
{
  g_assert (IDE_IS_DIAGNOSTIC_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_object (IDE_TASK (result), error);
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

  if (g_strcmp0 (program_name, priv->program_name) != 0)
    {
      g_free (priv->program_name);
      priv->program_name = g_strdup (program_name);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PROGRAM_NAME]);
    }
}
