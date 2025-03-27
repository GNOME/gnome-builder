/*
 * gbp-arduino-board-options-widget.c
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

#define G_LOG_DOMAIN "gbp-arduino-board-options-widget"

#include "config.h"

#include <libide-foundry.h>
#include <libide-gtk.h>
#include <libide-gui.h>

#include "gbp-arduino-application-addin.h"
#include "gbp-arduino-board-option-row.h"
#include "gbp-arduino-board-option.h"
#include "gbp-arduino-board-options-widget.h"
#include "gbp-arduino-board.h"
#include "gbp-arduino-option-value.h"
#include "gbp-arduino-profile.h"

struct _GbpArduinoBoardOptionsWidget
{
  GtkWidget parent_instance;

  IdeTweaksBinding *binding;

  GListStore *fqbn_list_model;
  GListStore *options_list_model;
  GListStore *programmers_list_model;

  AdwComboRow *fqbn_combo;
  AdwPreferencesGroup *programmer_group;
  AdwComboRow *programmer_combo;
  GtkBox      *box;
  GtkListBox  *list_box;
};

enum
{
  PROP_0,
  PROP_BINDING,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GbpArduinoBoardOptionsWidget, gbp_arduino_board_options_widget, GTK_TYPE_WIDGET)

static GParamSpec *properties[N_PROPS];

gboolean
parse_fqbn_and_flags (const char *fqbn_and_flags,
                      char      **fqbn_out,
                      GStrv      *flags_out)
{
  g_auto (GStrv) fbqn_parts = NULL;
  g_auto (GStrv) flags = NULL;

  g_return_val_if_fail (fqbn_and_flags != NULL, FALSE);
  g_return_val_if_fail (fqbn_out != NULL, FALSE);
  g_return_val_if_fail (flags_out != NULL, FALSE);

  *fqbn_out = NULL;
  *flags_out = NULL;

  if (ide_str_empty0 (fqbn_and_flags))
    {
      g_warning ("Empty FQBN");
      return FALSE;
    }

  fbqn_parts = g_regex_split_simple ("^([^:]+:[^:]+:[^:]+)(?::(.+))?$",
                                     fqbn_and_flags,
                                     0,
                                     0);

  if (fbqn_parts == NULL || fbqn_parts[1] == NULL)
    {
      g_warning ("Invalid FQBN format: %s", fqbn_and_flags);
      return FALSE;
    }

  *fqbn_out = g_strdup (fbqn_parts[1]);

  if (fbqn_parts[2] != NULL && *fbqn_parts[2] != '\0')
    {
      flags = g_regex_split_simple (",",
                                    fbqn_parts[2],
                                    0,
                                    0);
      if (flags == NULL)
        {
          return TRUE;
        }
      *flags_out = g_steal_pointer (&flags);
    }
  return TRUE;
}

gboolean
parse_flag_and_value (const char *flag_string,
                      char      **flag_name_out,
                      char      **flag_value_out)
{
  g_auto (GStrv) parts = NULL;

  g_return_val_if_fail (flag_string != NULL, FALSE);
  g_return_val_if_fail (flag_name_out != NULL, FALSE);
  g_return_val_if_fail (flag_value_out != NULL, FALSE);

  *flag_name_out = NULL;
  *flag_value_out = NULL;

  parts = g_regex_split_simple ("^([^=]+)(?:=(.+))?$",
                                flag_string,
                                0,
                                0);

  if (parts == NULL || parts[1] == NULL)
    {
      g_warning ("Invalid flag format: %s", flag_string);
      return FALSE;
    }

  *flag_name_out = g_strdup (parts[1]);

  if (parts[2] != NULL)
    {
      *flag_value_out = g_strdup (parts[2]);
    }

  return TRUE;
}

static void
update_board_options (GbpArduinoBoardOptionsWidget *self)
{
  g_autofree char *fqbn_and_flags = NULL;
  g_autofree char *fqbn = NULL;
  g_auto (GStrv) flags = NULL;
  guint options_n;

  g_assert (GBP_IS_ARDUINO_OPTIONS_WIDGET (self));

  fqbn_and_flags = ide_tweaks_binding_dup_string (self->binding);

  if (!parse_fqbn_and_flags (fqbn_and_flags, &fqbn, &flags))
    {
      g_warning ("Could not parse fqbn and flags");
      return;
    }

  if (flags == NULL)
    {
      return;
    }

  options_n = g_list_model_get_n_items (G_LIST_MODEL (self->options_list_model));

  for (guint i = 0; i < options_n; i++)
    {
      GbpArduinoBoardOptionRow *option_row;
      GbpArduinoBoardOption *option;
      GListModel *values;
      const char *option_flag;

      option_row = GBP_ARDUINO_BOARD_OPTION_ROW (gtk_list_box_get_row_at_index (self->list_box, i));
      option = gbp_arduino_board_option_row_get_option (option_row);
      values = G_LIST_MODEL (gbp_arduino_board_option_get_values (option));
      option_flag = gbp_arduino_board_option_get_option (option);

      for (guint j = 0; flags[j] != NULL; j++)
        {
          g_autofree char *flag_name;
          g_autofree char *flag_value;

          if (parse_flag_and_value (flags[j], &flag_name, &flag_value))
            {
              if (ide_str_equal0 (option_flag, flag_name))
                {
                  for (guint k = 0; k < g_list_model_get_n_items (values); k++)
                    {
                      g_autoptr(GbpArduinoOptionValue) option_value = NULL;
                      const char *value;

                      option_value = g_list_model_get_item (values, k);
                      value = gbp_arduino_option_value_get_value (option_value);
                      if (ide_str_equal0 (value, flag_value))
                        {
                          adw_combo_row_set_selected (ADW_COMBO_ROW (option_row), k);
                          break;
                        }
                    }
                  break;
                }
            }
        }
    }
}

static void
update_programmer_combo (GbpArduinoBoardOptionsWidget *self)
{
  g_autoptr(GbpArduinoProfile) config = GBP_ARDUINO_PROFILE (ide_tweaks_property_dup_object (IDE_TWEAKS_PROPERTY (self->binding)));
  const char *programmer = gbp_arduino_profile_get_programmer (config);

  for (guint i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (self->programmers_list_model)); i++)
    {
      g_autoptr(GbpArduinoOptionValue) option_value = g_list_model_get_item (G_LIST_MODEL (self->programmers_list_model), i);
      const char *programmer_key = gbp_arduino_option_value_get_value (option_value);

      if (ide_str_equal0 (programmer_key, programmer))
        {
          adw_combo_row_set_selected (self->programmer_combo, i);
          break;
        }
    }
}

static gpointer
map_option_cb (gpointer item,
               gpointer user_data)
{
  g_autoptr (GbpArduinoOptionValue) option_value = item;
  return gtk_string_object_new (gbp_arduino_option_value_get_value_label (option_value));
}

static void
update_fqbn_from_ui (GbpArduinoBoardOptionsWidget *self)
{
  g_autofree char *fqbn = NULL;
  g_auto (GStrv) flags = NULL;
  g_autofree char *fqbn_and_flags = NULL;
  g_autofree char *new_fqbn_and_flags = NULL;
  guint length;
  gboolean first_flag = TRUE;

  fqbn_and_flags = ide_tweaks_binding_dup_string (self->binding);

  if (!parse_fqbn_and_flags (fqbn_and_flags, &fqbn, &flags))
    {
      g_warning ("Could not parse fqbn and flags");
      return;
    }

  new_fqbn_and_flags = g_strdup (fqbn);

  length = g_list_model_get_n_items (G_LIST_MODEL (self->options_list_model));

  for (guint i = 0; i < length; i++)
    {
      GbpArduinoBoardOptionRow *option_row;
      GbpArduinoBoardOption *option;
      g_autoptr(GbpArduinoOptionValue) option_value = NULL;
      GListModel *values;
      guint selected = 0;
      const char *key;
      const char *value;

      option_row = GBP_ARDUINO_BOARD_OPTION_ROW (gtk_list_box_get_row_at_index (self->list_box, i));
      selected = adw_combo_row_get_selected (ADW_COMBO_ROW (option_row));
      option = gbp_arduino_board_option_row_get_option (option_row);
      values = G_LIST_MODEL (gbp_arduino_board_option_get_values (option));

      if (selected != 0)
        {
          key = gbp_arduino_board_option_get_option (option);
          option_value = g_list_model_get_item (values, selected);
          value = gbp_arduino_option_value_get_value (option_value);

          if (first_flag)
            {
              new_fqbn_and_flags = g_strconcat (new_fqbn_and_flags, ":", key, "=", value, NULL);
              first_flag = FALSE;
            }
          else
            {
              new_fqbn_and_flags = g_strconcat (new_fqbn_and_flags, ",", key, "=", value, NULL);
            }
        }
    }

  ide_tweaks_binding_set_string (self->binding, new_fqbn_and_flags);
}

static GtkWidget *
create_option_row_cb (gpointer item,
                      gpointer user_data)
{
  GbpArduinoBoardOptionsWidget *self = user_data;
  GbpArduinoBoardOption *option = item;
  GtkWidget *row;

  row = gbp_arduino_board_option_row_new (option);

  g_signal_connect_object (row,
                           "notify::selected",
                           G_CALLBACK (update_fqbn_from_ui),
                           self,
                           G_CONNECT_SWAPPED);

  return row;
}

static void
populate_board_options_list (GbpArduinoBoardOptionsWidget *self)
{
  GbpArduinoApplicationAddin *arduino_app;
  g_autofree char *fqbn_and_flags = NULL;
  g_autofree char *fqbn = NULL;
  g_auto (GStrv) flags = NULL;
  g_autoptr(GtkMapListModel) mapped = NULL;

  g_assert (GBP_IS_ARDUINO_OPTIONS_WIDGET (self));

  arduino_app = ide_application_find_addin_by_module_name (IDE_APPLICATION_DEFAULT, "arduino");

  fqbn_and_flags = ide_tweaks_binding_dup_string (self->binding);

  if (!parse_fqbn_and_flags (fqbn_and_flags, &fqbn, &flags))
    {
      g_warning ("Could not parse fqbn and flags");
      return;
    }

  for (guint i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (self->fqbn_list_model)); i++)
    {
      g_autoptr(GbpArduinoBoard) available_board = g_list_model_get_item (G_LIST_MODEL (self->fqbn_list_model), i);

      if (ide_str_equal0 (gbp_arduino_board_get_fqbn (available_board), fqbn))
        {
          adw_combo_row_set_selected (self->fqbn_combo, i);
          break;
        }
    }

  g_clear_object (&self->options_list_model);
  g_clear_object (&self->programmers_list_model);

  gbp_arduino_application_addin_get_options_for_fqbn (arduino_app,
                                                      fqbn,
                                                      &self->options_list_model,
                                                      &self->programmers_list_model);

  mapped = gtk_map_list_model_new (G_LIST_MODEL (g_object_ref (self->programmers_list_model)),
                                   map_option_cb,
                                   NULL,
                                   NULL);

  if (g_list_model_get_n_items (G_LIST_MODEL (self->programmers_list_model)) > 0)
    {
      adw_combo_row_set_model (self->programmer_combo, G_LIST_MODEL (mapped));

      gtk_widget_set_visible (GTK_WIDGET (self->programmer_group),
                              g_list_model_get_n_items (G_LIST_MODEL (self->programmers_list_model)) > 0);
    }
  else
    {
      adw_combo_row_set_model (self->programmer_combo, NULL);

      gtk_widget_set_visible (GTK_WIDGET (self->programmer_group), FALSE);
    }

  gtk_list_box_bind_model (self->list_box,
                           G_LIST_MODEL (self->options_list_model),
                           create_option_row_cb,
                           self, NULL);

  gtk_widget_set_visible (GTK_WIDGET (self->list_box),
                          g_list_model_get_n_items (G_LIST_MODEL (self->options_list_model)) > 0);

  update_board_options (self);
  update_programmer_combo (self);
}

static void
on_board_changed_cb (GbpArduinoBoardOptionsWidget *self)
{
  guint selected = adw_combo_row_get_selected (self->fqbn_combo);
  g_autoptr(GbpArduinoBoard) board = g_list_model_get_item (G_LIST_MODEL (self->fqbn_list_model), selected);
  g_autofree char *fqbn = NULL;
  g_autofree char **flags = NULL;
  g_autofree char *new_fqbn = NULL;
  g_autofree char *fqbn_and_flags = ide_tweaks_binding_dup_string (self->binding);
  new_fqbn = g_strdup (gbp_arduino_board_get_fqbn (board));

  parse_fqbn_and_flags (fqbn_and_flags, &fqbn, &flags);

  if (!ide_str_equal0 (fqbn, new_fqbn))
    {
      ide_tweaks_binding_set_string (self->binding, new_fqbn);
      populate_board_options_list (self);
    }
}

static void
update_board_combo_from_binding (GbpArduinoBoardOptionsWidget *self)
{
  g_autofree char *fqbn_and_flags = NULL;
  g_autofree char *fqbn = NULL;
  g_auto (GStrv) flags = NULL;

  g_assert (GBP_IS_ARDUINO_OPTIONS_WIDGET (self));

  fqbn_and_flags = ide_tweaks_binding_dup_string (self->binding);

  if (!parse_fqbn_and_flags (fqbn_and_flags, &fqbn, &flags))
    {
      adw_combo_row_set_selected (self->fqbn_combo, 0);
      on_board_changed_cb (self);
      return;
    }

  for (guint i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (self->fqbn_list_model)); i++)
    {
      g_autoptr(GbpArduinoBoard) available_board = g_list_model_get_item (G_LIST_MODEL (self->fqbn_list_model), i);

      if (ide_str_equal0 (gbp_arduino_board_get_fqbn (available_board), fqbn))
        {
          adw_combo_row_set_selected (self->fqbn_combo, i);
          return;
        }
    }
}

static gpointer
map_board_cb (gpointer item,
              gpointer user_data)
{
  g_autoptr (GbpArduinoBoard) board = item;
  return gtk_string_object_new (gbp_arduino_board_get_name (board));
}

static void
populate_board_row_boards (GbpArduinoBoardOptionsWidget *self)
{
  g_autoptr(GtkMapListModel) mapped = NULL;
  GbpArduinoApplicationAddin *arduino_app = NULL;

  if (self->binding == NULL)
    return;

  arduino_app = ide_application_find_addin_by_module_name (IDE_APPLICATION_DEFAULT, "arduino");
  g_object_get (arduino_app, "available-boards", &self->fqbn_list_model, NULL);
  mapped = gtk_map_list_model_new (G_LIST_MODEL (g_object_ref (self->fqbn_list_model)),
                                   map_board_cb,
                                   NULL,
                                   NULL);

  adw_combo_row_set_model (self->fqbn_combo, G_LIST_MODEL (mapped));

  g_signal_connect_object (self->fqbn_combo,
                           "notify::selected",
                           G_CALLBACK (on_board_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
on_binding_changed_cb (GbpArduinoBoardOptionsWidget *self,
                       IdeTweaksBinding *binding)
{
}

static void
on_programmer_changed_cb (GbpArduinoBoardOptionsWidget *self)
{
  g_autoptr(GbpArduinoProfile) config = NULL;
  guint selected;

  selected = adw_combo_row_get_selected (self->programmer_combo);

  if (selected != 0)
    {
      g_autoptr(GbpArduinoOptionValue) option_value = NULL;

      option_value = g_list_model_get_item (G_LIST_MODEL (self->programmers_list_model), selected);
      config = GBP_ARDUINO_PROFILE (ide_tweaks_property_dup_object (IDE_TWEAKS_PROPERTY (self->binding)));
      gbp_arduino_profile_set_programmer (config, gbp_arduino_option_value_get_value (option_value));
      ide_config_set_dirty (IDE_CONFIG (config), TRUE);
    }
  else
    {
      config = GBP_ARDUINO_PROFILE (ide_tweaks_property_dup_object (IDE_TWEAKS_PROPERTY (self->binding)));
      gbp_arduino_profile_set_programmer (config, NULL);
      ide_config_set_dirty (IDE_CONFIG (config), TRUE);
    }
}

static void
gbp_arduino_board_options_widget_constructed (GObject *object)
{
  GbpArduinoBoardOptionsWidget *self = (GbpArduinoBoardOptionsWidget *) object;

  if (self->binding == NULL)
    return;

  populate_board_row_boards (self);

  update_board_combo_from_binding (self);

  populate_board_options_list (self);

  g_signal_connect_object (self->binding,
                           "changed",
                           G_CALLBACK (on_binding_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->programmer_combo,
                           "notify::selected",
                           G_CALLBACK (on_programmer_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  G_OBJECT_CLASS (gbp_arduino_board_options_widget_parent_class)->constructed (object);
}

static void
gbp_arduino_board_options_widget_dispose (GObject *object)
{
  GbpArduinoBoardOptionsWidget *self = (GbpArduinoBoardOptionsWidget *) object;

  g_clear_pointer ((GtkWidget **) &self->box, gtk_widget_unparent);

  G_OBJECT_CLASS (gbp_arduino_board_options_widget_parent_class)->dispose (object);
}

static void
gbp_arduino_board_options_widget_get_property (GObject    *object,
                                               guint       prop_id,
                                               GValue     *value,
                                               GParamSpec *pspec)
{
  GbpArduinoBoardOptionsWidget *self = GBP_ARDUINO_OPTIONS_WIDGET (object);

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
gbp_arduino_board_options_widget_set_property (GObject      *object,
                                               guint         prop_id,
                                               const GValue *value,
                                               GParamSpec   *pspec)
{
  GbpArduinoBoardOptionsWidget *self = GBP_ARDUINO_OPTIONS_WIDGET (object);

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
gbp_arduino_board_options_widget_class_init (GbpArduinoBoardOptionsWidgetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gbp_arduino_board_options_widget_constructed;
  object_class->dispose = gbp_arduino_board_options_widget_dispose;
  object_class->get_property = gbp_arduino_board_options_widget_get_property;
  object_class->set_property = gbp_arduino_board_options_widget_set_property;

  properties[PROP_BINDING] =
      g_param_spec_object ("binding", NULL, NULL,
                           IDE_TYPE_TWEAKS_BINDING,
                           (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/arduino/gbp-arduino-board-options-widget.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpArduinoBoardOptionsWidget, fqbn_combo);
  gtk_widget_class_bind_template_child (widget_class, GbpArduinoBoardOptionsWidget, box);
  gtk_widget_class_bind_template_child (widget_class, GbpArduinoBoardOptionsWidget, list_box);
  gtk_widget_class_bind_template_child (widget_class, GbpArduinoBoardOptionsWidget, programmer_group);
  gtk_widget_class_bind_template_child (widget_class, GbpArduinoBoardOptionsWidget, programmer_combo);
}

static void
gbp_arduino_board_options_widget_init (GbpArduinoBoardOptionsWidget *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
gbp_arduino_board_options_widget_new (IdeTweaksBinding *binding)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_BINDING (binding), NULL);

  return g_object_new (GBP_TYPE_ARDUINO_OPTIONS_WIDGET,
                       "binding", binding,
                       NULL);
}

