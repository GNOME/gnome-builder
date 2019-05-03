/* gbp-vagrant-table.h
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#include <libide-io.h>

G_BEGIN_DECLS

typedef struct _GbpVagrantTable GbpVagrantTable;

typedef struct
{
  /*< private >*/
  IdeLineReader  reader;
  gchar         *cur;
} GbpVagrantTableIter;

GbpVagrantTable *gbp_vagrant_table_new_take        (gchar               *data);
void             gbp_vagrant_table_free            (GbpVagrantTable     *self);
void             gbp_vagrant_table_iter_init       (GbpVagrantTableIter *iter,
                                                    GbpVagrantTable     *self);
gboolean         gbp_vagrant_table_iter_next       (GbpVagrantTableIter *iter);
gchar           *gbp_vagrant_table_iter_get_column (GbpVagrantTableIter *iter,
                                                    guint                column);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GbpVagrantTable, gbp_vagrant_table_free)

G_END_DECLS
