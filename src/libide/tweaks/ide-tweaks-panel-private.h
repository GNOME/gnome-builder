/* ide-tweaks-panel-private.h
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

#include <adwaita.h>

#include "ide-tweaks-page.h"

G_BEGIN_DECLS

#define IDE_TYPE_TWEAKS_PANEL (ide_tweaks_panel_get_type())

G_DECLARE_FINAL_TYPE (IdeTweaksPanel, ide_tweaks_panel, IDE, TWEAKS_PANEL, AdwNavigationPage)

GtkWidget     *ide_tweaks_panel_new        (IdeTweaksPage  *page);
IdeTweaksPage *ide_tweaks_panel_get_page   (IdeTweaksPanel *self);
gboolean       ide_tweaks_panel_get_folded (IdeTweaksPanel *self);
void           ide_tweaks_panel_set_folded (IdeTweaksPanel *self,
                                            gboolean        folded);

G_END_DECLS
