/* ide-word-completion-item.h
 *
 * Copyright (C) 2017 Umang Jain <mailumangjain@gmail.com>
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

#ifndef IDE_WORD_COMPLETION_ITEM_H
#define IDE_WORD_COMPLETION_ITEM_H

#include <gtksourceview/gtksource.h>

#include "sourceview/ide-completion-item.h"

G_BEGIN_DECLS

#define IDE_TYPE_WORD_COMPLETION_ITEM (ide_word_completion_item_get_type ())

G_DECLARE_FINAL_TYPE (IdeWordCompletionItem, ide_word_completion_item,
                      IDE, WORD_COMPLETION_ITEM, IdeCompletionItem)

IdeWordCompletionItem *ide_word_completion_item_new        (const gchar *word,
                                                            gint         offset,
                                                            GIcon       *icon);
const gchar           *ide_word_completion_item_get_word   (IdeWordCompletionItem *proposal);
gint                   ide_word_completion_item_get_offset (IdeWordCompletionItem *proposal);

G_END_DECLS

#endif /* IDE_WORD_COMPLETION_ITEM_H */
