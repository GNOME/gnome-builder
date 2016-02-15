/* ide-runtime.h
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#ifndef IDE_RUNTIME_H
#define IDE_RUNTIME_H

#include <gio/gio.h>

#include "ide-object.h"
#include "ide-subprocess-launcher.h"

G_BEGIN_DECLS

#define IDE_TYPE_RUNTIME (ide_runtime_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeRuntime, ide_runtime, IDE, RUNTIME, IdeObject)

struct _IdeRuntimeClass
{
  IdeObjectClass parent;

  void                   (*prebuild_async)           (IdeRuntime           *self,
                                                      GCancellable         *cancellable,
                                                      GAsyncReadyCallback   callback,
                                                      gpointer              user_data);
  gboolean               (*prebuild_finish)          (IdeRuntime           *self,
                                                      GAsyncResult         *result,
                                                      GError              **error);
  void                   (*postbuild_async)          (IdeRuntime           *self,
                                                      GCancellable         *cancellable,
                                                      GAsyncReadyCallback   callback,
                                                      gpointer              user_data);
  gboolean               (*postbuild_finish)         (IdeRuntime           *self,
                                                      GAsyncResult         *result,
                                                      GError              **error);
  gboolean               (*contains_program_in_path) (IdeRuntime           *self,
                                                      const gchar          *program,
                                                      GCancellable         *cancellable);
  IdeSubprocessLauncher *(*create_launcher)          (IdeRuntime           *self,
                                                      GError              **error);
  void                   (*prepare_configuration)    (IdeRuntime           *self,
                                                      IdeConfiguration     *configuration);
};

void                   ide_runtime_prebuild_async           (IdeRuntime           *self,
                                                             GCancellable         *cancellable,
                                                             GAsyncReadyCallback   callback,
                                                             gpointer              user_data);
gboolean               ide_runtime_prebuild_finish          (IdeRuntime           *self,
                                                             GAsyncResult         *result,
                                                             GError              **error);
void                   ide_runtime_postbuild_async          (IdeRuntime           *self,
                                                             GCancellable         *cancellable,
                                                             GAsyncReadyCallback   callback,
                                                             gpointer              user_data);
gboolean               ide_runtime_postbuild_finish         (IdeRuntime           *self,
                                                             GAsyncResult         *result,
                                                             GError              **error);
gboolean               ide_runtime_contains_program_in_path (IdeRuntime           *self,
                                                             const gchar          *program,
                                                             GCancellable         *cancellable);
IdeSubprocessLauncher *ide_runtime_create_launcher          (IdeRuntime           *self,
                                                             GError              **error);
void                   ide_runtime_prepare_configuration    (IdeRuntime           *self,
                                                             IdeConfiguration     *configuration);
IdeRuntime            *ide_runtime_new                      (IdeContext           *context,
                                                             const gchar          *id,
                                                             const gchar          *title);
const gchar           *ide_runtime_get_id                   (IdeRuntime           *self);
void                   ide_runtime_set_id                   (IdeRuntime           *self,
                                                             const gchar          *id);
const gchar           *ide_runtime_get_display_name         (IdeRuntime           *self);
void                   ide_runtime_set_display_name         (IdeRuntime           *self,
                                                             const gchar          *display_name);

G_END_DECLS

#endif /* IDE_RUNTIME_H */
