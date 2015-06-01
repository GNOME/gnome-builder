/* rg-table.h
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

#ifndef RG_TABLE_H
#define RG_TABLE_H

#include <glib-object.h>

#include "rg-column.h"

G_BEGIN_DECLS

#define RG_TYPE_TABLE (rg_table_get_type())

G_DECLARE_DERIVABLE_TYPE (RgTable, rg_table, RG, TABLE, GObject)

struct _RgTableClass
{
  GObjectClass parent;
};

typedef struct
{
  gpointer data[8];
} RgTableIter;

RgTable   *rg_table_new                (void);
guint      rg_table_add_column         (RgTable     *self,
                                        RgColumn    *column);
GTimeSpan  rg_table_get_timespan       (RgTable     *self);
void       rg_table_set_timespan       (RgTable     *self,
                                        GTimeSpan    timespan);
gint64     rg_table_get_end_time       (RgTable     *self);
guint      rg_table_get_max_samples    (RgTable     *self);
void       rg_table_set_max_samples    (RgTable     *self,
                                        guint        n_rows);
void       rg_table_push               (RgTable     *self,
                                        RgTableIter *iter,
                                        gint64       timestamp);
gboolean   rg_table_get_iter_first     (RgTable     *self,
                                        RgTableIter *iter);
gboolean   rg_table_get_iter_last      (RgTable     *self,
                                        RgTableIter *iter);
gboolean   rg_table_iter_next          (RgTableIter *iter);
void       rg_table_iter_get           (RgTableIter *iter,
                                        gint         first_column,
                                        ...);
void       rg_table_iter_get_value     (RgTableIter *iter,
                                        guint        column,
                                        GValue      *value);
gint64     rg_table_iter_get_timestamp (RgTableIter *iter);
void       rg_table_iter_set           (RgTableIter *iter,
                                        gint         first_column,
                                        ...);

G_END_DECLS

#endif /* RG_TABLE_H */
