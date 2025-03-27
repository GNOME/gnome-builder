/*
 * gbp-arduino-platforms-editor.c
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

#define G_LOG_DOMAIN "gbp-arduino-platforms-editor"

#include "config.h"

#include <libide-foundry.h>
#include <libide-gtk.h>
#include <libide-gui.h>
#include <libide-tweaks.h>

#include "gbp-arduino-application-addin.h"
#include "gbp-arduino-platforms-editor.h"
#include "gbp-arduino-platform-editor-row.h"
#include "gbp-arduino-profile.h"
#include "gbp-arduino-platform.h"
#include "gbp-arduino-platform-info.h"

struct _GbpArduinoPlatformsEditor
{
  GtkWidget parent_instance;

  IdeTweaksBinding *binding;

  AdwPreferencesGroup *group;
  GtkStack *stack;
  GtkLabel *label;
  GtkListBox *list_box;
  GtkListBox *platforms_list_box;
  GListModel *platforms_list_model;
};

enum
{
  PROP_0,
  PROP_BINDING,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GbpArduinoPlatformsEditor, gbp_arduino_platforms_editor, GTK_TYPE_WIDGET)

static GParamSpec *properties[N_PROPS];

static void
on_row_remove_cb (GbpArduinoPlatformsEditor *self,
                  GbpArduinoPlatformEditorRow *row)
{
  g_autoptr (GbpArduinoProfile) config = NULL;

  g_assert (GBP_IS_ARDUINO_PLATFORMS_EDITOR (self));
  g_assert (GBP_IS_ARDUINO_PLATFORM_EDITOR_ROW (row));

  if (self->binding == NULL)
    return;

  config = GBP_ARDUINO_PROFILE (ide_tweaks_property_dup_object (IDE_TWEAKS_PROPERTY (self->binding)));

  gbp_arduino_profile_remove_platform (config, gbp_arduino_platform_editor_row_get_platform (row));
}

static GtkWidget *
gbp_arduino_platforms_editor_create_row_cb (gpointer item,
                                            gpointer user_data)
{
  GbpArduinoPlatformsEditor *self = user_data;
  GbpArduinoPlatform *platform = item;
  GtkWidget *row;

  g_return_val_if_fail (GBP_IS_ARDUINO_PLATFORMS_EDITOR (self), NULL);
  g_return_val_if_fail (GBP_IS_ARDUINO_PLATFORM (platform), NULL);

  row = gbp_arduino_platform_editor_row_new (platform);
  if (!GTK_IS_WIDGET (row))
    return NULL;

  g_signal_connect_object (row,
                           "remove",
                           G_CALLBACK (on_row_remove_cb),
                           self,
                           G_CONNECT_SWAPPED);

  return row;
}

GtkWidget *
platforms_create_row_cb (gpointer item,
                         gpointer user_data)
{
  GbpArduinoPlatformInfo *platform = item;
  GtkWidget *box;
  GtkWidget *header_box;
  GtkWidget *name_label;
  GtkWidget *version_label;
  GtkWidget *author_label;
  g_autofree char *author_text = NULL;

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

  return box;
}

static void
on_search_list_row_activated_cb (GbpArduinoPlatformsEditor *self,
                                 GtkListBoxRow             *row,
                                 GtkListBox                *list_box)
{
  g_autoptr (GbpArduinoPlatformInfo) platform_info = NULL;
  g_autoptr (GbpArduinoProfile) config = NULL;
  g_autoptr (GbpArduinoPlatform) platform = NULL;
  guint index;

  g_assert (GBP_IS_ARDUINO_PLATFORMS_EDITOR (self));
  g_assert (GTK_IS_LIST_BOX (list_box));
  g_assert (GTK_IS_LIST_BOX_ROW (row));

  if (self->binding == NULL)
    return;

  index = gtk_list_box_row_get_index (row);

  platform_info = g_list_model_get_item (G_LIST_MODEL (self->platforms_list_model), index);

  platform = gbp_arduino_platform_new (gbp_arduino_platform_info_get_name (platform_info),
                                       gbp_arduino_platform_info_get_version (platform_info),
                                       NULL);

  config = GBP_ARDUINO_PROFILE (ide_tweaks_property_dup_object (IDE_TWEAKS_PROPERTY (self->binding)));

  gbp_arduino_profile_add_platform (config, platform);
}

static void
update_visibility (GbpArduinoPlatformsEditor *self,
                   GListModel *model)
{
  const char *visible_page = g_list_model_get_n_items (G_LIST_MODEL (model)) > 0 ? "platforms" : "empty";
  gtk_stack_set_visible_child_name (GTK_STACK (self->stack), visible_page);
}

static void
on_platforms_model_changed_cb (GbpArduinoPlatformsEditor *self,
                               guint                      position,
                               guint                      removed,
                               guint                      added,
                               GListModel                *model)
{
  update_visibility (self, model);
}

static void
gbp_arduino_platforms_editor_constructed (GObject *object)
{
  GbpArduinoPlatformsEditor *self = (GbpArduinoPlatformsEditor *)object;
  GbpArduinoApplicationAddin *arduino_app;
  g_autoptr (GbpArduinoProfile) config = NULL;
  GListModel *model;

  g_assert (GBP_IS_ARDUINO_PLATFORMS_EDITOR (self));

  if (self->binding == NULL || G_OBJECT_GET_CLASS (self->binding) == NULL)
    return;

  config = GBP_ARDUINO_PROFILE (ide_tweaks_property_dup_object (IDE_TWEAKS_PROPERTY (self->binding)));

  model = gbp_arduino_profile_get_platforms (config);

  gtk_list_box_bind_model (self->list_box,
                           model,
                           gbp_arduino_platforms_editor_create_row_cb,
                           self, NULL);

  arduino_app = ide_application_find_addin_by_module_name (IDE_APPLICATION_DEFAULT, "arduino");

  self->platforms_list_model = gbp_arduino_application_addin_get_installed_platforms (arduino_app);

  gtk_list_box_bind_model (self->platforms_list_box,
                           self->platforms_list_model,
                           platforms_create_row_cb,
                           self, NULL);

  g_signal_connect_object (model,
                           "items-changed",
                           G_CALLBACK (on_platforms_model_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  update_visibility (self, model);
}

static void
gbp_arduino_platforms_editor_dispose (GObject *object)
{
  GbpArduinoPlatformsEditor *self = (GbpArduinoPlatformsEditor *)object;

  g_clear_pointer ((GtkWidget **)&self->group, gtk_widget_unparent);

  g_clear_object (&self->binding);

  G_OBJECT_CLASS (gbp_arduino_platforms_editor_parent_class)->dispose (object);
}

static void
gbp_arduino_platforms_editor_get_property (GObject    *object,
                                           guint       prop_id,
                                           GValue     *value,
                                           GParamSpec *pspec)
{
  GbpArduinoPlatformsEditor *self = GBP_ARDUINO_PLATFORMS_EDITOR (object);

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
gbp_arduino_platforms_editor_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  GbpArduinoPlatformsEditor *self = GBP_ARDUINO_PLATFORMS_EDITOR (object);

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
gbp_arduino_platforms_editor_class_init (GbpArduinoPlatformsEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gbp_arduino_platforms_editor_constructed;
  object_class->dispose = gbp_arduino_platforms_editor_dispose;
  object_class->get_property = gbp_arduino_platforms_editor_get_property;
  object_class->set_property = gbp_arduino_platforms_editor_set_property;

  properties[PROP_BINDING] =
      g_param_spec_object ("binding", NULL, NULL,
                           IDE_TYPE_TWEAKS_BINDING,
                           (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/arduino/gbp-arduino-platforms-editor.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpArduinoPlatformsEditor, group);
  gtk_widget_class_bind_template_child (widget_class, GbpArduinoPlatformsEditor, list_box);
  gtk_widget_class_bind_template_child (widget_class, GbpArduinoPlatformsEditor, platforms_list_box);
  gtk_widget_class_bind_template_child (widget_class, GbpArduinoPlatformsEditor, stack);

  gtk_widget_class_bind_template_callback (widget_class, on_search_list_row_activated_cb);

  g_type_ensure (IDE_TYPE_ENTRY_POPOVER);
}

static void
gbp_arduino_platforms_editor_init (GbpArduinoPlatformsEditor *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
gbp_arduino_platforms_editor_new (IdeTweaksBinding *binding)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_BINDING (binding), NULL);

  return g_object_new (GBP_TYPE_ARDUINO_PLATFORMS_EDITOR,
                       "binding", binding,
                       NULL);
}

