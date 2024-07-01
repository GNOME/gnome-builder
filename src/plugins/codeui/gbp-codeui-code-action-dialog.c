/* gbp-codeui-code-action-dialog.c
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

#define G_LOG_DOMAIN "gbp-codeui-code-action-dialog"

#include "config.h"

#include "gbp-codeui-code-action-dialog.h"

struct _GbpCodeuiCodeActionDialog
{
  AdwAlertDialog    parent_instance;

  IdeBuffer        *buffer;
  IdeCodeAction    *selected_action;

  GtkStackPage     *empty;
  GtkStackPage     *failed;
  AdwActionRow     *failed_row;
  GtkStackPage     *list;
  GtkListBox       *list_box;
  GtkStackPage     *loading;
  GtkStack         *stack;
};

enum {
  PROP_0,
  PROP_BUFFER,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GbpCodeuiCodeActionDialog, gbp_codeui_code_action_dialog, ADW_TYPE_ALERT_DIALOG)

static GParamSpec *properties [N_PROPS];

static void
gbp_codeui_code_action_dialog_apply_cb (GbpCodeuiCodeActionDialog *self,
                                        const char                *response)
{
  IDE_ENTRY;

  g_assert (GBP_IS_CODEUI_CODE_ACTION_DIALOG (self));
  g_assert (g_str_equal (response, "apply"));
  g_assert (IDE_IS_CODE_ACTION (self->selected_action));

  ide_code_action_execute_async (self->selected_action, NULL, NULL, NULL);

  IDE_EXIT;
}

static GtkWidget *
gbp_codeui_code_action_dialog_create_row_cb (gpointer item,
                                             gpointer user_data)
{
  IdeCodeAction *code_action = item;
  g_autofree char *markup = NULL;
  g_autofree char *title = NULL;
  GtkWidget *row;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CODE_ACTION (code_action));
  g_assert (user_data == NULL);

  title = ide_code_action_get_title (code_action);
  markup = g_markup_escape_text (title, -1);

  row = g_object_new (ADW_TYPE_ACTION_ROW,
                      "title", markup,
                      NULL);

  g_object_set_data_full (G_OBJECT (row),
                          "CODE_ACTION",
                          g_object_ref (code_action),
                          g_object_unref);

  return GTK_WIDGET (row);
}

static void
list_code_actions_cb (GObject      *object,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  IdeBuffer *buffer = (IdeBuffer *)object;
  g_autoptr(GbpCodeuiCodeActionDialog) self = user_data;
  g_autoptr(GListStore) store = NULL;
  g_autoptr(GPtrArray) ar = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_CODEUI_CODE_ACTION_DIALOG (self));

  if (!(ar = ide_buffer_code_action_query_finish (buffer, result, &error)))
    {
      g_autofree char *markup = g_markup_escape_text (error->message, -1);
      adw_action_row_set_subtitle (self->failed_row, markup);
      gtk_stack_set_visible_child (self->stack, gtk_stack_page_get_child (self->failed));
      IDE_EXIT;
    }

  IDE_PTR_ARRAY_SET_FREE_FUNC (ar, g_object_unref);

  if (ar->len == 0)
    {
      gtk_stack_set_visible_child (self->stack, gtk_stack_page_get_child (self->empty));
      IDE_EXIT;
    }

  store = g_list_store_new (IDE_TYPE_CODE_ACTION);
  for (guint i = 0; i < ar->len; i++)
    g_list_store_append (store, g_ptr_array_index (ar, i));

  gtk_list_box_bind_model (self->list_box,
                           G_LIST_MODEL (store),
                           gbp_codeui_code_action_dialog_create_row_cb,
                           NULL, NULL);
  gtk_stack_set_visible_child (self->stack, gtk_stack_page_get_child (self->list));

  IDE_EXIT;
}

static void
gbp_codeui_code_action_dialog_constructed (GObject *object)
{
  GbpCodeuiCodeActionDialog *self = (GbpCodeuiCodeActionDialog *)object;

  IDE_ENTRY;

  g_assert (GBP_IS_CODEUI_CODE_ACTION_DIALOG (self));
  g_assert (IDE_IS_BUFFER (self->buffer));

  G_OBJECT_CLASS (gbp_codeui_code_action_dialog_parent_class)->constructed (object);

  ide_buffer_code_action_query_async (self->buffer,
                                      NULL,
                                      list_code_actions_cb,
                                      g_object_ref (self));

  IDE_EXIT;
}

static void
gbp_codeui_code_action_dialog_row_selected_cb (GbpCodeuiCodeActionDialog *self,
                                               GtkListBoxRow             *row,
                                               GtkListBox                *list_box)
{
  IdeCodeAction *action = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODEUI_CODE_ACTION_DIALOG (self));
  g_assert (!row || GTK_IS_LIST_BOX_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));

  if (row != NULL)
    action = g_object_get_data (G_OBJECT (row), "CODE_ACTION");

  if (g_set_object (&self->selected_action, action))
    adw_alert_dialog_set_response_enabled (ADW_ALERT_DIALOG (self),
                                           "apply",
                                           !!self->selected_action);

  IDE_EXIT;
}

static void
gbp_codeui_code_action_dialog_dispose (GObject *object)
{
  GbpCodeuiCodeActionDialog *self = (GbpCodeuiCodeActionDialog *)object;

  g_clear_object (&self->buffer);
  g_clear_object (&self->selected_action);

  G_OBJECT_CLASS (gbp_codeui_code_action_dialog_parent_class)->dispose (object);
}

static void
gbp_codeui_code_action_dialog_get_property (GObject    *object,
                                            guint       prop_id,
                                            GValue     *value,
                                            GParamSpec *pspec)
{
  GbpCodeuiCodeActionDialog *self = GBP_CODEUI_CODE_ACTION_DIALOG (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      g_value_set_object (value, self->buffer);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_codeui_code_action_dialog_set_property (GObject      *object,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec)
{
  GbpCodeuiCodeActionDialog *self = GBP_CODEUI_CODE_ACTION_DIALOG (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      self->buffer = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_codeui_code_action_dialog_class_init (GbpCodeuiCodeActionDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gbp_codeui_code_action_dialog_constructed;
  object_class->dispose = gbp_codeui_code_action_dialog_dispose;
  object_class->get_property = gbp_codeui_code_action_dialog_get_property;
  object_class->set_property = gbp_codeui_code_action_dialog_set_property;

  properties [PROP_BUFFER] =
    g_param_spec_object ("buffer", NULL, NULL,
                         IDE_TYPE_BUFFER,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/codeui/gbp-codeui-code-action-dialog.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpCodeuiCodeActionDialog, empty);
  gtk_widget_class_bind_template_child (widget_class, GbpCodeuiCodeActionDialog, failed);
  gtk_widget_class_bind_template_child (widget_class, GbpCodeuiCodeActionDialog, failed_row);
  gtk_widget_class_bind_template_child (widget_class, GbpCodeuiCodeActionDialog, list);
  gtk_widget_class_bind_template_child (widget_class, GbpCodeuiCodeActionDialog, list_box);
  gtk_widget_class_bind_template_child (widget_class, GbpCodeuiCodeActionDialog, loading);
  gtk_widget_class_bind_template_child (widget_class, GbpCodeuiCodeActionDialog, stack);
  gtk_widget_class_bind_template_callback (widget_class, gbp_codeui_code_action_dialog_apply_cb);
  gtk_widget_class_bind_template_callback (widget_class, gbp_codeui_code_action_dialog_row_selected_cb);
}

static void
gbp_codeui_code_action_dialog_init (GbpCodeuiCodeActionDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  adw_alert_dialog_set_response_enabled (ADW_ALERT_DIALOG (self), "apply", FALSE);
}

AdwDialog *
gbp_codeui_code_action_dialog_new (IdeBuffer *buffer)
{
  g_return_val_if_fail (IDE_IS_BUFFER (buffer), NULL);

  return g_object_new (GBP_TYPE_CODEUI_CODE_ACTION_DIALOG,
                       "buffer", buffer,
                       NULL);
}
