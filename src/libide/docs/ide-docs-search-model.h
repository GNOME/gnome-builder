/* ide-docs-search-model.h
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

#include "ide-docs-item.h"

G_BEGIN_DECLS

#define IDE_TYPE_DOCS_SEARCH_MODEL (ide_docs_search_model_get_type())

G_DECLARE_FINAL_TYPE (IdeDocsSearchModel, ide_docs_search_model, IDE, DOCS_SEARCH_MODEL, GObject)

IdeDocsSearchModel *ide_docs_search_model_new              (void);
void                ide_docs_search_model_set_max_children (IdeDocsSearchModel *self,
                                                            guint               max_children);
void                ide_docs_search_model_add_group        (IdeDocsSearchModel *self,
                                                            IdeDocsItem        *group);
void                ide_docs_search_model_collapse_group   (IdeDocsSearchModel *self,
                                                            IdeDocsItem        *group);
void                ide_docs_search_model_expand_group     (IdeDocsSearchModel *self,
                                                            IdeDocsItem        *group);

G_END_DECLS
