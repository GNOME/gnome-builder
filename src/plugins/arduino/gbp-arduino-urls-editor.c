/*
 * gbp-arduino-urls_editor.c
 *
 * Copyright 2025 Nokse <nokse@posteo.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#define G_LOG_DOMAIN "gbp-arduino-urls_editor"

#include "config.h"

#include <libide-foundry.h>
#include <libide-gtk.h>
#include <libide-gui.h>
#include <libide-tweaks.h>

#include "gbp-arduino-application-addin.h"
#include "gbp-arduino-urls-editor.h"
#include "gbp-arduino-string-row.h"

struct _GbpArduinoUrlsEditor
{
  GtkWidget parent_instance;

  GtkBox     *box;
  GtkListBox *list_box;
  GtkStack   *stack;
};

G_DEFINE_FINAL_TYPE (GbpArduinoUrlsEditor, gbp_arduino_urls_editor, GTK_TYPE_WIDGET)

static void
update_list_box (GbpArduinoUrlsEditor *self);

static void
on_row_remove_cb (GbpArduinoUrlsEditor *self,
                  GbpArduinoStringRow  *row)
{
  GbpArduinoApplicationAddin *arduino_app;
  const char *url;

  g_assert (GBP_IS_ARDUINO_URLS_EDITOR (self));
  g_assert (GBP_IS_ARDUINO_STRING_ROW (row));

  arduino_app = ide_application_find_addin_by_module_name (IDE_APPLICATION_DEFAULT, "arduino");

  url = gbp_arduino_string_row_get_name (row);

  if (gbp_arduino_application_addin_remove_additional_url (arduino_app, url))
    {
      update_list_box(self);
    }
}

static GtkWidget *
gbp_arduino_urls_editor_create_row_cb (gpointer item,
                                       gpointer user_data)
{
  GbpArduinoUrlsEditor *self = user_data;
  GtkStringObject *object = item;
  const char *str = gtk_string_object_get_string (object);
  GtkWidget *row = gbp_arduino_string_row_new (str);

  g_signal_connect_object (row,
                           "remove",
                           G_CALLBACK (on_row_remove_cb),
                           self,
                           G_CONNECT_SWAPPED);

  return row;
}

static void
update_list_box (GbpArduinoUrlsEditor *self)
{
  GbpArduinoApplicationAddin *arduino_app;
  GtkStringList *string_list;
  const char * const *strv;
  const char *visible_page;

  arduino_app = ide_application_find_addin_by_module_name (IDE_APPLICATION_DEFAULT, "arduino");

  strv = gbp_arduino_application_addin_get_additional_urls (arduino_app);

  string_list = gtk_string_list_new (strv);

  gtk_list_box_bind_model (self->list_box,
                           G_LIST_MODEL (string_list),
                           gbp_arduino_urls_editor_create_row_cb,
                           self, NULL);

  visible_page = g_list_model_get_n_items (G_LIST_MODEL (string_list)) > 0 ? "urls" : "empty";
  gtk_stack_set_visible_child_name (GTK_STACK (self->stack), visible_page);
}

static void
on_url_entry_activate_cb (GbpArduinoUrlsEditor *self,
                          const char           *text,
                          IdeEntryPopover      *popover)
{
  GbpArduinoApplicationAddin *arduino_app;

  g_assert (GBP_IS_ARDUINO_URLS_EDITOR (self));
  g_assert (IDE_IS_ENTRY_POPOVER (popover));

  arduino_app = ide_application_find_addin_by_module_name (IDE_APPLICATION_DEFAULT, "arduino");

  if (gbp_arduino_application_addin_add_additional_url (arduino_app, text))
    {
      update_list_box (self);
    }
}

static void
on_url_entry_changed_cb (GbpArduinoUrlsEditor *self,
                         IdeEntryPopover      *popover)
{
  const char *text;

  g_assert (IDE_IS_ENTRY_POPOVER (popover));
  g_assert (GBP_IS_ARDUINO_URLS_EDITOR (self));

  text = ide_entry_popover_get_text (popover);

  ide_entry_popover_set_ready (popover, !ide_str_empty0 (text));
}

static void
gbp_arduino_urls_editor_constructed (GObject *object)
{
  G_OBJECT_CLASS (gbp_arduino_urls_editor_parent_class)->constructed (object);
}

static void
gbp_arduino_urls_editor_dispose (GObject *object)
{
  GbpArduinoUrlsEditor *self = (GbpArduinoUrlsEditor *)object;

  g_clear_pointer ((GtkWidget **)&self->box, gtk_widget_unparent);

  G_OBJECT_CLASS (gbp_arduino_urls_editor_parent_class)->dispose (object);
}

static void
gbp_arduino_urls_editor_class_init (GbpArduinoUrlsEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gbp_arduino_urls_editor_constructed;
  object_class->dispose = gbp_arduino_urls_editor_dispose;

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/arduino/gbp-arduino-urls-editor.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpArduinoUrlsEditor, box);
  gtk_widget_class_bind_template_child (widget_class, GbpArduinoUrlsEditor, list_box);
  gtk_widget_class_bind_template_child (widget_class, GbpArduinoUrlsEditor, stack);

  gtk_widget_class_bind_template_callback (widget_class, on_url_entry_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_url_entry_activate_cb);

  g_type_ensure (IDE_TYPE_ENTRY_POPOVER);
}

static void
gbp_arduino_urls_editor_init (GbpArduinoUrlsEditor *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  update_list_box(self);
}

GtkWidget *
gbp_arduino_urls_editor_new (void)
{
  return g_object_new (GBP_TYPE_ARDUINO_URLS_EDITOR,
                       NULL);
}

