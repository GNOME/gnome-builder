/* ide-docs-search-group.h
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

#include <gtk/gtk.h>

#include "ide-docs-item.h"

G_BEGIN_DECLS

#define IDE_TYPE_DOCS_SEARCH_GROUP (ide_docs_search_group_get_type())

G_DECLARE_FINAL_TYPE (IdeDocsSearchGroup, ide_docs_search_group, IDE, DOCS_SEARCH_GROUP, GtkBin)

GtkWidget   *ide_docs_search_group_new           (const gchar        *title);
const gchar *ide_docs_search_group_get_title     (IdeDocsSearchGroup *self);
gint         ide_docs_search_group_get_priority  (IdeDocsSearchGroup *self);
void         ide_docs_search_group_set_priority  (IdeDocsSearchGroup *self,
                                                  gint                priority);
guint        ide_docs_search_group_get_max_items (IdeDocsSearchGroup *self);
void         ide_docs_search_group_set_max_items (IdeDocsSearchGroup *self,
                                                  guint               max_items);
void         ide_docs_search_group_add_items     (IdeDocsSearchGroup *self,
                                                  IdeDocsItem        *parent);
void         ide_docs_search_group_toggle        (IdeDocsSearchGroup *self);

G_END_DECLS
