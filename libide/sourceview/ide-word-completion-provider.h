/* ide-word-completion-provider.h
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

#ifndef IDE_WORD_COMPLETION_PROVIDER_H
#define IDE_WORD_COMPLETION_PROVIDER_H

#include <gtksourceview/gtksource.h>

G_BEGIN_DECLS

#define IDE_TYPE_WORD_COMPLETION_PROVIDER (ide_word_completion_provider_get_type ())

G_DECLARE_FINAL_TYPE (IdeWordCompletionProvider, ide_word_completion_provider, IDE, WORD_COMPLETION_PROVIDER, GObject)

IdeWordCompletionProvider *ide_word_completion_provider_new (const gchar *name, GIcon *icon);

G_END_DECLS

#endif /* IDE_WORD_COMPLETION_PROVIDER_H */
