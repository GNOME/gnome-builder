/* ide-shortcut-info.h
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

#include <libide-core.h>

G_BEGIN_DECLS

typedef struct _IdeShortcutInfo IdeShortcutInfo;

typedef void (*IdeShortcutInfoFunc) (const IdeShortcutInfo *info,
                                     gpointer               user_data);

IDE_AVAILABLE_IN_44
void        ide_shortcut_info_foreach           (GListModel            *shortcuts,
                                                 IdeShortcutInfoFunc    func,
                                                 gpointer               func_data);
IDE_AVAILABLE_IN_44
const char *ide_shortcut_info_get_id            (const IdeShortcutInfo *self);
IDE_AVAILABLE_IN_44
const char *ide_shortcut_info_get_icon_name     (const IdeShortcutInfo *self);
IDE_AVAILABLE_IN_44
const char *ide_shortcut_info_get_accelerator   (const IdeShortcutInfo *self);
IDE_AVAILABLE_IN_44
const char *ide_shortcut_info_get_action_name   (const IdeShortcutInfo *self);
IDE_AVAILABLE_IN_44
GVariant   *ide_shortcut_info_get_action_target (const IdeShortcutInfo *self);
IDE_AVAILABLE_IN_44
const char *ide_shortcut_info_get_page          (const IdeShortcutInfo *self);
IDE_AVAILABLE_IN_44
const char *ide_shortcut_info_get_group         (const IdeShortcutInfo *self);
IDE_AVAILABLE_IN_44
const char *ide_shortcut_info_get_title         (const IdeShortcutInfo *self);
IDE_AVAILABLE_IN_44
const char *ide_shortcut_info_get_subtitle      (const IdeShortcutInfo *self);

G_END_DECLS
