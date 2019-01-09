/* ide-spaces-style.h
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#if !defined (IDE_CODE_INSIDE) && !defined (IDE_CODE_COMPILATION)
# error "Only <libide-code.h> can be included directly."
#endif

#include <glib.h>

G_BEGIN_DECLS

typedef enum
{
  IDE_SPACES_STYLE_IGNORE              = 0,
  IDE_SPACES_STYLE_BEFORE_LEFT_PAREN   = 1 << 0,
  IDE_SPACES_STYLE_BEFORE_LEFT_BRACKET = 1 << 1,
  IDE_SPACES_STYLE_BEFORE_LEFT_BRACE   = 1 << 2,
  IDE_SPACES_STYLE_BEFORE_LEFT_ANGLE   = 1 << 3,
  IDE_SPACES_STYLE_BEFORE_COLON        = 1 << 4,
  IDE_SPACES_STYLE_BEFORE_COMMA        = 1 << 5,
  IDE_SPACES_STYLE_BEFORE_SEMICOLON    = 1 << 6,
} IdeSpacesStyle;

G_END_DECLS
