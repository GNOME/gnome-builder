/* c-parse-helper.h
 *
 * Copyright 2014-2019 Christian Hergert <christian@hergert.me>
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

#include <glib.h>

G_BEGIN_DECLS

typedef struct
{
  gchar *type;
  gchar *name;
  guint  ellipsis : 1;
  guint  n_star   : 4;
} Parameter;

gboolean   parameter_validate (Parameter       *param);
void       parameter_free     (Parameter       *p);
Parameter *parameter_copy     (const Parameter *src);
GSList    *parse_parameters   (const gchar     *text);

G_END_DECLS
