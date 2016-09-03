/* ide-build-command.h
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

#ifndef IDE_BUILD_COMMAND_H
#define IDE_BUILD_COMMAND_H

#include <gio/gio.h>

#include "ide-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_BUILD_COMMAND (ide_build_command_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeBuildCommand, ide_build_command, IDE, BUILD_COMMAND, GObject)

struct _IdeBuildCommandClass
{
  GObjectClass parent_class;

  gboolean  (*run)        (IdeBuildCommand     *self,
                           IdeRuntime          *runtime,
                           IdeEnvironment      *environment,
                           IdeBuildResult      *build_result,
                           GCancellable        *cancellable,
                           GError             **error);
  void      (*run_async)  (IdeBuildCommand     *self,
                           IdeRuntime          *runtime,
                           IdeEnvironment      *environment,
                           IdeBuildResult      *build_result,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data);
  gboolean  (*run_finish) (IdeBuildCommand     *self,
                           GAsyncResult        *result,
                           GError             **error);

  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
  gpointer _reserved5;
  gpointer _reserved6;
  gpointer _reserved7;
  gpointer _reserved8;
};

IdeBuildCommand *ide_build_command_new              (void);
const gchar     *ide_build_command_get_command_text (IdeBuildCommand      *self);
void             ide_build_command_set_command_text (IdeBuildCommand      *self,
                                                     const gchar          *command_text);
gboolean         ide_build_command_run              (IdeBuildCommand      *self,
                                                     IdeRuntime           *runtime,
                                                     IdeEnvironment       *environment,
                                                     IdeBuildResult       *build_result,
                                                     GCancellable         *cancellable,
                                                     GError              **error);
void             ide_build_command_run_async        (IdeBuildCommand      *self,
                                                     IdeRuntime           *runtime,
                                                     IdeEnvironment       *environment,
                                                     IdeBuildResult       *build_result,
                                                     GCancellable         *cancellable,
                                                     GAsyncReadyCallback   callback,
                                                     gpointer              user_data);
gboolean         ide_build_command_run_finish       (IdeBuildCommand      *self,
                                                     GAsyncResult         *result,
                                                     GError              **error);

G_END_DECLS

#endif /* IDE_BUILD_COMMAND_H */
