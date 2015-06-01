/* rg-cpu-table.h
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

#ifndef RG_CPU_TABLE_H
#define RG_CPU_TABLE_H

#include "rg-table.h"

G_BEGIN_DECLS

#define RG_TYPE_CPU_TABLE (rg_cpu_table_get_type())

G_DECLARE_FINAL_TYPE (RgCpuTable, rg_cpu_table, RG, CPU_TABLE, RgTable)

RgTable *rg_cpu_table_new (void);

G_END_DECLS

#endif /* RG_CPU_TABLE_H */
