/* gb-navigation-list.h
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#ifndef GB_NAVIGATION_LIST_H
#define GB_NAVIGATION_LIST_H

#include <glib-object.h>

#include "gb-navigation-item.h"

G_BEGIN_DECLS

#define GB_TYPE_NAVIGATION_LIST            (gb_navigation_list_get_type())
#define GB_NAVIGATION_LIST(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_NAVIGATION_LIST, GbNavigationList))
#define GB_NAVIGATION_LIST_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_NAVIGATION_LIST, GbNavigationList const))
#define GB_NAVIGATION_LIST_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_NAVIGATION_LIST, GbNavigationListClass))
#define GB_IS_NAVIGATION_LIST(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_NAVIGATION_LIST))
#define GB_IS_NAVIGATION_LIST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_NAVIGATION_LIST))
#define GB_NAVIGATION_LIST_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_NAVIGATION_LIST, GbNavigationListClass))

typedef struct _GbNavigationList        GbNavigationList;
typedef struct _GbNavigationListClass   GbNavigationListClass;
typedef struct _GbNavigationListPrivate GbNavigationListPrivate;

struct _GbNavigationList
{
  GObject parent;

  /*< private >*/
  GbNavigationListPrivate *priv;
};

struct _GbNavigationListClass
{
  GObjectClass parent;
};

GType                 gb_navigation_list_get_type            (void) G_GNUC_CONST;
GbNavigationList     *gb_navigation_list_new                 (void);
gboolean              gb_navigation_list_get_can_go_backward (GbNavigationList *list);
gboolean              gb_navigation_list_get_can_go_forward  (GbNavigationList *list);
GbNavigationItem     *gb_navigation_list_get_current_item    (GbNavigationList *list);
void                  gb_navigation_list_append              (GbNavigationList *list,
                                                              GbNavigationItem *item);
void                  gb_navigation_list_go_backward         (GbNavigationList *list);
void                  gb_navigation_list_go_forward          (GbNavigationList *list);

G_END_DECLS

#endif /* GB_NAVIGATION_LIST_H */
