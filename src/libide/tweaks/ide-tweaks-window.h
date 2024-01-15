/* ide-tweaks-window.h
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

#include <adwaita.h>

#include "ide-tweaks.h"

G_BEGIN_DECLS

#define IDE_TYPE_TWEAKS_WINDOW (ide_tweaks_window_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeTweaksWindow, ide_tweaks_window, IDE, TWEAKS_WINDOW, AdwWindow)

IDE_AVAILABLE_IN_ALL
GtkWidget *ide_tweaks_window_new                   (void);
IDE_AVAILABLE_IN_ALL
IdeTweaks *ide_tweaks_window_get_tweaks            (IdeTweaksWindow *self);
IDE_AVAILABLE_IN_ALL
void       ide_tweaks_window_set_tweaks            (IdeTweaksWindow *self,
                                                    IdeTweaks       *tweaks);
IDE_AVAILABLE_IN_ALL
void       ide_tweaks_window_navigate_to           (IdeTweaksWindow *self,
                                                    IdeTweaksItem   *item);
IDE_AVAILABLE_IN_ALL
void       ide_tweaks_window_navigate_initial      (IdeTweaksWindow *self);

G_END_DECLS
