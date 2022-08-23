/*
 * ide-tweaks-page.h
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

#define IDE_TYPE_TWEAKS_PAGE (ide_tweaks_page_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeTweaksPage, ide_tweaks_page, IDE, TWEAKS_PAGE, IdeTweaksItem)

IDE_AVAILABLE_IN_ALL
IdeTweaksPage *ide_tweaks_page_new             (void);
IDE_AVAILABLE_IN_ALL
const char    *ide_tweaks_page_get_icon_name   (IdeTweaksPage *self);
IDE_AVAILABLE_IN_ALL
void           ide_tweaks_page_set_icon_name   (IdeTweaksPage *self,
                                                const char    *icon_name);
IDE_AVAILABLE_IN_ALL
IdeTweaksItem *ide_tweaks_page_get_section     (IdeTweaksPage *self);
IDE_AVAILABLE_IN_ALL
const char    *ide_tweaks_page_get_title       (IdeTweaksPage *self);
IDE_AVAILABLE_IN_ALL
void           ide_tweaks_page_set_title       (IdeTweaksPage *self,
                                                const char    *title);
IDE_AVAILABLE_IN_ALL
gboolean       ide_tweaks_page_get_show_icon   (IdeTweaksPage *self);
IDE_AVAILABLE_IN_ALL
void           ide_tweaks_page_set_show_icon   (IdeTweaksPage *self,
                                                gboolean       show_icon);
IDE_AVAILABLE_IN_ALL
gboolean       ide_tweaks_page_get_has_subpage (IdeTweaksPage *self);
IDE_AVAILABLE_IN_ALL
gboolean       ide_tweaks_page_get_show_search (IdeTweaksPage *self);
IDE_AVAILABLE_IN_ALL
void           ide_tweaks_page_set_show_search (IdeTweaksPage *self,
                                                gboolean       show_search);

G_END_DECLS
