/* ide-autotools-build-task.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <fcntl.h>
#include <glib/gi18n.h>
#include <ide.h>
#include <stdlib.h>
#include <unistd.h>

#include "ide-autotools-build-task.h"

struct _IdeAutotoolsBuildTask
{
  IdeObject         parent_instance;
  IdeConfiguration *configuration;
  GFile            *directory;
  GPtrArray        *extra_targets;
  guint             require_autogen : 1;
  guint             require_configure : 1;
  guint             executed : 1;
};

typedef struct
{
  gchar       *directory_path;
  gchar       *project_path;
  gchar       *parallel;
  gchar       *system_type;
  gchar      **configure_argv;
  gchar      **make_targets;
  IdeRuntime  *runtime;
  guint        require_autogen : 1;
  guint        require_configure : 1;
  guint        bootstrap_only : 1;
} WorkerState;

typedef gboolean (*WorkStep) (GTask                 *task,
                              IdeAutotoolsBuildTask *self,
                              WorkerState           *state,
                              GCancellable          *cancellable);

G_DEFINE_TYPE (IdeAutotoolsBuildTask, ide_autotools_build_task, IDE_TYPE_BUILD_RESULT)

enum {
  PROP_0,
  PROP_CONFIGURATION,
  PROP_DIRECTORY,
  PROP_REQUIRE_AUTOGEN,
  PROP_REQUIRE_CONFIGURE,
  LAST_PROP
};

static GSubprocess *log_and_spawn     (IdeAutotoolsBuildTask  *self,
                                       IdeSubprocessLauncher  *launcher,
                                       GError                **error,
                                       const gchar            *argv0,
                                       ...) G_GNUC_NULL_TERMINATED;
static gboolean     step_mkdirs       (GTask                  *task,
                                       IdeAutotoolsBuildTask  *self,
                                       WorkerState            *state,
                                       GCancellable           *cancellable);
static gboolean     step_autogen      (GTask                  *task,
                                       IdeAutotoolsBuildTask  *self,
                                       WorkerState            *state,
                                       GCancellable           *cancellable);
static gboolean     step_configure    (GTask                  *task,
                                       IdeAutotoolsBuildTask  *self,
                                       WorkerState            *state,
                                       GCancellable           *cancellable);
static gboolean     step_make_all     (GTask                  *task,
                                       IdeAutotoolsBuildTask  *self,
                                       WorkerState            *state,
                                       GCancellable           *cancellable);
static void         apply_environment (IdeAutotoolsBuildTask  *self,
                                       IdeSubprocessLauncher  *launcher);

static GParamSpec *properties [LAST_PROP];
static WorkStep workSteps [] = {
  step_mkdirs,
  step_autogen,
  step_configure,
  step_make_all,
  NULL
};

gboolean
ide_autotools_build_task_get_require_autogen (IdeAutotoolsBuildTask *self)
{
  g_return_val_if_fail (IDE_IS_AUTOTOOLS_BUILD_TASK (self), FALSE);

  return self->require_autogen;
}

static void
ide_autotools_build_task_set_require_autogen (IdeAutotoolsBuildTask *self,
                                              gboolean               require_autogen)
{
  g_return_if_fail (IDE_IS_AUTOTOOLS_BUILD_TASK (self));

  self->require_autogen = !!require_autogen;
}

gboolean
ide_autotools_build_task_get_require_configure (IdeAutotoolsBuildTask *self)
{
  g_return_val_if_fail (IDE_IS_AUTOTOOLS_BUILD_TASK (self), FALSE);

  return self->require_configure;
}

static void
ide_autotools_build_task_set_require_configure (IdeAutotoolsBuildTask *self,
                                                gboolean               require_configure)
{
  g_return_if_fail (IDE_IS_AUTOTOOLS_BUILD_TASK (self));

  self->require_autogen = !!require_configure;
}

/**
 * ide_autotools_build_task_get_directory:
 *
 * Fetches the build directory that was used.
 *
 * Returns: (transfer none): A #GFile.
 */
GFile *
ide_autotools_build_task_get_directory (IdeAutotoolsBuildTask *self)
{
  g_return_val_if_fail (IDE_IS_AUTOTOOLS_BUILD_TASK (self), NULL);

  return self->directory;
}

static void
ide_autotools_build_task_set_directory (IdeAutotoolsBuildTask *self,
                                        GFile                 *directory)
{
  g_return_if_fail (IDE_IS_AUTOTOOLS_BUILD_TASK (self));
  g_return_if_fail (!directory || G_IS_FILE (directory));

  /*
   * We require a build directory that is accessable via a native path.
   */
  if (directory)
    {
      g_autofree gchar *path = NULL;

      path = g_file_get_path (directory);

      if (!path)
        {
          g_warning (_("Directory must be on a locally mounted filesystem."));
          return;
        }
    }

  if (self->directory != directory)
    if (g_set_object (&self->directory, directory))
      g_object_notify_by_pspec (G_OBJECT (self),
                                properties [PROP_DIRECTORY]);
}

/**
 * ide_autotools_build_task_get_configuration:
 * @self: An #IdeAutotoolsBuildTask
 *
 * Gets the configuration to use for the build.
 */
IdeConfiguration *
ide_autotools_build_task_get_configuration (IdeAutotoolsBuildTask *self)
{
  g_return_val_if_fail (IDE_IS_AUTOTOOLS_BUILD_TASK (self), NULL);

  return self->configuration;
}

static void
ide_autotools_build_task_set_configuration (IdeAutotoolsBuildTask *self,
                                            IdeConfiguration      *configuration)
{
  g_assert (IDE_IS_AUTOTOOLS_BUILD_TASK (self));
  g_assert (IDE_IS_CONFIGURATION (configuration));

  if (g_set_object (&self->configuration, configuration))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CONFIGURATION]);
}

static void
ide_autotools_build_task_finalize (GObject *object)
{
  IdeAutotoolsBuildTask *self = (IdeAutotoolsBuildTask *)object;

  g_clear_object (&self->directory);
  g_clear_object (&self->configuration);

  G_OBJECT_CLASS (ide_autotools_build_task_parent_class)->finalize (object);
}

static void
ide_autotools_build_task_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  IdeAutotoolsBuildTask *self = IDE_AUTOTOOLS_BUILD_TASK (object);

  switch (prop_id)
    {
    case PROP_CONFIGURATION:
      g_value_set_object (value, ide_autotools_build_task_get_configuration (self));
      break;

    case PROP_DIRECTORY:
      g_value_set_object (value, ide_autotools_build_task_get_directory (self));
      break;

    case PROP_REQUIRE_AUTOGEN:
      g_value_set_boolean (value, ide_autotools_build_task_get_require_autogen (self));
      break;

    case PROP_REQUIRE_CONFIGURE:
      g_value_set_boolean (value, ide_autotools_build_task_get_require_configure (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_autotools_build_task_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  IdeAutotoolsBuildTask *self = IDE_AUTOTOOLS_BUILD_TASK (object);

  switch (prop_id)
    {
    case PROP_CONFIGURATION:
      ide_autotools_build_task_set_configuration (self, g_value_get_object (value));
      break;

    case PROP_DIRECTORY:
      ide_autotools_build_task_set_directory (self, g_value_get_object (value));
      break;

    case PROP_REQUIRE_AUTOGEN:
      ide_autotools_build_task_set_require_autogen (self, g_value_get_boolean (value));
      break;

    case PROP_REQUIRE_CONFIGURE:
      ide_autotools_build_task_set_require_configure (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_autotools_build_task_class_init (IdeAutotoolsBuildTaskClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_autotools_build_task_finalize;
  object_class->get_property = ide_autotools_build_task_get_property;
  object_class->set_property = ide_autotools_build_task_set_property;

  properties [PROP_CONFIGURATION] =
    g_param_spec_object ("configuration",
                        "Configuration",
                        "The configuration for this build.",
                        IDE_TYPE_CONFIGURATION,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS));

  properties [PROP_DIRECTORY] =
    g_param_spec_object ("directory",
                         "Directory",
                         "The directory to perform the build within.",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_REQUIRE_AUTOGEN] =
    g_param_spec_boolean ("require-autogen",
                          "Require Autogen",
                          "If autogen.sh should be forced to execute.",
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_STATIC_STRINGS));

  properties [PROP_REQUIRE_CONFIGURE] =
    g_param_spec_boolean ("require-configure",
                          "Require Configure",
                          "If configure should be forced to execute.",
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
ide_autotools_build_task_init (IdeAutotoolsBuildTask *self)
{
}

static gchar **
gen_configure_argv (IdeAutotoolsBuildTask *self,
                    WorkerState           *state)
{
  IdeDevice *device;
  GPtrArray *ar;
  const gchar *opts;
  const gchar *system_type;
  gchar *prefix;
  gchar *configure_path;

  g_assert (IDE_IS_AUTOTOOLS_BUILD_TASK (self));
  g_assert (state != NULL);

  ar = g_ptr_array_new_with_free_func (g_free);

  /* ./configure */
  configure_path = g_build_filename (state->project_path, "configure", NULL);
  g_ptr_array_add (ar, configure_path);

  /* --prefix /app */
  if (NULL == (prefix = g_strdup (ide_configuration_get_prefix (self->configuration))))
    prefix = g_build_filename (state->project_path, "_install", NULL);
  g_ptr_array_add (ar, g_strdup_printf ("--prefix=%s", prefix));
  g_free (prefix);

  /* --host=triplet */
  device = ide_configuration_get_device (self->configuration);
  system_type = ide_device_get_system_type (device);
  g_ptr_array_add (ar, g_strdup_printf ("--host=%s", system_type));

  if (NULL != (opts = ide_configuration_get_config_opts (self->configuration)))
    {
      GError *error = NULL;
      gint argc;
      gchar **argv;

      if (g_shell_parse_argv (opts, &argc, &argv, &error))
        {
          for (guint i = 0; i < argc; i++)
            g_ptr_array_add (ar, argv [i]);
          g_free (argv);
        }
      else
        {
          g_warning ("%s", error->message);
          g_clear_error (&error);
        }
    }

  g_ptr_array_add (ar, NULL);

  return (gchar **)g_ptr_array_free (ar, FALSE);
}

static WorkerState *
worker_state_new (IdeAutotoolsBuildTask *self,
                  IdeBuilderBuildFlags   flags)
{
  g_autofree gchar *name = NULL;
  IdeContext *context;
  IdeDevice *device;
  IdeRuntime *runtime;
  GPtrArray *make_targets;
  GFile *project_dir;
  GFile *project_file;
  WorkerState *state;
  gint val32;

  g_return_val_if_fail (IDE_IS_AUTOTOOLS_BUILD_TASK (self), NULL);
  g_return_val_if_fail (IDE_IS_CONFIGURATION (self->configuration), NULL);

  context = ide_object_get_context (IDE_OBJECT (self));
  project_file = ide_context_get_project_file (context);

  device = ide_configuration_get_device (self->configuration);
  runtime = ide_configuration_get_runtime (self->configuration);

  name = g_file_get_basename (project_file);

  if (g_str_has_prefix (name, "configure."))
    project_dir = g_file_get_parent (project_file);
  else
    project_dir = g_object_ref (project_file);

  state = g_slice_new0 (WorkerState);
  state->require_autogen = self->require_autogen || !!(flags & IDE_BUILDER_BUILD_FLAGS_FORCE_BOOTSTRAP);
  state->require_configure = self->require_configure || (state->require_autogen && !(flags & IDE_BUILDER_BUILD_FLAGS_NO_CONFIGURE));
  state->directory_path = g_file_get_path (self->directory);
  state->project_path = g_file_get_path (project_dir);
  state->system_type = g_strdup (ide_device_get_system_type (device));
  state->runtime = g_object_ref (runtime);

  val32 = atoi (ide_configuration_getenv (self->configuration, "PARALLEL") ?: "-1");

  if (val32 == -1)
    state->parallel = g_strdup_printf ("-j%u", g_get_num_processors () + 1);
  else if (val32 == 0)
    state->parallel = g_strdup_printf ("-j%u", g_get_num_processors ());
  else
    state->parallel = g_strdup_printf ("-j%u", val32);

  make_targets = g_ptr_array_new ();

  if (0 != (flags & IDE_BUILDER_BUILD_FLAGS_FORCE_CLEAN))
    {
      state->require_autogen = TRUE;
      state->require_configure = TRUE;
      g_ptr_array_add (make_targets, g_strdup ("clean"));
    }

  if (0 == (flags & IDE_BUILDER_BUILD_FLAGS_NO_BUILD))
    g_ptr_array_add (make_targets, g_strdup ("all"));

  g_ptr_array_add (make_targets, NULL);

  state->make_targets = (gchar **)g_ptr_array_free (make_targets, FALSE);

  if (0 != (flags & IDE_BUILDER_BUILD_FLAGS_NO_CONFIGURE))
    {
      state->require_autogen = TRUE;
      state->require_configure = TRUE;
      state->bootstrap_only = TRUE;
      g_clear_pointer (&state->make_targets, (GDestroyNotify)g_strfreev);
    }

  state->configure_argv = gen_configure_argv (self, state);

  return state;
}

static void
worker_state_free (void *data)
{
  WorkerState *state = data;

  g_free (state->directory_path);
  g_free (state->project_path);
  g_free (state->system_type);
  g_free (state->parallel);
  g_strfreev (state->configure_argv);
  g_strfreev (state->make_targets);
  g_clear_object (&state->runtime);
  g_slice_free (WorkerState, state);
}

static void
ide_autotools_build_task_execute_worker (GTask        *task,
                                         gpointer      source_object,
                                         gpointer      task_data,
                                         GCancellable *cancellable)
{
  IdeAutotoolsBuildTask *self = source_object;
  WorkerState *state = task_data;
  guint i;

  g_return_if_fail (G_IS_TASK (task));
  g_return_if_fail (IDE_IS_AUTOTOOLS_BUILD_TASK (self));
  g_return_if_fail (state);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  for (i = 0; workSteps [i]; i++)
    {
      if (g_cancellable_is_cancelled (cancellable) ||
          !workSteps [i] (task, self, state, cancellable))
        return;
    }

  g_task_return_boolean (task, TRUE);
}

static void
ide_autotools_build_task_prebuild_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  IdeRuntime *runtime = (IdeRuntime *)object;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;

  g_assert (IDE_IS_RUNTIME (runtime));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!ide_runtime_prebuild_finish (runtime, result, &error))
    {
      g_task_return_error (task, error);
      return;
    }

  g_task_run_in_thread (task, ide_autotools_build_task_execute_worker);
}

void
ide_autotools_build_task_execute_async (IdeAutotoolsBuildTask *self,
                                        IdeBuilderBuildFlags   flags,
                                        GCancellable          *cancellable,
                                        GAsyncReadyCallback    callback,
                                        gpointer               user_data)
{
  g_autoptr(GTask) task = NULL;
  WorkerState *state;

  g_return_if_fail (IDE_IS_AUTOTOOLS_BUILD_TASK (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (self->executed)
    {
      g_task_report_new_error (self, callback, user_data,
                               ide_autotools_build_task_execute_async,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               _("Cannot execute build task more than once."));
      return;
    }

  self->executed = TRUE;

  state = worker_state_new (self, flags);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_task_data (task, state, worker_state_free);

  /*
   * Execute the pre-hook for the runtime before we start building.
   */
  ide_runtime_prebuild_async (state->runtime,
                              cancellable,
                              ide_autotools_build_task_prebuild_cb,
                              g_object_ref (task));
}

gboolean
ide_autotools_build_task_execute_finish (IdeAutotoolsBuildTask  *self,
                                         GAsyncResult           *result,
                                         GError                **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (IDE_IS_AUTOTOOLS_BUILD_TASK (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);
  g_return_val_if_fail (G_IS_TASK (task), FALSE);

  return g_task_propagate_boolean (task, error);
}

static GSubprocess *
log_and_spawn (IdeAutotoolsBuildTask  *self,
               IdeSubprocessLauncher  *launcher,
               GError                **error,
               const gchar           *argv0,
               ...)
{
  GSubprocess *ret;
  GString *log;
  gchar *item;
  va_list args;

  log = g_string_new (argv0);
  ide_subprocess_launcher_push_argv (launcher, argv0);

  va_start (args, argv0);
  while (NULL != (item = va_arg (args, gchar *)))
    {
      ide_subprocess_launcher_push_argv (launcher, item);
      g_string_append_printf (log, " '%s'", item);
    }
  va_end (args);

  ide_build_result_log_stdout (IDE_BUILD_RESULT (self), "%s", log->str);
  ret = ide_subprocess_launcher_spawn_sync (launcher, NULL, error);
  g_string_free (log, TRUE);

  return ret;
}

static gboolean
step_mkdirs (GTask                 *task,
             IdeAutotoolsBuildTask *self,
             WorkerState           *state,
             GCancellable          *cancellable)
{
  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_AUTOTOOLS_BUILD_TASK (self));
  g_assert (state);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (!g_file_test (state->directory_path, G_FILE_TEST_EXISTS))
    {
      if (g_mkdir_with_parents (state->directory_path, 0750) != 0)
        {
          g_task_return_new_error (task,
                                   G_IO_ERROR,
                                   G_IO_ERROR_FAILED,
                                   _("Failed to create build directory."));
          return FALSE;
        }
    }
  else if (!g_file_test (state->directory_path, G_FILE_TEST_IS_DIR))
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_DIRECTORY,
                               _("'%s' is not a directory."),
                               state->directory_path);
      return FALSE;
    }

  return TRUE;
}

static gboolean
step_autogen (GTask                 *task,
              IdeAutotoolsBuildTask *self,
              WorkerState           *state,
              GCancellable          *cancellable)
{
  g_autofree gchar *autogen_sh_path = NULL;
  g_autofree gchar *configure_path = NULL;
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(GSubprocess) process = NULL;
  GError *error = NULL;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_AUTOTOOLS_BUILD_TASK (self));
  g_assert (state);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  configure_path = g_build_filename (state->project_path, "configure", NULL);

  if (!state->require_autogen)
    {
      if (g_file_test (configure_path, G_FILE_TEST_IS_REGULAR))
        return TRUE;
    }

  autogen_sh_path = g_build_filename (state->project_path, "autogen.sh", NULL);
  if (!g_file_test (autogen_sh_path, G_FILE_TEST_EXISTS))
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               _("autogen.sh is missing from project directory (%s)."),
                               state->project_path);
      return FALSE;
    }

  if (!g_file_test (autogen_sh_path, G_FILE_TEST_IS_EXECUTABLE))
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               _("autogen.sh is not executable."));
      return FALSE;
    }

  ide_build_result_set_mode (IDE_BUILD_RESULT (self), _("Running autogen…"));

  if (NULL == (launcher = ide_runtime_create_launcher (state->runtime, &error)))
    {
      g_task_return_error (task, error);
      return FALSE;
    }

  ide_subprocess_launcher_set_cwd (launcher, state->project_path);
  ide_subprocess_launcher_setenv (launcher, "LANG", "C", TRUE);
  ide_subprocess_launcher_setenv (launcher, "NOCONFIGURE", "1", TRUE);
  apply_environment (self, launcher);

  process = log_and_spawn (self, launcher, &error, autogen_sh_path, NULL);

  if (!process)
    {
      g_task_return_error (task, error);
      return FALSE;
    }

  ide_build_result_log_subprocess (IDE_BUILD_RESULT (self), process);

  if (!g_subprocess_wait_check (process, cancellable, &error))
    {
      g_task_return_error (task, error);
      return FALSE;
    }

  if (!g_file_test (configure_path, G_FILE_TEST_IS_EXECUTABLE))
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               _("autogen.sh failed to create configure (%s)"),
                               configure_path);
      return FALSE;
    }

  return TRUE;
}

static gboolean
step_configure (GTask                 *task,
                IdeAutotoolsBuildTask *self,
                WorkerState           *state,
                GCancellable          *cancellable)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(GSubprocess) process = NULL;
  g_autofree gchar *makefile_path = NULL;
  g_autofree gchar *config_log = NULL;
  GError *error = NULL;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_AUTOTOOLS_BUILD_TASK (self));
  g_assert (state);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (!state->require_configure)
    {
      /*
       * Skip configure if we already have a makefile.
       */
      makefile_path = g_build_filename (state->directory_path, "Makefile", NULL);
      if (g_file_test (makefile_path, G_FILE_TEST_EXISTS))
        return TRUE;
    }

  ide_build_result_set_mode (IDE_BUILD_RESULT (self), _("Running configure…"));

  if (NULL == (launcher = ide_runtime_create_launcher (state->runtime, &error)))
    return FALSE;

  ide_subprocess_launcher_set_flags (launcher,
                                     (G_SUBPROCESS_FLAGS_STDERR_PIPE |
                                      G_SUBPROCESS_FLAGS_STDOUT_PIPE));
  ide_subprocess_launcher_set_cwd (launcher, state->directory_path);
  ide_subprocess_launcher_setenv (launcher, "LANG", "C", TRUE);
  apply_environment (self, launcher);

  config_log = g_strjoinv (" ", state->configure_argv);
  ide_build_result_log_stdout (IDE_BUILD_RESULT (self), "%s", config_log);
  ide_subprocess_launcher_push_args (launcher, (const gchar * const *)state->configure_argv);

  if (NULL == (process = ide_subprocess_launcher_spawn_sync (launcher, cancellable, &error)))
    {
      g_task_return_error (task, error);
      return FALSE;
    }

  ide_build_result_log_subprocess (IDE_BUILD_RESULT (self), process);

  if (!g_subprocess_wait_check (process, cancellable, &error))
    {
      g_task_return_error (task, error);
      return FALSE;
    }

  if (state->bootstrap_only)
    {
      g_task_return_boolean (task, TRUE);
      return FALSE;
    }

  return TRUE;
}

static gboolean
step_make_all  (GTask                 *task,
                IdeAutotoolsBuildTask *self,
                WorkerState           *state,
                GCancellable          *cancellable)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(GSubprocess) process = NULL;
  const gchar * const *targets;
  const gchar *make = NULL;
  gchar *default_targets[] = { "all", NULL };
  GError *error = NULL;
  guint i;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_AUTOTOOLS_BUILD_TASK (self));
  g_assert (state);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (NULL == (launcher = ide_runtime_create_launcher (state->runtime, &error)))
    {
      g_task_return_error (task, error);
      return FALSE;
    }

  ide_subprocess_launcher_set_flags (launcher,
                                     (G_SUBPROCESS_FLAGS_STDERR_PIPE |
                                      G_SUBPROCESS_FLAGS_STDOUT_PIPE));
  ide_subprocess_launcher_set_cwd (launcher, state->directory_path);
  ide_subprocess_launcher_setenv (launcher, "LANG", "C", TRUE);
  apply_environment (self, launcher);

  /*
   * Try to locate GNU make within the runtime.
   */
  if (ide_runtime_contains_program_in_path (state->runtime, "gmake", cancellable))
    make = "gmake";
  else if (ide_runtime_contains_program_in_path (state->runtime, "make", cancellable))
    make = "make";
  else
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_FOUND,
                               "Failed to locate make.");
      return FALSE;
    }

  if (!g_strv_length (state->make_targets))
    targets = (const gchar * const *)default_targets;
  else
    targets = (const gchar * const *)state->make_targets;

  for (i = 0; targets [i]; i++)
    {
      const gchar *target = targets [i];

      if (ide_str_equal0 (target, "clean"))
        ide_build_result_set_mode (IDE_BUILD_RESULT (self), _("Cleaning…"));
      else
        ide_build_result_set_mode (IDE_BUILD_RESULT (self), _("Building…"));

      process = log_and_spawn (self, launcher, &error, make, target, state->parallel, NULL);

      if (!process)
        {
          g_task_return_error (task, error);
          return FALSE;
        }

      ide_build_result_log_subprocess (IDE_BUILD_RESULT (self), process);

      if (!g_subprocess_wait_check (process, cancellable, &error))
        {
          g_task_return_error (task, error);
          return FALSE;
        }
    }

  return TRUE;
}

static void
apply_environment (IdeAutotoolsBuildTask *self,
                   IdeSubprocessLauncher *launcher)
{
  IdeEnvironment *environment;

  g_assert (IDE_IS_AUTOTOOLS_BUILD_TASK (self));
  g_assert (IDE_IS_SUBPROCESS_LAUNCHER (launcher));

  environment = ide_configuration_get_environment (self->configuration);
  ide_subprocess_launcher_overlay_environment (launcher, environment);
}
