/*
 * ide-gsettings-action-group.h
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

#include <gio/gio.h>

#include "ide-version-macros.h"

G_BEGIN_DECLS

#define IDE_TYPE_GSETTINGS_ACTION_GROUP (ide_gsettings_action_group_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeGSettingsActionGroup, ide_gsettings_action_group, IDE, GSETTINGS_ACTION_GROUP, GObject)

IDE_AVAILABLE_IN_ALL
GActionGroup *ide_gsettings_action_group_new (GSettings *settings);

G_END_DECLS
