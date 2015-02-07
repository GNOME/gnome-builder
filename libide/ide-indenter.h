/* ide-indenter.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef IDE_INDENTER_H
#define IDE_INDENTER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define IDE_TYPE_INDENTER               (ide_indenter_get_type ())
#define IDE_INDENTER(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), IDE_TYPE_INDENTER, IdeIndenter))
#define IDE_IS_INDENTER(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), IDE_TYPE_INDENTER))
#define IDE_INDENTER_GET_INTERFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), IDE_TYPE_INDENTER, IdeIndenterInterface))

struct _IdeIndenterInterface
{
  GTypeInterface parent;
};

GType ide_indenter_get_type (void);

G_END_DECLS

#endif /* IDE_INDENTER_H */
