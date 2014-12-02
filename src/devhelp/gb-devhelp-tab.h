/* gb-devhelp-tab.h
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

#ifndef GB_DEVHELP_TAB_H
#define GB_DEVHELP_TAB_H

#include "gb-tab.h"

G_BEGIN_DECLS

#define GB_TYPE_DEVHELP_TAB            (gb_devhelp_tab_get_type())
#define GB_DEVHELP_TAB(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_DEVHELP_TAB, GbDevhelpTab))
#define GB_DEVHELP_TAB_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_DEVHELP_TAB, GbDevhelpTab const))
#define GB_DEVHELP_TAB_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_DEVHELP_TAB, GbDevhelpTabClass))
#define GB_IS_DEVHELP_TAB(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_DEVHELP_TAB))
#define GB_IS_DEVHELP_TAB_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_DEVHELP_TAB))
#define GB_DEVHELP_TAB_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_DEVHELP_TAB, GbDevhelpTabClass))

typedef struct _GbDevhelpTab        GbDevhelpTab;
typedef struct _GbDevhelpTabClass   GbDevhelpTabClass;
typedef struct _GbDevhelpTabPrivate GbDevhelpTabPrivate;

struct _GbDevhelpTab
{
  GbTab parent;

  /*< private >*/
  GbDevhelpTabPrivate *priv;
};

struct _GbDevhelpTabClass
{
  GbTabClass parent;
};

GType         gb_devhelp_tab_get_type        (void);
GbDevhelpTab *gb_devhelp_tab_new             (void);
void          gb_devhelp_tab_jump_to_keyword (GbDevhelpTab *tab,
                                              const gchar  *keyword);

G_END_DECLS

#endif /* GB_DEVHELP_TAB_H */
