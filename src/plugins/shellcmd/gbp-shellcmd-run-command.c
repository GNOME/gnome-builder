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
  char                *keywords;

  GbpShellcmdLocality  locality : 3;
  guint                use_shell : 1;
};

enum {
  PROP_0,
  PROP_ACCELERATOR,
  PROP_ACCELERATOR_LABEL,
  PROP_LOCALITY,
  PROP_SETTINGS_PATH,
  PROP_SUBTITLE,
  PROP_USE_SHELL,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GbpShellcmdRunCommand, gbp_shellcmd_run_command, IDE_TYPE_RUN_COMMAND)

static GParamSpec *properties [N_PROPS];

static void
clear_keywords_cb (GbpShellcmdRunCommand *self)
{
  g_assert (GBP_IS_SHELLCMD_RUN_COMMAND (self));

  g_clear_pointer (&self->keywords, g_free);
}

static void
gbp_shellcmd_run_command_prepare_to_run (IdeRunCommand *run_command,
                                         IdeRunContext *run_context,
                                         IdeContext    *context)
{
  GbpShellcmdRunCommand *self = (GbpShellcmdRunCommand *)run_command;
  IdePipeline *pipeline = NULL;
  IdeRuntime *runtime = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SHELLCMD_RUN_COMMAND (self));
  g_assert (IDE_IS_RUN_CONTEXT (run_context));
  g_assert (IDE_IS_CONTEXT (context));

  if (ide_context_has_project (context))
    {
      IdeBuildManager *build_manager = ide_build_manager_from_context (context);

      if ((pipeline = ide_build_manager_get_pipeline (build_manager)))
        runtime = ide_pipeline_get_runtime (pipeline);
    }

  switch (self->locality)
    {
    case GBP_SHELLCMD_LOCALITY_PIPELINE:
      if (pipeline == NULL)
        ide_run_context_push_error (run_context,
                                    g_error_new (G_IO_ERROR,
                                                 G_IO_ERROR_NOT_INITIALIZED,
                                                 "No pipeline available for run command"));
      else
        ide_pipeline_prepare_run_context (pipeline, run_context);
      break;

    case GBP_SHELLCMD_LOCALITY_HOST:
      ide_run_context_push_host (run_context);
      break;

    case GBP_SHELLCMD_LOCALITY_SUBPROCESS:
      break;

    case GBP_SHELLCMD_LOCALITY_RUNTIME: {
      if (pipeline == NULL || runtime == NULL)
        ide_run_context_push_error (run_context,
                                    g_error_new (G_IO_ERROR,
                                                 G_IO_ERROR_NOT_INITIALIZED,
                                                 "No pipeline available for run command"));
      else
        ide_runtime_prepare_to_run (runtime, pipeline, run_context);
      break;
    }

    default:
      g_assert_not_reached ();
    }

  if (self->use_shell)
    {
      if (runtime != NULL &&
          ide_runtime_contains_program_in_path (runtime, ide_get_user_shell (), NULL))
        ide_run_context_push_user_shell (run_context, IDE_RUN_CONTEXT_SHELL_DEFAULT);
      else
        ide_run_context_push_shell (run_context, IDE_RUN_CONTEXT_SHELL_DEFAULT);
    }

  IDE_RUN_COMMAND_CLASS (gbp_shellcmd_run_command_parent_class)->prepare_to_run (run_command, run_context, context);

  IDE_EXIT;
}

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
  g_settings_bind (self->settings, "env", self, "environ", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings, "argv", self, "argv", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings, "cwd", self, "cwd", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings, "accelerator", self, "accelerator", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings, "locality", self, "locality", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings, "use-shell", self, "use-shell", G_SETTINGS_BIND_DEFAULT);
}

static void
subtitle_changed_cb (GbpShellcmdRunCommand *self)
{
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SUBTITLE]);
}

char *
gbp_shellcmd_run_command_dup_subtitle (GbpShellcmdRunCommand *self)
{
  const char * const *argv;

  g_assert (GBP_IS_SHELLCMD_RUN_COMMAND (self));

  if ((argv = ide_run_command_get_argv (IDE_RUN_COMMAND (self))))
    return g_strjoinv (" ", (char **)argv);

  return NULL;
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
  g_clear_pointer (&self->keywords, g_free);

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
      g_value_take_string (value, gbp_shellcmd_run_command_dup_subtitle (self));
      break;

    case PROP_USE_SHELL:
      g_value_set_boolean (value, gbp_shellcmd_run_command_get_use_shell (self));
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

    case PROP_USE_SHELL:
      gbp_shellcmd_run_command_set_use_shell (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_shellcmd_run_command_class_init (GbpShellcmdRunCommandClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeRunCommandClass *run_command_class = IDE_RUN_COMMAND_CLASS (klass);

  object_class->constructed = gbp_shellcmd_run_command_constructed;
  object_class->dispose = gbp_shellcmd_run_command_dispose;
  object_class->get_property = gbp_shellcmd_run_command_get_property;
  object_class->set_property = gbp_shellcmd_run_command_set_property;

  run_command_class->prepare_to_run = gbp_shellcmd_run_command_prepare_to_run;

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

  properties [PROP_USE_SHELL] =
    g_param_spec_boolean ("use-shell", NULL, NULL, FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_shellcmd_run_command_init (GbpShellcmdRunCommand *self)
{
  self->locality = GBP_SHELLCMD_LOCALITY_PIPELINE;

  ide_run_command_set_kind (IDE_RUN_COMMAND (self),
                            IDE_RUN_COMMAND_KIND_USER_DEFINED);

  g_signal_connect (self, "notify::accelerator", G_CALLBACK (accelerator_label_changed_cb), NULL);
  g_signal_connect (self, "notify::cwd", G_CALLBACK (subtitle_changed_cb), NULL);
  g_signal_connect (self, "notify::argv", G_CALLBACK (subtitle_changed_cb), NULL);
  g_signal_connect (self, "notify", G_CALLBACK (clear_keywords_cb), NULL);
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

  if (g_set_str (&self->accelerator, accelerator))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACCELERATOR]);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACCELERATOR_LABEL]);
    }
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

const char *
gbp_shellcmd_run_command_get_keywords (GbpShellcmdRunCommand *self)
{
  g_return_val_if_fail (GBP_IS_SHELLCMD_RUN_COMMAND (self), NULL);

  if (self->keywords == NULL)
    {
      GString *str = g_string_new (NULL);
      const char * const *argv;
      const char *name;

      if ((name = ide_run_command_get_display_name (IDE_RUN_COMMAND (self))))
        g_string_append (str, name);

      if ((argv = ide_run_command_get_argv (IDE_RUN_COMMAND (self))))
        {
          for (guint i = 0; argv[i]; i++)
            {
              g_string_append_c (str, ' ');
              g_string_append (str, argv[i]);
            }
        }

      self->keywords = g_string_free (str, FALSE);
    }

  return self->keywords;
}

gboolean
gbp_shellcmd_run_command_get_use_shell (GbpShellcmdRunCommand *self)
{
  g_return_val_if_fail (GBP_IS_SHELLCMD_RUN_COMMAND (self), FALSE);

  return self->use_shell;
}

void
gbp_shellcmd_run_command_set_use_shell (GbpShellcmdRunCommand *self,
                                        gboolean               use_shell)
{
  g_return_if_fail (GBP_IS_SHELLCMD_RUN_COMMAND (self));

  use_shell = !!use_shell;

  if (use_shell != self->use_shell)
    {
      self->use_shell = use_shell;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_USE_SHELL]);
    }
}
