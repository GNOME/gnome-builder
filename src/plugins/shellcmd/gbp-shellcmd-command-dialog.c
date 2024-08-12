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
#include "gbp-shellcmd-enums.h"

struct _GbpShellcmdCommandDialog
{
  AdwWindow              parent_instance;

  GbpShellcmdRunCommand *command;

  AdwEntryRow           *argv;
  AdwEntryRow           *location;
  AdwEntryRow           *name;
  AdwComboRow           *locality;
  GtkStringList         *envvars;
  GtkListBox            *envvars_list_box;
  GtkLabel              *shortcut_label;
  GtkButton             *save;
  GtkButton             *delete_button;
  AdwSwitchRow          *use_shell;

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

static char **
string_list_to_strv (GtkStringList *strlist)
{
  g_autoptr(GStrvBuilder) builder = g_strv_builder_new ();
  GListModel *model = G_LIST_MODEL (strlist);
  guint n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(GtkStringObject) strobj = g_list_model_get_item (model, i);
      const char *str = gtk_string_object_get_string (strobj);

      g_strv_builder_add (builder, str);
    }

  return g_strv_builder_end (builder);
}

static void
delete_envvar_cb (GbpShellcmdCommandDialog *self,
                  GtkButton                *button)
{
  const char *envvar;
  guint n_items;

  g_assert (GBP_IS_SHELLCMD_COMMAND_DIALOG (self));
  g_assert (GTK_IS_BUTTON (button));

  envvar = g_object_get_data (G_OBJECT (button), "ENVVAR");
  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->envvars));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(GtkStringObject) str = g_list_model_get_item (G_LIST_MODEL (self->envvars), i);

      if (g_strcmp0 (envvar, gtk_string_object_get_string (str)) == 0)
        {
          gtk_string_list_remove (self->envvars, i);
          break;
        }
    }
}

static GtkWidget *
create_envvar_row_cb (gpointer item,
                      gpointer user_data)
{
  GbpShellcmdCommandDialog *self = user_data;
  GtkStringObject *obj = item;
  const char *str = gtk_string_object_get_string (obj);
  g_autofree char *markup = NULL;
  g_autofree char *escaped = NULL;
  AdwActionRow *row;
  GtkButton *button;

  escaped = g_markup_escape_text (str, -1);
  markup = g_strdup_printf ("<tt>%s</tt>", escaped);
  row = g_object_new (ADW_TYPE_ACTION_ROW,
                      "title", markup,
                      "title-selectable", TRUE,
                      NULL);
  button = g_object_new (GTK_TYPE_BUTTON,
                         "icon-name", "list-remove-symbolic",
                         "css-classes", IDE_STRV_INIT ("flat", "circular"),
                         "valign", GTK_ALIGN_CENTER,
                         NULL);
  g_object_set_data_full (G_OBJECT (button),
                          "ENVVAR",
                          g_strdup (str),
                          g_free);
  g_signal_connect_object (button,
                           "clicked",
                           G_CALLBACK (delete_envvar_cb),
                           self,
                           G_CONNECT_SWAPPED);
  adw_action_row_add_suffix (row, GTK_WIDGET (button));

  return GTK_WIDGET (row);
}

static void
on_env_entry_changed_cb (GbpShellcmdCommandDialog *self,
                         IdeEntryPopover          *popover)
{
  const char *errstr = NULL;
  gboolean valid = FALSE;
  const char *text;
  const char *eq;

  g_assert (GBP_IS_SHELLCMD_COMMAND_DIALOG (self));
  g_assert (IDE_IS_ENTRY_POPOVER (popover));

  text = ide_entry_popover_get_text (popover);
  eq = strchr (text, '=');

  if (!ide_str_empty0 (text) && eq == NULL)
    errstr = _("Use KEY=VALUE to set an environment variable");

  if (eq != NULL && eq != text)
    {
      if (g_unichar_isdigit (g_utf8_get_char (text)))
        {
          errstr = _("Keys may not start with a number");
          goto failure;

        }
      for (const char *iter = text; iter < eq; iter = g_utf8_next_char (iter))
        {
          gunichar ch = g_utf8_get_char (iter);

          if (!g_unichar_isalnum (ch) && ch != '_')
            {
              errstr = _("Keys may only contain alpha-numerics or underline.");
              goto failure;
            }
        }

      if (g_ascii_isalpha (*text))
        valid = TRUE;
    }

failure:
  ide_entry_popover_set_ready (popover, valid);
  ide_entry_popover_set_message (popover, errstr);
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
  GbpShellcmdLocality locality;
  const char * const *argv;
  const char * const *env;
  const char *accel;
  const char *name;
  const char *cwd;
  gboolean use_shell;

  IDE_ENTRY;

  g_assert (GBP_IS_SHELLCMD_COMMAND_DIALOG (self));
  g_assert (!command || GBP_IS_SHELLCMD_RUN_COMMAND (command));

  if (!g_set_object (&self->command, command))
    IDE_EXIT;

  name = ide_run_command_get_display_name (IDE_RUN_COMMAND (command));
  argv = ide_run_command_get_argv (IDE_RUN_COMMAND (command));
  env = ide_run_command_get_environ (IDE_RUN_COMMAND (command));
  cwd = ide_run_command_get_cwd (IDE_RUN_COMMAND (command));
  accel = gbp_shellcmd_run_command_get_accelerator (command);
  locality = gbp_shellcmd_run_command_get_locality (command);
  use_shell = gbp_shellcmd_run_command_get_use_shell (command);

  argvstr = normalize_argv (argv);

  gtk_editable_set_text (GTK_EDITABLE (self->argv), argvstr);
  gtk_editable_set_text (GTK_EDITABLE (self->location), cwd);
  gtk_editable_set_text (GTK_EDITABLE (self->name), name);
  adw_switch_row_set_active (self->use_shell, use_shell);
  set_accel (self, accel);

  /* locality value equates to position in list model for simplicity */
  adw_combo_row_set_selected (self->locality, locality);

  if (env != NULL)
    {
      for (guint i = 0; env[i]; i++)
        gtk_string_list_append (self->envvars, env[i]);
    }

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
                         NULL);
  g_signal_connect_object (dialog,
                           "shortcut-set",
                           G_CALLBACK (set_accel),
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
  g_autoptr(GEnumClass) enum_class = NULL;
  g_auto(GStrv) argv = NULL;
  g_auto(GStrv) env = NULL;
  const char *argvstr;
  IdeEnumObject *item;
  const char *nick;
  GEnumValue *value;
  int argc;

  IDE_ENTRY;

  g_assert (GBP_IS_SHELLCMD_COMMAND_DIALOG (self));

  g_object_freeze_notify (G_OBJECT (self->command));

  argvstr = gtk_editable_get_text (GTK_EDITABLE (self->argv));
  if (g_shell_parse_argv (argvstr, &argc, &argv, NULL))
    ide_run_command_set_argv (IDE_RUN_COMMAND (self->command), (const char * const *)argv);

  ide_run_command_set_display_name (IDE_RUN_COMMAND (self->command),
                                    gtk_editable_get_text (GTK_EDITABLE (self->name)));
  ide_run_command_set_cwd (IDE_RUN_COMMAND (self->command),
                           gtk_editable_get_text (GTK_EDITABLE (self->location)));
  gbp_shellcmd_run_command_set_accelerator (self->command, self->accel);
  gbp_shellcmd_run_command_set_use_shell (self->command,
                                          adw_switch_row_get_active (self->use_shell));

  env = string_list_to_strv (self->envvars);
  ide_run_command_set_environ (IDE_RUN_COMMAND (self->command),
                               (const char * const *)env);

  item = adw_combo_row_get_selected_item (self->locality);
  nick = ide_enum_object_get_nick (item);
  enum_class = g_type_class_ref (GBP_TYPE_SHELLCMD_LOCALITY);
  value = g_enum_get_value_by_nick (enum_class, nick);
  gbp_shellcmd_run_command_set_locality (self->command, value->value);

  g_object_thaw_notify (G_OBJECT (self->command));

 gtk_window_destroy (GTK_WINDOW (self));

  IDE_EXIT;
}

static void
select_folder_response_cb (GObject      *object,
                           GAsyncResult *result,
                           gpointer      user_data)
{
  GtkFileDialog *dialog = (GtkFileDialog *)object;
  g_autoptr(AdwEntryRow) row = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) file = NULL;
  g_autofree char *path = NULL;

  g_assert (GTK_IS_FILE_DIALOG (dialog));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (ADW_IS_ENTRY_ROW (row));

  if (!(file = gtk_file_dialog_select_folder_finish (dialog, result, &error)))
    return;

  path = ide_path_collapse (g_file_peek_path (file));
  gtk_editable_set_text (GTK_EDITABLE (row), path);
}

static void
select_folder_action (GtkWidget  *widget,
                      const char *action_name,
                      GVariant   *param)
{
  GbpShellcmdCommandDialog *self = (GbpShellcmdCommandDialog *)widget;
  g_autoptr(GtkFileDialog) dialog = NULL;
  g_autofree char *expanded = NULL;
  g_autoptr(GFile) file = NULL;
  const char *cwd;
  GtkRoot *root;

  g_assert (GBP_IS_SHELLCMD_COMMAND_DIALOG (self));

  cwd = gtk_editable_get_text (GTK_EDITABLE (self->location));
  expanded = ide_path_expand (cwd);
  file = g_file_new_for_path (expanded);

  root = gtk_widget_get_root (widget);
  dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_title (dialog, _("Select Working Directory"));
  gtk_file_dialog_set_accept_label (dialog, _("Select"));
  gtk_file_dialog_set_initial_folder (dialog, file);

  gtk_file_dialog_select_folder (dialog,
                                 GTK_WINDOW (root),
                                 NULL,
                                 select_folder_response_cb,
                                 g_object_ref (self->location));
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
          gtk_widget_hide (GTK_WIDGET (self->delete_button));
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
  gtk_widget_class_install_action (widget_class, "command.select-folder", NULL, select_folder_action);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, 0, "command.cancel", NULL);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/shellcmd/gbp-shellcmd-command-dialog.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpShellcmdCommandDialog, argv);
  gtk_widget_class_bind_template_child (widget_class, GbpShellcmdCommandDialog, delete_button);
  gtk_widget_class_bind_template_child (widget_class, GbpShellcmdCommandDialog, envvars);
  gtk_widget_class_bind_template_child (widget_class, GbpShellcmdCommandDialog, envvars_list_box);
  gtk_widget_class_bind_template_child (widget_class, GbpShellcmdCommandDialog, locality);
  gtk_widget_class_bind_template_child (widget_class, GbpShellcmdCommandDialog, location);
  gtk_widget_class_bind_template_child (widget_class, GbpShellcmdCommandDialog, name);
  gtk_widget_class_bind_template_child (widget_class, GbpShellcmdCommandDialog, save);
  gtk_widget_class_bind_template_child (widget_class, GbpShellcmdCommandDialog, shortcut_label);
  gtk_widget_class_bind_template_child (widget_class, GbpShellcmdCommandDialog, use_shell);
  gtk_widget_class_bind_template_callback (widget_class, on_env_entry_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_env_entry_activate_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_shortcut_activated_cb);
}

static void
gbp_shellcmd_command_dialog_init (GbpShellcmdCommandDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

#ifdef DEVELOPMENT_BUILD
  gtk_widget_add_css_class (GTK_WIDGET (self), "devel");
#endif

  gtk_list_box_bind_model (self->envvars_list_box,
                           G_LIST_MODEL (self->envvars),
                           create_envvar_row_cb,
                           self, NULL);
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
