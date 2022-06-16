/*
 * gbp-buildui-runnables-dialog.c
 *
 * Copyright 2022 Christian Hergert <>
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

#define G_LOG_DOMAIN "gbp-buildui-runnables-dialog"

#include "config.h"

#include <glib/gi18n.h>
#include <libpeas/peas.h>

#include <libide-gtk.h>
#include <libide-gui.h>

#include "gbp-buildui-runnables-dialog.h"

struct _GbpBuilduiRunnablesDialog
{
  AdwWindow   parent_instance;
  GtkListBox *list_box;
  GtkSpinner *spinner;
  GtkStack   *stack;
  guint       busy : 1;
};

G_DEFINE_FINAL_TYPE (GbpBuilduiRunnablesDialog, gbp_buildui_runnables_dialog, ADW_TYPE_WINDOW)

enum {
  PROP_0,
  PROP_BUSY,
  PROP_CONTEXT,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static GtkWidget *
create_run_command_row (gpointer item,
                        gpointer user_data)
{
  IdeRunCommand *run_command = item;
  g_autoptr(GVariant) idv = NULL;
  g_autoptr(GString) subtitle = NULL;
  const char * const *argv;
  AdwActionRow *row;
  const char *id;
  GtkWidget *check;

  g_assert (IDE_IS_RUN_COMMAND (run_command));

  id = ide_run_command_get_id (run_command);
  idv = g_variant_take_ref (g_variant_new_string (id ? id : ""));
  argv = ide_run_command_get_argv (run_command);

  if (argv != NULL)
    {
      subtitle = g_string_new ("<tt>");

      for (guint i = 0; argv[i]; i++)
        {
          const char *arg = argv[i];
          g_autofree char *quote = NULL;
          g_autofree char *arg_escaped = NULL;

          if (i > 0)
            g_string_append_c (subtitle, ' ');

          /* NOTE: Params can be file-system encoding, but everywhere we run
           * that is UTF-8. May need to adjust should that change.
           */
          for (const char *c = arg; *c; c = g_utf8_next_char (c))
            {
              if (*c == ' ' || *c == '"' || *c == '\'')
                {
                  quote = g_shell_quote (arg);
                  break;
                }
            }

          if (quote != NULL)
            arg_escaped = g_markup_escape_text (quote, -1);
          else
            arg_escaped = g_markup_escape_text (arg, -1);

          g_string_append (subtitle, arg_escaped);
        }

      g_string_append (subtitle, "</tt>");
    }

  check = g_object_new (GTK_TYPE_CHECK_BUTTON,
                        "action-name", "run-manager.default-run-command",
                        "css-classes", IDE_STRV_INIT ("checkimage"),
                        "action-target", idv,
                        "valign", GTK_ALIGN_CENTER,
                        "can-focus", FALSE,
                        NULL);
  row = g_object_new (ADW_TYPE_ACTION_ROW,
                      "title", ide_run_command_get_display_name (item),
                      "subtitle", subtitle ? subtitle->str : NULL,
                      "activatable-widget", check,
                      NULL);
  adw_action_row_add_suffix (row, check);

  return GTK_WIDGET (row);
}

static void
gbp_buildui_runnables_dialog_list_commands_cb (GObject      *object,
                                               GAsyncResult *result,
                                               gpointer      user_data)
{
  IdeRunManager *run_manager = (IdeRunManager *)object;
  g_autoptr(GbpBuilduiRunnablesDialog) self = user_data;
  g_autoptr(GListModel) model = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_RUN_MANAGER (run_manager));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_BUILDUI_RUNNABLES_DIALOG (self));

  self->busy = FALSE;
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BUSY]);

  if (!(model = ide_run_manager_list_commands_finish (run_manager, result, &error)))
    {
      if (!ide_error_ignore (error))
        ide_object_warning (run_manager,
                            /* translators: %s is replaced with the error message */
                            _("Failed to list run commands: %s"),
                            error->message);
      IDE_EXIT;
    }

  gtk_list_box_bind_model (self->list_box, model, create_run_command_row, NULL, NULL);

  IDE_EXIT;
}

static void
gbp_buildui_runnables_dialog_set_context (GbpBuilduiRunnablesDialog *self,
                                          IdeContext                *context)
{
  IdeRunManager *run_manager;

  IDE_ENTRY;

  g_assert (GBP_IS_BUILDUI_RUNNABLES_DIALOG (self));
  g_assert (!context || IDE_IS_CONTEXT (context));

  if (context == NULL)
    IDE_EXIT;

  self->busy = TRUE;

  run_manager = ide_run_manager_from_context (context);
  gtk_widget_insert_action_group (GTK_WIDGET (self),
                                  "run-manager",
                                  G_ACTION_GROUP (run_manager));
  ide_run_manager_list_commands_async (run_manager,
                                       NULL,
                                       gbp_buildui_runnables_dialog_list_commands_cb,
                                       g_object_ref (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BUSY]);

  IDE_EXIT;
}

static void
gbp_buildui_runnables_dialog_get_property (GObject    *object,
                                           guint       prop_id,
                                           GValue     *value,
                                           GParamSpec *pspec)
{
  GbpBuilduiRunnablesDialog *self = GBP_BUILDUI_RUNNABLES_DIALOG (object);

  switch (prop_id)
    {
    case PROP_BUSY:
      g_value_set_boolean (value, self->busy);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_buildui_runnables_dialog_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  GbpBuilduiRunnablesDialog *self = GBP_BUILDUI_RUNNABLES_DIALOG (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      gbp_buildui_runnables_dialog_set_context (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_buildui_runnables_dialog_class_init (GbpBuilduiRunnablesDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gbp_buildui_runnables_dialog_get_property;
  object_class->set_property = gbp_buildui_runnables_dialog_set_property;

  properties [PROP_BUSY] =
    g_param_spec_boolean ("busy", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_CONTEXT] =
    g_param_spec_object ("context", NULL, NULL,
                         IDE_TYPE_CONTEXT,
                         (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, 0, "window.close", NULL);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/buildui/gbp-buildui-runnables-dialog.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiRunnablesDialog, list_box);
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiRunnablesDialog, spinner);
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiRunnablesDialog, stack);

  g_type_ensure (IDE_TYPE_ENUM_OBJECT);
}

static void
gbp_buildui_runnables_dialog_init (GbpBuilduiRunnablesDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

#ifdef DEVELOPMENT_BUILD
  gtk_widget_add_css_class (GTK_WIDGET (self), "devel");
#endif
}
