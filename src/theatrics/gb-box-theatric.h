/* gb-box-theatric.h
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

#ifndef GB_BOX_THEATRIC_H
#define GB_BOX_THEATRIC_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GB_TYPE_BOX_THEATRIC            (gb_box_theatric_get_type())
#define GB_BOX_THEATRIC(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_BOX_THEATRIC, GbBoxTheatric))
#define GB_BOX_THEATRIC_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_BOX_THEATRIC, GbBoxTheatric const))
#define GB_BOX_THEATRIC_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_BOX_THEATRIC, GbBoxTheatricClass))
#define GB_IS_BOX_THEATRIC(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_BOX_THEATRIC))
#define GB_IS_BOX_THEATRIC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_BOX_THEATRIC))
#define GB_BOX_THEATRIC_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_BOX_THEATRIC, GbBoxTheatricClass))

typedef struct _GbBoxTheatric        GbBoxTheatric;
typedef struct _GbBoxTheatricClass   GbBoxTheatricClass;
typedef struct _GbBoxTheatricPrivate GbBoxTheatricPrivate;

struct _GbBoxTheatric
{
  GObject parent;

  /*< private >*/
  GbBoxTheatricPrivate *priv;
};

struct _GbBoxTheatricClass
{
  GObjectClass parent_class;
};

GType gb_box_theatric_get_type (void);

G_END_DECLS

#endif /* GB_BOX_THEATRIC_H */
