/*
 * gbp-arduino-libraries-editor.c
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

#define G_LOG_DOMAIN "gbp-arduino-libraries-editor"

#include "config.h"

#include <libide-foundry.h>
#include <libide-gtk.h>
#include <libide-gui.h>
#include <libide-tweaks.h>

#include "gbp-arduino-application-addin.h"
#include "gbp-arduino-libraries-editor.h"
#include "gbp-arduino-string-row.h"
#include "gbp-arduino-library-info.h"

struct _GbpArduinoLibrariesEditor
{
  GtkWidget parent_instance;

  IdeTweaksBinding *binding;

  AdwPreferencesGroup *group;
  GtkStack *stack;
  GtkListBox *list_box;
  GtkListBox *libraries_list_box;
  GListModel *libraries_list_model;
};

enum
{
  PROP_0,
  PROP_BINDING,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GbpArduinoLibrariesEditor, gbp_arduino_libraries_editor, GTK_TYPE_WIDGET)

static GParamSpec *properties[N_PROPS];

static void
on_row_remove_cb (GbpArduinoLibrariesEditor *self,
                  GbpArduinoStringRow       *row)
{
  g_auto (GStrv) value = NULL;
  const char *library_name;

  g_assert (GBP_IS_ARDUINO_LIBRARIES_EDITOR (self));
  g_assert (GBP_IS_ARDUINO_STRING_ROW (row));

  if (self->binding == NULL)
    return;

  if (!(library_name = gbp_arduino_string_row_get_name (row)))
    return;

  value = ide_tweaks_binding_dup_strv (self->binding);
  if (ide_strv_remove_from_set (value, library_name))
    ide_tweaks_binding_set_strv (self->binding, (const char *const *) value);
}

static GtkWidget *
gbp_arduino_libraries_editor_create_row_cb (gpointer item,
                                            gpointer user_data)
{
  GbpArduinoLibrariesEditor *self = user_data;
  GtkStringObject *object = item;
  const char *str = gtk_string_object_get_string (object);
  GtkWidget *row = gbp_arduino_string_row_new (str);

  g_assert (GBP_IS_ARDUINO_LIBRARIES_EDITOR (self));

  g_signal_connect_object (row,
                           "remove",
                           G_CALLBACK (on_row_remove_cb),
                           self,
                           G_CONNECT_SWAPPED);

  return row;
}

static void
on_binding_changed_cb (GbpArduinoLibrariesEditor *self,
                       IdeTweaksBinding          *binding)
{
  g_auto (GStrv) strv = NULL;
  GType expected_type;
  g_autoptr (GtkStringList) model = NULL;
  g_auto (GValue) value = G_VALUE_INIT;
  const char *visible_page;

  g_assert (GBP_IS_ARDUINO_LIBRARIES_EDITOR (self));
  g_assert (IDE_IS_TWEAKS_BINDING (binding));

  g_value_init (&value, G_TYPE_STRV);

  if (binding == NULL || G_OBJECT_GET_CLASS (binding) == NULL)
    return;

  ide_tweaks_binding_get_expected_type (binding, &expected_type);

  strv = ide_tweaks_binding_dup_strv (binding);
  model = gtk_string_list_new ((const char *const *) strv);

  gtk_list_box_bind_model (self->list_box,
                           G_LIST_MODEL (model),
                           gbp_arduino_libraries_editor_create_row_cb,
                           self, NULL);

  visible_page = g_list_model_get_n_items (G_LIST_MODEL (model)) > 0 ? "libraries" : "empty";
  gtk_stack_set_visible_child_name (GTK_STACK (self->stack), visible_page);

  gtk_widget_set_visible (GTK_WIDGET (self->list_box),
                          g_list_model_get_n_items (G_LIST_MODEL (model)) > 0);
}

GtkWidget *
libraries_create_row_cb (gpointer item,
                         gpointer user_data)
{
  GbpArduinoLibraryInfo *library = item;
  GtkWidget *box;
  GtkWidget *header_box;
  GtkWidget *name_label;
  GtkWidget *version_label;
  GtkWidget *author_label;
  GtkWidget *description_label;
  g_autofree char *author_text = NULL;

  g_assert (GBP_IS_ARDUINO_LIBRARY_INFO (library));

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_widget_set_margin_top (box, 6);
  gtk_widget_set_margin_bottom (box, 6);
  gtk_widget_set_margin_start (box, 6);
  gtk_widget_set_margin_end (box, 6);
  header_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_append (GTK_BOX (box), header_box);

  /* Library name label */
  name_label = gtk_label_new (gbp_arduino_library_info_get_name (library));
  gtk_widget_add_css_class (name_label, "heading");
  gtk_label_set_ellipsize (GTK_LABEL (name_label), PANGO_ELLIPSIZE_END);
  gtk_label_set_xalign (GTK_LABEL (name_label), 0.0f);
  gtk_box_append (GTK_BOX (header_box), name_label);

  /* Author label */
  author_text = g_strdup_printf ("by %s", gbp_arduino_library_info_get_author (library));
  author_label = gtk_label_new (author_text);
  gtk_widget_set_hexpand (author_label, TRUE);
  gtk_label_set_ellipsize (GTK_LABEL (author_label), PANGO_ELLIPSIZE_END);
  gtk_label_set_xalign (GTK_LABEL (author_label), 0.0f);
  gtk_widget_add_css_class (author_label, "dim-label");
  gtk_widget_add_css_class (author_label, "caption");
  gtk_box_append (GTK_BOX (header_box), author_label);

  /* Version label */
  version_label = gtk_label_new (gbp_arduino_library_info_get_latest_version (library));
  gtk_widget_add_css_class (version_label, "dim-label");
  gtk_box_append (GTK_BOX (header_box), version_label);

  /* Description label */
  description_label = gtk_label_new (gbp_arduino_library_info_get_description (library));
  gtk_label_set_ellipsize (GTK_LABEL (description_label), PANGO_ELLIPSIZE_END);
  gtk_label_set_lines (GTK_LABEL (description_label), 2);
  gtk_label_set_xalign (GTK_LABEL (description_label), 0.0f);
  gtk_label_set_wrap (GTK_LABEL (description_label), TRUE);
  gtk_widget_add_css_class (description_label, "caption");
  gtk_box_append (GTK_BOX (box), description_label);

  return box;
}

static void
on_search_list_row_activated_cb (GbpArduinoLibrariesEditor *self,
                                 GtkListBoxRow             *row,
                                 GtkListBox                *list_box)
{
  g_autoptr(GbpArduinoLibraryInfo) library_info = NULL;
  const char *library_text;
  guint index;
  g_auto (GStrv) libraries = NULL;

  g_assert (GBP_IS_ARDUINO_LIBRARIES_EDITOR (self));
  g_assert (GTK_IS_LIST_BOX (list_box));
  g_assert (GTK_IS_LIST_BOX_ROW (row));

  if (self->binding == NULL)
    return;

  index = gtk_list_box_row_get_index (row);

  library_info = g_list_model_get_item (G_LIST_MODEL (self->libraries_list_model), index);

  library_text = g_strdup_printf ("%s (%s)",
                                  gbp_arduino_library_info_get_name (library_info),
                                  gbp_arduino_library_info_get_latest_version (library_info));

  libraries = ide_tweaks_binding_dup_strv (self->binding);

  if (ide_strv_add_to_set (&libraries, g_strdup (library_text)))
    {
      ide_tweaks_binding_set_strv (self->binding, (const char *const *) libraries);
    }
}

static void
gbp_arduino_libraries_editor_constructed (GObject *object)
{
  GbpArduinoLibrariesEditor *self = (GbpArduinoLibrariesEditor *) object;
  GbpArduinoApplicationAddin *arduino_app;
  GType type;

  g_assert (GBP_IS_ARDUINO_LIBRARIES_EDITOR (self));

  G_OBJECT_CLASS (gbp_arduino_libraries_editor_parent_class)->constructed (object);

  if (self->binding == NULL)
    return;

  if (!ide_tweaks_binding_get_expected_type (self->binding, &type) || type != G_TYPE_STRV)
    return;

  arduino_app = ide_application_find_addin_by_module_name (IDE_APPLICATION_DEFAULT, "arduino");

  self->libraries_list_model = gbp_arduino_application_addin_get_installed_libraries (arduino_app);

  gtk_list_box_bind_model (self->libraries_list_box,
                           self->libraries_list_model,
                           libraries_create_row_cb,
                           self, NULL);

  g_signal_connect_object (self->binding,
                           "changed",
                           G_CALLBACK (on_binding_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  on_binding_changed_cb (self, self->binding);
}

static void
gbp_arduino_libraries_editor_dispose (GObject *object)
{
  GbpArduinoLibrariesEditor *self = (GbpArduinoLibrariesEditor *)object;

  g_clear_pointer ((GtkWidget **)&self->group, gtk_widget_unparent);

  g_clear_object (&self->binding);

  G_OBJECT_CLASS (gbp_arduino_libraries_editor_parent_class)->dispose (object);
}

static void
gbp_arduino_libraries_editor_get_property (GObject    *object,
                                           guint       prop_id,
                                           GValue     *value,
                                           GParamSpec *pspec)
{
  GbpArduinoLibrariesEditor *self = GBP_ARDUINO_LIBRARIES_EDITOR (object);

  switch (prop_id)
    {
    case PROP_BINDING:
      g_value_set_object (value, self->binding);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_arduino_libraries_editor_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  GbpArduinoLibrariesEditor *self = GBP_ARDUINO_LIBRARIES_EDITOR (object);

  switch (prop_id)
    {
    case PROP_BINDING:
      g_set_object (&self->binding, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_arduino_libraries_editor_class_init (GbpArduinoLibrariesEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gbp_arduino_libraries_editor_constructed;
  object_class->dispose = gbp_arduino_libraries_editor_dispose;
  object_class->get_property = gbp_arduino_libraries_editor_get_property;
  object_class->set_property = gbp_arduino_libraries_editor_set_property;

  properties[PROP_BINDING] =
      g_param_spec_object ("binding", NULL, NULL,
                           IDE_TYPE_TWEAKS_BINDING,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/arduino/gbp-arduino-libraries-editor.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpArduinoLibrariesEditor, group);
  gtk_widget_class_bind_template_child (widget_class, GbpArduinoLibrariesEditor, list_box);
  gtk_widget_class_bind_template_child (widget_class, GbpArduinoLibrariesEditor, libraries_list_box);
  gtk_widget_class_bind_template_child (widget_class, GbpArduinoLibrariesEditor, stack);

  gtk_widget_class_bind_template_callback (widget_class, on_search_list_row_activated_cb);

  g_type_ensure (IDE_TYPE_ENTRY_POPOVER);
}

static void
gbp_arduino_libraries_editor_init (GbpArduinoLibrariesEditor *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
gbp_arduino_libraries_editor_new (IdeTweaksBinding *binding)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_BINDING (binding), NULL);

  return g_object_new (GBP_TYPE_ARDUINO_LIBRARIES_EDITOR,
                       "binding", binding,
                       NULL);
}

