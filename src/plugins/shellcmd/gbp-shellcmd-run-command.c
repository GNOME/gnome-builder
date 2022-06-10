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

#include "gbp-shellcmd-run-command.h"

struct _GbpShellcmdRunCommand
{
  IdeRunCommand  parent_instance;
  char          *settings_path;
  GSettings     *settings;
};

enum {
  PROP_0,
  PROP_SETTINGS_PATH,
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

  self->settings = g_settings_new_with_path ("org.gnome.builder.shellcmd.command", self->settings_path);

  path_split = g_strsplit (self->settings_path, "/", 0);
  n_parts = g_strv_length (path_split);
  id = g_strdup_printf ("shellcmd:%s", path_split[n_parts-1]);

  ide_run_command_set_id (IDE_RUN_COMMAND (self), id);
  g_settings_bind (self->settings, "display-name", self, "display-name", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings, "env", self, "env", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings, "argv", self, "argv", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings, "cwd", self, "cwd", G_SETTINGS_BIND_DEFAULT);
}

static void
gbp_shellcmd_run_command_dispose (GObject *object)
{
  GbpShellcmdRunCommand *self = (GbpShellcmdRunCommand *)object;

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
    case PROP_SETTINGS_PATH:
      g_value_set_string (value, self->settings_path);
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

  properties [PROP_SETTINGS_PATH] =
    g_param_spec_string ("settings-path", NULL, NULL, NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_shellcmd_run_command_init (GbpShellcmdRunCommand *self)
{
}

GbpShellcmdRunCommand *
gbp_shellcmd_run_command_new (const char *settings_path)
{
  return g_object_new (GBP_TYPE_SHELLCMD_RUN_COMMAND,
                       "settings-path", settings_path,
                       NULL);
}
