/* gbp-shortcutui-dialog.c
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

#define G_LOG_DOMAIN "gbp-shortcutui-dialog"

#include "config.h"

#include <libide-gui.h>

#include "gbp-shortcutui-action.h"
#include "gbp-shortcutui-action-model.h"
#include "gbp-shortcutui-dialog.h"
#include "gbp-shortcutui-row.h"

struct _GbpShortcutuiDialog
{
  GtkWindow           parent_instance;
  GtkSearchEntry     *search;
  GtkListBox         *results_list_box;
  AdwPreferencesPage *overview;
  AdwPreferencesPage *results;
  AdwPreferencesPage *empty;
  GtkStringFilter    *string_filter;
  GtkFilterListModel *filter_model;
  guint               update_source;
};

G_DEFINE_FINAL_TYPE (GbpShortcutuiDialog, gbp_shortcutui_dialog, GTK_TYPE_WINDOW)

static void
gbp_shortcutui_dialog_update_header_cb (GtkListBoxRow *row,
                                        GtkListBoxRow *before,
                                        gpointer       user_data)
{
  g_assert (GBP_IS_SHORTCUTUI_ROW (row));
  g_assert (!before || GBP_IS_SHORTCUTUI_ROW (before));

  gbp_shortcutui_row_update_header (GBP_SHORTCUTUI_ROW (row),
                                    GBP_SHORTCUTUI_ROW (before));
}

static GtkWidget *
gbp_shortcutui_dialog_create_row_cb (gpointer item,
                                     gpointer user_data)
{
  GbpShortcutuiAction *action = item;

  g_assert (GBP_IS_SHORTCUTUI_ACTION (action));

  return g_object_new (GBP_TYPE_SHORTCUTUI_ROW,
                       "action", action,
                       NULL);
}

static gboolean
gbp_shortcutui_dialog_update_visible (gpointer user_data)
{
  GbpShortcutuiDialog *self = user_data;
  const char *text;
  guint n_items;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SHORTCUTUI_DIALOG (self));

  self->update_source = 0;

  text = gtk_editable_get_text (GTK_EDITABLE (self->search));
  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->filter_model));

  if (ide_str_empty0 (text))
    {
      gtk_widget_show (GTK_WIDGET (self->overview));
      gtk_widget_hide (GTK_WIDGET (self->results));
      gtk_widget_hide (GTK_WIDGET (self->empty));
    }
  else
    {
      gboolean has_results = n_items > 0;

      gtk_widget_hide (GTK_WIDGET (self->overview));
      gtk_widget_set_visible (GTK_WIDGET (self->results), has_results);
      gtk_widget_set_visible (GTK_WIDGET (self->empty), !has_results);
    }

  return G_SOURCE_REMOVE;
}

static void
gbp_shortcutui_dialog_queue_update (GbpShortcutuiDialog *self)
{
  g_assert (GBP_IS_SHORTCUTUI_DIALOG (self));

  if (self->update_source == 0)
    self->update_source = g_timeout_add (250, gbp_shortcutui_dialog_update_visible, self);
}

static void
gbp_shortcutui_dialog_dispose (GObject *object)
{
  GbpShortcutuiDialog *self = (GbpShortcutuiDialog *)object;

  g_clear_handle_id (&self->update_source, g_source_remove);

  G_OBJECT_CLASS (gbp_shortcutui_dialog_parent_class)->dispose (object);
}

void
gbp_shortcutui_dialog_set_model (GbpShortcutuiDialog *self,
                                 GListModel          *model)
{
  g_autoptr(GListModel) wrapped = NULL;

  g_return_if_fail (GBP_IS_SHORTCUTUI_DIALOG (self));
  g_return_if_fail (G_IS_LIST_MODEL (model));

  wrapped = gbp_shortcutui_action_model_new (model);
  gtk_filter_list_model_set_model (self->filter_model, wrapped);
}

static void
gbp_shortcutui_dialog_class_init (GbpShortcutuiDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gbp_shortcutui_dialog_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/shortcutui/gbp-shortcutui-dialog.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpShortcutuiDialog, empty);
  gtk_widget_class_bind_template_child (widget_class, GbpShortcutuiDialog, filter_model);
  gtk_widget_class_bind_template_child (widget_class, GbpShortcutuiDialog, overview);
  gtk_widget_class_bind_template_child (widget_class, GbpShortcutuiDialog, results);
  gtk_widget_class_bind_template_child (widget_class, GbpShortcutuiDialog, results_list_box);
  gtk_widget_class_bind_template_child (widget_class, GbpShortcutuiDialog, search);
  gtk_widget_class_bind_template_child (widget_class, GbpShortcutuiDialog, string_filter);
  gtk_widget_class_bind_template_callback (widget_class, gbp_shortcutui_dialog_queue_update);
}

static void
gbp_shortcutui_dialog_init (GbpShortcutuiDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_header_func (self->results_list_box,
                                gbp_shortcutui_dialog_update_header_cb,
                                NULL, NULL);
  gtk_list_box_bind_model (self->results_list_box,
                           G_LIST_MODEL (self->filter_model),
                           gbp_shortcutui_dialog_create_row_cb,
                           NULL, NULL);
}
