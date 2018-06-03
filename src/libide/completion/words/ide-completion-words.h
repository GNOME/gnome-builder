/* ide-completion-words.h
 *
 * Copyright 2016 Christian Hergert <chergert@redhat.com>
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

#define IDE_TYPE_COMPLETION_WORDS            (ide_completion_words_get_type())
#define IDE_COMPLETION_WORDS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), IDE_TYPE_COMPLETION_WORDS, IdeCompletionWords))
#define IDE_COMPLETION_WORDS_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), IDE_TYPE_COMPLETION_WORDS, IdeCompletionWords const))
#define IDE_COMPLETION_WORDS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  IDE_TYPE_COMPLETION_WORDS, IdeCompletionWordsClass))
#define IDE_IS_COMPLETION_WORDS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), IDE_TYPE_COMPLETION_WORDS))
#define IDE_IS_COMPLETION_WORDS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  IDE_TYPE_COMPLETION_WORDS))
#define IDE_COMPLETION_WORDS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  IDE_TYPE_COMPLETION_WORDS, IdeCompletionWordsClass))

typedef struct _IdeCompletionWords        IdeCompletionWords;
typedef struct _IdeCompletionWordsClass   IdeCompletionWordsClass;

struct _IdeCompletionWordsClass
{
  GtkSourceCompletionWordsClass parent_class;

  /*< private >*/
  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
};

GType ide_completion_words_get_type (void);

G_END_DECLS
