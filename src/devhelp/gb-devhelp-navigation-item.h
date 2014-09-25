/* gb-devhelp-navigation-item.h
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

#ifndef GB_DEVHELP_NAVIGATION_ITEM_H
#define GB_DEVHELP_NAVIGATION_ITEM_H

#include "gb-navigation-item.h"

G_BEGIN_DECLS

#define GB_TYPE_DEVHELP_NAVIGATION_ITEM            (gb_devhelp_navigation_item_get_type())
#define GB_DEVHELP_NAVIGATION_ITEM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_DEVHELP_NAVIGATION_ITEM, GbDevhelpNavigationItem))
#define GB_DEVHELP_NAVIGATION_ITEM_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_DEVHELP_NAVIGATION_ITEM, GbDevhelpNavigationItem const))
#define GB_DEVHELP_NAVIGATION_ITEM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_DEVHELP_NAVIGATION_ITEM, GbDevhelpNavigationItemClass))
#define GB_IS_DEVHELP_NAVIGATION_ITEM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_DEVHELP_NAVIGATION_ITEM))
#define GB_IS_DEVHELP_NAVIGATION_ITEM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_DEVHELP_NAVIGATION_ITEM))
#define GB_DEVHELP_NAVIGATION_ITEM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_DEVHELP_NAVIGATION_ITEM, GbDevhelpNavigationItemClass))

typedef struct _GbDevhelpNavigationItem        GbDevhelpNavigationItem;
typedef struct _GbDevhelpNavigationItemClass   GbDevhelpNavigationItemClass;
typedef struct _GbDevhelpNavigationItemPrivate GbDevhelpNavigationItemPrivate;

struct _GbDevhelpNavigationItem
{
  GbNavigationItem parent;

  /*< private >*/
  GbDevhelpNavigationItemPrivate *priv;
};

struct _GbDevhelpNavigationItemClass
{
  GbNavigationItemClass parent;
};

GType gb_devhelp_navigation_item_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* GB_DEVHELP_NAVIGATION_ITEM_H */
