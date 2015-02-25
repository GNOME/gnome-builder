/* ide-box-theatric.h
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

#ifndef IDE_BOX_THEATRIC_H
#define IDE_BOX_THEATRIC_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define IDE_TYPE_BOX_THEATRIC            (ide_box_theatric_get_type())
#define IDE_BOX_THEATRIC(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), IDE_TYPE_BOX_THEATRIC, IdeBoxTheatric))
#define IDE_BOX_THEATRIC_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), IDE_TYPE_BOX_THEATRIC, IdeBoxTheatric const))
#define IDE_BOX_THEATRIC_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  IDE_TYPE_BOX_THEATRIC, IdeBoxTheatricClass))
#define IDE_IS_BOX_THEATRIC(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), IDE_TYPE_BOX_THEATRIC))
#define IDE_IS_BOX_THEATRIC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  IDE_TYPE_BOX_THEATRIC))
#define IDE_BOX_THEATRIC_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  IDE_TYPE_BOX_THEATRIC, IdeBoxTheatricClass))

typedef struct _IdeBoxTheatric        IdeBoxTheatric;
typedef struct _IdeBoxTheatricClass   IdeBoxTheatricClass;
typedef struct _IdeBoxTheatricPrivate IdeBoxTheatricPrivate;

struct _IdeBoxTheatric
{
  GObject parent;

  /*< private >*/
  IdeBoxTheatricPrivate *priv;
};

struct _IdeBoxTheatricClass
{
  GObjectClass parent_class;
};

GType ide_box_theatric_get_type (void);

G_END_DECLS

#endif /* IDE_BOX_THEATRIC_H */
