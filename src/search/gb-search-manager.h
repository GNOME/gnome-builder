/* gb-search-manager.h
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

#ifndef GB_SEARCH_MANAGER_H
#define GB_SEARCH_MANAGER_H

#include <glib-object.h>

#include "gb-search-context.h"
#include "gb-search-provider.h"

G_BEGIN_DECLS

#define GB_SEARCH_MANAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SEARCH_MANAGER, GbSearchManager))
#define GB_SEARCH_MANAGER_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SEARCH_MANAGER, GbSearchManager const))
#define GB_SEARCH_MANAGER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_SEARCH_MANAGER, GbSearchManagerClass))
#define GB_IS_SEARCH_MANAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_SEARCH_MANAGER))
#define GB_IS_SEARCH_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_SEARCH_MANAGER))
#define GB_SEARCH_MANAGER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_SEARCH_MANAGER, GbSearchManagerClass))

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

GbSearchManager *gb_search_manager_new           (void);
GList           *gb_search_manager_get_providers (GbSearchManager  *manager);
void             gb_search_manager_add_provider  (GbSearchManager  *manager,
                                                  GbSearchProvider *provider);
GbSearchContext *gb_search_manager_search        (GbSearchManager  *manager,
                                                  const GList      *providers,
                                                  const gchar      *search_terms);

G_END_DECLS

#endif /* GB_SEARCH_MANAGER_H */
