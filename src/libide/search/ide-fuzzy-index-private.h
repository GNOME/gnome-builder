/* ide-fuzzy-index-private.h
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#include "ide-fuzzy-index.h"

G_BEGIN_DECLS

GVariant *_ide_fuzzy_index_lookup_document (IdeFuzzyIndex  *self,
                                            guint           document_id);
gboolean  _ide_fuzzy_index_resolve         (IdeFuzzyIndex  *self,
                                            guint           lookaside_id,
                                            guint          *document_id,
                                            const char    **key,
                                            guint          *priority,
                                            guint           in_score,
                                            guint           last_offset,
                                            float          *out_score);

G_END_DECLS
