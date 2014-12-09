/* gb-devhelp-view.h
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

#ifndef GB_DEVHELP_VIEW_H
#define GB_DEVHELP_VIEW_H

#include "gb-devhelp-document.h"
#include "gb-document-view.h"

G_BEGIN_DECLS

#define GB_TYPE_DEVHELP_VIEW            (gb_devhelp_view_get_type())
#define GB_DEVHELP_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_DEVHELP_VIEW, GbDevhelpView))
#define GB_DEVHELP_VIEW_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_DEVHELP_VIEW, GbDevhelpView const))
#define GB_DEVHELP_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_DEVHELP_VIEW, GbDevhelpViewClass))
#define GB_IS_DEVHELP_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_DEVHELP_VIEW))
#define GB_IS_DEVHELP_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_DEVHELP_VIEW))
#define GB_DEVHELP_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_DEVHELP_VIEW, GbDevhelpViewClass))

typedef struct _GbDevhelpView        GbDevhelpView;
typedef struct _GbDevhelpViewClass   GbDevhelpViewClass;
typedef struct _GbDevhelpViewPrivate GbDevhelpViewPrivate;

struct _GbDevhelpView
{
  GbDocumentView parent;

  /*< private >*/
  GbDevhelpViewPrivate *priv;
};

struct _GbDevhelpViewClass
{
  GbDocumentViewClass parent;
};

GType           gb_devhelp_view_get_type (void);
GbDocumentView *gb_devhelp_view_new      (GbDevhelpDocument *document);

G_END_DECLS

#endif /* GB_DEVHELP_VIEW_H */
