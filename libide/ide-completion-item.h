/* ide-completion-item.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#ifndef IDE_COMPLETION_ITEM_H
#define IDE_COMPLETION_ITEM_H

#include <gtksourceview/gtksource.h>

G_BEGIN_DECLS

#define IDE_TYPE_COMPLETION_ITEM (ide_completion_item_get_type())

/*
 * We provide this private, but defined in the header API for IdeCompletionItemHead
 * so that we have a location to store a GList node without allocating one. This is
 * used for sorting of result structures by IdeCompletionResults without allocating
 * GList items for GtkSourceCompletionContext.
 */
typedef struct
{
  GObject parent;
  /*< semi-public >*/
  GList link;
  guint priority;
} IdeCompletionItemHead;

/*
 * We require the following cleanup function so that G_DECLARE_DERIVABLE_TYPE() can
 * work with our semi-public IdeCompletionItemHead node.
 */
G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeCompletionItemHead, g_object_unref)

G_DECLARE_DERIVABLE_TYPE (IdeCompletionItem,
                          ide_completion_item,
                          IDE, COMPLETION_ITEM,
                          IdeCompletionItemHead)

typedef enum
{
  IDE_COMPLETION_COLUMN_PRIMARY = 0,
  IDE_COMPLETION_COLUMN_PREFIX  = 1,
  IDE_COMPLETION_COLUMN_SUFFIX  = 2,
  IDE_COMPLETION_COLUMN_INFO    = 3,
} IdeCompletionColumn;

struct _IdeCompletionItemClass
{
  GObjectClass parent;

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

  /**
   * IdeCompletionItem::get_column_markup:
   * @self: An #IdeCompletionItem.
   * @column: The #IdeCompletionItemColumn to retrieve.
   *
   * This function returns the text for a particular column.
   * This allows for Builder to organize results with aligned
   * columns in GtkSourceView. NOTE: This is not yet performed
   * today, but will in the future.
   */
  gchar *(*get_column_markup) (IdeCompletionItem   *self,
                               IdeCompletionColumn  column);
};

IdeCompletionItem *ide_completion_item_new               (void);
gboolean           ide_completion_item_match             (IdeCompletionItem   *self,
                                                          const gchar         *query,
                                                          const gchar         *casefold);
gchar             *ide_completion_item_get_column_markup (IdeCompletionItem   *self,
                                                          IdeCompletionColumn  column);
gboolean           ide_completion_item_fuzzy_match       (const gchar         *haystack,
                                                          const gchar         *casefold_needle,
                                                          guint               *priority);

G_END_DECLS

#endif /* IDE_COMPLETION_ITEM_H */
