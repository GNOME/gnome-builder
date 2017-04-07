/* egg-suggestion-popover.h
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


#ifndef EGG_SUGGESTION_POPOVER_H
#define EGG_SUGGESTION_POPOVER_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EGG_TYPE_SUGGESTION_POPOVER (egg_suggestion_popover_get_type())

G_DECLARE_FINAL_TYPE (EggSuggestionPopover, egg_suggestion_popover, EGG, SUGGESTION_POPOVER, GtkWindow)

GtkWidget     *egg_suggestion_popover_new               (void);
GtkWidget     *egg_suggestion_popover_get_relative_to   (EggSuggestionPopover *self);
void           egg_suggestion_popover_set_relative_to   (EggSuggestionPopover *self,
                                                         GtkWidget            *widget);
void           egg_suggestion_popover_popup             (EggSuggestionPopover *self);
void           egg_suggestion_popover_popdown           (EggSuggestionPopover *self);
GListModel    *egg_suggestion_popover_get_model         (EggSuggestionPopover *self);
void           egg_suggestion_popover_set_model         (EggSuggestionPopover *self,
                                                         GListModel           *model);
void           egg_suggestion_popover_move_by           (EggSuggestionPopover *self,
                                                         gint                  amount);
EggSuggestion *egg_suggestion_popover_get_selected      (EggSuggestionPopover *self);
void           egg_suggestion_popover_set_selected      (EggSuggestionPopover *self,
                                                         EggSuggestion        *suggestion);
void           egg_suggestion_popover_activate_selected (EggSuggestionPopover *self);

G_END_DECLS

#endif /* EGG_SUGGESTION_POPOVER_H */
