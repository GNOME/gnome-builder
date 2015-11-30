/* ide-genesis-perspective.h
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
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
 */

#ifndef IDE_GENESIS_PERSPECTIVE_H
#define IDE_GENESIS_PERSPECTIVE_H

#include "ide-perspective.h"

G_BEGIN_DECLS

#define IDE_TYPE_GENESIS_PERSPECTIVE (ide_genesis_perspective_get_type())

G_DECLARE_FINAL_TYPE (IdeGenesisPerspective, ide_genesis_perspective, IDE, GENESIS_PERSPECTIVE, GtkBin)

G_END_DECLS

#endif /* IDE_GENESIS_PERSPECTIVE_H */
