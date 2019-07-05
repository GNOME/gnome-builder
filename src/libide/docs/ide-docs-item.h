/* ide-docs-item.h
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

#define IDE_TYPE_DOCS_ITEM (ide_docs_item_get_type())

typedef enum
{
  IDE_DOCS_ITEM_KIND_NONE = 0,
  IDE_DOCS_ITEM_KIND_COLLECTION,
  IDE_DOCS_ITEM_KIND_BOOK,
  IDE_DOCS_ITEM_KIND_CHAPTER,
  IDE_DOCS_ITEM_KIND_CLASS,
  IDE_DOCS_ITEM_KIND_CONSTANT,
  IDE_DOCS_ITEM_KIND_ENUM,
  IDE_DOCS_ITEM_KIND_FUNCTION,
  IDE_DOCS_ITEM_KIND_MACRO,
  IDE_DOCS_ITEM_KIND_MEMBER,
  IDE_DOCS_ITEM_KIND_METHOD,
  IDE_DOCS_ITEM_KIND_PROPERTY,
  IDE_DOCS_ITEM_KIND_SIGNAL,
  IDE_DOCS_ITEM_KIND_STRUCT,
  IDE_DOCS_ITEM_KIND_UNION,
} IdeDocsItemKind;

IDE_AVAILABLE_IN_3_34
G_DECLARE_DERIVABLE_TYPE (IdeDocsItem, ide_docs_item, IDE, DOCS_ITEM, GObject)

struct _IdeDocsItemClass
{
  GObjectClass parent_class;

  /*< private >*/
  gpointer _reserved[16];
};

IDE_AVAILABLE_IN_3_34
IdeDocsItem     *ide_docs_item_new              (void);
IDE_AVAILABLE_IN_3_34
const gchar     *ide_docs_item_get_id           (IdeDocsItem     *self);
IDE_AVAILABLE_IN_3_34
void             ide_docs_item_set_id           (IdeDocsItem     *self,
                                                 const gchar     *id);
IDE_AVAILABLE_IN_3_34
IdeDocsItem     *ide_docs_item_find_child_by_id (IdeDocsItem     *self,
                                                 const gchar     *id);
IDE_AVAILABLE_IN_3_34
const gchar     *ide_docs_item_get_title        (IdeDocsItem     *self);
IDE_AVAILABLE_IN_3_34
void             ide_docs_item_set_title        (IdeDocsItem     *self,
                                                 const gchar     *title);
IDE_AVAILABLE_IN_3_34
const gchar     *ide_docs_item_get_display_name (IdeDocsItem     *self);
IDE_AVAILABLE_IN_3_34
void             ide_docs_item_set_display_name (IdeDocsItem     *self,
                                                 const gchar     *display_name);
IDE_AVAILABLE_IN_3_34
const gchar     *ide_docs_item_get_url          (IdeDocsItem     *self);
IDE_AVAILABLE_IN_3_34
void             ide_docs_item_set_url          (IdeDocsItem     *self,
                                                 const gchar     *url);
IDE_AVAILABLE_IN_3_34
IdeDocsItemKind  ide_docs_item_get_kind         (IdeDocsItem     *self);
IDE_AVAILABLE_IN_3_34
void             ide_docs_item_set_kind         (IdeDocsItem     *self,
                                                 IdeDocsItemKind  kind);
IDE_AVAILABLE_IN_3_34
gboolean         ide_docs_item_get_deprecated   (IdeDocsItem     *self);
IDE_AVAILABLE_IN_3_34
void             ide_docs_item_set_deprecated   (IdeDocsItem     *self,
                                                 gboolean         deprecated);
IDE_AVAILABLE_IN_3_34
const gchar     *ide_docs_item_get_since        (IdeDocsItem     *self);
IDE_AVAILABLE_IN_3_34
void             ide_docs_item_set_since        (IdeDocsItem     *self,
                                                 const gchar     *since);
IDE_AVAILABLE_IN_3_34
gint             ide_docs_item_get_priority     (IdeDocsItem     *self);
IDE_AVAILABLE_IN_3_34
void             ide_docs_item_set_priority     (IdeDocsItem     *self,
                                                 gint             priority);
IDE_AVAILABLE_IN_3_34
void             ide_docs_item_append           (IdeDocsItem     *self,
                                                 IdeDocsItem     *child);
IDE_AVAILABLE_IN_3_34
void             ide_docs_item_prepend          (IdeDocsItem     *self,
                                                 IdeDocsItem     *child);
IDE_AVAILABLE_IN_3_34
void             ide_docs_item_remove           (IdeDocsItem     *self,
                                                 IdeDocsItem     *child);
IDE_AVAILABLE_IN_3_34
gboolean         ide_docs_item_has_child        (IdeDocsItem     *self);
IDE_AVAILABLE_IN_3_34
guint            ide_docs_item_get_n_children   (IdeDocsItem     *self);
IDE_AVAILABLE_IN_3_34
const GList     *ide_docs_item_get_children     (IdeDocsItem     *self);
IDE_AVAILABLE_IN_3_34
gboolean         ide_docs_item_is_root          (IdeDocsItem     *self);
IDE_AVAILABLE_IN_3_34
IdeDocsItem     *ide_docs_item_get_parent       (IdeDocsItem     *self);
IDE_AVAILABLE_IN_3_34
void             ide_docs_item_sort_by_priority (IdeDocsItem     *self);
IDE_AVAILABLE_IN_3_34
void             ide_docs_item_truncate         (IdeDocsItem     *self,
                                                 guint            max_items);

G_END_DECLS
