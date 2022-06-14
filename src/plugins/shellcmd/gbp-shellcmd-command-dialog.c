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

#include <glib/gi18n.h>

#include <libide-gtk.h>

#include "gbp-shellcmd-command-dialog.h"

struct _GbpShellcmdCommandDialog
{
  AdwWindow              parent_instance;

  GbpShellcmdRunCommand *command;

  AdwEntryRow           *argv;
  AdwEntryRow           *location;
  AdwEntryRow           *name;
  GtkStringList         *envvars;
  GtkListBox            *envvars_list_box;
  GtkLabel              *shortcut_label;
  GtkButton             *save;

  char                  *accel;

  guint                  delete_on_cancel : 1;
};

enum {
  PROP_0,
  PROP_COMMAND,
  PROP_DELETE_ON_CANCEL,
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

static char *
normalize_argv (const char * const *argv)
{
  g_autofree char *joined = NULL;
  g_auto(GStrv) parsed = NULL;
  int argc;

  if (argv == NULL || argv[0] == NULL)
    return g_strdup ("");

  /* The goal here is to only quote the argv if the string would
   * parse back differently than it's initial form.
   */
  joined = g_strjoinv (" ", (char **)argv);
  if (!g_shell_parse_argv (joined, &argc, &parsed, NULL) ||
      !g_strv_equal ((const char * const *)parsed, argv))
    {
      GString *str = g_string_new (NULL);

      for (guint i = 0; argv[i]; i++)
        {
          g_autofree char *quoted = g_shell_quote (argv[i]);

          if (str->len > 0)
            g_string_append_c (str, ' ');
          g_string_append (str, quoted);
        }

      return g_string_free (str, FALSE);
    }

  return g_steal_pointer (&joined);
}

static void
set_accel (GbpShellcmdCommandDialog *self,
           const char               *accel)
{
  g_autofree char *label = NULL;
  guint keyval;
  GdkModifierType state;

  g_assert (GBP_IS_SHELLCMD_COMMAND_DIALOG (self));

  if (ide_str_equal0 (self->accel, accel))
    return;

  g_free (self->accel);
  self->accel = g_strdup (accel);

  if (accel && gtk_accelerator_parse (accel, &keyval, &state))
    label = gtk_accelerator_get_label (keyval, state);

  gtk_label_set_label (self->shortcut_label, label);
}

static void
gbp_shellcmd_command_dialog_set_command (GbpShellcmdCommandDialog *self,
                                         GbpShellcmdRunCommand    *command)
{
  g_autofree char *argvstr = NULL;
  const char * const *argv;
  const char *accel;
  const char *name;
  const char *cwd;

  IDE_ENTRY;

  g_assert (GBP_IS_SHELLCMD_COMMAND_DIALOG (self));
  g_assert (!command || GBP_IS_SHELLCMD_RUN_COMMAND (command));

  if (!g_set_object (&self->command, command))
    IDE_EXIT;

  name = ide_run_command_get_display_name (IDE_RUN_COMMAND (command));
  argv = ide_run_command_get_argv (IDE_RUN_COMMAND (command));
  cwd = ide_run_command_get_cwd (IDE_RUN_COMMAND (command));
  accel = ide_run_command_get_accelerator (IDE_RUN_COMMAND (command));

  argvstr = normalize_argv (argv);

  gtk_editable_set_text (GTK_EDITABLE (self->argv), argvstr);
  gtk_editable_set_text (GTK_EDITABLE (self->location), cwd);
  gtk_editable_set_text (GTK_EDITABLE (self->name), name);
  set_accel (self, accel);

  IDE_EXIT;
}

static void
on_shortcut_dialog_respnose (GbpShellcmdCommandDialog *self,
                             int                       response_id,
                             IdeShortcutAccelDialog   *dialog)
{
  IDE_ENTRY;

  g_assert (GBP_IS_SHELLCMD_COMMAND_DIALOG (self));
  g_assert (IDE_IS_SHORTCUT_ACCEL_DIALOG (dialog));

  if (response_id == GTK_RESPONSE_ACCEPT)
    {
      const char *accel;

      accel = ide_shortcut_accel_dialog_get_accelerator (dialog);
      set_accel (self, accel);
    }

  gtk_window_destroy (GTK_WINDOW (dialog));

  IDE_EXIT;
}

static void
on_shortcut_activated_cb (GbpShellcmdCommandDialog *self,
                          AdwActionRow             *shortcut_row)
{
  IdeShortcutAccelDialog *dialog;
  const char *name;

  IDE_ENTRY;

  g_assert (GBP_IS_SHELLCMD_COMMAND_DIALOG (self));
  g_assert (ADW_IS_ACTION_ROW (shortcut_row));

  name = gtk_editable_get_text (GTK_EDITABLE (self->name));
  if (ide_str_empty0 (name))
    name = _("Untitled Command");

  dialog = g_object_new (IDE_TYPE_SHORTCUT_ACCEL_DIALOG,
                         "accelerator", self->accel,
                         "transient-for", self,
                         "modal", TRUE,
                         "shortcut-title", name,
                         "title", _("Set Shortcut"),
                         "use-header-bar", 1,
                         NULL);
  g_signal_connect_object (dialog,
                           "response",
                           G_CALLBACK (on_shortcut_dialog_respnose),
                           self,
                           G_CONNECT_SWAPPED);
  gtk_window_present (GTK_WINDOW (dialog));

  IDE_EXIT;
}

static void
command_delete_action (GtkWidget  *widget,
                       const char *action_name,
                       GVariant   *param)
{
  GbpShellcmdCommandDialog *self = (GbpShellcmdCommandDialog *)widget;

  IDE_ENTRY;

  g_assert (GBP_IS_SHELLCMD_COMMAND_DIALOG (self));

  gbp_shellcmd_run_command_delete (self->command);

  gtk_window_destroy (GTK_WINDOW (self));

  IDE_EXIT;
}

static void
command_cancel_action (GtkWidget  *widget,
                       const char *action_name,
                       GVariant   *param)
{
  GbpShellcmdCommandDialog *self = (GbpShellcmdCommandDialog *)widget;

  IDE_ENTRY;

  g_assert (GBP_IS_SHELLCMD_COMMAND_DIALOG (self));

  if (self->delete_on_cancel)
    gbp_shellcmd_run_command_delete (self->command);

  gtk_window_destroy (GTK_WINDOW (self));

  IDE_EXIT;
}

static void
command_save_action (GtkWidget  *widget,
                     const char *action_name,
                     GVariant   *param)
{
  GbpShellcmdCommandDialog *self = (GbpShellcmdCommandDialog *)widget;
  g_auto(GStrv) argv = NULL;
  const char *argvstr;
  int argc;

  IDE_ENTRY;

  g_assert (GBP_IS_SHELLCMD_COMMAND_DIALOG (self));

  argvstr = gtk_editable_get_text (GTK_EDITABLE (self->argv));
  if (g_shell_parse_argv (argvstr, &argc, &argv, NULL))
    ide_run_command_set_argv (IDE_RUN_COMMAND (self->command), (const char * const *)argv);

  ide_run_command_set_display_name (IDE_RUN_COMMAND (self->command),
                                    gtk_editable_get_text (GTK_EDITABLE (self->name)));
  ide_run_command_set_cwd (IDE_RUN_COMMAND (self->command),
                           gtk_editable_get_text (GTK_EDITABLE (self->location)));
  ide_run_command_set_accelerator (IDE_RUN_COMMAND (self->command), self->accel);

  gtk_window_destroy (GTK_WINDOW (self));

  IDE_EXIT;
}

static void
gbp_shellcmd_command_dialog_dispose (GObject *object)
{
  GbpShellcmdCommandDialog *self = (GbpShellcmdCommandDialog *)object;

  g_clear_object (&self->command);
  g_clear_pointer (&self->accel, g_free);

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

    case PROP_DELETE_ON_CANCEL:
      g_value_set_boolean (value, self->delete_on_cancel);
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
      gbp_shellcmd_command_dialog_set_command (self, g_value_get_object (value));
      break;

    case PROP_DELETE_ON_CANCEL:
      self->delete_on_cancel = g_value_get_boolean (value);
      if (self->delete_on_cancel)
        {
          gtk_window_set_title (GTK_WINDOW (self), _("Create Command"));
          gtk_button_set_label (self->save, _("Cre_ate"));
        }
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

  properties [PROP_DELETE_ON_CANCEL] =
    g_param_spec_boolean ("delete-on-cancel", NULL, NULL, FALSE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_install_action (widget_class, "command.save", NULL, command_save_action);
  gtk_widget_class_install_action (widget_class, "command.delete", NULL, command_delete_action);
  gtk_widget_class_install_action (widget_class, "command.cancel", NULL, command_cancel_action);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/shellcmd/gbp-shellcmd-command-dialog.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpShellcmdCommandDialog, argv);
  gtk_widget_class_bind_template_child (widget_class, GbpShellcmdCommandDialog, envvars);
  gtk_widget_class_bind_template_child (widget_class, GbpShellcmdCommandDialog, envvars_list_box);
  gtk_widget_class_bind_template_child (widget_class, GbpShellcmdCommandDialog, location);
  gtk_widget_class_bind_template_child (widget_class, GbpShellcmdCommandDialog, name);
  gtk_widget_class_bind_template_child (widget_class, GbpShellcmdCommandDialog, save);
  gtk_widget_class_bind_template_child (widget_class, GbpShellcmdCommandDialog, shortcut_label);
  gtk_widget_class_bind_template_callback (widget_class, on_env_entry_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_env_entry_activate_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_shortcut_activated_cb);
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
gbp_shellcmd_command_dialog_new (GbpShellcmdRunCommand *command,
                                 gboolean               delete_on_cancel)
{
  g_return_val_if_fail (GBP_IS_SHELLCMD_RUN_COMMAND (command), NULL);

  return g_object_new (GBP_TYPE_SHELLCMD_COMMAND_DIALOG,
                       "command", command,
                       "delete-on-cancel", delete_on_cancel,
                       NULL);
}
