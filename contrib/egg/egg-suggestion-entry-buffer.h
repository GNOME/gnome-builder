/* egg-suggestion-entry-buffer.h
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef EGG_SUGGESTION_ENTRY_BUFFER_H
#define EGG_SUGGESTION_ENTRY_BUFFER_H

#include <gtk/gtk.h>

#include "egg-suggestion.h"

G_BEGIN_DECLS

#define EGG_TYPE_SUGGESTION_ENTRY_BUFFER (egg_suggestion_entry_buffer_get_type())

G_DECLARE_DERIVABLE_TYPE (EggSuggestionEntryBuffer, egg_suggestion_entry_buffer, EGG, SUGGESTION_ENTRY_BUFFER, GtkEntryBuffer)

struct _EggSuggestionEntryBufferClass
{
  GtkEntryBufferClass parent_class;

  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
};

EggSuggestionEntryBuffer *egg_suggestion_entry_buffer_new              (void);
EggSuggestion            *egg_suggestion_entry_buffer_get_suggestion   (EggSuggestionEntryBuffer *self);
void                      egg_suggestion_entry_buffer_set_suggestion   (EggSuggestionEntryBuffer *self,
                                                                        EggSuggestion            *suggestion);
const gchar              *egg_suggestion_entry_buffer_get_typed_text   (EggSuggestionEntryBuffer *self);
guint                     egg_suggestion_entry_buffer_get_typed_length (EggSuggestionEntryBuffer *self);
void                      egg_suggestion_entry_buffer_commit           (EggSuggestionEntryBuffer *self);

G_END_DECLS

#endif /* EGG_SUGGESTION_ENTRY_BUFFER_H */
