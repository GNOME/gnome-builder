/* gb-search-manager.h
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

#ifndef GB_SEARCH_MANAGER_H
#define GB_SEARCH_MANAGER_H

#include <gio/gio.h>

#include "gb-search-types.h"

G_BEGIN_DECLS

#define GB_TYPE_SEARCH_MANAGER            (gb_search_manager_get_type())
#define GB_SEARCH_MANAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SEARCH_MANAGER, GbSearchManager))
#define GB_SEARCH_MANAGER_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SEARCH_MANAGER, GbSearchManager const))
#define GB_SEARCH_MANAGER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_SEARCH_MANAGER, GbSearchManagerClass))
#define GB_IS_SEARCH_MANAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_SEARCH_MANAGER))
#define GB_IS_SEARCH_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_SEARCH_MANAGER))
#define GB_SEARCH_MANAGER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_SEARCH_MANAGER, GbSearchManagerClass))

typedef struct _GbSearchManager        GbSearchManager;
typedef struct _GbSearchManagerClass   GbSearchManagerClass;
typedef struct _GbSearchManagerPrivate GbSearchManagerPrivate;

struct _GbSearchManager
{
  GObject parent;

  /*< private >*/
  GbSearchManagerPrivate *priv;
};

struct _GbSearchManagerClass
{
  GObjectClass parent;
};

GType            gb_search_manager_get_type     (void);
GbSearchManager *gb_search_manager_new          (void);
GbSearchManager *gb_search_manager_get_default  (void);
void             gb_search_manager_add_provider (GbSearchManager  *manager,
                                                 GbSearchProvider *provider);
GbSearchContext *gb_search_manager_search       (GbSearchManager  *manager,
                                                 const gchar      *search_text);

G_END_DECLS

#endif /* GB_SEARCH_MANAGER_H */
