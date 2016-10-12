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

#define G_LOG_DOMAIN "ide-autotools-build-task"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <fcntl.h>
#include <glib/gi18n.h>
#include <ide.h>
#include <stdlib.h>
#include <unistd.h>

#include "ide-autotools-build-task.h"

#define FLAG_SET(_f,_n) (((_f) & (_n)) != 0)
#define FLAG_UNSET(_f,_n) (((_f) & (_n)) == 0)

struct _IdeAutotoolsBuildTask
{
  IdeBuildResult    parent;
  IdeConfiguration *configuration;
  GFile            *directory;
  GPtrArray        *extra_targets;
  guint             require_autogen : 1;
  guint             require_configure : 1;
  guint             executed : 1;
};

typedef struct
{
  gchar                 *directory_path;
  gchar                 *project_path;
  gchar                 *parallel;
  gchar                 *system_type;
  gchar                **configure_argv;
  gchar                **make_targets;
  IdeRuntime            *runtime;
  IdeBuildCommandQueue  *postbuild;
  IdeEnvironment        *environment;
  guint                  sequence;
  guint                  require_autogen : 1;
  guint                  require_configure : 1;
  guint                  bootstrap_only : 1;
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

static IdeSubprocess *log_and_spawn     (IdeAutotoolsBuildTask  *self,
                                         IdeSubprocessLauncher  *launcher,
                                         GCancellable           *cancellable,
                                         GError                **error,
                                         const gchar            *argv0,
                                         ...) G_GNUC_NULL_TERMINATED;
static gboolean       step_mkdirs       (GTask                  *task,
                                         IdeAutotoolsBuildTask  *self,
                                         WorkerState            *state,
                                         GCancellable           *cancellable);
static gboolean       step_autogen      (GTask                  *task,
                                         IdeAutotoolsBuildTask  *self,
                                         WorkerState            *state,
                                         GCancellable           *cancellable);
static gboolean       step_configure    (GTask                  *task,
                                         IdeAutotoolsBuildTask  *self,
                                         WorkerState            *state,
                                         GCancellable           *cancellable);
static gboolean       step_make_all     (GTask                  *task,
                                         IdeAutotoolsBuildTask  *self,
                                         WorkerState            *state,
                                         GCancellable           *cancellable);
static void           apply_environment (IdeAutotoolsBuildTask  *self,
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
  g_clear_pointer (&self->extra_targets, g_ptr_array_unref);

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

  opts = ide_configuration_get_config_opts (self->configuration);

  if (!ide_str_empty0 (opts))
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
worker_state_new (IdeAutotoolsBuildTask  *self,
                  IdeBuilderBuildFlags    flags,
                  GError                **error)
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

  if (device == NULL)
    {
      g_set_error (error,
                   IDE_DEVICE_ERROR,
                   IDE_DEVICE_ERROR_NO_SUCH_DEVICE,
                   "%s “%s”",
                   _("Failed to locate device"),
                   ide_configuration_get_device_id (self->configuration));
      return NULL;
    }

  if (runtime == NULL)
    {
      g_set_error (error,
                   IDE_RUNTIME_ERROR,
                   IDE_RUNTIME_ERROR_NO_SUCH_RUNTIME,
                   "%s “%s”",
                   _("Failed to locate runtime"),
                   ide_configuration_get_runtime_id (self->configuration));
      return NULL;
    }

  name = g_file_get_basename (project_file);

  if (g_str_has_prefix (name, "configure."))
    project_dir = g_file_get_parent (project_file);
  else
    project_dir = g_object_ref (project_file);

  state = g_slice_new0 (WorkerState);
  state->sequence = ide_configuration_get_sequence (self->configuration);
  state->require_autogen = self->require_autogen || FLAG_SET (flags, IDE_BUILDER_BUILD_FLAGS_FORCE_BOOTSTRAP);
  state->require_configure = self->require_configure || (state->require_autogen && FLAG_UNSET (flags, IDE_BUILDER_BUILD_FLAGS_NO_CONFIGURE));
  state->directory_path = g_file_get_path (self->directory);
  state->project_path = g_file_get_path (project_dir);
  state->system_type = g_strdup (ide_device_get_system_type (device));
  state->runtime = g_object_ref (runtime);
  state->postbuild = ide_configuration_get_postbuild (self->configuration);
  state->environment = ide_environment_copy (ide_configuration_get_environment (self->configuration));

  val32 = ide_configuration_get_parallelism (self->configuration);

  if (val32 == -1)
    state->parallel = g_strdup_printf ("-j%u", g_get_num_processors () + 1);
  else if (val32 == 0)
    state->parallel = g_strdup_printf ("-j%u", g_get_num_processors ());
  else
    state->parallel = g_strdup_printf ("-j%u", val32);

  make_targets = g_ptr_array_new ();

  if (FLAG_SET (flags, IDE_BUILDER_BUILD_FLAGS_FORCE_CLEAN))
    {
      if (FLAG_UNSET (flags, IDE_BUILDER_BUILD_FLAGS_NO_BUILD))
        {
          state->require_autogen = TRUE;
          state->require_configure = TRUE;
        }
      g_ptr_array_add (make_targets, g_strdup ("clean"));
    }

  if (FLAG_UNSET (flags, IDE_BUILDER_BUILD_FLAGS_NO_BUILD))
    g_ptr_array_add (make_targets, g_strdup ("all"));

  if (self->extra_targets != NULL)
    {
      for (guint i = 0; i < self->extra_targets->len; i++)
        {
          const gchar *target = g_ptr_array_index (self->extra_targets, i);

          g_ptr_array_add (make_targets, g_strdup (target));
        }
    }

  g_ptr_array_add (make_targets, NULL);

  state->make_targets = (gchar **)g_ptr_array_free (make_targets, FALSE);

  if (FLAG_SET (flags, IDE_BUILDER_BUILD_FLAGS_NO_CONFIGURE))
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
  g_clear_object (&state->postbuild);
  g_clear_object (&state->environment);
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
  GError *error = NULL;

  g_return_if_fail (G_IS_TASK (task));
  g_return_if_fail (IDE_IS_AUTOTOOLS_BUILD_TASK (self));
  g_return_if_fail (state);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  for (guint i = 0; workSteps [i]; i++)
    {
      if (g_cancellable_is_cancelled (cancellable) ||
          !workSteps [i] (task, self, state, cancellable))
        return;
    }

  if (!ide_build_command_queue_execute (state->postbuild,
                                        state->runtime,
                                        state->environment,
                                        IDE_BUILD_RESULT (self),
                                        cancellable,
                                        &error))
    {
      ide_build_result_log_stderr (IDE_BUILD_RESULT (self), "%s %s", _("Build Failed: "), error->message);
      g_task_return_error (task, error);
      return;
    }

  g_task_return_boolean (task, TRUE);
}

static void
ide_autotools_build_task_configuration_prebuild_cb (GObject      *object,
                                                    GAsyncResult *result,
                                                    gpointer      user_data)
{
  IdeBuildCommandQueue *cmdq = (IdeBuildCommandQueue *)object;
  g_autoptr(GTask) task = user_data;
  IdeAutotoolsBuildTask *self;
  GError *error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_COMMAND_QUEUE (cmdq));
  g_assert (G_IS_ASYNC_RESULT (result));

  self = g_task_get_source_object (task);

  if (!ide_build_command_queue_execute_finish (cmdq, result, &error))
    {

      ide_build_result_log_stderr (IDE_BUILD_RESULT (self), "%s %s", _("Build Failed: "), error->message);
      g_task_return_error (task, error);
      IDE_EXIT;
    }

  g_task_run_in_thread (task, ide_autotools_build_task_execute_worker);

  IDE_EXIT;
}

static void
ide_autotools_build_task_runtime_prebuild_cb (GObject      *object,
                                              GAsyncResult *result,
                                              gpointer      user_data)
{
  IdeRuntime *runtime = (IdeRuntime *)object;
  IdeAutotoolsBuildTask *self;
  g_autoptr(GTask) task = user_data;
  g_autoptr(IdeBuildCommandQueue) prebuild = NULL;
  GCancellable *cancellable;
  GError *error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_RUNTIME (runtime));
  g_assert (G_IS_TASK (task));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!ide_runtime_prebuild_finish (runtime, result, &error))
    {
      g_task_return_error (task, error);
      IDE_EXIT;
    }

  /*
   * Now that the runtime has prepared itself, we need to allow the
   * configuration's prebuild commands to be executed.
   */

  self = g_task_get_source_object (task);
  g_assert (IDE_IS_AUTOTOOLS_BUILD_TASK (self));

  prebuild = ide_configuration_get_prebuild (self->configuration);
  g_assert (IDE_IS_BUILD_COMMAND_QUEUE (prebuild));

  cancellable = g_task_get_cancellable (task);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  ide_build_command_queue_execute_async (prebuild,
                                         runtime,
                                         ide_configuration_get_environment (self->configuration),
                                         IDE_BUILD_RESULT (self),
                                         cancellable,
                                         ide_autotools_build_task_configuration_prebuild_cb,
                                         g_steal_pointer (&task));

  IDE_EXIT;
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
  GError *error = NULL;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_AUTOTOOLS_BUILD_TASK (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (callback != NULL);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_autotools_build_task_execute_async);

  if (self->executed)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "%s",
                               _("Cannot execute build task more than once"));
      IDE_EXIT;
    }

  self->executed = TRUE;

  state = worker_state_new (self, flags, &error);

  if (state == NULL)
    {
      g_task_return_error (task, error);
      IDE_EXIT;
    }

  g_task_set_task_data (task, state, worker_state_free);

  /* Execute the pre-hook for the runtime before we start building. */
  ide_runtime_prebuild_async (state->runtime,
                              cancellable,
                              ide_autotools_build_task_runtime_prebuild_cb,
                              g_steal_pointer (&task));

  IDE_EXIT;
}

gboolean
ide_autotools_build_task_execute_finish (IdeAutotoolsBuildTask  *self,
                                         GAsyncResult           *result,
                                         GError                **error)
{
  GTask *task = (GTask *)result;
  WorkerState *state;
  guint sequence;
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_AUTOTOOLS_BUILD_TASK (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (task), FALSE);

  state = g_task_get_task_data (G_TASK (task));
  sequence = ide_configuration_get_sequence (self->configuration);

  if ((state != NULL) && (state->sequence == sequence))
    ide_configuration_set_dirty (self->configuration, FALSE);

  ret = g_task_propagate_boolean (task, error);

  /* Mark the task as failed */
  if (ret == FALSE)
    ide_build_result_set_failed (IDE_BUILD_RESULT (self), TRUE);

  ide_build_result_set_running (IDE_BUILD_RESULT (self), FALSE);

  IDE_RETURN (ret);
}

static void
ide_autotools_build_task_postbuild_runtime_cb (GObject      *object,
                                               GAsyncResult *result,
                                               gpointer      user_data)
{
  IdeRuntime *runtime = (IdeRuntime *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_RUNTIME (runtime));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!ide_runtime_postbuild_finish (runtime, result, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_task_return_boolean (task, TRUE);
}

static void
ide_autotools_build_task_execute_with_postbuild_cb (GObject      *object,
                                                    GAsyncResult *result,
                                                    gpointer      user_data)
{
  IdeAutotoolsBuildTask *self = (IdeAutotoolsBuildTask *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeRuntime *runtime;
  GCancellable *cancellable;

  g_assert (IDE_IS_AUTOTOOLS_BUILD_TASK (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!ide_autotools_build_task_execute_finish (self, result, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  runtime = ide_configuration_get_runtime (self->configuration);

  if (runtime == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               "%s",
                               _("Failed to access runtime for postbuild"));
      return;
    }

  cancellable = g_task_get_cancellable (task);

  ide_runtime_postbuild_async (runtime,
                               cancellable,
                               ide_autotools_build_task_postbuild_runtime_cb,
                               g_steal_pointer (&task));
}

void
ide_autotools_build_task_execute_with_postbuild (IdeAutotoolsBuildTask *self,
                                                 IdeBuilderBuildFlags   flags,
                                                 GCancellable          *cancellable,
                                                 GAsyncReadyCallback    callback,
                                                 gpointer               user_data)
{
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (IDE_IS_AUTOTOOLS_BUILD_TASK (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_autotools_build_task_execute_with_postbuild);

  ide_autotools_build_task_execute_async (self,
                                          flags,
                                          cancellable,
                                          ide_autotools_build_task_execute_with_postbuild_cb,
                                          g_steal_pointer (&task));
}

gboolean
ide_autotools_build_task_execute_with_postbuild_finish (IdeAutotoolsBuildTask  *self,
                                                        GAsyncResult           *result,
                                                        GError                **error)
{
  g_return_val_if_fail (IDE_IS_AUTOTOOLS_BUILD_TASK (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static gboolean
log_in_main (gpointer data)
{
  struct {
    IdeBuildResult *result;
    gchar *message;
  } *pair = data;

  ide_build_result_log_stdout (pair->result, "%s", pair->message);

  g_free (pair->message);
  g_object_unref (pair->result);
  g_slice_free1 (sizeof *pair, pair);

  return G_SOURCE_REMOVE;
}

static IdeSubprocess *
log_and_spawn (IdeAutotoolsBuildTask  *self,
               IdeSubprocessLauncher  *launcher,
               GCancellable           *cancellable,
               GError                **error,
               const gchar           *argv0,
               ...)
{
  g_autoptr(GError) local_error = NULL;
  IdeSubprocess *ret;
  struct {
    IdeBuildResult *result;
    gchar *message;
  } *pair;
  GString *log;
  gchar *item;
  va_list args;
  gint popcnt = 0;

  g_assert (IDE_IS_AUTOTOOLS_BUILD_TASK (self));
  g_assert (IDE_IS_SUBPROCESS_LAUNCHER (launcher));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  log = g_string_new (argv0);
  ide_subprocess_launcher_push_argv (launcher, argv0);

  va_start (args, argv0);
  while (NULL != (item = va_arg (args, gchar *)))
    {
      ide_subprocess_launcher_push_argv (launcher, item);
      g_string_append_printf (log, " '%s'", item);
      popcnt++;
    }
  va_end (args);

  pair = g_slice_alloc (sizeof *pair);
  pair->result = g_object_ref (self);
  pair->message = g_string_free (log, FALSE);
  g_timeout_add (0, log_in_main, pair);

  ret = ide_subprocess_launcher_spawn_sync (launcher, cancellable, &local_error);

  if (ret == NULL)
    {
      ide_build_result_log_stderr (IDE_BUILD_RESULT (self), "%s %s",
                                   _("Build Failed: "),
                                   local_error->message);
      g_propagate_error (error, g_steal_pointer (&local_error));
    }

  /* pop make args */
  for (; popcnt; popcnt--)
    g_free (ide_subprocess_launcher_pop_argv (launcher));

  /* pop "make" */
  g_free (ide_subprocess_launcher_pop_argv (launcher));

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
  g_autoptr(IdeSubprocess) process = NULL;
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

  process = log_and_spawn (self, launcher, cancellable, &error, autogen_sh_path, NULL);

  if (!process)
    {
      g_task_return_error (task, error);
      return FALSE;
    }

  ide_build_result_log_subprocess (IDE_BUILD_RESULT (self), process);

  if (!ide_subprocess_wait_check (process, cancellable, &error))
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
  g_autoptr(IdeSubprocess) process = NULL;
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

  if (!ide_subprocess_wait_check (process, cancellable, &error))
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
  g_autoptr(IdeSubprocess) process = NULL;
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

      process = log_and_spawn (self, launcher, cancellable, &error,
                               make, target, state->parallel, NULL);

      if (!process)
        {
          g_task_return_error (task, error);
          return FALSE;
        }

      ide_build_result_log_subprocess (IDE_BUILD_RESULT (self), process);

      if (!ide_subprocess_wait_check (process, cancellable, &error))
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

void
ide_autotools_build_task_add_target (IdeAutotoolsBuildTask *self,
                                     const gchar           *target)
{
  g_return_if_fail (IDE_IS_AUTOTOOLS_BUILD_TASK (self));
  g_return_if_fail (target != NULL);

  if (self->extra_targets == NULL)
    self->extra_targets = g_ptr_array_new_with_free_func (g_free);

  g_ptr_array_add (self->extra_targets, g_strdup (target));
}
