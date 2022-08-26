/* ide-tweaks-radio.h
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

#pragma once

#include "ide-tweaks-widget.h"

G_BEGIN_DECLS

#define IDE_TYPE_TWEAKS_RADIO (ide_tweaks_radio_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeTweaksRadio, ide_tweaks_radio, IDE, TWEAKS_RADIO, IdeTweaksWidget)

IDE_AVAILABLE_IN_ALL
IdeTweaksRadio *ide_tweaks_radio_new             (void);
IDE_AVAILABLE_IN_ALL
const char     *ide_tweaks_radio_get_title       (IdeTweaksRadio *self);
IDE_AVAILABLE_IN_ALL
void            ide_tweaks_radio_set_title       (IdeTweaksRadio *self,
                                                  const char     *title);
IDE_AVAILABLE_IN_ALL
const char     *ide_tweaks_radio_get_subtitle    (IdeTweaksRadio *self);
IDE_AVAILABLE_IN_ALL
void            ide_tweaks_radio_set_subtitle    (IdeTweaksRadio *self,
                                                  const char     *subtitle);
IDE_AVAILABLE_IN_ALL
GVariant       *ide_tweaks_radio_get_value       (IdeTweaksRadio *self);
IDE_AVAILABLE_IN_ALL
void            ide_tweaks_radio_set_value       (IdeTweaksRadio *self,
                                                  GVariant       *value);

G_END_DECLS
