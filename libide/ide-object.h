/* ide-object.h
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

#ifndef IDE_OBJECT_H
#define IDE_OBJECT_H

#include <gio/gio.h>

#include "ide-types.h"

G_BEGIN_DECLS

G_DECLARE_DERIVABLE_TYPE (IdeObject, ide_object, IDE, OBJECT, GObject)

#define IDE_TYPE_OBJECT (ide_object_get_type())

struct _IdeObjectClass
{
  GObjectClass parent;
};

IdeContext *ide_object_get_context (IdeObject            *object);
void        ide_object_new_async   (const gchar          *extension_point,
                                    int                   io_priority,
                                    GCancellable         *cancellable,
                                    GAsyncReadyCallback   callback,
                                    gpointer              user_data,
                                    const gchar          *first_property,
                                    ...);
IdeObject  *ide_object_new_finish  (GAsyncResult         *result,
                                    GError              **error);

G_END_DECLS

#endif /* IDE_OBJECT_H */
