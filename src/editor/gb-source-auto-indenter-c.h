/* gb-source-auto-indenter-c.h
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GB_SOURCE_AUTO_INDENTER_C_H
#define GB_SOURCE_AUTO_INDENTER_C_H

#include "gb-source-auto-indenter.h"

G_BEGIN_DECLS

#define GB_TYPE_SOURCE_AUTO_INDENTER_C            (gb_source_auto_indenter_c_get_type())
#define GB_SOURCE_AUTO_INDENTER_C(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SOURCE_AUTO_INDENTER_C, GbSourceAutoIndenterC))
#define GB_SOURCE_AUTO_INDENTER_C_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SOURCE_AUTO_INDENTER_C, GbSourceAutoIndenterC const))
#define GB_SOURCE_AUTO_INDENTER_C_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_SOURCE_AUTO_INDENTER_C, GbSourceAutoIndenterCClass))
#define GB_IS_SOURCE_AUTO_INDENTER_C(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_SOURCE_AUTO_INDENTER_C))
#define GB_IS_SOURCE_AUTO_INDENTER_C_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_SOURCE_AUTO_INDENTER_C))
#define GB_SOURCE_AUTO_INDENTER_C_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_SOURCE_AUTO_INDENTER_C, GbSourceAutoIndenterCClass))

typedef struct _GbSourceAutoIndenterC        GbSourceAutoIndenterC;
typedef struct _GbSourceAutoIndenterCClass   GbSourceAutoIndenterCClass;
typedef struct _GbSourceAutoIndenterCPrivate GbSourceAutoIndenterCPrivate;

struct _GbSourceAutoIndenterC
{
  GbSourceAutoIndenter parent;

  /*< private >*/
  GbSourceAutoIndenterCPrivate *priv;
};

struct _GbSourceAutoIndenterCClass
{
  GbSourceAutoIndenterClass parent_class;
};

GbSourceAutoIndenter *gb_source_auto_indenter_c_new      (void);
GType                 gb_source_auto_indenter_c_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* GB_SOURCE_AUTO_INDENTER_C_H */
