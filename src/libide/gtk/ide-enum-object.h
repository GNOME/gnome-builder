/* ide-enum-object.h
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

#define IDE_TYPE_ENUM_OBJECT (ide_enum_object_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeEnumObject, ide_enum_object, IDE, ENUM_OBJECT, GObject)

IDE_AVAILABLE_IN_ALL
IdeEnumObject *ide_enum_object_new             (const char    *nick,
                                                const char    *title,
                                                const char    *description);
IDE_AVAILABLE_IN_ALL
const char    *ide_enum_object_get_description (IdeEnumObject *self);
IDE_AVAILABLE_IN_ALL
const char    *ide_enum_object_get_nick        (IdeEnumObject *self);
IDE_AVAILABLE_IN_ALL
const char    *ide_enum_object_get_title       (IdeEnumObject *self);

G_END_DECLS
