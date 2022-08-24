/* ide-tweaks-choice.h
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

#if !defined (IDE_TWEAKS_INSIDE) && !defined (IDE_TWEAKS_COMPILATION)
# error "Only <libide-tweaks.h> can be included directly."
#endif

#include "ide-tweaks-item.h"

G_BEGIN_DECLS

#define IDE_TYPE_TWEAKS_CHOICE (ide_tweaks_choice_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeTweaksChoice, ide_tweaks_choice, IDE, TWEAKS_CHOICE, IdeTweaksItem)

IDE_AVAILABLE_IN_ALL
IdeTweaksChoice *ide_tweaks_choice_new          (void);
IDE_AVAILABLE_IN_ALL
const char      *ide_tweaks_choice_get_title    (IdeTweaksChoice *self);
IDE_AVAILABLE_IN_ALL
void             ide_tweaks_choice_set_title    (IdeTweaksChoice *self,
                                                 const char      *title);
IDE_AVAILABLE_IN_ALL
const char      *ide_tweaks_choice_get_subtitle (IdeTweaksChoice *self);
IDE_AVAILABLE_IN_ALL
void             ide_tweaks_choice_set_subtitle (IdeTweaksChoice *self,
                                                 const char      *subtitle);
IDE_AVAILABLE_IN_ALL
GVariant        *ide_tweaks_choice_get_value    (IdeTweaksChoice *self);
IDE_AVAILABLE_IN_ALL
void             ide_tweaks_choice_set_value    (IdeTweaksChoice *self,
                                                 GVariant        *value);

G_END_DECLS
