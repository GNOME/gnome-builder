/* ide-completion-item.h
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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

#include <gtksourceview/gtksource.h>

G_BEGIN_DECLS

#define IDE_TYPE_COMPLETION_ITEM            (ide_completion_item_get_type())
#define IDE_COMPLETION_ITEM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), IDE_TYPE_COMPLETION_ITEM, IdeCompletionItem))
#define IDE_COMPLETION_ITEM_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), IDE_TYPE_COMPLETION_ITEM, IdeCompletionItem const))
#define IDE_COMPLETION_ITEM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  IDE_TYPE_COMPLETION_ITEM, IdeCompletionItemClass))
#define IDE_IS_COMPLETION_ITEM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), IDE_TYPE_COMPLETION_ITEM))
#define IDE_IS_COMPLETION_ITEM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  IDE_TYPE_COMPLETION_ITEM))
#define IDE_COMPLETION_ITEM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  IDE_TYPE_COMPLETION_ITEM, IdeCompletionItemClass))

typedef struct _IdeCompletionItem      IdeCompletionItem;
typedef struct _IdeCompletionItemClass IdeCompletionItemClass;

struct _IdeCompletionItem
{
  GObject parent_instance;

  /*< private >*/
  GList link;
  guint priority;
};

struct _IdeCompletionItemClass
{
  GObjectClass parent_class;

  /**
   * IdeCompletionItem::match:
   *
   * This virtual function checks to see if a particular query matches
   * the #IdeCompletionItem in question. You can use helper functions
   * defined in this module for simple requests like case-insensitive
   * fuzzy matching.
   *
   * The default implementation of this virtual function performs a
   * strstr() to match @query exactly in the items label.
   *
   * Returns: %TRUE if the item matches.
   */
  gboolean (*match) (IdeCompletionItem *self,
                     const gchar       *query,
                     const gchar       *casefold);
};

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeCompletionItem, g_object_unref)

GType              ide_completion_item_get_type        (void);
IdeCompletionItem *ide_completion_item_new             (void);
gboolean           ide_completion_item_match           (IdeCompletionItem   *self,
                                                        const gchar         *query,
                                                        const gchar         *casefold);
void               ide_completion_item_set_priority    (IdeCompletionItem   *self,
                                                        guint                priority);
gboolean           ide_completion_item_fuzzy_match     (const gchar         *haystack,
                                                        const gchar         *casefold_needle,
                                                        guint               *priority);
gchar             *ide_completion_item_fuzzy_highlight (const gchar         *haystack,
                                                        const gchar         *casefold_query);

G_END_DECLS
