/* ide-tweaks-item.h
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#if !defined (IDE_TWEAKS_INSIDE) && !defined (IDE_TWEAKS_COMPILATION)
# error "Only <libide-tweaks.h> can be included directly."
#endif

#include <libide-core.h>
#include <libide-search.h>

G_BEGIN_DECLS

typedef enum
{
  IDE_TWEAKS_ITEM_VISIT_STOP = 1,
  IDE_TWEAKS_ITEM_VISIT_CONTINUE,
  IDE_TWEAKS_ITEM_VISIT_RECURSE,
  IDE_TWEAKS_ITEM_VISIT_ACCEPT_AND_CONTINUE,
} IdeTweaksItemVisitResult;

#define IDE_TYPE_TWEAKS_ITEM (ide_tweaks_item_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeTweaksItem, ide_tweaks_item, IDE, TWEAKS_ITEM, GObject)

/**
 * IdeTweaksItemVisitor:
 * @item: an #IdeTweaksItem being visited
 * @user_data: user data provided
 *
 * Called for every matching item while visiting the item graph.
 *
 * Returns: an #IdeTweaksItemVisitResult
 */
typedef IdeTweaksItemVisitResult (*IdeTweaksItemVisitor) (IdeTweaksItem *item,
                                                          gpointer       user_data);

struct _IdeTweaksItemClass
{
  GObjectClass parent_class;

  gboolean       (*accepts) (IdeTweaksItem  *self,
                             IdeTweaksItem  *child);
  IdeTweaksItem *(*copy)    (IdeTweaksItem  *self);
  gboolean       (*match)   (IdeTweaksItem  *self,
                             IdePatternSpec *spec);
};

IDE_AVAILABLE_IN_ALL
const char         *ide_tweaks_item_get_id               (IdeTweaksItem        *self);
IDE_AVAILABLE_IN_ALL
IdeTweaksItem      *ide_tweaks_item_copy                 (IdeTweaksItem        *self);
IDE_AVAILABLE_IN_ALL
const char         *ide_tweaks_item_get_hidden_when      (IdeTweaksItem        *self);
IDE_AVAILABLE_IN_ALL
void                ide_tweaks_item_set_hidden_when      (IdeTweaksItem        *self,
                                                          const char           *hidden_when);
IDE_AVAILABLE_IN_ALL
const char * const *ide_tweaks_item_get_keywords         (IdeTweaksItem        *self);
IDE_AVAILABLE_IN_ALL
void                ide_tweaks_item_set_keywords         (IdeTweaksItem        *self,
                                                          const char * const   *keywords);
IDE_AVAILABLE_IN_ALL
gboolean            ide_tweaks_item_match                (IdeTweaksItem        *self,
                                                          IdePatternSpec       *spec);
IDE_AVAILABLE_IN_ALL
gboolean            ide_tweaks_item_is_ancestor          (IdeTweaksItem        *self,
                                                          IdeTweaksItem        *ancestor);
IDE_AVAILABLE_IN_ALL
IdeTweaksItem      *ide_tweaks_item_get_root             (IdeTweaksItem        *self);
IDE_AVAILABLE_IN_ALL
IdeTweaksItem      *ide_tweaks_item_get_parent           (IdeTweaksItem        *self);
IDE_AVAILABLE_IN_ALL
IdeTweaksItem      *ide_tweaks_item_get_last_child       (IdeTweaksItem        *self);
IDE_AVAILABLE_IN_ALL
IdeTweaksItem      *ide_tweaks_item_get_first_child      (IdeTweaksItem        *self);
IDE_AVAILABLE_IN_ALL
IdeTweaksItem      *ide_tweaks_item_get_previous_sibling (IdeTweaksItem        *self);
IDE_AVAILABLE_IN_ALL
IdeTweaksItem      *ide_tweaks_item_get_next_sibling     (IdeTweaksItem        *self);
IDE_AVAILABLE_IN_ALL
void                ide_tweaks_item_insert_after         (IdeTweaksItem        *self,
                                                          IdeTweaksItem        *parent,
                                                          IdeTweaksItem        *previous_sibling);
IDE_AVAILABLE_IN_ALL
void                ide_tweaks_item_insert_before        (IdeTweaksItem        *self,
                                                          IdeTweaksItem        *parent,
                                                          IdeTweaksItem        *next_sibling);
IDE_AVAILABLE_IN_ALL
gpointer            ide_tweaks_item_get_ancestor         (IdeTweaksItem        *self,
                                                          GType                 ancestor_type);
IDE_AVAILABLE_IN_ALL
void                ide_tweaks_item_unparent             (IdeTweaksItem        *self);
IDE_AVAILABLE_IN_ALL
gboolean            ide_tweaks_item_visit_children       (IdeTweaksItem        *self,
                                                          IdeTweaksItemVisitor  visitor,
                                                          gpointer              visitor_data);

G_END_DECLS
