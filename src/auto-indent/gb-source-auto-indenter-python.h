/* gb-source-auto-indenter-python.h
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

#ifndef GB_SOURCE_AUTO_INDENTER_PYTHON_H
#define GB_SOURCE_AUTO_INDENTER_PYTHON_H

#include "gb-source-auto-indenter.h"

G_BEGIN_DECLS

#define GB_TYPE_SOURCE_AUTO_INDENTER_PYTHON            (gb_source_auto_indenter_python_get_type())
#define GB_SOURCE_AUTO_INDENTER_PYTHON(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SOURCE_AUTO_INDENTER_PYTHON, GbSourceAutoIndenterPython))
#define GB_SOURCE_AUTO_INDENTER_PYTHON_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SOURCE_AUTO_INDENTER_PYTHON, GbSourceAutoIndenterPython const))
#define GB_SOURCE_AUTO_INDENTER_PYTHON_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_SOURCE_AUTO_INDENTER_PYTHON, GbSourceAutoIndenterPythonClass))
#define GB_IS_SOURCE_AUTO_INDENTER_PYTHON(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_SOURCE_AUTO_INDENTER_PYTHON))
#define GB_IS_SOURCE_AUTO_INDENTER_PYTHON_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_SOURCE_AUTO_INDENTER_PYTHON))
#define GB_SOURCE_AUTO_INDENTER_PYTHON_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_SOURCE_AUTO_INDENTER_PYTHON, GbSourceAutoIndenterPythonClass))

typedef struct _GbSourceAutoIndenterPython        GbSourceAutoIndenterPython;
typedef struct _GbSourceAutoIndenterPythonClass   GbSourceAutoIndenterPythonClass;
typedef struct _GbSourceAutoIndenterPythonPrivate GbSourceAutoIndenterPythonPrivate;

struct _GbSourceAutoIndenterPython
{
  GbSourceAutoIndenter parent;

  /*< private >*/
  GbSourceAutoIndenterPythonPrivate *priv;
};

struct _GbSourceAutoIndenterPythonClass
{
  GbSourceAutoIndenterClass parent;
};

GType                 gb_source_auto_indenter_python_get_type (void);
GbSourceAutoIndenter *gb_source_auto_indenter_python_new      (void);

G_END_DECLS

#endif /* GB_SOURCE_AUTO_INDENTER_PYTHON_H */
