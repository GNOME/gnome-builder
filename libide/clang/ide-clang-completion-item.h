/* ide-clang-completion-item.h
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

#ifndef IDE_CLANG_COMPLETION_ITEM_H
#define IDE_CLANG_COMPLETION_ITEM_H

#include <glib-object.h>

G_BEGIN_DECLS

#define IDE_TYPE_CLANG_COMPLETION_ITEM            (ide_clang_completion_item_get_type())
#define IDE_CLANG_COMPLETION_ITEM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), IDE_TYPE_CLANG_COMPLETION_ITEM, IdeClangCompletionItem))
#define IDE_CLANG_COMPLETION_ITEM_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), IDE_TYPE_CLANG_COMPLETION_ITEM, IdeClangCompletionItem const))
#define IDE_CLANG_COMPLETION_ITEM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  IDE_TYPE_CLANG_COMPLETION_ITEM, IdeClangCompletionItemClass))
#define IDE_IS_CLANG_COMPLETION_ITEM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), IDE_TYPE_CLANG_COMPLETION_ITEM))
#define IDE_IS_CLANG_COMPLETION_ITEM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  IDE_TYPE_CLANG_COMPLETION_ITEM))
#define IDE_CLANG_COMPLETION_ITEM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  IDE_TYPE_CLANG_COMPLETION_ITEM, IdeClangCompletionItemClass))

typedef struct _IdeClangCompletionItem        IdeClangCompletionItem;
typedef struct _IdeClangCompletionItemClass   IdeClangCompletionItemClass;

GType    ide_clang_completion_item_get_type (void);
gboolean ide_clang_completion_item_matches  (IdeClangCompletionItem *self,
                                             const gchar            *text);
gint     ide_clang_completion_item_sort     (gconstpointer           a,
                                             gconstpointer           b);

G_END_DECLS

#endif /* IDE_CLANG_COMPLETION_ITEM_H */
