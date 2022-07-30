/* ide-tweaks-variable.h
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

#include "ide-tweaks-item.h"

G_BEGIN_DECLS

#define IDE_TYPE_TWEAKS_VARIABLE (ide_tweaks_variable_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeTweaksVariable, ide_tweaks_variable, IDE, TWEAKS_VARIABLE, IdeTweaksItem)

IDE_AVAILABLE_IN_ALL
IdeTweaksVariable *ide_tweaks_variable_new       (const char        *key,
                                                  const char        *value);
IDE_AVAILABLE_IN_ALL
const char        *ide_tweaks_variable_get_key   (IdeTweaksVariable *self);
IDE_AVAILABLE_IN_ALL
const char        *ide_tweaks_variable_get_value (IdeTweaksVariable *self);

G_END_DECLS
