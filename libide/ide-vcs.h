/* ide-vcs.h
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

#ifndef IDE_VCS_H
#define IDE_VCS_H

#include <gio/gio.h>

#include "ide-object.h"

G_BEGIN_DECLS

#define IDE_TYPE_VCS            (ide_vcs_get_type())
#define IDE_VCS_EXTENSION_POINT "org.gnome.libide.extensions.vcs"

G_DECLARE_DERIVABLE_TYPE (IdeVcs, ide_vcs, IDE, VCS, IdeObject)

struct _IdeVcsClass
{
  IdeObjectClass parent;

  GFile                  *(*get_working_directory)     (IdeVcs    *self);
  IdeBufferChangeMonitor *(*get_buffer_change_monitor) (IdeVcs    *self,
                                                        IdeBuffer *buffer);
};

IdeBufferChangeMonitor *ide_vcs_get_buffer_change_monitor (IdeVcs               *self,
                                                           IdeBuffer            *buffer);
GFile                  *ide_vcs_get_working_directory     (IdeVcs               *self);
void                    ide_vcs_new_async                 (IdeContext           *context,
                                                           int                   io_priority,
                                                           GCancellable         *cancellable,
                                                           GAsyncReadyCallback   callback,
                                                           gpointer              user_data);
IdeVcs                 *ide_vcs_new_finish                (GAsyncResult         *result,
                                                           GError              **error);

G_END_DECLS

#endif /* IDE_VCS_H */
