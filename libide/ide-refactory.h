/* ide-refactory.h
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

#ifndef IDE_REFACTORY_H
#define IDE_REFACTORY_H

#include "ide-object.h"

G_BEGIN_DECLS

#define IDE_TYPE_REFACTORY               (ide_refactory_get_type ())
#define IDE_REFACTORY(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), IDE_TYPE_REFACTORY, IdeRefactory))
#define IDE_IS_REFACTORY(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), IDE_TYPE_REFACTORY))
#define IDE_REFACTORY_GET_INTERFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), IDE_TYPE_REFACTORY, IdeRefactoryInterface))

struct _IdeRefactoryInterface
{
  GTypeInterface parent;
};

GType ide_refactory_get_type (void);

G_END_DECLS

#endif /* IDE_REFACTORY_H */
