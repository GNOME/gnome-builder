/* gbp-shellcmd-run-command.c
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

#define G_LOG_DOMAIN "gbp-shellcmd-run-command"

#include "config.h"

#include <glib/gi18n.h>

#include "gbp-shellcmd-enums.h"
#include "gbp-shellcmd-run-command.h"

struct _GbpShellcmdRunCommand
{
  IdeRunCommand        parent_instance;

  char                *settings_path;
  GSettings           *settings;
  char                *id;
  char                *accelerator;

  GbpShellcmdLocality  locality;
};

enum {
  PROP_0,
  PROP_ACCELERATOR,
  PROP_ACCELERATOR_LABEL,
  PROP_LOCALITY,
  PROP_SETTINGS_PATH,
  PROP_SUBTITLE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GbpShellcmdRunCommand, gbp_shellcmd_run_command, IDE_TYPE_RUN_COMMAND)

static GParamSpec *properties [N_PROPS];

static void
gbp_shellcmd_run_command_constructed (GObject *object)
{
  GbpShellcmdRunCommand *self = (GbpShellcmdRunCommand *)object;
  g_autofree char *id = NULL;
  g_auto(GStrv) path_split = NULL;
  gsize n_parts;

  g_assert (GBP_IS_SHELLCMD_RUN_COMMAND (self));
  g_assert (self->settings_path != NULL);
  g_assert (g_str_has_suffix (self->settings_path, "/"));

  self->settings = g_settings_new_with_path ("org.gnome.builder.shellcmd.command", self->settings_path);

  path_split = g_strsplit (self->settings_path, "/", 0);
  n_parts = g_strv_length (path_split);
  g_assert (n_parts >= 2);

  self->id = g_strdup (path_split[n_parts-2]);
  id = g_strdup_printf ("shellcmd:%s", self->id);

  ide_run_command_set_id (IDE_RUN_COMMAND (self), id);
  g_settings_bind (self->settings, "display-name", self, "display-name", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings, "env", self, "env", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings, "argv", self, "argv", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings, "cwd", self, "cwd", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings, "accelerator", self, "accelerator", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings, "locality", self, "locality", G_SETTINGS_BIND_DEFAULT);
}

static void
subtitle_changed_cb (GbpShellcmdRunCommand *self)
{
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SUBTITLE]);
}

static char *
get_subtitle (GbpShellcmdRunCommand *self)
{
  g_autofree char *joined = NULL;
  const char * const *argv;
  const char *cwd;

  g_assert (GBP_IS_SHELLCMD_RUN_COMMAND (self));

  argv = ide_run_command_get_argv (IDE_RUN_COMMAND (self));
  cwd = ide_run_command_get_cwd (IDE_RUN_COMMAND (self));

  if (argv != NULL)
    joined = g_strjoinv (" ", (char **)argv);

  if (joined && cwd)
    /* something like a bash prompt */
    return g_strdup_printf ("<tt>%s&gt; %s</tt>", cwd, joined);

  if (cwd)
    return g_strdup_printf ("%s&gt; ", cwd);

  return g_steal_pointer (&joined);
}

static void
accelerator_label_changed_cb (GbpShellcmdRunCommand *self)
{
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACCELERATOR_LABEL]);
}

static char *
get_accelerator_label (GbpShellcmdRunCommand *self)
{
  GdkModifierType state;
  guint keyval;

  if (ide_str_empty0 (self->accelerator))
    return NULL;

  if (gtk_accelerator_parse (self->accelerator, &keyval, &state))
    return gtk_accelerator_get_label (keyval, state);

  return NULL;
}

static void
gbp_shellcmd_run_command_dispose (GObject *object)
{
  GbpShellcmdRunCommand *self = (GbpShellcmdRunCommand *)object;

  g_clear_pointer (&self->accelerator, g_free);
  g_clear_pointer (&self->id, g_free);
  g_clear_pointer (&self->settings_path, g_free);
  g_clear_object (&self->settings);

  G_OBJECT_CLASS (gbp_shellcmd_run_command_parent_class)->dispose (object);
}

static void
gbp_shellcmd_run_command_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  GbpShellcmdRunCommand *self = GBP_SHELLCMD_RUN_COMMAND (object);

  switch (prop_id)
    {
    case PROP_ACCELERATOR:
      g_value_set_string (value, gbp_shellcmd_run_command_get_accelerator (self));
      break;

    case PROP_ACCELERATOR_LABEL:
      g_value_take_string (value, get_accelerator_label (self));
      break;

    case PROP_LOCALITY:
      g_value_set_enum (value, gbp_shellcmd_run_command_get_locality (self));
      break;

    case PROP_SETTINGS_PATH:
      g_value_set_string (value, self->settings_path);
      break;

    case PROP_SUBTITLE:
      g_value_take_string (value, get_subtitle (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_shellcmd_run_command_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  GbpShellcmdRunCommand *self = GBP_SHELLCMD_RUN_COMMAND (object);

  switch (prop_id)
    {
    case PROP_ACCELERATOR:
      gbp_shellcmd_run_command_set_accelerator (self, g_value_get_string (value));
      break;

    case PROP_LOCALITY:
      gbp_shellcmd_run_command_set_locality (self, g_value_get_enum (value));
      break;

    case PROP_SETTINGS_PATH:
      self->settings_path = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_shellcmd_run_command_class_init (GbpShellcmdRunCommandClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gbp_shellcmd_run_command_constructed;
  object_class->dispose = gbp_shellcmd_run_command_dispose;
  object_class->get_property = gbp_shellcmd_run_command_get_property;
  object_class->set_property = gbp_shellcmd_run_command_set_property;

  properties [PROP_ACCELERATOR] =
    g_param_spec_string ("accelerator", NULL, NULL, NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_ACCELERATOR_LABEL] =
    g_param_spec_string ("accelerator-label", NULL, NULL, NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_LOCALITY] =
    g_param_spec_enum ("locality", NULL, NULL,
                       GBP_TYPE_SHELLCMD_LOCALITY,
                       GBP_SHELLCMD_LOCALITY_PIPELINE,
                       (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_SETTINGS_PATH] =
    g_param_spec_string ("settings-path", NULL, NULL, NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_SUBTITLE] =
    g_param_spec_string ("subtitle", NULL, NULL, NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_shellcmd_run_command_init (GbpShellcmdRunCommand *self)
{
  self->locality = GBP_SHELLCMD_LOCALITY_PIPELINE;

  g_signal_connect (self, "notify::accelerator", G_CALLBACK (accelerator_label_changed_cb), NULL);
  g_signal_connect (self, "notify::cwd", G_CALLBACK (subtitle_changed_cb), NULL);
  g_signal_connect (self, "notify::argv", G_CALLBACK (subtitle_changed_cb), NULL);
}

GbpShellcmdRunCommand *
gbp_shellcmd_run_command_new (const char *settings_path)
{
  return g_object_new (GBP_TYPE_SHELLCMD_RUN_COMMAND,
                       "settings-path", settings_path,
                       NULL);
}

void
gbp_shellcmd_run_command_delete (GbpShellcmdRunCommand *self)
{
  g_autoptr(GSettingsSchema) schema = NULL;
  g_autoptr(GStrvBuilder) builder = NULL;
  g_autoptr(GSettings) list = NULL;
  g_autoptr(GString) parent_path = NULL;
  g_auto(GStrv) commands = NULL;
  g_auto(GStrv) keys = NULL;

  g_return_if_fail (GBP_IS_SHELLCMD_RUN_COMMAND (self));

  /* Get parent settings path */
  parent_path = g_string_new (self->settings_path);
  if (parent_path->len)
    g_string_truncate (parent_path, parent_path->len-1);
  while (parent_path->len && parent_path->str[parent_path->len-1] != '/')
    g_string_truncate (parent_path, parent_path->len-1);

  /* First remove the item from the parent list of commands */
  list = g_settings_new_with_path ("org.gnome.builder.shellcmd", parent_path->str);
  commands = g_settings_get_strv (list, "run-commands");
  builder = g_strv_builder_new ();
  for (guint i = 0; commands[i]; i++)
    {
      if (!ide_str_equal0 (commands[i], self->id))
        g_strv_builder_add (builder, commands[i]);
    }
  g_clear_pointer (&commands, g_strfreev);
  commands = g_strv_builder_end (builder);
  g_settings_set_strv (list, "run-commands", (const char * const *)commands);

  /* Now reset the keys so the entry does not take up space in storage */
  g_object_get (self->settings,
                "settings-schema", &schema,
                NULL);
  keys = g_settings_schema_list_keys (schema);
  for (guint i = 0; keys[i]; i++)
    g_settings_reset (self->settings, keys[i]);
}

const char *
gbp_shellcmd_run_command_get_accelerator (GbpShellcmdRunCommand *self)
{
  g_return_val_if_fail (GBP_IS_SHELLCMD_RUN_COMMAND (self), NULL);

  return self->accelerator;
}

void
gbp_shellcmd_run_command_set_accelerator (GbpShellcmdRunCommand *self,
                                          const char            *accelerator)
{
  g_return_if_fail (GBP_IS_SHELLCMD_RUN_COMMAND (self));

  if (ide_set_string (&self->accelerator, accelerator))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACCELERATOR]);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACCELERATOR_LABEL]);
    }
}

static char *
expand_arg (const char *arg,
            ...)
{
  va_list args;
  const char *key;
  char *ret = NULL;

  if (arg == NULL)
    return g_strdup (g_get_home_dir ());

  if (arg[0] == '~' && arg[1] == '/')
    return ide_path_expand (arg);

  if (strchr (arg, '$') == NULL)
    return g_strdup (arg);

  ret = g_strdup (arg);

  va_start (args, arg);
  while ((key = va_arg (args, const char *)))
    {
      const char *value = va_arg (args, const char *);

      if (value == NULL)
        continue;

      if (strstr (ret, key) != NULL)
        {
          GString *gstr = g_string_new (ret);
          g_string_replace (gstr, key, value, 0);
          g_free (ret);
          ret = g_string_free (gstr, FALSE);
        }
    }
  va_end (args);

  return ret;
}

IdeTerminalLauncher *
gbp_shellcmd_run_command_create_launcher (GbpShellcmdRunCommand *self,
                                          IdeContext            *context)
{
  g_autofree char *cwd_expanded = NULL;
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(GStrvBuilder) argv_builder = NULL;
  g_autoptr(IdeRunner) runner = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) workdir = NULL;
  g_auto(GStrv) argv_expanded = NULL;
  IdeBuildManager *build_manager = NULL;
  IdePipeline *pipeline = NULL;
  IdeRuntime *runtime = NULL;
  const char * const *argv;
  const char * const *env;
  const char *cwd;
  const char *builddir;
  const char *srcdir;
  const char *home;

  g_return_val_if_fail (GBP_IS_SHELLCMD_RUN_COMMAND (self), NULL);
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  workdir = ide_context_ref_workdir (context);
  home = g_get_home_dir ();
  srcdir = g_file_peek_path (workdir);
  builddir = g_file_peek_path (workdir);

  if (ide_context_has_project (context))
    {
      build_manager = ide_build_manager_from_context (context);
      pipeline = ide_build_manager_get_pipeline (build_manager);
      builddir = ide_pipeline_get_builddir (pipeline);
      srcdir = ide_pipeline_get_srcdir (pipeline);
    }

  switch (self->locality)
    {
    case GBP_SHELLCMD_LOCALITY_PIPELINE:
      if (pipeline == NULL ||
          !(launcher = ide_pipeline_create_launcher (pipeline, &error)))
        goto handle_error;
      break;

    case GBP_SHELLCMD_LOCALITY_HOST:
      launcher = ide_subprocess_launcher_new (0);
      ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
      break;

    case GBP_SHELLCMD_LOCALITY_SUBPROCESS:
      launcher = ide_subprocess_launcher_new (0);
      break;

    case GBP_SHELLCMD_LOCALITY_RUNNER:
      if (!(runtime = ide_pipeline_get_runtime (pipeline)) ||
          !(runner = ide_runtime_create_runner (runtime, NULL)))
        goto handle_error;
      break;

    default:
      g_assert_not_reached ();
    }

  cwd = ide_run_command_get_cwd (IDE_RUN_COMMAND (self));
  argv = ide_run_command_get_argv (IDE_RUN_COMMAND (self));
  env = ide_run_command_get_env (IDE_RUN_COMMAND (self));
  cwd = ide_run_command_get_cwd (IDE_RUN_COMMAND (self));

  g_return_val_if_fail (argv != NULL, NULL);

  argv_builder = g_strv_builder_new ();
  for (guint i = 0; argv[i]; i++)
    {
      g_autofree char *expanded = NULL;

      expanded = expand_arg (argv[i],
                             "$HOME", home,
                             "$BUILDDIR", builddir,
                             "$SRCDIR", srcdir,
                             NULL);
      g_strv_builder_add (argv_builder, expanded);
    }
  argv_expanded = g_strv_builder_end (argv_builder);

  cwd_expanded = expand_arg (cwd,
                             "$HOME", home,
                             "$BUILDDIR", builddir,
                             "$SRCDIR", srcdir,
                             NULL);

  g_assert (runner != NULL || launcher != NULL);

  if (runner != NULL)
    launcher = IDE_RUNNER_GET_CLASS (runner)->create_launcher (runner);

  g_assert (launcher != NULL);

  ide_subprocess_launcher_set_cwd (launcher, cwd_expanded);
  ide_subprocess_launcher_push_args (launcher, (const char * const *)argv_expanded);

  if (runner != NULL)
    {
      ide_subprocess_launcher_set_run_on_host (launcher, ide_runner_get_run_on_host (runner));
      ide_subprocess_launcher_set_clear_env (launcher, ide_runner_get_clear_env (runner));
      ide_subprocess_launcher_overlay_environment (launcher, ide_runner_get_environment (runner));
    }

  if (env != NULL)
    {
      for (guint i = 0; env[i]; i++)
        {
          g_autofree char *key = NULL;
          g_autofree char *value = NULL;

          if (ide_environ_parse (env[i], &key, &value))
            ide_subprocess_launcher_setenv (launcher, key, value, TRUE);
        }
    }

  if (runner != NULL)
    IDE_RUNNER_GET_CLASS (runner)->fixup_launcher (runner, launcher);

  return ide_terminal_launcher_new_for_launcher (launcher);

handle_error:
  if (error != NULL)
    ide_object_warning (context,
                        "%s: %s",
                        _("Failed to launch command"),
                        error->message);

  return NULL;
}

GbpShellcmdLocality
gbp_shellcmd_run_command_get_locality (GbpShellcmdRunCommand *self)
{
  g_return_val_if_fail (GBP_IS_SHELLCMD_RUN_COMMAND (self), 0);

  return self->locality;
}

void
gbp_shellcmd_run_command_set_locality (GbpShellcmdRunCommand *self,
                                       GbpShellcmdLocality    locality)
{
  g_return_if_fail (GBP_IS_SHELLCMD_RUN_COMMAND (self));

  if (locality != self->locality)
    {
      self->locality = locality;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LOCALITY]);
    }
}
