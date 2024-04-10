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

struct _IdeTerminalLauncher
{
  GObject         parent_instance;
  IdeRunCommand  *run_command;
  IdeContext     *context;
  char          **override_environ;
};

G_DEFINE_FINAL_TYPE (IdeTerminalLauncher, ide_terminal_launcher, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_CONTEXT,
  PROP_RUN_COMMAND,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
ide_terminal_launcher_wait_check_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_subprocess_wait_check_finish (subprocess, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

void
ide_terminal_launcher_spawn_async (IdeTerminalLauncher *self,
                                   VtePty              *pty,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  g_autoptr(IdeRunContext) run_context = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TERMINAL_LAUNCHER (self));
  g_assert (VTE_IS_PTY (pty));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_terminal_launcher_spawn_async);

  run_context = ide_run_context_new ();

  /* Paranoia check to ensure we've been constructed right */
  if (self->run_command == NULL || self->context == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_INITIALIZED,
                                 "%s is improperly configured",
                                 G_OBJECT_TYPE_NAME (self));
      IDE_EXIT;
    }

  ide_run_command_prepare_to_run (self->run_command, run_context, self->context);

  /* Add some environment for custom bashrc, VTE, etc */
  ide_run_context_setenv (run_context, "INSIDE_GNOME_BUILDER", PACKAGE_VERSION);
  ide_run_context_setenv (run_context, "TERM", "xterm-256color");

  /* Apply override environment if specified */
  if (self->override_environ)
    ide_run_context_add_environ (run_context, (const char * const *)self->override_environ);

  /* Attach the PTY to stdin/stdout/stderr */
  ide_run_context_set_pty (run_context, pty);

  /* Now attempt to spawn the process */
  if (!(subprocess = ide_run_context_spawn (run_context, &error)))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_subprocess_wait_check_async (subprocess,
                                     cancellable,
                                     ide_terminal_launcher_wait_check_cb,
                                     g_steal_pointer (&task));

  IDE_EXIT;
}

/**
 * ide_terminal_launcher_spawn_finish:
 * @self: a #IdeTerminalLauncher
 *
 * Completes a request to ide_terminal_launcher_spawn_async()
 *
 * Returns: %TRUE if the process executed successfully; otherwise %FALSE
 *   and @error is set.
 */
gboolean
ide_terminal_launcher_spawn_finish (IdeTerminalLauncher  *self,
                                    GAsyncResult         *result,
                                    GError              **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_TERMINAL_LAUNCHER (self));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
ide_terminal_launcher_dispose (GObject *object)
{
  IdeTerminalLauncher *self = (IdeTerminalLauncher *)object;

  g_clear_object (&self->context);
  g_clear_object (&self->run_command);
  g_clear_pointer (&self->override_environ, g_strfreev);

  G_OBJECT_CLASS (ide_terminal_launcher_parent_class)->dispose (object);
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
    case PROP_CONTEXT:
      g_value_set_object (value, self->context);
      break;

    case PROP_RUN_COMMAND:
      g_value_set_object (value, self->run_command);
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
    case PROP_CONTEXT:
      g_set_object (&self->context, g_value_get_object (value));
      break;

    case PROP_RUN_COMMAND:
      g_set_object (&self->run_command, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_terminal_launcher_class_init (IdeTerminalLauncherClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_terminal_launcher_dispose;
  object_class->get_property = ide_terminal_launcher_get_property;
  object_class->set_property = ide_terminal_launcher_set_property;

  properties [PROP_CONTEXT] =
    g_param_spec_object ("context",
                         "Context",
                         "The context for the launcher",
                         IDE_TYPE_CONTEXT,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_RUN_COMMAND] =
    g_param_spec_object ("run-command",
                         "Run Command",
                         "The run command to spawn",
                         IDE_TYPE_RUN_COMMAND,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_terminal_launcher_init (IdeTerminalLauncher *self)
{
}

/**
 * ide_terminal_launcher_new:
 * @context: an #IdeContext
 * @run_command: an #IdeRunCommand to spawn
 *
 * Create an #IdeTerminalLauncher that spawns @run_command.
 *
 * Returns: (transfer full): a newly created #IdeTerminalLauncher
 */
IdeTerminalLauncher *
ide_terminal_launcher_new (IdeContext    *context,
                           IdeRunCommand *run_command)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (IDE_IS_RUN_COMMAND (run_command), NULL);

  return g_object_new (IDE_TYPE_TERMINAL_LAUNCHER,
                       "context", context,
                       "run-command", run_command,
                       NULL);
}

/**
 * ide_terminal_launcher_copy:
 * @self: an #IdeTerminalLauncher
 *
 * Copies @self into a new launcher.
 *
 * Returns: (transfer full): a newly created #IdeTerminalLauncher
 */
IdeTerminalLauncher *
ide_terminal_launcher_copy (IdeTerminalLauncher *self)
{
  g_return_val_if_fail (IDE_IS_TERMINAL_LAUNCHER (self), NULL);

  return g_object_new (IDE_TYPE_TERMINAL_LAUNCHER,
                       "context", self->context,
                       "run-command", self->run_command,
                       NULL);
}

void
ide_terminal_launcher_set_override_environ (IdeTerminalLauncher *self,
                                            const char * const  *override_environ)
{
  char **copy;

  g_return_if_fail (IDE_IS_TERMINAL_LAUNCHER (self));

  copy = g_strdupv ((char **)override_environ);
  g_strfreev (self->override_environ);
  self->override_environ = copy;
}
