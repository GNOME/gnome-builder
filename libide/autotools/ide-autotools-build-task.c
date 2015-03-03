/* ide-autotools-build-task.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <fcntl.h>
#include <glib/gi18n.h>
#include <unistd.h>

#include "ide-autotools-build-task.h"
#include "ide-context.h"
#include "ide-device.h"
#include "ide-project.h"

typedef struct
{
  GKeyFile  *config;
  IdeDevice *device;
  GFile     *directory;
  guint      require_autogen : 1;
  guint      require_configure : 1;
  guint      executed : 1;
} IdeAutotoolsBuildTaskPrivate;

typedef struct
{
  gchar  *directory_path;
  gchar  *project_path;
  gchar  *system_type;
  gchar **configure_argv;
  gchar **make_targets;
  guint   require_autogen : 1;
  guint   require_configure : 1;
  guint   bootstrap_only : 1;
} WorkerState;

typedef gboolean (*WorkStep) (GTask                 *task,
                              IdeAutotoolsBuildTask *self,
                              WorkerState           *state,
                              GCancellable          *cancellable);

G_DEFINE_TYPE_WITH_PRIVATE (IdeAutotoolsBuildTask, ide_autotools_build_task,
                            IDE_TYPE_BUILD_RESULT)

enum {
  PROP_0,
  PROP_CONFIG,
  PROP_DEVICE,
  PROP_DIRECTORY,
  PROP_REQUIRE_AUTOGEN,
  PROP_REQUIRE_CONFIGURE,
  LAST_PROP
};

static GSubprocess *log_and_spawn  (IdeAutotoolsBuildTask  *self,
                                    GSubprocessLauncher    *launcher,
                                    GError                **error,
                                    const gchar            *argv0,
                                    ...) G_GNUC_NULL_TERMINATED;
static gboolean step_mkdirs        (GTask                  *task,
                                    IdeAutotoolsBuildTask  *self,
                                    WorkerState            *state,
                                    GCancellable           *cancellable);
static gboolean step_autogen       (GTask                  *task,
                                    IdeAutotoolsBuildTask  *self,
                                    WorkerState            *state,
                                    GCancellable           *cancellable);
static gboolean step_configure     (GTask                  *task,
                                    IdeAutotoolsBuildTask  *self,
                                    WorkerState            *state,
                                    GCancellable           *cancellable);
static gboolean step_make_all      (GTask                  *task,
                                    IdeAutotoolsBuildTask  *self,
                                    WorkerState            *state,
                                    GCancellable           *cancellable);

static GParamSpec *gParamSpecs [LAST_PROP];
static WorkStep gWorkSteps [] = {
  step_mkdirs,
  step_autogen,
  step_configure,
  step_make_all,
  NULL
};

gboolean
ide_autotools_build_task_get_require_autogen (IdeAutotoolsBuildTask *task)
{
  IdeAutotoolsBuildTaskPrivate *priv;

  g_return_val_if_fail (IDE_IS_AUTOTOOLS_BUILD_TASK (task), FALSE);

  priv = ide_autotools_build_task_get_instance_private (task);

  return priv->require_autogen;
}

static void
ide_autotools_build_task_set_require_autogen (IdeAutotoolsBuildTask *task,
                                              gboolean               require_autogen)
{
  IdeAutotoolsBuildTaskPrivate *priv;

  g_return_if_fail (IDE_IS_AUTOTOOLS_BUILD_TASK (task));

  priv = ide_autotools_build_task_get_instance_private (task);

  priv->require_autogen = !!require_autogen;
}

gboolean
ide_autotools_build_task_get_require_configure (IdeAutotoolsBuildTask *task)
{
  IdeAutotoolsBuildTaskPrivate *priv;

  g_return_val_if_fail (IDE_IS_AUTOTOOLS_BUILD_TASK (task), FALSE);

  priv = ide_autotools_build_task_get_instance_private (task);

  return priv->require_configure;
}

static void
ide_autotools_build_task_set_require_configure (IdeAutotoolsBuildTask *task,
                                                gboolean               require_configure)
{
  IdeAutotoolsBuildTaskPrivate *priv;

  g_return_if_fail (IDE_IS_AUTOTOOLS_BUILD_TASK (task));

  priv = ide_autotools_build_task_get_instance_private (task);

  priv->require_autogen = !!require_configure;
}

/**
 * ide_autotools_build_task_get_config:
 * @self: A #IdeAutotoolsBuildTask.
 *
 * Gets the "config" property of the task. This is the overlay config to be
 * applied on top of the device config when compiling.
 *
 * Returns: (transfer none) (nullable): A #GKeyFile or %NULL.
 */
GKeyFile *
ide_autotools_build_task_get_config (IdeAutotoolsBuildTask *self)
{
  IdeAutotoolsBuildTaskPrivate *priv;

  g_return_val_if_fail (IDE_IS_AUTOTOOLS_BUILD_TASK (self), NULL);

  priv = ide_autotools_build_task_get_instance_private (self);

  return priv->config;
}

static void
ide_autotools_build_task_set_config (IdeAutotoolsBuildTask *self,
                                     GKeyFile              *config)
{
  IdeAutotoolsBuildTaskPrivate *priv;

  g_return_if_fail (IDE_IS_AUTOTOOLS_BUILD_TASK (self));

  priv = ide_autotools_build_task_get_instance_private (self);

  if (priv->config != config)
    {
      g_clear_pointer (&priv->config, g_key_file_unref);
      priv->config = config ? g_key_file_ref (config) : NULL;
      g_object_notify_by_pspec (G_OBJECT (self),
                                gParamSpecs [PROP_CONFIG]);
    }
}

/**
 * ide_autotools_build_task_get_device:
 * @self: A #IdeAutotoolsBuildTask.
 *
 * Gets the "device" property. This is the device we are compiling for,
 * which may involve cross-compiling.
 *
 * Returns: (transfer none): An #IdeDevice.
 */
IdeDevice *
ide_autotools_build_task_get_device (IdeAutotoolsBuildTask *self)
{
  IdeAutotoolsBuildTaskPrivate *priv;

  g_return_val_if_fail (IDE_IS_AUTOTOOLS_BUILD_TASK (self), NULL);

  priv = ide_autotools_build_task_get_instance_private (self);

  return priv->device;
}

static void
ide_autotools_build_task_set_device (IdeAutotoolsBuildTask *self,
                                     IdeDevice             *device)
{
  IdeAutotoolsBuildTaskPrivate *priv;

  g_return_if_fail (IDE_IS_AUTOTOOLS_BUILD_TASK (self));

  priv = ide_autotools_build_task_get_instance_private (self);

  if (g_set_object (&priv->device, device))
    g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_DEVICE]);
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
  IdeAutotoolsBuildTaskPrivate *priv;

  g_return_val_if_fail (IDE_IS_AUTOTOOLS_BUILD_TASK (self), NULL);

  priv = ide_autotools_build_task_get_instance_private (self);

  return priv->directory;
}

static void
ide_autotools_build_task_set_directory (IdeAutotoolsBuildTask *self,
                                        GFile                 *directory)
{
  IdeAutotoolsBuildTaskPrivate *priv;

  g_return_if_fail (IDE_IS_AUTOTOOLS_BUILD_TASK (self));
  g_return_if_fail (!directory || G_IS_FILE (directory));

  priv = ide_autotools_build_task_get_instance_private (self);

  /*
   * We require a build directory that is accessable via a native path.
   */
  if (directory)
    {
      g_autofree gchar *path;

      path = g_file_get_path (directory);

      if (!path)
        {
          g_warning (_("Directory must be on a locally mounted filesystem."));
          return;
        }
    }

  if (priv->directory != directory)
    if (g_set_object (&priv->directory, directory))
      g_object_notify_by_pspec (G_OBJECT (self),
                                gParamSpecs [PROP_DIRECTORY]);
}

static void
ide_autotools_build_task_finalize (GObject *object)
{
  IdeAutotoolsBuildTask *self = (IdeAutotoolsBuildTask *)object;
  IdeAutotoolsBuildTaskPrivate *priv;

  priv = ide_autotools_build_task_get_instance_private (self);

  g_clear_object (&priv->device);
  g_clear_object (&priv->directory);
  g_clear_pointer (&priv->config, g_key_file_unref);

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
    case PROP_CONFIG:
      g_value_set_object (value, ide_autotools_build_task_get_config (self));
      break;

    case PROP_DEVICE:
      g_value_set_object (value, ide_autotools_build_task_get_device (self));
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
    case PROP_CONFIG:
      ide_autotools_build_task_set_config (self, g_value_get_boxed (value));
      break;

    case PROP_DEVICE:
      ide_autotools_build_task_set_device (self, g_value_get_object (value));
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

  gParamSpecs [PROP_CONFIG] =
    g_param_spec_boxed ("config",
                        _("Config"),
                        _("The overlay config for the compilation."),
                        G_TYPE_KEY_FILE,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_CONFIG,
                                   gParamSpecs [PROP_CONFIG]);

  gParamSpecs [PROP_DEVICE] =
    g_param_spec_object ("device",
                         _("Device"),
                         _("The device to build for."),
                         IDE_TYPE_DEVICE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_DEVICE,
                                   gParamSpecs [PROP_DEVICE]);

  gParamSpecs [PROP_DIRECTORY] =
    g_param_spec_object ("directory",
                         _("Directory"),
                         _("The directory to perform the build within."),
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_DIRECTORY,
                                   gParamSpecs [PROP_DIRECTORY]);

  gParamSpecs [PROP_REQUIRE_AUTOGEN] =
    g_param_spec_boolean ("require-autogen",
                          _("Require Autogen"),
                          _("If autogen.sh should be forced to execute."),
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_REQUIRE_AUTOGEN,
                                   gParamSpecs [PROP_REQUIRE_AUTOGEN]);

  gParamSpecs [PROP_REQUIRE_CONFIGURE] =
    g_param_spec_boolean ("require-configure",
                          _("Require Configure"),
                          _("If configure should be forced to execute."),
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_REQUIRE_CONFIGURE,
                                   gParamSpecs [PROP_REQUIRE_CONFIGURE]);
}

static void
ide_autotools_build_task_init (IdeAutotoolsBuildTask *self)
{
}

static gchar **
gen_configure_argv (IdeAutotoolsBuildTask *self,
                    WorkerState           *state)
{
  IdeAutotoolsBuildTaskPrivate *priv;
  GKeyFile *configs[2];
  GPtrArray *ar;
  GHashTable *ht;
  gpointer k, v;
  GHashTableIter iter;
  gchar *configure_path;
  guint j;

  g_return_val_if_fail (IDE_IS_AUTOTOOLS_BUILD_TASK (self), NULL);

  priv = ide_autotools_build_task_get_instance_private (self);

  ht = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  configs [0] = ide_device_get_config (priv->device);
  configs [1] = priv->config;

  for (j = 0; j < G_N_ELEMENTS (configs); j++)
    {
      GKeyFile *config = configs [j];

      if (config)
        {
          if (g_key_file_has_group (config, "autoconf"))
            {
              gchar **keys;
              gsize len;
              gsize i;

              keys = g_key_file_get_keys (config, "autoconf", &len, NULL);

              for (i = 0; i < len; i++)
                {
                  gchar *value;

                  if (*keys [i] == '-')
                    {
                      value = g_key_file_get_string (config,
                                                     "autoconf", keys [i],
                                                     NULL);
                      if (value)
                        g_hash_table_replace (ht, g_strdup (keys [i]), value);
                    }
                }

              g_strfreev (keys);
            }
        }
    }

  ar = g_ptr_array_new ();
  configure_path = g_build_filename (state->project_path, "configure", NULL);
  g_ptr_array_add (ar, configure_path);

  g_hash_table_iter_init (&iter, ht);

  while (g_hash_table_iter_next (&iter, &k, &v))
    {
      g_ptr_array_add (ar, g_strdup (k));
      if (v && *(gchar *)v)
        g_ptr_array_add (ar, g_strdup (v));
    }

  if (!g_hash_table_lookup (ht, "--prefix"))
    {
      gchar *prefix;

      prefix = g_build_filename (state->project_path, "_install", NULL);
      g_ptr_array_add (ar, g_strdup_printf ("--prefix=%s", prefix));
      g_free (prefix);
    }

  g_ptr_array_add (ar, NULL);
  g_hash_table_unref (ht);

  return (gchar **)g_ptr_array_free (ar, FALSE);
}

static WorkerState *
worker_state_new (IdeAutotoolsBuildTask *self)
{
  IdeAutotoolsBuildTaskPrivate *priv;
  g_autofree gchar *name = NULL;
  IdeContext *context;
  GPtrArray *make_targets;
  GFile *project_dir;
  GFile *project_file;
  WorkerState *state;

  g_return_val_if_fail (IDE_IS_AUTOTOOLS_BUILD_TASK (self), NULL);

  priv = ide_autotools_build_task_get_instance_private (self);

  context = ide_object_get_context (IDE_OBJECT (self));
  project_file = ide_context_get_project_file (context);

  name = g_file_get_basename (project_file);

  if (g_str_has_prefix (name, "configure."))
    project_dir = g_file_get_parent (project_file);
  else
    project_dir = g_object_ref (project_file);

  state = g_slice_new0 (WorkerState);
  state->require_autogen = priv->require_autogen;
  state->require_configure = priv->require_configure;
  state->directory_path = g_file_get_path (priv->directory);
  state->project_path = g_file_get_path (project_dir);
  state->system_type = g_strdup (ide_device_get_system_type (priv->device));

  make_targets = g_ptr_array_new ();

  if (priv->config && g_key_file_get_boolean (priv->config, "autotools", "rebuild", NULL))
    {
      state->require_autogen = TRUE;
      state->require_configure = TRUE;
      g_ptr_array_add (make_targets, g_strdup ("clean"));
    }

  g_ptr_array_add (make_targets, g_strdup ("all"));
  g_ptr_array_add (make_targets, NULL);
  state->make_targets = (gchar **)g_ptr_array_free (make_targets, FALSE);

  if (g_key_file_get_boolean (priv->config, "autotools", "bootstrap-only", NULL))
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

  for (i = 0; gWorkSteps [i]; i++)
    {
      if (g_cancellable_is_cancelled (cancellable) ||
          !gWorkSteps [i] (task, self, state, cancellable))
        return;
    }

  g_task_return_boolean (task, TRUE);
}

void
ide_autotools_build_task_execute_async (IdeAutotoolsBuildTask *self,
                                        GCancellable          *cancellable,
                                        GAsyncReadyCallback    callback,
                                        gpointer               user_data)
{
  IdeAutotoolsBuildTaskPrivate *priv;
  g_autoptr(GTask) task = NULL;
  WorkerState *state;

  g_return_if_fail (IDE_IS_AUTOTOOLS_BUILD_TASK (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  priv = ide_autotools_build_task_get_instance_private (self);

  if (priv->executed)
    {
      g_task_report_new_error (self, callback, user_data,
                               ide_autotools_build_task_execute_async,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               _("Cannot execute build task more than once."));
      return;
    }

  priv->executed = TRUE;

  state = worker_state_new (self);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_task_data (task, state, worker_state_free);
  g_task_run_in_thread (task, ide_autotools_build_task_execute_worker);
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
               GSubprocessLauncher    *launcher,
               GError                **error,
               const gchar           *argv0,
               ...)
{
  GSubprocess *ret;
  GPtrArray *argv;
  GString *log;
  gchar *item;
  va_list args;

  log = g_string_new (NULL);
  g_string_append (log, argv0);

  argv = g_ptr_array_new ();
  g_ptr_array_add (argv, (gchar *)argv0);

  va_start (args, argv0);
  while ((item = va_arg (args, gchar *)))
    {
      g_ptr_array_add (argv, item);
      g_string_append_printf (log, " '%s'", item);
    }
  va_end (args);

  g_ptr_array_add (argv, NULL);

  ide_build_result_log_stdout (IDE_BUILD_RESULT (self), "%s", log->str);
  ret = g_subprocess_launcher_spawnv (launcher,
                                      (const gchar * const *)argv->pdata,
                                      error);

  g_string_free (log, TRUE);
  g_ptr_array_unref (argv);

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
  g_autoptr(GSubprocessLauncher) launcher = NULL;
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
                               _("autogen.sh is missing from project directory."));
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

  launcher = g_subprocess_launcher_new ((G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                         G_SUBPROCESS_FLAGS_STDERR_PIPE));
  g_subprocess_launcher_set_cwd (launcher, state->project_path);
  g_subprocess_launcher_setenv (launcher, "NOCONFIGURE", "1", TRUE);

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
  g_autoptr(GSubprocessLauncher) launcher = NULL;
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

  launcher = g_subprocess_launcher_new ((G_SUBPROCESS_FLAGS_STDERR_PIPE |
                                         G_SUBPROCESS_FLAGS_STDOUT_PIPE));
  g_subprocess_launcher_set_cwd (launcher, state->directory_path);

  config_log = g_strjoinv (" ", state->configure_argv);
  ide_build_result_log_stdout (IDE_BUILD_RESULT (self), "%s", config_log);

  process = g_subprocess_launcher_spawnv (
      launcher,
      (const gchar * const *)state->configure_argv,
      &error);

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
  g_autoptr(GSubprocessLauncher) launcher = NULL;
  g_autoptr(GSubprocess) process = NULL;
  const gchar * const *targets;
  gchar *default_targets[] = { "all", NULL };
  GError *error = NULL;
  guint i;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_AUTOTOOLS_BUILD_TASK (self));
  g_assert (state);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  launcher = g_subprocess_launcher_new ((G_SUBPROCESS_FLAGS_STDERR_PIPE |
                                         G_SUBPROCESS_FLAGS_STDOUT_PIPE));
  g_subprocess_launcher_set_cwd (launcher, state->directory_path);

  if (!g_strv_length (state->make_targets))
    targets = (const gchar * const *)default_targets;
  else
    targets = (const gchar * const *)state->make_targets;

  for (i = 0; targets [i]; i++)
    {
      const gchar *target = targets [i];

      process = log_and_spawn (self, launcher, &error, "make", target, NULL);

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
