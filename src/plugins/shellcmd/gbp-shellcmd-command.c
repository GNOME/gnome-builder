/* gbp-shellcmd-command.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-shellcmd-command"

#include "config.h"

#include <glib/gi18n.h>
#include <libide-gui.h>
#include <libide-editor.h>
#include <libide-terminal.h>
#include <libide-threading.h>

#include "ide-gui-private.h"

#include "gbp-shellcmd-command.h"
#include "gbp-shellcmd-enums.h"

struct _GbpShellcmdCommand
{
  IdeObject                   parent_instance;
  GbpShellcmdCommandLocality  locality;
  gchar                      *shortcut;
  gchar                      *subtitle;
  gchar                      *title;
  gchar                      *command;
  gchar                      *cwd;
  IdeEnvironment             *environment;
};

enum {
  PROP_0,
  PROP_COMMAND,
  PROP_CWD,
  PROP_ENVIRONMENT,
  PROP_LOCALITY,
  PROP_SHORTCUT,
  PROP_SUBTITLE,
  PROP_TITLE,
  N_PROPS
};

static void command_iface_init (IdeCommandInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GbpShellcmdCommand, gbp_shellcmd_command, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_COMMAND, command_iface_init))

static GParamSpec *properties [N_PROPS];

static void
gbp_shellcmd_command_finalize (GObject *object)
{
  GbpShellcmdCommand *self = (GbpShellcmdCommand *)object;

  g_clear_pointer (&self->shortcut, g_free);
  g_clear_pointer (&self->title, g_free);
  g_clear_pointer (&self->command, g_free);
  g_clear_pointer (&self->cwd, g_free);
  g_clear_object (&self->environment);

  G_OBJECT_CLASS (gbp_shellcmd_command_parent_class)->finalize (object);
}

static void
gbp_shellcmd_command_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GbpShellcmdCommand *self = GBP_SHELLCMD_COMMAND (object);

  switch (prop_id)
    {
    case PROP_COMMAND:
      g_value_set_string (value, gbp_shellcmd_command_get_command (self));
      break;

    case PROP_ENVIRONMENT:
      g_value_set_object (value, gbp_shellcmd_command_get_environment (self));
      break;

    case PROP_CWD:
      g_value_set_string (value, gbp_shellcmd_command_get_cwd (self));
      break;

    case PROP_LOCALITY:
      g_value_set_enum (value, gbp_shellcmd_command_get_locality (self));
      break;

    case PROP_SHORTCUT:
      g_value_set_string (value, gbp_shellcmd_command_get_shortcut (self));
      break;

    case PROP_SUBTITLE:
      g_value_take_string (value, ide_command_get_subtitle (IDE_COMMAND (self)));
      break;

    case PROP_TITLE:
      g_value_take_string (value, ide_command_get_title (IDE_COMMAND (self)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_shellcmd_command_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GbpShellcmdCommand *self = GBP_SHELLCMD_COMMAND (object);

  switch (prop_id)
    {
    case PROP_COMMAND:
      gbp_shellcmd_command_set_command (self, g_value_get_string (value));
      break;

    case PROP_CWD:
      gbp_shellcmd_command_set_cwd (self, g_value_get_string (value));
      break;

    case PROP_LOCALITY:
      gbp_shellcmd_command_set_locality (self, g_value_get_enum (value));
      break;

    case PROP_SHORTCUT:
      gbp_shellcmd_command_set_shortcut (self, g_value_get_string (value));
      break;

    case PROP_SUBTITLE:
      gbp_shellcmd_command_set_subtitle (self, g_value_get_string (value));
      break;

    case PROP_TITLE:
      gbp_shellcmd_command_set_title (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_shellcmd_command_class_init (GbpShellcmdCommandClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_shellcmd_command_finalize;
  object_class->get_property = gbp_shellcmd_command_get_property;
  object_class->set_property = gbp_shellcmd_command_set_property;

  properties [PROP_COMMAND] =
    g_param_spec_string ("command",
                         "Command",
                         "The shell command to execute",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_CWD] =
    g_param_spec_string ("cwd",
                         "CWD",
                         "The working directory for the command",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_ENVIRONMENT] =
    g_param_spec_object ("environment",
                         "Environment",
                         "The environment variables for the command",
                         IDE_TYPE_ENVIRONMENT,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_LOCALITY] =
    g_param_spec_enum ("locality",
                       "Locality",
                       "Where the command should be executed",
                       GBP_TYPE_SHELLCMD_COMMAND_LOCALITY,
                       GBP_SHELLCMD_COMMAND_LOCALITY_HOST,
                       (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_SHORTCUT] =
    g_param_spec_string ("shortcut",
                         "Shortcut",
                         "The shortcut to use to activate the command",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_SUBTITLE] =
    g_param_spec_string ("subtitle",
                         "Subtitle",
                         "The subtitle of the command for display purposes",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_TITLE] =
    g_param_spec_string ("title",
                         "Title",
                         "The title of the command for display purposes",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));
  
  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_shellcmd_command_init (GbpShellcmdCommand *self)
{
}

const gchar *
gbp_shellcmd_command_get_cwd (GbpShellcmdCommand *self)
{
  g_return_val_if_fail (GBP_IS_SHELLCMD_COMMAND (self), NULL);

  return self->cwd;
}

void
gbp_shellcmd_command_set_cwd (GbpShellcmdCommand *self,
                              const gchar        *cwd)
{
  g_return_if_fail (GBP_IS_SHELLCMD_COMMAND (self));

  if (!ide_str_equal0 (cwd, self->cwd))
    {
      g_free (self->cwd);
      self->cwd = g_strdup (cwd);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CWD]);
    }
}

static gchar *
gbp_shellcmd_command_get_title (IdeCommand *command)
{
  GbpShellcmdCommand *self = (GbpShellcmdCommand *)command;

  g_assert (GBP_IS_SHELLCMD_COMMAND (self));

  if (self->title == NULL)
    return g_strdup (_("Shell command"));
  else
    return g_strdup (self->title);
}

static gchar *
gbp_shellcmd_command_get_subtitle (IdeCommand *command)
{
  GbpShellcmdCommand *self = (GbpShellcmdCommand *)command;

  g_assert (GBP_IS_SHELLCMD_COMMAND (self));

  if (self->subtitle)
    return g_strdup (self->subtitle);
  else
    return g_strdup (self->command);
}

static void
gbp_shellcmd_command_wait_check_cb (GObject      *object,
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
gbp_shellcmd_command_apply (GbpShellcmdCommand    *self,
                            IdeSubprocessLauncher *launcher,
                            GFile                 *relative_to)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) cwd = NULL;

  g_assert (GBP_IS_SHELLCMD_COMMAND (self));
  g_assert (IDE_IS_SUBPROCESS_LAUNCHER (launcher));
  g_assert (G_IS_FILE (relative_to));

  if (self->cwd != NULL)
    {
      if (g_path_is_absolute (self->cwd))
        cwd = g_file_new_for_path (self->cwd);
      else
        cwd = g_file_get_child (relative_to, self->cwd);
    }
  else
    {
      cwd = g_object_ref (relative_to);
    }

  ide_subprocess_launcher_set_cwd (launcher, g_file_peek_path (cwd));

  if (self->environment != NULL)
    {
      g_auto(GStrv) env = ide_environment_get_environ (self->environment);

      if (env != NULL)
        {
          for (guint i = 0; env[i]; i++)
            {
              g_autofree gchar *key = NULL;
              g_autofree gchar *val = NULL;

              if (ide_environ_parse (env[i], &key, &val))
                ide_subprocess_launcher_setenv (launcher, key, val, TRUE);
            }
        }
    }
}

static void
gbp_shellcmd_command_run_host (GbpShellcmdCommand  *self,
                               gchar              **argv,
                               IdeTask             *task)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeTerminalLauncher) tlauncher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) workdir = NULL;
  IdeWorkspace *workspace;
  IdeWorkbench *workbench;
  IdeSurface *surface;
  IdePage *page;

  g_assert (GBP_IS_SHELLCMD_COMMAND (self));
  g_assert (argv != NULL);
  g_assert (IDE_IS_TASK (task));

  context = ide_object_ref_context (IDE_OBJECT (self));
  workdir = ide_context_ref_workdir (context);

  if (!(workbench = _ide_workbench_from_context (context)) ||
      (!(workspace = ide_workbench_get_workspace_by_type (workbench, IDE_TYPE_PRIMARY_WORKSPACE)) &&
       !(workspace = ide_workbench_get_workspace_by_type (workbench, IDE_TYPE_EDITOR_WORKSPACE)) &&
       !(workspace = ide_workbench_get_workspace_by_type (workbench, IDE_TYPE_TERMINAL_WORKSPACE))) ||
      (!(surface = ide_workspace_get_surface_by_name (workspace, "editor")) &&
       !(surface = ide_workspace_get_surface_by_name (workspace, "terminal"))))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_FOUND,
                                 "Failed to locate a workspace for the terminal page");
      return;
    }

  launcher = ide_subprocess_launcher_new (0);

  ide_subprocess_launcher_push_args (launcher, (const gchar * const *)argv);
  ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
  ide_subprocess_launcher_set_clear_env (launcher, FALSE);

  gbp_shellcmd_command_apply (self, launcher, workdir);

  tlauncher = ide_terminal_launcher_new_for_launcher (launcher);
  page = g_object_new (IDE_TYPE_TERMINAL_PAGE,
                       "close-on-exit", FALSE,
                       "launcher", tlauncher,
                       "manage-spawn", TRUE,
                       "respawn-on-exit", FALSE,
                       "visible", TRUE,
                       NULL);
  gtk_container_add (GTK_CONTAINER (surface), GTK_WIDGET (page));

  ide_task_return_boolean (task, TRUE);
}

static void
gbp_shellcmd_command_run_app (GbpShellcmdCommand  *self,
                              gchar              **argv,
                              IdeTask             *task)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeTerminalLauncher) tlauncher = NULL;
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) workdir = NULL;
  IdeWorkspace *workspace;
  IdeWorkbench *workbench;
  IdeSurface *surface;
  IdePage *page;

  g_assert (GBP_IS_SHELLCMD_COMMAND (self));
  g_assert (argv != NULL);
  g_assert (IDE_IS_TASK (task));

  context = ide_object_ref_context (IDE_OBJECT (self));
  workdir = ide_context_ref_workdir (context);

  if (!(workbench = _ide_workbench_from_context (context)) ||
      (!(workspace = ide_workbench_get_workspace_by_type (workbench, IDE_TYPE_PRIMARY_WORKSPACE)) &&
       !(workspace = ide_workbench_get_workspace_by_type (workbench, IDE_TYPE_EDITOR_WORKSPACE)) &&
       !(workspace = ide_workbench_get_workspace_by_type (workbench, IDE_TYPE_TERMINAL_WORKSPACE))) ||
      (!(surface = ide_workspace_get_surface_by_name (workspace, "editor")) &&
       !(surface = ide_workspace_get_surface_by_name (workspace, "terminal"))))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_FOUND,
                                 "Failed to locate a workspace for the terminal page");
      return;
    }

  launcher = ide_subprocess_launcher_new (0);

  ide_subprocess_launcher_push_args (launcher, (const gchar * const *)argv);
  ide_subprocess_launcher_set_run_on_host (launcher, FALSE);
  ide_subprocess_launcher_set_clear_env (launcher, FALSE);

  gbp_shellcmd_command_apply (self, launcher, workdir);

  tlauncher = ide_terminal_launcher_new_for_launcher (launcher);
  page = g_object_new (IDE_TYPE_TERMINAL_PAGE,
                       "close-on-exit", FALSE,
                       "launcher", tlauncher,
                       "manage-spawn", TRUE,
                       "respawn-on-exit", FALSE,
                       "visible", TRUE,
                       NULL);
  gtk_container_add (GTK_CONTAINER (surface), GTK_WIDGET (page));

  ide_task_return_boolean (task, TRUE);
}

static void
gbp_shellcmd_command_run_runner (GbpShellcmdCommand  *self,
                                 gchar              **argv,
                                 IdeTask             *task)
{
  g_autoptr(IdeTerminalLauncher) launcher = NULL;
  g_autoptr(IdeContext) context = NULL;
  g_autofree gchar *cwd = NULL;
  IdeBuildManager *build_manager;
  IdeWorkspace *workspace;
  IdeWorkbench *workbench;
  IdePipeline *pipeline;
  IdeRuntime *runtime;
  IdeSurface *surface;
  IdePage *page;

  g_assert (GBP_IS_SHELLCMD_COMMAND (self));
  g_assert (argv != NULL);
  g_assert (IDE_IS_TASK (task));

  if (!(context = ide_object_ref_context (IDE_OBJECT (self))) ||
      !(build_manager = ide_build_manager_from_context (context)) ||
      !(pipeline = ide_build_manager_get_pipeline (build_manager)) ||
      !(runtime = ide_pipeline_get_runtime (pipeline)))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_INITIALIZED,
                                 _("Cannot spawn terminal in runtime environment because build pipeline is not initialized"));
      return;
    }

  if (!(workbench = _ide_workbench_from_context (context)) ||
      (!(workspace = ide_workbench_get_workspace_by_type (workbench, IDE_TYPE_PRIMARY_WORKSPACE)) &&
       !(workspace = ide_workbench_get_workspace_by_type (workbench, IDE_TYPE_EDITOR_WORKSPACE)) &&
       !(workspace = ide_workbench_get_workspace_by_type (workbench, IDE_TYPE_TERMINAL_WORKSPACE))) ||
      (!(surface = ide_workspace_get_surface_by_name (workspace, "editor")) &&
       !(surface = ide_workspace_get_surface_by_name (workspace, "terminal"))))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_FOUND,
                                 "Failed to locate a workspace for the terminal page");
      return;
    }

  launcher = ide_terminal_launcher_new_for_runner (runtime);
  ide_terminal_launcher_set_shell (launcher, argv[0]);
  ide_terminal_launcher_set_args (launcher, (const gchar * const *)&argv[1]);

  if (self->cwd != NULL)
    {
      if (g_path_is_absolute (self->cwd))
        cwd = g_strdup (self->cwd);
      else
        ide_pipeline_build_builddir_path (pipeline, self->cwd, NULL);
    }

  if (cwd != NULL)
    ide_terminal_launcher_set_cwd (launcher, cwd);
  else
    ide_terminal_launcher_set_cwd (launcher,
                                   ide_pipeline_get_builddir (pipeline));

  page = g_object_new (IDE_TYPE_TERMINAL_PAGE,
                       "close-on-exit", FALSE,
                       "launcher", launcher,
                       "manage-spawn", TRUE,
                       "respawn-on-exit", FALSE,
                       "visible", TRUE,
                       NULL);
  gtk_container_add (GTK_CONTAINER (surface), GTK_WIDGET (page));

  ide_task_return_boolean (task, TRUE);
}

static void
gbp_shellcmd_command_run_build (GbpShellcmdCommand  *self,
                                gchar              **argv,
                                IdeTask             *task)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) builddir = NULL;
  IdeBuildManager *build_manager;
  GCancellable *cancellable;
  IdeWorkbench *workbench;
  IdeWorkspace *workspace;
  IdePipeline *pipeline;

  g_assert (GBP_IS_SHELLCMD_COMMAND (self));
  g_assert (argv != NULL);
  g_assert (IDE_IS_TASK (task));

  context = ide_object_ref_context (IDE_OBJECT (self));
  workbench = _ide_workbench_from_context (context);
  workspace = ide_workbench_get_current_workspace (workbench);
  build_manager = ide_build_manager_from_context (context);
  pipeline = ide_build_manager_get_pipeline (build_manager);

  if (pipeline == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_INITIALIZED,
                                 _("Cannot spawn process because build pipeline is not yet available"));
      return;
    }

  builddir = g_file_new_for_path (ide_pipeline_get_builddir (pipeline));
  launcher = ide_pipeline_create_launcher (pipeline, &error);

  if (launcher == NULL)
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  ide_pipeline_attach_pty (pipeline, launcher);
  ide_subprocess_launcher_push_args (launcher, (const gchar * const *)argv);

  if (G_IS_ACTION_GROUP (workspace) &&
      g_action_group_has_action (G_ACTION_GROUP (workspace), "view-output"))
    dzl_gtk_widget_action (GTK_WIDGET (workspace), "win", "view-output", NULL);

  gbp_shellcmd_command_apply (self, launcher, builddir);

  cancellable = ide_task_get_cancellable (task);
  subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, &error);

  if (subprocess == NULL)
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_subprocess_wait_check_async (subprocess,
                                     cancellable,
                                     gbp_shellcmd_command_wait_check_cb,
                                     g_object_ref (task));
}

static void
gbp_shellcmd_command_run_async (IdeCommand          *command,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  GbpShellcmdCommand *self = (GbpShellcmdCommand *)command;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GError) error = NULL;
  g_auto(GStrv) argv = NULL;
  gint argc = 0;

  g_assert (GBP_IS_SHELLCMD_COMMAND (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_shellcmd_command_run_async);

  if (self->command == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "No command to execute");
      return;
    }

  if (!g_shell_parse_argv (self->command, &argc, &argv, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  switch (self->locality)
    {
    case GBP_SHELLCMD_COMMAND_LOCALITY_HOST:
      gbp_shellcmd_command_run_host (self, argv, task);
      break;

    case GBP_SHELLCMD_COMMAND_LOCALITY_APP:
      gbp_shellcmd_command_run_app (self, argv, task);
      break;

    case GBP_SHELLCMD_COMMAND_LOCALITY_BUILD:
      gbp_shellcmd_command_run_build (self, argv, task);
      break;

    case GBP_SHELLCMD_COMMAND_LOCALITY_RUN:
      gbp_shellcmd_command_run_runner (self, argv, task);
      break;

    default:
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "Unknown command locality");
      return;
    }
}

static gboolean
gbp_shellcmd_command_run_finish (IdeCommand    *command,
                                 GAsyncResult  *result,
                                 GError       **error)
{
  g_assert (GBP_IS_SHELLCMD_COMMAND (command));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
command_iface_init (IdeCommandInterface *iface)
{
  iface->get_title = gbp_shellcmd_command_get_title;
  iface->get_subtitle = gbp_shellcmd_command_get_subtitle;
  iface->run_async = gbp_shellcmd_command_run_async;
  iface->run_finish = gbp_shellcmd_command_run_finish;
}

GbpShellcmdCommandLocality
gbp_shellcmd_command_get_locality (GbpShellcmdCommand *self)
{
  g_return_val_if_fail (GBP_IS_SHELLCMD_COMMAND (self), 0);

  return self->locality;
}

void
gbp_shellcmd_command_set_locality (GbpShellcmdCommand         *self,
                                   GbpShellcmdCommandLocality  locality)
{
  g_return_if_fail (GBP_IS_SHELLCMD_COMMAND (self));

  if (self->locality != locality)
    {
      self->locality = locality;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LOCALITY]);
    }

}

const gchar *
gbp_shellcmd_command_get_command (GbpShellcmdCommand *self)
{
  g_return_val_if_fail (GBP_IS_SHELLCMD_COMMAND (self), NULL);

  return self->command;
}

void
gbp_shellcmd_command_set_command (GbpShellcmdCommand *self,
                                  const gchar        *command)
{
  g_return_if_fail (GBP_IS_SHELLCMD_COMMAND (self));

  if (!ide_str_equal0 (command, self->command))
    {
      g_free (self->command);
      self->command = g_strdup (command);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_COMMAND]);
    }
}

IdeEnvironment *
gbp_shellcmd_command_get_environment (GbpShellcmdCommand *self)
{
  g_return_val_if_fail (GBP_IS_SHELLCMD_COMMAND (self), NULL);

  if (self->environment == NULL)
    self->environment = ide_environment_new ();

  return self->environment;
}

const gchar *
gbp_shellcmd_command_get_shortcut (GbpShellcmdCommand *self)
{
  g_return_val_if_fail (GBP_IS_SHELLCMD_COMMAND (self), NULL);

  return self->shortcut;
}

void
gbp_shellcmd_command_set_shortcut (GbpShellcmdCommand *self,
                                   const gchar        *shortcut)
{
  g_return_if_fail (GBP_IS_SHELLCMD_COMMAND (self));

  if (!ide_str_equal0 (shortcut, self->shortcut))
    {
      g_free (self->shortcut);
      self->shortcut = g_strdup (shortcut);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SHORTCUT]);
    }
}

void
gbp_shellcmd_command_set_title (GbpShellcmdCommand *self,
                                const gchar        *title)
{
  g_return_if_fail (GBP_IS_SHELLCMD_COMMAND (self));

  if (!ide_str_equal0 (title, self->title))
    {
      g_free (self->title);
      self->title = g_strdup (title);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TITLE]);
    }
}

void
gbp_shellcmd_command_set_subtitle (GbpShellcmdCommand *self,
                                   const gchar        *subtitle)
{
  g_return_if_fail (GBP_IS_SHELLCMD_COMMAND (self));

  if (!ide_str_equal0 (subtitle, self->subtitle))
    {
      g_free (self->subtitle);
      self->subtitle = g_strdup (subtitle);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SUBTITLE]);
    }
}
