/* ide-gi-complete.h
 *
 * Copyright © 2017 Sebastien Lafargue <slafargue@gnome.org>
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

#pragma once

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
  IDE_GI_COMPLETE_TYPE_NONE          = 0,

  IDE_GI_COMPLETE_ROOT_ALIAS         = 1 << 0,
  IDE_GI_COMPLETE_ROOT_CLASS         = 1 << 1,
  IDE_GI_COMPLETE_ROOT_CONSTANT      = 1 << 2,
  IDE_GI_COMPLETE_ROOT_ENUM          = 1 << 3,
  IDE_GI_COMPLETE_ROOT_FIELD         = 1 << 4,
  IDE_GI_COMPLETE_ROOT_FUNCTION      = 1 << 5,
  IDE_GI_COMPLETE_ROOT_INTERFACE     = 1 << 6,
  IDE_GI_COMPLETE_ROOT_RECORD        = 1 << 7,
  IDE_GI_COMPLETE_ROOT_UNION         = 1 << 8,
} IdeGiCompleteRootFlags;

G_END_DECLS

