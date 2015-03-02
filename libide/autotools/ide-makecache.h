/* ide-makecache.h
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

#ifndef IDE_MAKECACHE_H
#define IDE_MAKECACHE_H

#include "ide-object.h"

G_BEGIN_DECLS

#define IDE_TYPE_MAKECACHE (ide_makecache_get_type())

G_DECLARE_FINAL_TYPE (IdeMakecache, ide_makecache, IDE, MAKECACHE, IdeObject)

void                 ide_makecache_new_for_makefile_async  (IdeContext           *context,
                                                            GFile                *makefile,
                                                            GCancellable         *cancellable,
                                                            GAsyncReadyCallback   callback,
                                                            gpointer              user_data);
IdeMakecache        *ide_makecache_new_for_makefile_finish (GAsyncResult         *result,
                                                            GError              **error);
GFile               *ide_makecache_get_makefile            (IdeMakecache         *self);
const gchar * const *ide_makecache_get_file_targets        (IdeMakecache         *self,
                                                            GFile                *file);
void                 ide_makecache_get_file_flags_async    (IdeMakecache         *self,
                                                            GFile                *file,
                                                            GCancellable         *cancellable,
                                                            GAsyncReadyCallback   callback,
                                                            gpointer              user_data);
gchar              **ide_makecache_get_file_flags_finish   (IdeMakecache         *self,
                                                            GAsyncResult         *result,
                                                            GError              **error);

G_END_DECLS

#endif /* IDE_MAKECACHE_H */
