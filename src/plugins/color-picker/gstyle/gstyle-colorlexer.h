/* gstyle-colorlexer.h
 *
 * Copyright 2016 sebastien lafargue <slafargue@gnome.org>
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

#include "gstyle-types.h"
#include "gstyle-color.h"

G_BEGIN_DECLS

typedef struct
{
  const gchar *start;
  const gchar *cursor;
  const gchar *ptr;
  const gchar *ptr_ctx;
} GstyleColorScanner;

typedef enum
{
  GSTYLE_COLOR_TOKEN_EOF,
  GSTYLE_COLOR_TOKEN_HEX,
  GSTYLE_COLOR_TOKEN_RGB,
  GSTYLE_COLOR_TOKEN_HSL,
  GSTYLE_COLOR_TOKEN_NAMED
} GstyleColorToken;

GPtrArray       *gstyle_colorlexer_parse       (const gchar     *data);

G_END_DECLS
