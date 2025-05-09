/*
 * gbp-arduino-platforms-manager.c
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
 * along with this platform; if not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#define G_LOG_DOMAIN "gbp-arduino-platforms-manager"

#include "config.h"

#include <libide-foundry.h>
#include <libide-gtk.h>
#include <libide-gui.h>
#include <libide-tweaks.h>

#include "gbp-arduino-application-addin.h"
#include "gbp-arduino-platforms-manager.h"
#include "gbp-arduino-string-row.h"
#include "gbp-arduino-platform-info.h"

struct _GbpArduinoPlatformsManager
{
  GtkWidget parent_instance;

  GtkBox *box;
  GtkStack *stack;
  GtkStack *search_stack;
  GtkLabel *label;
  GtkListBox *list_box;
  GtkListBox *search_list_box;
  GtkSearchEntry *search_entry;
  GtkMenuButton *menu_button;
  GListStore *search_list_model;
};

G_DEFINE_FINAL_TYPE (GbpArduinoPlatformsManager, gbp_arduino_platforms_manager, GTK_TYPE_WIDGET)

static void
on_row_remove_cb (GbpArduinoPlatformsManager *self,
                  GbpArduinoStringRow        *row)
{
  GbpArduinoApplicationAddin *arduino_app;

  g_assert (GBP_IS_ARDUINO_PLATFORMS_MANAGER (self));
  g_assert (GBP_IS_ARDUINO_STRING_ROW (row));

  arduino_app = ide_application_find_addin_by_module_name (IDE_APPLICATION_DEFAULT, "arduino");

  gbp_arduino_application_addin_uninstall_platform (arduino_app,
                                                    gbp_arduino_string_row_get_name (row));
}

static GtkWidget *
gbp_arduino_platforms_manager_create_row_cb (gpointer item,
                                             gpointer user_data)
{
  GbpArduinoPlatformsManager *self = user_data;
  GbpArduinoPlatformInfo *platform_info = item;
  const char *str = gbp_arduino_platform_info_get_name (platform_info);
  GtkWidget *row = gbp_arduino_string_row_new (str);

  g_assert (GBP_IS_ARDUINO_PLATFORMS_MANAGER (self));

  g_signal_connect_object (row,
                           "remove",
                           G_CALLBACK (on_row_remove_cb),
                           self,
                           G_CONNECT_SWAPPED);

  return row;
}

GtkWidget *
platforms_search_create_row_cb (gpointer item,
                                gpointer user_data)
{
  GbpArduinoPlatformInfo *platform = item;
  GtkWidget *box;
  GtkWidget *header_box;
  GtkWidget *name_label;
  GtkWidget *version_label;
  GtkWidget *author_label;
  GtkWidget *description_label;
  g_autofree char *author_text = NULL;
  g_autofree char *description_text = NULL;
  const char *const *fqbns;

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_widget_set_margin_top (box, 6);
  gtk_widget_set_margin_bottom (box, 6);
  gtk_widget_set_margin_start (box, 6);
  gtk_widget_set_margin_end (box, 6);

  header_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_append (GTK_BOX (box), header_box);

  /* Library name label */
  name_label = gtk_label_new (gbp_arduino_platform_info_get_name (platform));
  gtk_widget_add_css_class (name_label, "heading");
  gtk_label_set_ellipsize (GTK_LABEL (name_label), PANGO_ELLIPSIZE_END);
  gtk_label_set_xalign (GTK_LABEL (name_label), 0.0f);
  gtk_box_append (GTK_BOX (header_box), name_label);

  /* Author label */
  author_text = g_strdup_printf ("by %s", gbp_arduino_platform_info_get_maintainer (platform));
  author_label = gtk_label_new (author_text);
  gtk_widget_set_hexpand (author_label, TRUE);
  gtk_label_set_ellipsize (GTK_LABEL (author_label), PANGO_ELLIPSIZE_END);
  gtk_label_set_xalign (GTK_LABEL (author_label), 0.0f);
  gtk_widget_add_css_class (author_label, "dim-label");
  gtk_widget_add_css_class (author_label, "caption");
  gtk_box_append (GTK_BOX (header_box), author_label);

  /* Version label */
  version_label = gtk_label_new (gbp_arduino_platform_info_get_version (platform));
  gtk_widget_add_css_class (version_label, "dim-label");
  gtk_box_append (GTK_BOX (header_box), version_label);

  fqbns = gbp_arduino_platform_info_get_supported_fqbns (platform);
  if (fqbns != NULL && *fqbns != NULL)
  {
    GString *fqbn_list = g_string_new (NULL);
    guint count = 0;

    for (guint i = 0; fqbns[i] != NULL && count < 5; i++, count++)
    {
      if (count > 0)
        g_string_append (fqbn_list, ", ");
      g_string_append (fqbn_list, fqbns[i]);
    }

    description_text = g_string_free (fqbn_list, FALSE);
  }

  if (description_text)
    {
      description_label = gtk_label_new (description_text);
      gtk_label_set_ellipsize (GTK_LABEL (description_label), PANGO_ELLIPSIZE_END);
      gtk_label_set_lines (GTK_LABEL (description_label), 2);
      gtk_label_set_xalign (GTK_LABEL (description_label), 0.0f);
      gtk_label_set_wrap (GTK_LABEL (description_label), TRUE);
      gtk_widget_add_css_class (description_label, "caption");
      gtk_box_append (GTK_BOX (box), description_label);
    }

  return box;
}

static void
on_search_entry_activate_cb (GbpArduinoPlatformsManager *self)
{
  GbpArduinoApplicationAddin *arduino_app;
  gboolean has_results;
  const char *search_text = NULL;

  g_assert (GBP_IS_ARDUINO_PLATFORMS_MANAGER (self));

  search_text = gtk_editable_get_text (GTK_EDITABLE (self->search_entry));

  arduino_app = ide_application_find_addin_by_module_name (IDE_APPLICATION_DEFAULT, "arduino");

  g_clear_object (&self->search_list_model);
  self->search_list_model = gbp_arduino_application_addin_search_platform (arduino_app, search_text);

  if (self->search_list_model)
    has_results = g_list_model_get_n_items (G_LIST_MODEL (self->search_list_model)) > 0;
  else
    has_results = FALSE;

  gtk_widget_set_visible (GTK_WIDGET(self->label), !has_results);
  gtk_stack_set_visible_child_name (GTK_STACK (self->search_stack), has_results ? "results" : "empty");

  gtk_list_box_bind_model (self->search_list_box,
                           G_LIST_MODEL (self->search_list_model),
                           platforms_search_create_row_cb,
                           self, NULL);
}

static void
on_search_list_row_activated_cb (GbpArduinoPlatformsManager *self,
                                 GtkListBoxRow              *row,
                                 GtkListBox                 *list_box)
{
  g_autoptr (GbpArduinoPlatformInfo) platform_info = NULL;
  GbpArduinoApplicationAddin *arduino_app;

  g_assert (GBP_IS_ARDUINO_PLATFORMS_MANAGER (self));
  g_assert (GTK_IS_LIST_BOX (list_box));
  g_assert (GTK_IS_LIST_BOX_ROW (row));

  arduino_app = ide_application_find_addin_by_module_name (IDE_APPLICATION_DEFAULT, "arduino");

  platform_info = g_list_model_get_item (G_LIST_MODEL (self->search_list_model),
                                         gtk_list_box_row_get_index (row));

  gbp_arduino_application_addin_install_platform (arduino_app,
                                                  gbp_arduino_platform_info_get_name (platform_info));

  gtk_menu_button_popdown (self->menu_button);
}

static void
on_installed_platforms_changed_cb (GbpArduinoPlatformsManager *self)
{
  GbpArduinoApplicationAddin *arduino_app;
  GListModel *model = NULL;
  const char *visible_page;

  arduino_app = ide_application_find_addin_by_module_name (IDE_APPLICATION_DEFAULT, "arduino");

  model = gbp_arduino_application_addin_get_installed_platforms (arduino_app);

  gtk_list_box_bind_model (self->list_box,
                           G_LIST_MODEL (model),
                           gbp_arduino_platforms_manager_create_row_cb,
                           self, NULL);

  visible_page = g_list_model_get_n_items (G_LIST_MODEL (model)) > 0 ? "platforms" : "empty";
  gtk_stack_set_visible_child_name (GTK_STACK (self->stack), visible_page);

}

static void
gbp_arduino_platforms_manager_constructed (GObject *object)
{
  GbpArduinoPlatformsManager *self = (GbpArduinoPlatformsManager *) object;
  GbpArduinoApplicationAddin *arduino_app;

  g_assert (GBP_IS_ARDUINO_PLATFORMS_MANAGER (self));

  arduino_app = ide_application_find_addin_by_module_name (IDE_APPLICATION_DEFAULT, "arduino");

  g_signal_connect_object (arduino_app,
                           "notify::installed-platforms",
                           G_CALLBACK (on_installed_platforms_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  on_installed_platforms_changed_cb (self);

  G_OBJECT_CLASS (gbp_arduino_platforms_manager_parent_class)->constructed (object);
}

static void
gbp_arduino_platforms_manager_dispose (GObject *object)
{
  GbpArduinoPlatformsManager *self = (GbpArduinoPlatformsManager *)object;

  g_clear_pointer ((GtkWidget **)&self->box, gtk_widget_unparent);

  g_clear_object (&self->search_list_model);

  G_OBJECT_CLASS (gbp_arduino_platforms_manager_parent_class)->dispose (object);
}

static void
gbp_arduino_platforms_manager_class_init (GbpArduinoPlatformsManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gbp_arduino_platforms_manager_constructed;
  object_class->dispose = gbp_arduino_platforms_manager_dispose;

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/arduino/gbp-arduino-platforms-manager.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpArduinoPlatformsManager, box);
  gtk_widget_class_bind_template_child (widget_class, GbpArduinoPlatformsManager, list_box);
  gtk_widget_class_bind_template_child (widget_class, GbpArduinoPlatformsManager, search_list_box);
  gtk_widget_class_bind_template_child (widget_class, GbpArduinoPlatformsManager, search_entry);
  gtk_widget_class_bind_template_child (widget_class, GbpArduinoPlatformsManager, stack);
  gtk_widget_class_bind_template_child (widget_class, GbpArduinoPlatformsManager, search_stack);
  gtk_widget_class_bind_template_child (widget_class, GbpArduinoPlatformsManager, label);
  gtk_widget_class_bind_template_child (widget_class, GbpArduinoPlatformsManager, menu_button);

  gtk_widget_class_bind_template_callback (widget_class, on_search_entry_activate_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_search_list_row_activated_cb);

  g_type_ensure (IDE_TYPE_ENTRY_POPOVER);
}

static void
gbp_arduino_platforms_manager_init (GbpArduinoPlatformsManager *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
gbp_arduino_platforms_manager_new (void)
{
  return g_object_new (GBP_TYPE_ARDUINO_PLATFORMS_MANAGER,
                       NULL);
}

