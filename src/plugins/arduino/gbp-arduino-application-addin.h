/*
 * gbp-arduino-application-addin.h
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

#pragma once

#include <json-glib/json-glib.h>

#include <libide-core.h>

#include "gbp-arduino-platform.h"

G_BEGIN_DECLS

#define GBP_TYPE_ARDUINO_APPLICATION_ADDIN (gbp_arduino_application_addin_get_type ())

G_DECLARE_FINAL_TYPE (GbpArduinoApplicationAddin, gbp_arduino_application_addin, GBP, ARDUINO_APPLICATION_ADDIN, GObject)

GListModel *
gbp_arduino_application_addin_get_available_boards (GbpArduinoApplicationAddin *self);

GListModel *
gbp_arduino_application_addin_get_installed_libraries (GbpArduinoApplicationAddin *self);

GListModel *
gbp_arduino_application_addin_get_installed_platforms (GbpArduinoApplicationAddin *self);

gboolean
gbp_arduino_application_addin_get_options_for_fqbn (GbpArduinoApplicationAddin *self,
                                                    const char                 *fqbn,
                                                    GListStore                **flags_out,
                                                    GListStore                **programmers_out);

GListStore *
gbp_arduino_application_addin_search_library (GbpArduinoApplicationAddin *self,
                                              const char                 *search_text);

GListStore *
gbp_arduino_application_addin_search_platform (GbpArduinoApplicationAddin *self,
                                               const char                 *search_text);

gboolean
gbp_arduino_application_addin_install_library (GbpArduinoApplicationAddin *self,
                                               const char                 *library_name);

gboolean
gbp_arduino_application_addin_uninstall_library (GbpArduinoApplicationAddin *self,
                                                 const char                 *library_name);

gboolean
gbp_arduino_application_addin_install_platform (GbpArduinoApplicationAddin *self,
                                                const char                 *platform_name);

gboolean
gbp_arduino_application_addin_uninstall_platform (GbpArduinoApplicationAddin *self,
                                                  const char                 *platform_name);

const char * const *
gbp_arduino_application_addin_get_additional_urls (GbpArduinoApplicationAddin *self);

gboolean
gbp_arduino_application_addin_add_additional_url (GbpArduinoApplicationAddin *self,
                                                  const char                 *new_url);

gboolean
gbp_arduino_application_addin_remove_additional_url (GbpArduinoApplicationAddin *self,
                                                     const char                 *url_to_remove);

gboolean
gbp_arduino_application_addin_has_arduino_cli (GbpArduinoApplicationAddin *self);

G_END_DECLS

