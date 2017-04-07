/* egg-suggestion-row.h
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

#ifndef EGG_SUGGESTION_ROW_H
#define EGG_SUGGESTION_ROW_H

#include <gtk/gtk.h>

#include "egg-suggestion.h"

G_BEGIN_DECLS

#define EGG_TYPE_SUGGESTION_ROW (egg_suggestion_row_get_type())

G_DECLARE_DERIVABLE_TYPE (EggSuggestionRow, egg_suggestion_row, EGG, SUGGESTION_ROW, GtkListBoxRow)

struct _EggSuggestionRowClass
{
  GtkListBoxRowClass parent_class;

  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
};

GtkWidget     *egg_suggestion_row_new            (void);
EggSuggestion *egg_suggestion_row_get_suggestion (EggSuggestionRow *self);
void           egg_suggestion_row_set_suggestion (EggSuggestionRow *self,
                                                  EggSuggestion    *suggestion);

G_END_DECLS

#endif /* EGG_SUGGESTION_ROW_H */
