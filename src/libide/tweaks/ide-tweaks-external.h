/* ide-tweaks-external.h
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

#define IDE_TYPE_TWEAKS_EXTERNAL (ide_tweaks_external_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeTweaksExternal, ide_tweaks_external, IDE, TWEAKS_EXTERNAL, IdeTweaksWidget)

IDE_AVAILABLE_IN_ALL
IdeTweaksExternal *ide_tweaks_external_new             (void);
IDE_AVAILABLE_IN_ALL
GType              ide_tweaks_external_get_widget_type (IdeTweaksExternal *self);
IDE_AVAILABLE_IN_ALL
void               ide_tweaks_external_set_widget_type (IdeTweaksExternal *self,
                                                        GType              widget_type);

G_END_DECLS
