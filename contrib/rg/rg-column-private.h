/* rg-column-private.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef RG_COLUMN_PRIVATE_H
#define RG_COLUMN_PRIVATE_H

#include <glib-object.h>

#include "rg-column.h"

G_BEGIN_DECLS

void  _rg_column_get_value  (RgColumn *self,
                             guint     index,
                             GValue   *value);
void  _rg_column_collect    (RgColumn *self,
                             guint     index,
                             va_list   args);
void  _rg_column_lcopy      (RgColumn *self,
                             guint     index,
                             va_list   args);
void  _rg_column_get        (RgColumn *column,
                             guint     index,
                             ...);
void  _rg_column_set        (RgColumn *column,
                             guint     index,
                             ...);
guint _rg_column_push       (RgColumn *column);
void  _rg_column_set_n_rows (RgColumn *column,
                             guint     n_rows);

G_END_DECLS

#endif /* RG_COLUMN_PRIVATE_H */
