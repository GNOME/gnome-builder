/* egg-suggestion.h
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


#ifndef EGG_SUGGESTION_H
#define EGG_SUGGESTION_H

#include <glib-object.h>

G_BEGIN_DECLS

#define EGG_TYPE_SUGGESTION (egg_suggestion_get_type())

G_DECLARE_DERIVABLE_TYPE (EggSuggestion, egg_suggestion, EGG, SUGGESTION, GObject)

struct _EggSuggestionClass
{
  GObjectClass parent_class;

  gchar *(*suggest_suffix)     (EggSuggestion *self,
                                const gchar   *typed_text);
  gchar *(*replace_typed_text) (EggSuggestion *self,
                                const gchar   *typed_text);

  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
};

EggSuggestion *egg_suggestion_new                (void);
const gchar   *egg_suggestion_get_id             (EggSuggestion *self);
void           egg_suggestion_set_id             (EggSuggestion *self,
                                                  const gchar   *id);
const gchar   *egg_suggestion_get_icon_name      (EggSuggestion *self);
void           egg_suggestion_set_icon_name      (EggSuggestion *self,
                                                  const gchar   *icon_name);
const gchar   *egg_suggestion_get_title          (EggSuggestion *self);
void           egg_suggestion_set_title          (EggSuggestion *self,
                                                  const gchar   *title);
const gchar   *egg_suggestion_get_subtitle       (EggSuggestion *self);
void           egg_suggestion_set_subtitle       (EggSuggestion *self,
                                                  const gchar   *subtitle);
gchar         *egg_suggestion_suggest_suffix     (EggSuggestion *self,
                                                  const gchar   *typed_text);
gchar         *egg_suggestion_replace_typed_text (EggSuggestion *self,
                                                  const gchar   *typed_text);

G_END_DECLS

#endif /* EGG_SUGGESTION_H */
