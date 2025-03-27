/*
 * gbp-arduino-profile.h
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

#include <libide-foundry.h>

#include "gbp-arduino-platform.h"

G_BEGIN_DECLS

#define GBP_TYPE_ARDUINO_PROFILE (gbp_arduino_profile_get_type ())
G_DECLARE_FINAL_TYPE (GbpArduinoProfile, gbp_arduino_profile, GBP, ARDUINO_PROFILE, IdeConfig)

const char *gbp_arduino_profile_get_port (GbpArduinoProfile *self);
void        gbp_arduino_profile_set_port (GbpArduinoProfile *self,
                                          const char        *port);

const char *gbp_arduino_profile_get_protocol (GbpArduinoProfile *self);
void        gbp_arduino_profile_set_protocol (GbpArduinoProfile *self,
                                              const char        *protocol);

const char *gbp_arduino_profile_get_programmer (GbpArduinoProfile *self);
void         gbp_arduino_profile_set_programmer (GbpArduinoProfile *self,
                                                 const char        *programmer);

const char *gbp_arduino_profile_get_fqbn (GbpArduinoProfile *self);
void        gbp_arduino_profile_set_fqbn (GbpArduinoProfile *self,
                                          const char        *fqbn);

const char *gbp_arduino_profile_get_notes (GbpArduinoProfile *self);
void        gbp_arduino_profile_set_notes (GbpArduinoProfile *self,
                                           const char        *notes);


void     gbp_arduino_profile_set_libraries (GbpArduinoProfile  *self,
                                            const char * const *libraries);
gboolean gbp_arduino_profile_add_library (GbpArduinoProfile *self,
                                          const char        *library);
void     gbp_arduino_profile_remove_library (GbpArduinoProfile *self,
                                             const char        *library);
const char * const  *gbp_arduino_profile_get_libraries (GbpArduinoProfile *self);


gboolean gbp_arduino_profile_add_platform (GbpArduinoProfile *self,
                                           GbpArduinoPlatform *platform);
void gbp_arduino_profile_remove_platform (GbpArduinoProfile *self,
                                          GbpArduinoPlatform *platform);
GListModel *gbp_arduino_profile_get_platforms (GbpArduinoProfile *self);


void gbp_arduino_profile_reset (GbpArduinoProfile *self);

G_END_DECLS


