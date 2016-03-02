/* egg-empty-state.h
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#ifndef EGG_EMPTY_STATE_H
#define EGG_EMPTY_STATE_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EGG_TYPE_EMPTY_STATE (egg_empty_state_get_type())

G_DECLARE_DERIVABLE_TYPE (EggEmptyState, egg_empty_state, EGG, EMPTY_STATE, GtkBin)

struct _EggEmptyStateClass
{
  GtkBinClass parent_class;
};

GtkWidget   *egg_empty_state_new           (void);
const gchar *egg_empty_state_get_icon_name (EggEmptyState *self);
void         egg_empty_state_set_icon_name (EggEmptyState *self,
                                            const gchar   *icon_name);
void         egg_empty_state_set_resource  (EggEmptyState *self,
                                            const gchar   *resource);
const gchar *egg_empty_state_get_title     (EggEmptyState *self);
void         egg_empty_state_set_title     (EggEmptyState *self,
                                            const gchar   *title);
const gchar *egg_empty_state_get_subtitle  (EggEmptyState *self);
void         egg_empty_state_set_subtitle  (EggEmptyState *self,
                                            const gchar   *title);

G_END_DECLS

#endif /* EGG_EMPTY_STATE_H */
