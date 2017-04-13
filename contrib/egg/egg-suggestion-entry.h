/* egg-suggestion-entry.h
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

#ifndef EGG_SUGGESTION_ENTRY_H
#define EGG_SUGGESTION_ENTRY_H

#include <gtk/gtk.h>

#include "egg-suggestion.h"

G_BEGIN_DECLS

#define EGG_TYPE_SUGGESTION_ENTRY (egg_suggestion_entry_get_type())

G_DECLARE_DERIVABLE_TYPE (EggSuggestionEntry, egg_suggestion_entry, EGG, SUGGESTION_ENTRY, GtkEntry)

struct _EggSuggestionEntryClass
{
  GtkEntryClass parent_class;

  void (*hide_suggestions)     (EggSuggestionEntry *self);
  void (*show_suggestions)     (EggSuggestionEntry *self);
  void (*move_suggestion )     (EggSuggestionEntry *self,
                                gint                amount);
  void (*suggestion_activated) (EggSuggestionEntry *self,
                                EggSuggestion      *suggestion);

  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
  gpointer _reserved5;
  gpointer _reserved6;
  gpointer _reserved7;
  gpointer _reserved8;
};

GtkWidget     *egg_suggestion_entry_new            (void);
void           egg_suggestion_entry_set_model      (EggSuggestionEntry *self,
                                                    GListModel         *model);
GListModel    *egg_suggestion_entry_get_model      (EggSuggestionEntry *self);
const gchar   *egg_suggestion_entry_get_typed_text (EggSuggestionEntry *self);
EggSuggestion *egg_suggestion_entry_get_suggestion (EggSuggestionEntry *self);
void           egg_suggestion_entry_set_suggestion (EggSuggestionEntry *self,
                                                    EggSuggestion      *suggestion);

G_END_DECLS

#endif /* EGG_SUGGESTION_ENTRY_H */
