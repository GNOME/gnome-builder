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

#include "buildsystem/ide-build-target.h"
#include "runner/ide-runner.h"
#include "subprocess/ide-subprocess-launcher.h"

G_BEGIN_DECLS

typedef enum
{
  IDE_RUNTIME_ERROR_NO_SUCH_RUNTIME = 1,
} IdeRuntimeError;

#define IDE_TYPE_RUNTIME (ide_runtime_get_type())
#define IDE_RUNTIME_ERROR (ide_runtime_error_quark())

G_DECLARE_DERIVABLE_TYPE (IdeRuntime, ide_runtime, IDE, RUNTIME, IdeObject)

struct _IdeRuntimeClass
{
  IdeObjectClass parent;

  gboolean               (*contains_program_in_path) (IdeRuntime           *self,
                                                      const gchar          *program,
                                                      GCancellable         *cancellable);
  IdeSubprocessLauncher *(*create_launcher)          (IdeRuntime           *self,
                                                      GError              **error);
  void                   (*prepare_configuration)    (IdeRuntime           *self,
                                                      IdeConfiguration     *configuration);
  IdeRunner             *(*create_runner)            (IdeRuntime           *self,
                                                      IdeBuildTarget       *build_target);
  GFile                 *(*translate_file)           (IdeRuntime           *self,
                                                      GFile                *file);

  gpointer _reserved4;
  gpointer _reserved5;
  gpointer _reserved6;
  gpointer _reserved7;
  gpointer _reserved8;
  gpointer _reserved9;
  gpointer _reserved10;
  gpointer _reserved11;
  gpointer _reserved12;
  gpointer _reserved13;
  gpointer _reserved14;
  gpointer _reserved15;
  gpointer _reserved16;
};

GQuark                 ide_runtime_error_quark              (void) G_GNUC_CONST;
gboolean               ide_runtime_contains_program_in_path (IdeRuntime           *self,
                                                             const gchar          *program,
                                                             GCancellable         *cancellable);
IdeSubprocessLauncher *ide_runtime_create_launcher          (IdeRuntime           *self,
                                                             GError              **error);
IdeRunner             *ide_runtime_create_runner            (IdeRuntime           *self,
                                                             IdeBuildTarget       *build_target);
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
GFile                 *ide_runtime_translate_file           (IdeRuntime           *self,
                                                             GFile                *file);

G_END_DECLS

#endif /* IDE_RUNTIME_H */
