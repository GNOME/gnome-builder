/* ide-terminal-run-command.c
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

#define G_LOG_DOMAIN "ide-terminal-run-command"

#include "config.h"

#include <libide-io.h>

#include "ide-terminal-run-command-private.h"

struct _IdeTerminalRunCommand
{
  IdeRunCommand          parent_instance;
  IdeTerminalRunLocality locality;
};

G_DEFINE_FINAL_TYPE (IdeTerminalRunCommand, ide_terminal_run_command, IDE_TYPE_RUN_COMMAND)

static void
ide_terminal_run_command_prepare_to_run (IdeRunCommand *run_command,
                                         IdeRunContext *run_context,
                                         IdeContext    *context)
{
  IdeTerminalRunCommand *self = (IdeTerminalRunCommand *)run_command;
  const char *user_shell;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TERMINAL_RUN_COMMAND (self));
  g_assert (IDE_IS_RUN_CONTEXT (run_context));
  g_assert (IDE_IS_CONTEXT (context));

  user_shell = ide_get_user_shell ();

  switch (self->locality)
    {
    case IDE_TERMINAL_RUN_ON_HOST:
      ide_run_context_push_host (run_context);
      ide_run_context_add_minimal_environment (run_context);
      ide_run_context_append_argv (run_context, user_shell);
      if (ide_shell_supports_dash_login (user_shell))
        ide_run_context_append_argv (run_context, "--login");
      break;

    case IDE_TERMINAL_RUN_AS_SUBPROCESS:
      ide_run_context_add_minimal_environment (run_context);
      if (g_find_program_in_path (user_shell))
        {
          ide_run_context_append_argv (run_context, user_shell);
          if (ide_shell_supports_dash_login (user_shell))
            ide_run_context_append_argv (run_context, "--login");
        }
      else
        {
          ide_run_context_append_argv (run_context, "/bin/sh");
          ide_run_context_append_argv (run_context, "--login");
        }
      break;

    case IDE_TERMINAL_RUN_IN_RUNTIME:
    case IDE_TERMINAL_RUN_IN_PIPELINE:
      {
        IdeBuildManager *build_manager;
        IdePipeline *pipeline;
        IdeRuntime *runtime;

        if (!ide_context_has_project (context) ||
            !(build_manager = ide_build_manager_from_context (context)) ||
            !(pipeline = ide_build_manager_get_pipeline (build_manager)) ||
            !(runtime = ide_pipeline_get_runtime (pipeline)))
          {
            ide_run_context_push_error (run_context,
                                        g_error_new (G_IO_ERROR,
                                                     G_IO_ERROR_NOT_INITIALIZED,
                                                     "Cannot spawn terminal without a pipeline"));
            break;
          }

        if (!ide_runtime_contains_program_in_path (runtime, user_shell, NULL))
          user_shell = "/bin/sh";

        if (self->locality == IDE_TERMINAL_RUN_IN_PIPELINE)
          ide_pipeline_prepare_run_context (pipeline, run_context);
        else
          ide_runtime_prepare_to_run (runtime, pipeline, run_context);

        ide_run_context_append_argv (run_context, user_shell);
        if (ide_shell_supports_dash_login (user_shell))
          ide_run_context_append_argv (run_context, "--login");
      }
      break;

    case IDE_TERMINAL_RUN_LAST:
    default:
      g_assert_not_reached ();
    }

  IDE_RUN_COMMAND_CLASS (ide_terminal_run_command_parent_class)->prepare_to_run (run_command, run_context, context);

  IDE_EXIT;
}

static void
ide_terminal_run_command_class_init (IdeTerminalRunCommandClass *klass)
{
  IdeRunCommandClass *run_command_class = IDE_RUN_COMMAND_CLASS (klass);

  run_command_class->prepare_to_run = ide_terminal_run_command_prepare_to_run;
}

static void
ide_terminal_run_command_init (IdeTerminalRunCommand *self)
{
}

IdeRunCommand *
ide_terminal_run_command_new (IdeTerminalRunLocality locality)
{
  IdeTerminalRunCommand *self;

  self = g_object_new (IDE_TYPE_TERMINAL_RUN_COMMAND, NULL);
  self->locality = locality;

  return IDE_RUN_COMMAND (self);
}
