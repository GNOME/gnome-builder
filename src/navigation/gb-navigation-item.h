/* gb-navigation-item.h
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

#ifndef GB_NAVIGATION_ITEM_H
#define GB_NAVIGATION_ITEM_H

#include <glib-object.h>

#include "gb-workspace.h"

G_BEGIN_DECLS

#define GB_TYPE_NAVIGATION_ITEM            (gb_navigation_item_get_type())
#define GB_NAVIGATION_ITEM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_NAVIGATION_ITEM, GbNavigationItem))
#define GB_NAVIGATION_ITEM_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_NAVIGATION_ITEM, GbNavigationItem const))
#define GB_NAVIGATION_ITEM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_NAVIGATION_ITEM, GbNavigationItemClass))
#define GB_IS_NAVIGATION_ITEM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_NAVIGATION_ITEM))
#define GB_IS_NAVIGATION_ITEM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_NAVIGATION_ITEM))
#define GB_NAVIGATION_ITEM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_NAVIGATION_ITEM, GbNavigationItemClass))

typedef struct _GbNavigationItem        GbNavigationItem;
typedef struct _GbNavigationItemClass   GbNavigationItemClass;
typedef struct _GbNavigationItemPrivate GbNavigationItemPrivate;

struct _GbNavigationItem
{
  GInitiallyUnowned parent;

  /*< private >*/
  GbNavigationItemPrivate *priv;
};

struct _GbNavigationItemClass
{
  GInitiallyUnownedClass parent;

  void (*activate) (GbNavigationItem *item);
};

GType             gb_navigation_item_get_type      (void) G_GNUC_CONST;
GbNavigationItem *gb_navigation_item_new           (const gchar      *label);
void              gb_navigation_item_activate      (GbNavigationItem *item);
const gchar      *gb_navigation_item_get_label     (GbNavigationItem *item);
void              gb_navigation_item_set_label     (GbNavigationItem *item,
                                                    const gchar      *label);
GbWorkspace      *gb_navigation_item_get_workspace (GbNavigationItem *item);

G_END_DECLS

#endif /* GB_NAVIGATION_ITEM_H */
