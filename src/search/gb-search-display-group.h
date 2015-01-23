/* gb-search-display-group.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#ifndef GB_SEARCH_DISPLAY_GROUP_H
#define GB_SEARCH_DISPLAY_GROUP_H

#include <gtk/gtk.h>

#include "gb-search-types.h"

G_BEGIN_DECLS

#define GB_SEARCH_DISPLAY_GROUP(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SEARCH_DISPLAY_GROUP, GbSearchDisplayGroup))
#define GB_SEARCH_DISPLAY_GROUP_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SEARCH_DISPLAY_GROUP, GbSearchDisplayGroup const))
#define GB_SEARCH_DISPLAY_GROUP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_SEARCH_DISPLAY_GROUP, GbSearchDisplayGroupClass))
#define GB_IS_SEARCH_DISPLAY_GROUP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_SEARCH_DISPLAY_GROUP))
#define GB_IS_SEARCH_DISPLAY_GROUP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_SEARCH_DISPLAY_GROUP))
#define GB_SEARCH_DISPLAY_GROUP_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_SEARCH_DISPLAY_GROUP, GbSearchDisplayGroupClass))

struct _GbSearchDisplayGroup
{
  GtkBox parent;

  /*< private >*/
  GbSearchDisplayGroupPrivate *priv;
};

struct _GbSearchDisplayGroupClass
{
  GtkBoxClass parent;
};

void              gb_search_display_group_clear         (GbSearchDisplayGroup *group);
GbSearchProvider *gb_search_display_group_get_provider  (GbSearchDisplayGroup *group);
void              gb_search_display_group_add_result    (GbSearchDisplayGroup *group,
                                                         GbSearchResult       *result);
void              gb_search_display_group_remove_result (GbSearchDisplayGroup *group,
                                                         GbSearchResult       *result);
void              gb_search_display_group_set_count     (GbSearchDisplayGroup *group,
                                                         guint64               count);
void              gb_search_display_group_unselect      (GbSearchDisplayGroup *group);
void              gb_search_display_group_focus_first   (GbSearchDisplayGroup *group);
void              gb_search_display_group_focus_last    (GbSearchDisplayGroup *group);
GbSearchResult   *gb_search_display_group_get_first     (GbSearchDisplayGroup *group);

G_END_DECLS

#endif /* GB_SEARCH_DISPLAY_GROUP_H */
