/* indexer-info.h
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

#include <libpeas.h>

G_BEGIN_DECLS

typedef struct
{
  const gchar  *module_name;
  GPtrArray    *specs;
  GPtrArray    *mime_types;
  gchar       **lang_ids;
} IndexerInfo;

void       indexer_info_free    (IndexerInfo       *info);
GPtrArray *collect_indexer_info (void);
gboolean   indexer_info_matches (const IndexerInfo *info,
                                 const gchar       *filename,
                                 const gchar       *filename_reversed,
                                 const gchar       *mime_type);

G_END_DECLS
