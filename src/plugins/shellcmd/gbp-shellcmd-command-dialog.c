/* gbp-shellcmd-command-dialog.c
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

#define G_LOG_DOMAIN "gbp-shellcmd-command-dialog"

#include "config.h"

#include <libide-gtk.h>

#include "gbp-shellcmd-command-dialog.h"

struct _GbpShellcmdCommandDialog
{
  AdwWindow              parent_instance;

  GbpShellcmdRunCommand *command;

  GtkStringList         *envvars;
  GtkListBox            *envvars_list_box;
};

enum {
  PROP_0,
  PROP_COMMAND,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GbpShellcmdCommandDialog, gbp_shellcmd_command_dialog, ADW_TYPE_WINDOW)

static GParamSpec *properties [N_PROPS];
static GtkWidget *
create_envvar_row_cb (gpointer item,
                      gpointer user_data)
{
  GtkStringObject *obj = item;
  const char *str = gtk_string_object_get_string (obj);
  AdwActionRow *row;
  GtkButton *button;

  row = g_object_new (ADW_TYPE_ACTION_ROW,
                      "title", str,
                      "title-selectable", TRUE,
                      NULL);
  button = g_object_new (GTK_TYPE_BUTTON,
                         "icon-name", "list-remove-symbolic",
                         "css-classes", IDE_STRV_INIT ("flat", "circular"),
                         "valign", GTK_ALIGN_CENTER,
                         NULL);
  adw_action_row_add_suffix (row, GTK_WIDGET (button));

  return GTK_WIDGET (row);
}

static void
on_env_entry_changed_cb (GbpShellcmdCommandDialog *self,
                         IdeEntryPopover          *popover)
{
  gboolean valid = FALSE;
  const char *text;
  const char *eq;

  g_assert (GBP_IS_SHELLCMD_COMMAND_DIALOG (self));
  g_assert (IDE_IS_ENTRY_POPOVER (popover));

  text = ide_entry_popover_get_text (popover);
  eq = strchr (text, '=');

  if (eq != NULL && eq != text)
    {
      for (const char *iter = text; iter < eq; iter = g_utf8_next_char (iter))
        {
          gunichar ch = g_utf8_get_char (iter);

          if (!g_unichar_isalnum (ch) && ch != '_')
            goto failure;
        }

      if (g_ascii_isalpha (*text))
        valid = TRUE;
    }

failure:
  ide_entry_popover_set_ready (popover, valid);
}

static void
on_env_entry_activate_cb (GbpShellcmdCommandDialog *self,
                          const char               *text,
                          IdeEntryPopover          *popover)
{
  IDE_ENTRY;

  g_assert (GBP_IS_SHELLCMD_COMMAND_DIALOG (self));
  g_assert (IDE_IS_ENTRY_POPOVER (popover));
  g_assert (GTK_IS_STRING_LIST (self->envvars));

  gtk_string_list_append (self->envvars, text);
  ide_entry_popover_set_text (popover, "");

  IDE_EXIT;
}

static void
gbp_shellcmd_command_dialog_dispose (GObject *object)
{
  GbpShellcmdCommandDialog *self = (GbpShellcmdCommandDialog *)object;

  g_clear_object (&self->command);

  G_OBJECT_CLASS (gbp_shellcmd_command_dialog_parent_class)->dispose (object);
}

static void
gbp_shellcmd_command_dialog_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  GbpShellcmdCommandDialog *self = GBP_SHELLCMD_COMMAND_DIALOG (object);

  switch (prop_id)
    {
    case PROP_COMMAND:
      g_value_set_object (value, self->command);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_shellcmd_command_dialog_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  GbpShellcmdCommandDialog *self = GBP_SHELLCMD_COMMAND_DIALOG (object);

  switch (prop_id)
    {
    case PROP_COMMAND:
      self->command = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_shellcmd_command_dialog_class_init (GbpShellcmdCommandDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gbp_shellcmd_command_dialog_dispose;
  object_class->get_property = gbp_shellcmd_command_dialog_get_property;
  object_class->set_property = gbp_shellcmd_command_dialog_set_property;

  properties [PROP_COMMAND] =
    g_param_spec_object ("command", NULL, NULL,
                         GBP_TYPE_SHELLCMD_RUN_COMMAND,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/shellcmd/gbp-shellcmd-command-dialog.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpShellcmdCommandDialog, envvars);
  gtk_widget_class_bind_template_child (widget_class, GbpShellcmdCommandDialog, envvars_list_box);
  gtk_widget_class_bind_template_callback (widget_class, on_env_entry_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_env_entry_activate_cb);
}

static void
gbp_shellcmd_command_dialog_init (GbpShellcmdCommandDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

#if DEVELOPMENT_BUILD
  gtk_widget_add_css_class (GTK_WIDGET (self), "devel");
#endif

  gtk_list_box_bind_model (self->envvars_list_box,
                           G_LIST_MODEL (self->envvars),
                           create_envvar_row_cb,
                           NULL, NULL);
  ide_gtk_widget_hide_when_empty (GTK_WIDGET (self->envvars_list_box),
                                  G_LIST_MODEL (self->envvars));
}

GbpShellcmdCommandDialog *
gbp_shellcmd_command_dialog_new (GbpShellcmdRunCommand *command)
{
  g_return_val_if_fail (GBP_IS_SHELLCMD_RUN_COMMAND (command), NULL);

  return g_object_new (GBP_TYPE_SHELLCMD_COMMAND_DIALOG,
                       "command", command,
                       NULL);
}
