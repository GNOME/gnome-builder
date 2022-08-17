/* ide-tweaks-section.h
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

#define IDE_TYPE_TWEAKS_SECTION (ide_tweaks_section_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeTweaksSection, ide_tweaks_section, IDE, TWEAKS_SECTION, IdeTweaksItem)

IDE_AVAILABLE_IN_ALL
IdeTweaksSection *ide_tweaks_section_new             (void);
IDE_AVAILABLE_IN_ALL
const char       *ide_tweaks_section_get_title       (IdeTweaksSection *self);
IDE_AVAILABLE_IN_ALL
void              ide_tweaks_section_set_title       (IdeTweaksSection *self,
                                                      const char       *title);
IDE_AVAILABLE_IN_ALL
gboolean          ide_tweaks_section_get_show_header (IdeTweaksSection *self);
IDE_AVAILABLE_IN_ALL
void              ide_tweaks_section_set_show_header (IdeTweaksSection *self,
                                                      gboolean          show_header);

G_END_DECLS
