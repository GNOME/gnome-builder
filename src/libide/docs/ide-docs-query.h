/* ide-docs-query.h
 *
 * Copyright 2019 Christian Hergert <unknown@domain.org>
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

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_DOCS_QUERY (ide_docs_query_get_type())

IDE_AVAILABLE_IN_3_34
G_DECLARE_FINAL_TYPE (IdeDocsQuery, ide_docs_query, IDE, DOCS_QUERY, GObject)

IDE_AVAILABLE_IN_3_34
IdeDocsQuery *ide_docs_query_new          (void);
IDE_AVAILABLE_IN_3_34
const gchar  *ide_docs_query_get_keyword  (IdeDocsQuery *self);
IDE_AVAILABLE_IN_3_34
void          ide_docs_query_set_keyword  (IdeDocsQuery *self,
                                           const gchar  *keyword);
IDE_AVAILABLE_IN_3_34
const gchar  *ide_docs_query_get_fuzzy    (IdeDocsQuery *self);
IDE_AVAILABLE_IN_3_34
const gchar  *ide_docs_query_get_sdk      (IdeDocsQuery *self);
IDE_AVAILABLE_IN_3_34
void          ide_docs_query_set_sdk      (IdeDocsQuery *self,
                                           const gchar  *sdk);
IDE_AVAILABLE_IN_3_34
const gchar  *ide_docs_query_get_language (IdeDocsQuery *self);
IDE_AVAILABLE_IN_3_34
void          ide_docs_query_set_language (IdeDocsQuery *self,
                                           const gchar  *language);

G_END_DECLS
