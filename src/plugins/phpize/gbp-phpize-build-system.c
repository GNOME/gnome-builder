/* gbp-phpize-build-system.c
 *
 * Copyright 2016-2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-phpize-build-system"

#include "config.h"

#include <libide-foundry.h>
#include <libide-io.h>

#include "gbp-phpize-build-system.h"

#define BUILD_FLAGS_STDIN_BUF "\n\
include Makefile\n\
\n\
print-%: ; @echo $* = $($*)\n\
"

struct _GbpPhpizeBuildSystem
{
  IdeObject  parent_instance;
  GFile     *project_file;
};

enum {
  PROP_0,
  PROP_PROJECT_FILE,
  N_PROPS
};

typedef enum {
  FILE_TYPE_UNKNOWN,
  FILE_TYPE_C,
  FILE_TYPE_CPP,
} FileType;

static guint
get_file_type (const char *path)
{
  static const char * const c_suffix[] = IDE_STRV_INIT (".c", ".h");
  static const char * const cpp_suffix[] = IDE_STRV_INIT (".cpp", ".c++", ".cxx", ".cc", ".hpp", ".h++", ".hxx", ".hh");
  const char *suffix = strrchr (path, '.');

  if (suffix == NULL)
    return FILE_TYPE_UNKNOWN;

  if (g_strv_contains (c_suffix, suffix))
    return FILE_TYPE_C;

  if (g_strv_contains (cpp_suffix, suffix))
    return FILE_TYPE_CPP;

  return FILE_TYPE_UNKNOWN;
}

static char *
gbp_phpize_build_system_get_id (IdeBuildSystem *build_system)
{
  return g_strdup ("phpize");
}

static char *
gbp_phpize_build_system_get_display_name (IdeBuildSystem *build_system)
{
  return g_strdup ("PHP Build System");
}

static int
gbp_phpize_build_system_get_priority (IdeBuildSystem *build_system)
{
  return 3000;
}

static void
gbp_phpize_build_system_communicate_cb (GObject      *object,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autofree char *stdout_buf = NULL;
  g_autoptr(GString) str = NULL;
  g_auto(GStrv) argv = NULL;
  IdeLineReader reader;
  const char *key = NULL;
  FileType file_type;
  char *line;
  gsize line_len;
  int argc;

  IDE_ENTRY;

  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  file_type = GPOINTER_TO_INT (ide_task_get_task_data (task));
  g_assert (file_type != FILE_TYPE_UNKNOWN);

  if (!ide_subprocess_communicate_utf8_finish (subprocess, result, &stdout_buf, NULL, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  switch (file_type)
    {
    case FILE_TYPE_C: key = "CFLAGS="; break;
    case FILE_TYPE_CPP: key = "CPPFLAGS="; break;
    case FILE_TYPE_UNKNOWN:
    default:
      g_assert_not_reached ();
    }

  str = g_string_new (NULL);

  ide_line_reader_init (&reader, stdout_buf, -1);
  while ((line = ide_line_reader_next (&reader, &line_len)))
    {
      line[line_len] = 0;

      if (g_str_has_prefix (line, "INCLUDES="))
        {
          g_string_append (str, &line[strlen ("INCLUDES=")]);
          g_string_append_c (str, ' ');
          continue;
        }

      if (g_str_has_prefix (line, key))
        {
          g_string_append (str, &line[strlen (key)]);
          g_string_append_c (str, ' ');
          continue;
        }
    }

  if (str->len == 0)
    {
      ide_task_return_unsupported_error (task);
      IDE_EXIT;
    }

  if (!g_shell_parse_argv (str->str, &argc, &argv, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_pointer (task, g_steal_pointer (&argv), g_strfreev);

  IDE_EXIT;
}

static void
gbp_phpize_build_system_get_build_flags_async (IdeBuildSystem      *build_system,
                                               GFile               *file,
                                               GCancellable        *cancellable,
                                               GAsyncReadyCallback  callback,
                                               gpointer             user_data)
{
  GbpPhpizeBuildSystem *self = (GbpPhpizeBuildSystem *)build_system;
  g_autoptr(IdeRunContext) run_context = NULL;
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GError) error = NULL;
  IdeBuildManager *build_manager;
  IdePipeline *pipeline;
  IdeContext *context;
  FileType file_type;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_PHPIZE_BUILD_SYSTEM (self));
  g_assert (!file || G_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_phpize_build_system_get_build_flags_async);

  if (!(file_type = get_file_type (g_file_peek_path (file))))
    {
      ide_task_return_unsupported_error (task);
      IDE_EXIT;
    }

  ide_task_set_task_data (task, GINT_TO_POINTER (file_type), (GDestroyNotify)NULL);

  /* To get the build flags, we run make with some custom code to
   * print variables, and then extract the values based on the file type.
   * But the pipeline must be configured for us to do that. If not, just
   * bail as most things don't want to force a build directly.
   */
  context = ide_object_get_context (IDE_OBJECT (self));
  build_manager = ide_build_manager_from_context (context);
  pipeline = ide_build_manager_get_pipeline (build_manager);

  if (pipeline == NULL ||
      !ide_pipeline_is_ready (pipeline) ||
      ide_pipeline_get_phase (pipeline) < IDE_PIPELINE_PHASE_CONFIGURE)
    {
      g_debug ("Pipeline not ready, cannot extract build flags");
      ide_task_return_unsupported_error (task);
      IDE_EXIT;
    }

  run_context = ide_run_context_new ();
  ide_pipeline_prepare_run_context (pipeline, run_context);
  ide_run_context_append_args (run_context, IDE_STRV_INIT ("make", "-f", "-", "print-CFLAGS", "print-CXXFLAGS", "print-INCLUDES"));
  ide_run_context_setenv (run_context, "V", "0");

  if (!(launcher = ide_run_context_end (run_context, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  ide_subprocess_launcher_set_flags (launcher,
                                     (G_SUBPROCESS_FLAGS_STDIN_PIPE |
                                      G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                      G_SUBPROCESS_FLAGS_STDERR_SILENCE));

  if (!(subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  ide_subprocess_communicate_utf8_async (subprocess,
                                         BUILD_FLAGS_STDIN_BUF,
                                         cancellable,
                                         gbp_phpize_build_system_communicate_cb,
                                         g_steal_pointer (&task));

  IDE_EXIT;
}

static char **
gbp_phpize_build_system_get_build_flags_finish (IdeBuildSystem  *build_system,
                                                GAsyncResult    *result,
                                                GError         **error)
{
  char **ret;

  IDE_ENTRY;

  g_assert (GBP_IS_PHPIZE_BUILD_SYSTEM (build_system));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
build_system_iface_init (IdeBuildSystemInterface *iface)
{
  iface->get_id = gbp_phpize_build_system_get_id;
  iface->get_display_name = gbp_phpize_build_system_get_display_name;
  iface->get_priority = gbp_phpize_build_system_get_priority;
  iface->get_build_flags_async = gbp_phpize_build_system_get_build_flags_async;
  iface->get_build_flags_finish = gbp_phpize_build_system_get_build_flags_finish;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpPhpizeBuildSystem, gbp_phpize_build_system, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_BUILD_SYSTEM, build_system_iface_init))

static GParamSpec *properties [N_PROPS];

static void
gbp_phpize_build_system_destroy (IdeObject *object)
{
  GbpPhpizeBuildSystem *self = (GbpPhpizeBuildSystem *)object;

  g_clear_object (&self->project_file);

  IDE_OBJECT_CLASS (gbp_phpize_build_system_parent_class)->destroy (object);
}

static void
gbp_phpize_build_system_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GbpPhpizeBuildSystem *self = GBP_PHPIZE_BUILD_SYSTEM (object);

  switch (prop_id)
    {
    case PROP_PROJECT_FILE:
      g_value_set_object (value, self->project_file);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_phpize_build_system_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GbpPhpizeBuildSystem *self = GBP_PHPIZE_BUILD_SYSTEM (object);

  switch (prop_id)
    {
    case PROP_PROJECT_FILE:
      g_set_object (&self->project_file, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_phpize_build_system_class_init (GbpPhpizeBuildSystemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  object_class->get_property = gbp_phpize_build_system_get_property;
  object_class->set_property = gbp_phpize_build_system_set_property;

  i_object_class->destroy = gbp_phpize_build_system_destroy;

  properties [PROP_PROJECT_FILE] =
    g_param_spec_object ("project-file",
                         "Project File",
                         "The project file (Phpize.toml)",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_phpize_build_system_init (GbpPhpizeBuildSystem *self)
{
}
