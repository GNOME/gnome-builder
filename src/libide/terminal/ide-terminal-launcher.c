/* ide-terminal-launcher.c
 *
 * Copyright 2019 Christian Hergert <unknown@domain.org>
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

#define G_LOG_DOMAIN "ide-terminal-launcher"

#include "config.h"

#include <errno.h>
#include <glib/gi18n.h>
#include <libide-foundry.h>
#include <libide-threading.h>

#include "ide-terminal-launcher.h"
#include "ide-terminal-util.h"

typedef enum
{
  LAUNCHER_KIND_HOST = 0,
  LAUNCHER_KIND_DEBUG,
  LAUNCHER_KIND_RUNTIME,
  LAUNCHER_KIND_RUNNER,
  LAUNCHER_KIND_LAUNCHER,
} LauncherKind;

struct _IdeTerminalLauncher
{
  GObject                parent_instance;
  gchar                 *cwd;
  gchar                 *shell;
  gchar                 *title;
  gchar                **args;
  IdeRuntime            *runtime;
  IdeContext            *context;
  IdeSubprocessLauncher *launcher;
  LauncherKind           kind;
};

G_DEFINE_TYPE (IdeTerminalLauncher, ide_terminal_launcher, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_ARGS,
  PROP_CWD,
  PROP_SHELL,
  PROP_TITLE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];
static const struct {
  const gchar *key;
  const gchar *value;
} default_environment[] = {
  { "INSIDE_GNOME_BUILDER", PACKAGE_VERSION },
  { "TERM", "xterm-256color" },
};

static gboolean
shell_supports_login (const gchar *shell)
{
  g_autofree gchar *name = NULL;

  /* Shells that support --login */
  static const gchar *supported[] = {
    "sh", "bash",
  };

  if (shell == NULL)
    return FALSE;

  if (!(name = g_path_get_basename (shell)))
    return FALSE;

  for (guint i = 0; i < G_N_ELEMENTS (supported); i++)
    {
      if (g_str_equal (name, supported[i]))
        return TRUE;
    }

  return FALSE;
}

static void
copy_envvars (gpointer instance)
{
  static const gchar *copy_env[] = {
    "COLORTERM",
    "DESKTOP_SESSION",
    "LANG",
    "WAYLAND_DISPLAY",
    "XDG_CURRENT_DESKTOP",
    "XDG_SEAT",
    "XDG_SESSION_DESKTOP",
    "XDG_SESSION_ID",
    "XDG_SESSION_TYPE",
    "XDG_VTNR",
  };
  IdeEnvironment *env = NULL;

  g_assert (IDE_IS_SUBPROCESS_LAUNCHER (instance) || IDE_IS_RUNNER (instance));

  if (IDE_IS_RUNNER (instance))
    env = ide_runner_get_environment (instance);

  for (guint i = 0; i < G_N_ELEMENTS (copy_env); i++)
    {
      const gchar *val = g_getenv (copy_env[i]);

      if (val != NULL)
        {
          if (IDE_IS_SUBPROCESS_LAUNCHER (instance))
            ide_subprocess_launcher_setenv (instance, copy_env[i], val, FALSE);
          else
            ide_environment_setenv (env, copy_env[i], val);
        }
    }
}

static void
apply_pipeline_info (gpointer   instance,
                     IdeObject *object)
{
  g_autoptr(GFile) workdir = NULL;
  IdeEnvironment *env = NULL;
  IdeContext *context;

  g_assert (IDE_IS_SUBPROCESS_LAUNCHER (instance) || IDE_IS_RUNNER (instance));
  g_assert (IDE_IS_OBJECT (object));

  context = ide_object_get_context (object);
  workdir = ide_context_ref_workdir (context);

  if (IDE_IS_RUNNER (instance))
    env = ide_runner_get_environment (instance);

  if (IDE_IS_SUBPROCESS_LAUNCHER (instance))
    ide_subprocess_launcher_setenv (instance, "SRCDIR", g_file_peek_path (workdir), FALSE);
  else
    ide_environment_setenv (env, "SRCDIR", g_file_peek_path (workdir));

  if (ide_context_has_project (context))
    {
      IdeBuildManager *build_manager = ide_build_manager_from_context (context);
      IdePipeline *pipeline = ide_build_manager_get_pipeline (build_manager);

      if (pipeline != NULL)
        {
          const gchar *builddir = ide_pipeline_get_builddir (pipeline);

          if (IDE_IS_SUBPROCESS_LAUNCHER (instance))
            ide_subprocess_launcher_setenv (instance, "BUILDDIR", builddir, FALSE);
          else
            ide_environment_setenv (env, "BUILDDIR", builddir);
        }
    }
}

static void
ide_terminal_launcher_wait_check_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_subprocess_wait_check_finish (subprocess, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);
}

static void
spawn_host_launcher (IdeTerminalLauncher *self,
                     IdeTask             *task,
                     gint                 pty_fd,
                     gboolean             run_on_host)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(GError) error = NULL;
  const gchar *shell;

  g_assert (IDE_IS_TERMINAL_LAUNCHER (self));
  g_assert (IDE_IS_TASK (task));
  g_assert (pty_fd >= 0);

  if (!(shell = ide_terminal_launcher_get_shell (self)))
    shell = ide_get_user_shell ();

  /* We only have sh/bash in our flatpak */
  if (self->kind == LAUNCHER_KIND_DEBUG && ide_is_flatpak ())
    shell = "/bin/bash";
 
  launcher = ide_subprocess_launcher_new (0);
  ide_subprocess_launcher_set_run_on_host (launcher, run_on_host);
  ide_subprocess_launcher_set_cwd (launcher, self->cwd ? self->cwd : g_get_home_dir ());
  ide_subprocess_launcher_set_clear_env (launcher, FALSE);

  ide_subprocess_launcher_push_argv (launcher, shell);
  if (shell_supports_login (shell))
    ide_subprocess_launcher_push_argv (launcher, "--login");

  ide_subprocess_launcher_take_stdin_fd (launcher, dup (pty_fd));
  ide_subprocess_launcher_take_stdout_fd (launcher, dup (pty_fd));
  ide_subprocess_launcher_take_stderr_fd (launcher, dup (pty_fd));

  g_assert (ide_subprocess_launcher_get_needs_tty (launcher));

  for (guint i = 0; i < G_N_ELEMENTS (default_environment); i++)
    ide_subprocess_launcher_setenv (launcher,
                                    default_environment[i].key,
                                    default_environment[i].value,
                                    FALSE);

  ide_subprocess_launcher_setenv (launcher, "SHELL", shell, TRUE);

  if (self->context != NULL)
    {
      g_autoptr(GFile) workdir = ide_context_ref_workdir (self->context);

      ide_subprocess_launcher_setenv (launcher,
                                      "SRCDIR",
                                      g_file_peek_path (workdir),
                                      FALSE);
    }

  if (!(subprocess = ide_subprocess_launcher_spawn (launcher, NULL, &error)))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_subprocess_wait_check_async (subprocess,
                                     ide_task_get_cancellable (task),
                                     ide_terminal_launcher_wait_check_cb,
                                     g_object_ref (task));
}

static void
spawn_launcher (IdeTerminalLauncher   *self,
                IdeTask               *task,
                IdeSubprocessLauncher *launcher,
                gint                   pty_fd)
{
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_TERMINAL_LAUNCHER (self));
  g_assert (IDE_IS_TASK (task));
  g_assert (!launcher || IDE_IS_SUBPROCESS_LAUNCHER (launcher));
  g_assert (pty_fd >= 0);

  if (launcher == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "process may only be spawned once");
      return;
    }

  ide_subprocess_launcher_set_flags (launcher, 0);

  ide_subprocess_launcher_take_stdin_fd (launcher, dup (pty_fd));
  ide_subprocess_launcher_take_stdout_fd (launcher, dup (pty_fd));
  ide_subprocess_launcher_take_stderr_fd (launcher, dup (pty_fd));

  if (!(subprocess = ide_subprocess_launcher_spawn (launcher, NULL, &error)))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_subprocess_wait_check_async (subprocess,
                                     ide_task_get_cancellable (task),
                                     ide_terminal_launcher_wait_check_cb,
                                     g_object_ref (task));
}

static void
spawn_runtime_launcher (IdeTerminalLauncher *self,
                        IdeTask             *task,
                        IdeRuntime          *runtime,
                        gint                 pty_fd)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(GError) error = NULL;
  const gchar *shell;

  g_assert (IDE_IS_TERMINAL_LAUNCHER (self));
  g_assert (IDE_IS_TASK (task));
  g_assert (IDE_IS_RUNTIME (runtime));
  g_assert (pty_fd >= 0);

  if (!(shell = ide_terminal_launcher_get_shell (self)))
    shell = ide_get_user_shell ();
 
  if (!(launcher = ide_runtime_create_launcher (runtime, NULL)))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 _("Failed to create shell within runtime “%s”"),
                                 ide_runtime_get_display_name (runtime));
      return;
    }

  ide_subprocess_launcher_set_flags (launcher, 0);

  if (!ide_runtime_contains_program_in_path (runtime, shell, NULL))
    shell = "/bin/sh";

  ide_subprocess_launcher_set_cwd (launcher, self->cwd ? self->cwd : g_get_home_dir ());

  ide_subprocess_launcher_push_argv (launcher, shell);
  if (shell_supports_login (shell))
    ide_subprocess_launcher_push_argv (launcher, "--login");

  ide_subprocess_launcher_take_stdin_fd (launcher, dup (pty_fd));
  ide_subprocess_launcher_take_stdout_fd (launcher, dup (pty_fd));
  ide_subprocess_launcher_take_stderr_fd (launcher, dup (pty_fd));

  g_assert (ide_subprocess_launcher_get_needs_tty (launcher));

  for (guint i = 0; i < G_N_ELEMENTS (default_environment); i++)
    ide_subprocess_launcher_setenv (launcher,
                                    default_environment[i].key,
                                    default_environment[i].value,
                                    FALSE);

  apply_pipeline_info (launcher, IDE_OBJECT (self->runtime));
  copy_envvars (launcher);

  ide_subprocess_launcher_setenv (launcher, "SHELL", shell, TRUE);

  if (!(subprocess = ide_subprocess_launcher_spawn (launcher, NULL, &error)))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_subprocess_wait_check_async (subprocess,
                                     ide_task_get_cancellable (task),
                                     ide_terminal_launcher_wait_check_cb,
                                     g_object_ref (task));
}

static void
ide_terminal_launcher_run_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  IdeRunner *runner = (IdeRunner *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_RUNNER (runner));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_runner_run_finish (runner, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  ide_object_destroy (IDE_OBJECT (runner));
}

static void
spawn_runner_launcher (IdeTerminalLauncher *self,
                       IdeTask             *task,
                       IdeRuntime          *runtime,
                       gint                 pty_fd)
{
  g_autoptr(IdeSimpleBuildTarget) build_target = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(IdeRunner) runner = NULL;
  g_autoptr(GPtrArray) argv = NULL;
  g_autoptr(GError) error = NULL;
  IdeEnvironment *env;
  const gchar *shell;

  g_assert (IDE_IS_TERMINAL_LAUNCHER (self));
  g_assert (IDE_IS_TASK (task));
  g_assert (IDE_IS_RUNTIME (runtime));
  g_assert (pty_fd >= 0);

  if (!(shell = ide_terminal_launcher_get_shell (self)))
    shell = ide_get_user_shell ();

  if (!ide_runtime_contains_program_in_path (runtime, shell, NULL))
    shell = "/bin/sh";

  argv = g_ptr_array_new ();
  g_ptr_array_add (argv, (gchar *)shell);

  if (self->args == NULL)
    {
      if (shell_supports_login (shell))
        g_ptr_array_add (argv, (gchar *)"--login");
    }
  else
    {
      for (guint i = 0; self->args[i]; i++)
        g_ptr_array_add (argv, self->args[i]);
    }

  g_ptr_array_add (argv, NULL);

  build_target = ide_simple_build_target_new (NULL);
  ide_simple_build_target_set_argv (build_target, (const gchar * const *)argv->pdata);
  ide_simple_build_target_set_cwd (build_target, self->cwd ? self->cwd : g_get_home_dir ());
 
  /* Creating runner should always succeed, but run_async() may fail */
  runner = ide_runtime_create_runner (runtime, IDE_BUILD_TARGET (build_target));
  env = ide_runner_get_environment (runner);
  ide_runner_take_tty_fd (runner, dup (pty_fd));

  for (guint i = 0; i < G_N_ELEMENTS (default_environment); i++)
    ide_environment_setenv (env, 
                            default_environment[i].key,
                            default_environment[i].value);

  apply_pipeline_info (runner, IDE_OBJECT (self->runtime));
  copy_envvars (runner);

  ide_environment_setenv (env, "SHELL", shell);

  ide_runner_run_async (runner,
                        ide_task_get_cancellable (task),
                        ide_terminal_launcher_run_cb,
                        g_object_ref (task));
}

void
ide_terminal_launcher_spawn_async (IdeTerminalLauncher *self,
                                   VtePty              *pty,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  gint pty_fd = -1;

  g_assert (IDE_IS_TERMINAL_LAUNCHER (self));
  g_assert (VTE_IS_PTY (pty));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_terminal_launcher_spawn_async);

  if ((pty_fd = ide_vte_pty_create_slave (pty)) == -1)
    {
      int errsv = errno;
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 g_io_error_from_errno (errsv),
                                 "%s", g_strerror (errsv));
      return;
    }

  switch (self->kind)
    {
    case LAUNCHER_KIND_RUNTIME:
      spawn_runtime_launcher (self, task, self->runtime, pty_fd);
      break;

    case LAUNCHER_KIND_RUNNER:
      spawn_runner_launcher (self, task, self->runtime, pty_fd);
      break;

    case LAUNCHER_KIND_LAUNCHER:
      spawn_launcher (self, task, self->launcher, pty_fd);
      g_clear_object (&self->launcher);
      break;

    case LAUNCHER_KIND_DEBUG:
    case LAUNCHER_KIND_HOST:
    default:
      spawn_host_launcher (self, task, pty_fd, self->kind == LAUNCHER_KIND_HOST);
      break;
    }

  if (pty_fd != -1)
    close (pty_fd);
}

/**
 * ide_terminal_launcher_spawn_finish:
 * @self: a #IdeTerminalLauncher
 *
 * Completes a request to ide_terminal_launcher_spawn_async()
 *
 * Returns: %TRUE if the process executed successfully; otherwise %FALSE
 *   and @error is set.
 *
 * Since: 3.34
 */
gboolean
ide_terminal_launcher_spawn_finish (IdeTerminalLauncher  *self,
                                    GAsyncResult         *result,
                                    GError              **error)
{
  g_assert (IDE_IS_TERMINAL_LAUNCHER (self));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
ide_terminal_launcher_finalize (GObject *object)
{
  IdeTerminalLauncher *self = (IdeTerminalLauncher *)object;
  
  g_clear_pointer (&self->args, g_strfreev);
  g_clear_pointer (&self->cwd, g_free);
  g_clear_pointer (&self->shell, g_free);
  g_clear_pointer (&self->title, g_free);
  g_clear_object (&self->launcher);
  g_clear_object (&self->runtime);

  G_OBJECT_CLASS (ide_terminal_launcher_parent_class)->finalize (object);
}

static void
ide_terminal_launcher_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  IdeTerminalLauncher *self = IDE_TERMINAL_LAUNCHER (object);

  switch (prop_id)
    {
    case PROP_ARGS:
      g_value_set_boxed (value, ide_terminal_launcher_get_args (self));
      break;

    case PROP_CWD:
      g_value_set_string (value, ide_terminal_launcher_get_cwd (self));
      break;

    case PROP_SHELL:
      g_value_set_string (value, ide_terminal_launcher_get_shell (self));
      break;

    case PROP_TITLE:
      g_value_set_string (value, ide_terminal_launcher_get_title (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_terminal_launcher_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  IdeTerminalLauncher *self = IDE_TERMINAL_LAUNCHER (object);

  switch (prop_id)
    {
    case PROP_ARGS:
      ide_terminal_launcher_set_args (self, g_value_get_boxed (value));
      break;

    case PROP_CWD:
      ide_terminal_launcher_set_cwd (self, g_value_get_string (value));
      break;

    case PROP_SHELL:
      ide_terminal_launcher_set_shell (self, g_value_get_string (value));
      break;

    case PROP_TITLE:
      ide_terminal_launcher_set_title (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_terminal_launcher_class_init (IdeTerminalLauncherClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_terminal_launcher_finalize;
  object_class->get_property = ide_terminal_launcher_get_property;
  object_class->set_property = ide_terminal_launcher_set_property;

  properties [PROP_ARGS] =
    g_param_spec_boxed ("args",
                         "Args",
                         "Arguments to shell",
                         G_TYPE_STRV,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  
  properties [PROP_CWD] =
    g_param_spec_string ("cwd",
                         "Cwd",
                         "The cwd to spawn in the subprocess",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SHELL] =
    g_param_spec_string ("shell",
                         "Shell",
                         "The shell to spawn in the subprocess",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_TITLE] =
    g_param_spec_string ("title",
                         "Title",
                         "The title for the subprocess launcher",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_terminal_launcher_init (IdeTerminalLauncher *self)
{
  self->cwd = NULL;
  self->shell = NULL;
  self->title = g_strdup (_("Untitled Terminal"));
}

const gchar *
ide_terminal_launcher_get_cwd (IdeTerminalLauncher *self)
{
  g_return_val_if_fail (IDE_IS_TERMINAL_LAUNCHER (self), NULL);

  return self->cwd;
}

void
ide_terminal_launcher_set_cwd (IdeTerminalLauncher *self,
                               const gchar         *cwd)
{
  g_return_if_fail (IDE_IS_TERMINAL_LAUNCHER (self));

  if (g_strcmp0 (self->cwd, cwd) != 0)
    {
      g_free (self->cwd);
      self->cwd = g_strdup (cwd);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CWD]);
    }
}

const gchar *
ide_terminal_launcher_get_shell (IdeTerminalLauncher *self)
{
  g_return_val_if_fail (IDE_IS_TERMINAL_LAUNCHER (self), NULL);

  return self->shell;
}

void
ide_terminal_launcher_set_shell (IdeTerminalLauncher *self,
                                 const gchar         *shell)
{
  g_return_if_fail (IDE_IS_TERMINAL_LAUNCHER (self));

  if (g_strcmp0 (self->shell, shell) != 0)
    {
      g_free (self->shell);
      self->shell = g_strdup (shell);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SHELL]);
    }
}

const gchar *
ide_terminal_launcher_get_title (IdeTerminalLauncher *self)
{
  g_return_val_if_fail (IDE_IS_TERMINAL_LAUNCHER (self), NULL);

  return self->title;
}

void
ide_terminal_launcher_set_title (IdeTerminalLauncher *self,
                                 const gchar         *title)
{
  g_return_if_fail (IDE_IS_TERMINAL_LAUNCHER (self));

  if (g_strcmp0 (self->title, title) != 0)
    {
      g_free (self->title);
      self->title = g_strdup (title);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TITLE]);
    }
}

/**
 * ide_terminal_launcher_new:
 *
 * Create a new #IdeTerminalLauncher that will spawn a terminal on the host.
 *
 * Returns: (transfer full): a newly created #IdeTerminalLauncher
 */
IdeTerminalLauncher *
ide_terminal_launcher_new (IdeContext *context)
{
  IdeTerminalLauncher *self;
  g_autoptr(GFile) workdir = NULL;

  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  workdir = ide_context_ref_workdir (context);

  self = g_object_new (IDE_TYPE_TERMINAL_LAUNCHER, NULL);
  self->kind = LAUNCHER_KIND_HOST;
  self->cwd = g_file_get_path (workdir);
  self->context = g_object_ref (context);

  return g_steal_pointer (&self);
}

/**
 * ide_terminal_launcher_new_for_launcher:
 * @launcher: an #IdeSubprocessLauncher
 *
 * Creates a new #IdeTerminalLauncher that can be used to launch a process
 * using the provided #IdeSubprocessLauncher.
 *
 * Returns: (transfer full): an #IdeTerminalLauncher
 *
 * Since: 3.34
 */
IdeTerminalLauncher *
ide_terminal_launcher_new_for_launcher (IdeSubprocessLauncher *launcher)
{
  IdeTerminalLauncher *self;

  g_return_val_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (launcher), NULL);

  self = g_object_new (IDE_TYPE_TERMINAL_LAUNCHER, NULL);
  self->kind = LAUNCHER_KIND_LAUNCHER;
  self->launcher = g_object_ref (launcher);

  return g_steal_pointer (&self);
}

/**
 * ide_terminal_launcher_new_for_debug
 *
 * Create a new #IdeTerminalLauncher that will spawn a terminal on the host.
 *
 * Returns: (transfer full): a newly created #IdeTerminalLauncher
 */
IdeTerminalLauncher *
ide_terminal_launcher_new_for_debug (void)
{
  IdeTerminalLauncher *self;

  self = g_object_new (IDE_TYPE_TERMINAL_LAUNCHER, NULL);
  self->kind = LAUNCHER_KIND_DEBUG;

  return g_steal_pointer (&self);
}

/**
 * ide_terminal_launcher_new_for_runtime:
 * @runtime: an #IdeRuntime
 *
 * Create a new #IdeTerminalLauncher that will spawn a terminal in the runtime.
 *
 * Returns: (transfer full): a newly created #IdeTerminalLauncher
 */
IdeTerminalLauncher *
ide_terminal_launcher_new_for_runtime (IdeRuntime *runtime)
{
  IdeTerminalLauncher *self;

  g_return_val_if_fail (IDE_IS_RUNTIME (runtime), NULL);

  self = g_object_new (IDE_TYPE_TERMINAL_LAUNCHER, NULL);
  self->runtime = g_object_ref (runtime);
  self->kind = LAUNCHER_KIND_RUNTIME;

  ide_terminal_launcher_set_title (self, ide_runtime_get_name (runtime));

  return g_steal_pointer (&self);
}

/**
 * ide_terminal_launcher_new_for_runner:
 * @runtime: an #IdeRuntime
 *
 * Create a new #IdeTerminalLauncher that will spawn a terminal in the runtime
 * but with a "runner" context similar to how the application would execute.
 *
 * Returns: (transfer full): a newly created #IdeTerminalLauncher
 */
IdeTerminalLauncher *
ide_terminal_launcher_new_for_runner (IdeRuntime *runtime)
{
  IdeTerminalLauncher *self;

  g_return_val_if_fail (IDE_IS_RUNTIME (runtime), NULL);

  self = g_object_new (IDE_TYPE_TERMINAL_LAUNCHER, NULL);
  self->runtime = g_object_ref (runtime);
  self->kind = LAUNCHER_KIND_RUNNER;

  return g_steal_pointer (&self);
}

gboolean
ide_terminal_launcher_can_respawn (IdeTerminalLauncher *self)
{
  g_return_val_if_fail (IDE_IS_TERMINAL_LAUNCHER (self), FALSE);

  return self->kind != LAUNCHER_KIND_LAUNCHER;
}

const gchar * const *
ide_terminal_launcher_get_args (IdeTerminalLauncher *self)
{
  g_return_val_if_fail (IDE_IS_TERMINAL_LAUNCHER (self), NULL);

  return (const gchar * const *)self->args;
}

void
ide_terminal_launcher_set_args (IdeTerminalLauncher *self,
                                const gchar * const *args)
{
  g_return_if_fail (IDE_IS_TERMINAL_LAUNCHER (self));

  if ((gchar **)args != self->args)
    {
      g_auto(GStrv) freeme = g_steal_pointer (&self->args);
      self->args = g_strdupv ((gchar **)args);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ARGS]);
    }
}
