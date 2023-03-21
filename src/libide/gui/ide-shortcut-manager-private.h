/* ide-shortcut-manager-private.h
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

#include <libpeas.h>

#include <libide-core.h>

#include "ide-shortcut-bundle-private.h"
#include "ide-shortcut-observer-private.h"

G_BEGIN_DECLS

#define IDE_TYPE_SHORTCUT_MANAGER (ide_shortcut_manager_get_type())

G_DECLARE_FINAL_TYPE (IdeShortcutManager, ide_shortcut_manager, IDE, SHORTCUT_MANAGER, IdeObject)

IdeShortcutManager  *ide_shortcut_manager_from_context     (IdeContext         *context);
void                 ide_shortcut_manager_add_resources    (const char         *resource_path);
void                 ide_shortcut_manager_remove_resources (const char         *resource_path);
IdeShortcutObserver *ide_shortcut_manager_get_observer     (IdeShortcutManager *self);
void                 ide_shortcut_manager_reset_user       (void);
IdeShortcutBundle   *ide_shortcut_manager_get_user_bundle  (void);

G_END_DECLS
