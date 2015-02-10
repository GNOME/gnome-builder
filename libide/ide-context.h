/* ide-context.h
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

#ifndef IDE_CONTEXT_H
#define IDE_CONTEXT_H

#include <gio/gio.h>

#include "ide-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_CONTEXT (ide_context_get_type())

G_DECLARE_FINAL_TYPE (IdeContext, ide_context, IDE, CONTEXT, GObject)

struct _IdeContext
{
  GObject parent_instance;
};

GFile              *ide_context_get_project_file      (IdeContext           *context);
IdeBuildSystem     *ide_context_get_build_system      (IdeContext           *context);
IdeDeviceManager   *ide_context_get_device_manager    (IdeContext           *context);
IdeProject         *ide_context_get_project           (IdeContext           *context);
IdeUnsavedFiles    *ide_context_get_unsaved_files     (IdeContext           *context);
IdeVcs             *ide_context_get_vcs               (IdeContext           *context);
const gchar        *ide_context_get_root_build_dir    (IdeContext           *context);
gpointer            ide_context_get_service_typed     (IdeContext           *context,
                                                       GType                 service_type);
void                ide_context_new_async             (GFile                *project_file,
                                                       GCancellable         *cancellable,
                                                       GAsyncReadyCallback   callback,
                                                       gpointer              user_data);
IdeContext         *ide_context_new_finish            (GAsyncResult         *result,
                                                       GError              **error);
void                ide_context_set_root_build_dir    (IdeContext           *context,
                                                       const gchar          *root_build_dir);

G_END_DECLS

#endif /* IDE_CONTEXT_H */
