/* egg-search-bar.h
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

#ifndef EGG_SEARCH_BAR_H
#define EGG_SEARCH_BAR_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EGG_TYPE_SEARCH_BAR (egg_search_bar_get_type())

G_DECLARE_DERIVABLE_TYPE (EggSearchBar, egg_search_bar, EGG, SEARCH_BAR, GtkBin)

struct _EggSearchBarClass
{
  GtkBinClass parent_class;
};

GtkWidget *egg_search_bar_new                     (void);
gboolean   egg_search_bar_get_search_mode_enabled (EggSearchBar *self);
void       egg_search_bar_set_search_mode_enabled (EggSearchBar *self,
                                                   gboolean      search_mode_enabled);
gboolean   egg_search_bar_get_show_close_button   (EggSearchBar *self);
void       egg_search_bar_set_show_close_button   (EggSearchBar *self,
                                                   gboolean      show_close_button);
GtkWidget *egg_search_bar_get_entry               (EggSearchBar *self);

G_END_DECLS

#endif /* EGG_SEARCH_BAR_H */
