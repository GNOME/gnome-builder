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
  gint                        priority;

  gchar                      *id;
  gchar                      *shortcut;
  gchar                      *subtitle;
  gchar                      *title;
  gchar                      *command;
  gchar                      *cwd;
  IdeEnvironment             *environment;

  guint                       close_on_exit : 1;
};

enum {
  PROP_0,
  PROP_CLOSE_ON_EXIT,
  PROP_COMMAND,
  PROP_CWD,
  PROP_ENV,
  PROP_ENVIRONMENT,
  PROP_ID,
  PROP_LOCALITY,
  PROP_SHORTCUT,
  PROP_SUBTITLE,
  PROP_TITLE,
  N_PROPS
};

enum {
  CHANGED,
  N_SIGNALS
};

static void command_iface_init (IdeCommandInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GbpShellcmdCommand, gbp_shellcmd_command, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_COMMAND, command_iface_init))

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

static void
gbp_shellcmd_command_changed (GbpShellcmdCommand *self)
{
  g_assert (GBP_IS_SHELLCMD_COMMAND (self));

  g_signal_emit (self, signals [CHANGED], 0);
}

static void
gbp_shellcmd_command_set_env (GbpShellcmdCommand  *self,
                              const gchar * const *env)
{
  IdeEnvironment *dest;

  g_return_if_fail (GBP_IS_SHELLCMD_COMMAND (self));

  dest = gbp_shellcmd_command_get_environment (self);
  ide_environment_set_environ (dest, env);
  gbp_shellcmd_command_changed (self);
}

static void
gbp_shellcmd_command_finalize (GObject *object)
{
  GbpShellcmdCommand *self = (GbpShellcmdCommand *)object;

  g_clear_pointer (&self->command, g_free);
  g_clear_pointer (&self->cwd, g_free);
  g_clear_pointer (&self->id, g_free);
  g_clear_pointer (&self->shortcut, g_free);
  g_clear_pointer (&self->title, g_free);
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
    case PROP_ID:
      g_value_set_string (value, gbp_shellcmd_command_get_id (self));
      break;

    case PROP_CLOSE_ON_EXIT:
      g_value_set_boolean (value, gbp_shellcmd_command_get_close_on_exit (self));
      break;

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
    case PROP_ID:
      self->id = g_value_dup_string (value);
      break;

    case PROP_CLOSE_ON_EXIT:
      gbp_shellcmd_command_set_close_on_exit (self, g_value_get_boolean (value));
      break;

    case PROP_COMMAND:
      gbp_shellcmd_command_set_command (self, g_value_get_string (value));
      break;

    case PROP_CWD:
      gbp_shellcmd_command_set_cwd (self, g_value_get_string (value));
      break;

    case PROP_ENV:
      gbp_shellcmd_command_set_env (self, g_value_get_boxed (value));
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

  properties [PROP_ID] =
    g_param_spec_string ("id",
                         "Id",
                         "The command identifier, if any",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_CLOSE_ON_EXIT] =
    g_param_spec_boolean ("close-on-exit",
                          "Close on Exit",
                          "If the terminal should automatically close after running",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

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

  properties [PROP_ENV] =
    g_param_spec_boxed ("env", NULL, NULL,
                        G_TYPE_STRV,
                        (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

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

  signals [CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void
gbp_shellcmd_command_init (GbpShellcmdCommand *self)
{
  self->priority = G_MAXINT;
}

const gchar *
gbp_shellcmd_command_get_cwd (GbpShellcmdCommand *self)
{
  g_return_val_if_fail (GBP_IS_SHELLCMD_COMMAND (self), NULL);

  return self->cwd ? self->cwd : "";
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
      gbp_shellcmd_command_changed (self);
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
gbp_shellcmd_command_apply (GbpShellcmdCommand    *self,
                            IdeSubprocessLauncher *launcher,
                            GFile                 *relative_to)
{
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) workdir = NULL;
  g_autoptr(GFile) cwd = NULL;
  const gchar *builddir = NULL;

  g_assert (GBP_IS_SHELLCMD_COMMAND (self));
  g_assert (IDE_IS_SUBPROCESS_LAUNCHER (launcher));
  g_assert (G_IS_FILE (relative_to));

  context = ide_object_ref_context (IDE_OBJECT (self));
  workdir = ide_context_ref_workdir (context);

  if (ide_context_has_project (context))
    {
      IdeBuildManager *build_manager = ide_build_manager_from_context (context);
      IdePipeline *pipeline = ide_build_manager_get_pipeline (build_manager);

      if (pipeline != NULL)
        builddir = ide_pipeline_get_builddir (pipeline);
    }

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

  ide_subprocess_launcher_setenv (launcher, "INSIDE_GNOME_BUILDER", PACKAGE_VERSION, TRUE);
  ide_subprocess_launcher_setenv (launcher, "SRCDIR", g_file_peek_path (workdir), TRUE);
  if (builddir != NULL)
    ide_subprocess_launcher_setenv (launcher, "BUILDDIR", builddir, TRUE);

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
                       "close-on-exit", self->close_on_exit,
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
                       "close-on-exit", self->close_on_exit,
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
                       "close-on-exit", self->close_on_exit,
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
  g_autoptr(IdeTerminalLauncher) tlauncher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) builddir = NULL;
  IdeBuildManager *build_manager;
  IdeWorkspace *workspace;
  IdeWorkbench *workbench;
  IdePipeline *pipeline;
  IdeSurface *surface;
  IdePage *page;

  g_assert (GBP_IS_SHELLCMD_COMMAND (self));
  g_assert (argv != NULL);
  g_assert (IDE_IS_TASK (task));

  context = ide_object_ref_context (IDE_OBJECT (self));
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


  builddir = g_file_new_for_path (ide_pipeline_get_builddir (pipeline));
  launcher = ide_pipeline_create_launcher (pipeline, &error);

  if (launcher == NULL)
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  ide_subprocess_launcher_push_args (launcher, (const gchar * const *)argv);
  gbp_shellcmd_command_apply (self, launcher, builddir);

  tlauncher = ide_terminal_launcher_new_for_launcher (launcher);
  page = g_object_new (IDE_TYPE_TERMINAL_PAGE,
                       "close-on-exit", self->close_on_exit,
                       "launcher", tlauncher,
                       "manage-spawn", TRUE,
                       "respawn-on-exit", FALSE,
                       "visible", TRUE,
                       NULL);
  gtk_container_add (GTK_CONTAINER (surface), GTK_WIDGET (page));

  ide_task_return_boolean (task, TRUE);
}

static void
gbp_shellcmd_command_run_async (IdeCommand          *command,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  GbpShellcmdCommand *self = (GbpShellcmdCommand *)command;
  g_autoptr(GPtrArray) with_sh = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GError) error = NULL;
  g_auto(GStrv) argv = NULL;
  gchar **real_argv;
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

  with_sh = g_ptr_array_new ();
  g_ptr_array_add (with_sh, (gchar *)"/bin/sh");
  g_ptr_array_add (with_sh, (gchar *)"-c");
  g_ptr_array_add (with_sh, (gchar *)self->command);
  g_ptr_array_add (with_sh, NULL);

  real_argv = (gchar **)(gpointer)with_sh->pdata;

  switch (self->locality)
    {
    case GBP_SHELLCMD_COMMAND_LOCALITY_HOST:
      gbp_shellcmd_command_run_host (self, real_argv, task);
      break;

    case GBP_SHELLCMD_COMMAND_LOCALITY_APP:
      gbp_shellcmd_command_run_app (self, real_argv, task);
      break;

    case GBP_SHELLCMD_COMMAND_LOCALITY_BUILD:
      gbp_shellcmd_command_run_build (self, real_argv, task);
      break;

    case GBP_SHELLCMD_COMMAND_LOCALITY_RUN:
      gbp_shellcmd_command_run_runner (self, real_argv, task);
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

static GIcon *
gbp_shellcmd_command_get_icon (IdeCommand *command)
{
  static GIcon *icon;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SHELLCMD_COMMAND (command));

  if (icon == NULL)
    icon = g_themed_icon_new ("utilities-terminal-symbolic");

  return g_object_ref (icon);
}

static gint
gbp_shellcmd_command_get_priority (IdeCommand *command)
{
  return GBP_SHELLCMD_COMMAND (command)->priority;
}

static void
command_iface_init (IdeCommandInterface *iface)
{
  iface->get_icon = gbp_shellcmd_command_get_icon;
  iface->get_title = gbp_shellcmd_command_get_title;
  iface->get_subtitle = gbp_shellcmd_command_get_subtitle;
  iface->run_async = gbp_shellcmd_command_run_async;
  iface->run_finish = gbp_shellcmd_command_run_finish;
  iface->get_priority = gbp_shellcmd_command_get_priority;
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
      gbp_shellcmd_command_changed (self);
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
      gbp_shellcmd_command_changed (self);
    }
}

IdeEnvironment *
gbp_shellcmd_command_get_environment (GbpShellcmdCommand *self)
{
  g_return_val_if_fail (GBP_IS_SHELLCMD_COMMAND (self), NULL);

  if (self->environment == NULL)
    {
      self->environment = ide_environment_new ();

      g_signal_connect_object (self->environment,
                               "changed",
                               G_CALLBACK (gbp_shellcmd_command_changed),
                               self,
                               G_CONNECT_SWAPPED);
    }

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
      gbp_shellcmd_command_changed (self);
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
      gbp_shellcmd_command_changed (self);
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
      gbp_shellcmd_command_changed (self);
    }
}

GbpShellcmdCommand *
gbp_shellcmd_command_from_key_file (GKeyFile     *keyfile,
                                    const gchar  *group,
                                    GError      **error)
{
  g_autoptr(GbpShellcmdCommand) self = NULL;
  g_autofree gchar *id = NULL;
  struct {
    const gchar *key_name;
    const gchar *prop_name;
    GType        type;
    gboolean     required;
    gboolean     found;
  } keys[] = {
    { "Locality", "locality", GBP_TYPE_SHELLCMD_COMMAND_LOCALITY, FALSE },
    { "Shortcut", "shortcut", G_TYPE_STRING, TRUE },
    { "Title", "title", G_TYPE_STRING, FALSE },
    { "Command", "command", G_TYPE_STRING, TRUE },
    { "Directory", "cwd", G_TYPE_STRING, FALSE },
    { "Environment", "env", G_TYPE_STRV, FALSE },
    { "CloseOnExit", "close-on-exit", G_TYPE_BOOLEAN, FALSE },
  };

  g_return_val_if_fail (keyfile != NULL, NULL);
  g_return_val_if_fail (group != NULL, NULL);

  id = g_strdelimit (g_strdup (group), "'\" ", '-');
  self = g_object_new (GBP_TYPE_SHELLCMD_COMMAND,
                       "id", id,
                       NULL);

  for (guint i = 0; i < G_N_ELEMENTS (keys); i++)
    {
      if (g_type_is_a (keys[i].type, G_TYPE_STRING))
        {
          g_autofree gchar *val = NULL;

          if (!(val = g_key_file_get_string (keyfile, group, keys[i].key_name, NULL)))
            continue;

          keys[i].found = TRUE;

          g_object_set (self, keys[i].prop_name, val, NULL);
        }
      else if (g_type_is_a (keys[i].type, G_TYPE_STRV))
        {
          g_auto(GStrv) val = NULL;

          if (!(val = g_key_file_get_string_list (keyfile, group, keys[i].key_name, NULL, NULL)))
            continue;

          keys[i].found = TRUE;

          g_object_set (self, keys[i].prop_name, val, NULL);
        }
      else if (g_type_is_a (keys[i].type, G_TYPE_BOOLEAN))
        {
          gboolean ret = g_key_file_get_boolean (keyfile, group, keys[i].key_name, NULL);

          keys[i].found = TRUE;

          g_object_set (self, keys[i].prop_name, ret, NULL);
        }
      else if (g_type_is_a (keys[i].type, G_TYPE_ENUM))
        {
          g_autoptr(GEnumClass) eclass = g_type_class_ref (keys[i].type);
          g_autofree gchar *val = NULL;
          GEnumValue *eval;

          if (!(val = g_key_file_get_string (keyfile, group, keys[i].key_name, NULL)))
            continue;

          if (!(eval = g_enum_get_value_by_nick (eclass, val)))
            continue;

          keys[i].found = TRUE;

          g_object_set (self, keys[i].prop_name, eval->value, NULL);
        }
      else
        {
          g_assert_not_reached ();
        }
    }

  for (guint i = 0; i < G_N_ELEMENTS (keys); i++)
    {
      if (keys[i].required && !keys[i].found)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_DATA,
                       "Missing key %s from command %s",
                       keys[i].key_name, group);
          return NULL;
        }
    }

  return g_steal_pointer (&self);
}

void
gbp_shellcmd_command_to_key_file (GbpShellcmdCommand  *self,
                                  GKeyFile            *keyfile)
{
  g_autoptr(GEnumClass) locality_class = NULL;
  const GEnumValue *value;
  g_auto(GStrv) env = NULL;
  const gchar *localitystr = NULL;
  const gchar *group;

  g_return_if_fail (GBP_IS_SHELLCMD_COMMAND (self));
  g_return_if_fail (keyfile != NULL);

  group = self->id;

  if (self->environment != NULL)
    env = ide_environment_get_environ (self->environment);
  else
    env = g_new0 (gchar *, 1);

  locality_class = g_type_class_ref (GBP_TYPE_SHELLCMD_COMMAND_LOCALITY);

  if ((value = g_enum_get_value (locality_class, self->locality)))
    localitystr = value->value_nick;

  g_key_file_set_string (keyfile, group, "Locality", localitystr ?: "");
  g_key_file_set_string (keyfile, group, "Shortcut", self->shortcut ?: "");
  g_key_file_set_string (keyfile, group, "Title", self->title ?: "");
  g_key_file_set_string (keyfile, group, "Command", self->command ?: "");
  g_key_file_set_string (keyfile, group, "Directory", self->cwd ?: "");
  g_key_file_set_boolean (keyfile, group, "CloseOnExit", self->close_on_exit);
  g_key_file_set_string_list (keyfile, group, "Environment", (const gchar * const *)env, g_strv_length (env));
}

const gchar *
gbp_shellcmd_command_get_id (GbpShellcmdCommand *self)
{
  g_return_val_if_fail (GBP_IS_SHELLCMD_COMMAND (self), NULL);

  return self->id;
}

GbpShellcmdCommand *
gbp_shellcmd_command_copy (GbpShellcmdCommand *self)
{
  GbpShellcmdCommand *ret;

  g_return_val_if_fail (GBP_IS_SHELLCMD_COMMAND (self), NULL);

  ret = g_object_new (GBP_TYPE_SHELLCMD_COMMAND, NULL);
  ret->locality = self->locality;
  ret->priority = self->priority;
  ret->id = g_strdup (self->id);
  ret->shortcut = g_strdup (self->shortcut);
  ret->title = g_strdup (self->title);
  ret->subtitle = g_strdup (self->subtitle);
  ret->command = g_strdup (self->command);
  ret->cwd = g_strdup (self->cwd);
  ret->close_on_exit = self->close_on_exit;

  if (self->environment != NULL)
    {
      g_auto(GStrv) env = ide_environment_get_environ (self->environment);
      IdeEnvironment *dest = gbp_shellcmd_command_get_environment (ret);
      ide_environment_set_environ (dest, (const gchar * const *)env);
    }

  return g_steal_pointer (&ret);
}

void
gbp_shellcmd_command_set_priority (GbpShellcmdCommand *self,
                                   gint                priority)
{
  g_return_if_fail (GBP_IS_SHELLCMD_COMMAND (self));

  self->priority = priority;
}

gboolean
gbp_shellcmd_command_get_close_on_exit (GbpShellcmdCommand *self)
{
  g_return_val_if_fail (GBP_IS_SHELLCMD_COMMAND (self), FALSE);

  return self->close_on_exit;
}

void
gbp_shellcmd_command_set_close_on_exit (GbpShellcmdCommand *self,
                                        gboolean            close_on_exit)
{
  g_return_if_fail (GBP_IS_SHELLCMD_COMMAND (self));

  close_on_exit = !!close_on_exit;

  if (close_on_exit != self->close_on_exit)
    {
      self->close_on_exit = close_on_exit;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CLOSE_ON_EXIT]);
      gbp_shellcmd_command_changed (self);
    }
}
