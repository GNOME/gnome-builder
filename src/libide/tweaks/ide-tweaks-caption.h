/* ide-tweaks-caption.h
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

#define IDE_TYPE_TWEAKS_CAPTION (ide_tweaks_caption_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeTweaksCaption, ide_tweaks_caption, IDE, TWEAKS_CAPTION, IdeTweaksWidget)

IDE_AVAILABLE_IN_ALL
IdeTweaksCaption *ide_tweaks_caption_new      (void);
IDE_AVAILABLE_IN_ALL
const char       *ide_tweaks_caption_get_text (IdeTweaksCaption *self);
IDE_AVAILABLE_IN_ALL
void              ide_tweaks_caption_set_text (IdeTweaksCaption *self,
                                               const char       *text);

G_END_DECLS
