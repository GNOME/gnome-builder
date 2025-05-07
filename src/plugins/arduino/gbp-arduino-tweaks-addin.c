/*
 * gbp-arduino-tweaks-addin.c
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

#define G_LOG_DOMAIN "gbp-arduino-tweaks-addin"

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <libide-foundry.h>
#include <libide-gui.h>

#include "gbp-arduino-application-addin.h"
#include "gbp-arduino-board-options-widget.h"
#include "gbp-arduino-board.h"
#include "gbp-arduino-libraries-editor.h"
#include "gbp-arduino-libraries-manager.h"
#include "gbp-arduino-platforms-editor.h"
#include "gbp-arduino-platforms-manager.h"
#include "gbp-arduino-port.h"
#include "gbp-arduino-urls-editor.h"
#include "gbp-arduino-profile.h"
#include "gbp-arduino-tweaks-addin.h"

struct _GbpArduinoTweaksAddin
{
  IdeTweaksAddin parent_instance;

  IdeContext *context;
};

G_DEFINE_FINAL_TYPE (GbpArduinoTweaksAddin, gbp_arduino_tweaks_addin, IDE_TYPE_TWEAKS_ADDIN)

static void
gbp_arduino_tweaks_addin_class_init (GbpArduinoTweaksAddinClass *klass)
{
}

static GtkWidget *
create_notes_entry_cb (GbpArduinoTweaksAddin *self,
                       IdeTweaksWidget       *widget,
                       IdeTweaksItem         *instance)
{
  IdeTweaksBinding *binding;
  AdwEntryRow *row;

  g_assert (GBP_IS_ARDUINO_TWEAKS_ADDIN (self));
  g_assert (IDE_IS_TWEAKS_WIDGET (widget));
  g_assert (IDE_IS_TWEAKS_ITEM (instance));

  if (!(binding = ide_tweaks_widget_get_binding (widget)))
    return NULL;

  if (!GBP_IS_ARDUINO_PROFILE (ide_tweaks_property_dup_object (IDE_TWEAKS_PROPERTY (binding))))
    return NULL;

  row = ADW_ENTRY_ROW(adw_entry_row_new());
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), _("Notes"));

  ide_tweaks_binding_bind(binding, row, "text");

  return GTK_WIDGET(row);
}

static GtkWidget *
create_board_options_cb (GbpArduinoTweaksAddin *self,
                         IdeTweaksWidget       *widget,
                         IdeTweaksItem         *instance)
{
  IdeTweaksBinding *binding;

  g_assert (GBP_IS_ARDUINO_TWEAKS_ADDIN (self));
  g_assert (IDE_IS_TWEAKS_WIDGET (widget));
  g_assert (IDE_IS_TWEAKS_WIDGET (instance));

  if (!(binding = ide_tweaks_widget_get_binding (widget)))
    return NULL;

  if (!GBP_IS_ARDUINO_PROFILE (ide_tweaks_property_dup_object (IDE_TWEAKS_PROPERTY (binding))))
    return NULL;

  return gbp_arduino_board_options_widget_new (binding);
}

static GtkWidget *
create_libraries_list_cb (GbpArduinoTweaksAddin *self,
                          IdeTweaksWidget       *widget,
                          IdeTweaksItem         *instance)
{
  IdeTweaksBinding *binding;

  g_assert (GBP_IS_ARDUINO_TWEAKS_ADDIN (self));
  g_assert (IDE_IS_TWEAKS_WIDGET (widget));
  g_assert (IDE_IS_TWEAKS_WIDGET (instance));

  if (!(binding = ide_tweaks_widget_get_binding (widget)))
    return NULL;

  if (!GBP_IS_ARDUINO_PROFILE (ide_tweaks_property_dup_object (IDE_TWEAKS_PROPERTY (binding))))
    return NULL;

  return gbp_arduino_libraries_editor_new (binding);
}

static GtkWidget *
create_platforms_list_cb (GbpArduinoTweaksAddin *self,
                          IdeTweaksWidget       *widget,
                          IdeTweaksItem         *instance)
{
  IdeTweaksBinding *binding;

  g_assert (GBP_IS_ARDUINO_TWEAKS_ADDIN (self));
  g_assert (IDE_IS_TWEAKS_WIDGET (widget));
  g_assert (IDE_IS_TWEAKS_WIDGET (instance));

  if (!(binding = ide_tweaks_widget_get_binding (widget)))
    return NULL;

  if (!GBP_IS_ARDUINO_PROFILE (ide_tweaks_property_dup_object (IDE_TWEAKS_PROPERTY (binding))))
    return NULL;

  return gbp_arduino_platforms_editor_new (binding);
}

static GtkWidget *
create_additional_urls_cb (GbpArduinoTweaksAddin *self,
                            IdeTweaksWidget       *widget,
                            IdeTweaksItem         *instance)
{
  g_assert (GBP_IS_ARDUINO_TWEAKS_ADDIN (self));

  return gbp_arduino_urls_editor_new ();
}

static GtkWidget *
create_libraries_manager_cb (GbpArduinoTweaksAddin *self,
                             IdeTweaksWidget       *widget,
                             IdeTweaksItem         *instance)
{
  g_assert (GBP_IS_ARDUINO_TWEAKS_ADDIN (self));

  return gbp_arduino_libraries_manager_new ();
}

static GtkWidget *
create_platforms_manager_cb (GbpArduinoTweaksAddin *self,
                             IdeTweaksWidget       *widget,
                             IdeTweaksItem         *instance)
{
  g_assert (GBP_IS_ARDUINO_TWEAKS_ADDIN (self));

  return gbp_arduino_platforms_manager_new ();
}

static void
gbp_arduino_tweaks_addin_init (GbpArduinoTweaksAddin *self)
{
  GbpArduinoApplicationAddin *arduino_app;

  arduino_app = ide_application_find_addin_by_module_name (IDE_APPLICATION_DEFAULT, "arduino");

  if (!gbp_arduino_application_addin_has_arduino_cli (arduino_app))
    return;

  ide_tweaks_addin_bind_callback (IDE_TWEAKS_ADDIN (self), create_notes_entry_cb);
  ide_tweaks_addin_bind_callback (IDE_TWEAKS_ADDIN (self), create_board_options_cb);
  ide_tweaks_addin_bind_callback (IDE_TWEAKS_ADDIN (self), create_libraries_list_cb);
  ide_tweaks_addin_bind_callback (IDE_TWEAKS_ADDIN (self), create_platforms_list_cb);
  ide_tweaks_addin_bind_callback (IDE_TWEAKS_ADDIN (self), create_additional_urls_cb);
  ide_tweaks_addin_bind_callback (IDE_TWEAKS_ADDIN (self), create_libraries_manager_cb);
  ide_tweaks_addin_bind_callback (IDE_TWEAKS_ADDIN (self), create_platforms_manager_cb);

  ide_tweaks_addin_set_resource_paths (IDE_TWEAKS_ADDIN (self),
                                       IDE_STRV_INIT ("/plugins/arduino/tweaks.ui",
                                                      "/plugins/arduino/tweaks-arduino-page.ui"));
}

