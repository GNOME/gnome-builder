/* ide-runtime.h
 *
 * Copyright 2016 Christian Hergert <chergert@redhat.com>
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

#pragma once

#include <gio/gio.h>

#include "ide-version-macros.h"

#include "ide-object.h"

#include "buildsystem/ide-build-target.h"
#include "runner/ide-runner.h"
#include "subprocess/ide-subprocess-launcher.h"
#include "toolchain/ide-toolchain.h"
#include "util/ide-triplet.h"

G_BEGIN_DECLS

typedef enum
{
  IDE_RUNTIME_ERROR_NO_SUCH_RUNTIME = 1,
} IdeRuntimeError;

#define IDE_TYPE_RUNTIME (ide_runtime_get_type())
#define IDE_RUNTIME_ERROR (ide_runtime_error_quark())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeRuntime, ide_runtime, IDE, RUNTIME, IdeObject)

struct _IdeRuntimeClass
{
  IdeObjectClass parent;

  gboolean                (*contains_program_in_path) (IdeRuntime           *self,
                                                       const gchar          *program,
                                                       GCancellable         *cancellable);
  IdeSubprocessLauncher  *(*create_launcher)          (IdeRuntime           *self,
                                                       GError              **error);
  void                    (*prepare_configuration)    (IdeRuntime           *self,
                                                       IdeConfiguration     *configuration);
  IdeRunner              *(*create_runner)            (IdeRuntime           *self,
                                                       IdeBuildTarget       *build_target);
  GFile                  *(*translate_file)           (IdeRuntime           *self,
                                                       GFile                *file);
  gchar                 **(*get_system_include_dirs)  (IdeRuntime           *self);
  IdeTriplet             *(*get_triplet)              (IdeRuntime           *self);
  gboolean                (*supports_toolchain)       (IdeRuntime           *self,
                                                       IdeToolchain         *toolchain);

  gpointer _reserved[12];
};

IDE_AVAILABLE_IN_ALL
GQuark                 ide_runtime_error_quark              (void) G_GNUC_CONST;
IDE_AVAILABLE_IN_ALL
gboolean               ide_runtime_contains_program_in_path (IdeRuntime           *self,
                                                             const gchar          *program,
                                                             GCancellable         *cancellable);
IDE_AVAILABLE_IN_ALL
IdeSubprocessLauncher *ide_runtime_create_launcher          (IdeRuntime           *self,
                                                             GError              **error);
IDE_AVAILABLE_IN_ALL
IdeRunner             *ide_runtime_create_runner            (IdeRuntime           *self,
                                                             IdeBuildTarget       *build_target);
IDE_AVAILABLE_IN_ALL
void                   ide_runtime_prepare_configuration    (IdeRuntime           *self,
                                                             IdeConfiguration     *configuration);
IDE_AVAILABLE_IN_ALL
IdeRuntime            *ide_runtime_new                      (IdeContext           *context,
                                                             const gchar          *id,
                                                             const gchar          *title);
IDE_AVAILABLE_IN_ALL
const gchar           *ide_runtime_get_id                   (IdeRuntime           *self);
IDE_AVAILABLE_IN_ALL
void                   ide_runtime_set_id                   (IdeRuntime           *self,
                                                             const gchar          *id);
IDE_AVAILABLE_IN_ALL
const gchar           *ide_runtime_get_display_name         (IdeRuntime           *self);
IDE_AVAILABLE_IN_ALL
void                   ide_runtime_set_display_name         (IdeRuntime           *self,
                                                             const gchar          *display_name);
IDE_AVAILABLE_IN_ALL
GFile                 *ide_runtime_translate_file           (IdeRuntime           *self,
                                                             GFile                *file);
IDE_AVAILABLE_IN_3_28
gchar                **ide_runtime_get_system_include_dirs  (IdeRuntime           *self);
IDE_AVAILABLE_IN_3_28
gchar                 *ide_runtime_get_arch                 (IdeRuntime           *self);
IDE_AVAILABLE_IN_3_30
IdeTriplet            *ide_runtime_get_triplet              (IdeRuntime           *self);
IDE_AVAILABLE_IN_3_30
gboolean               ide_runtime_supports_toolchain       (IdeRuntime           *self,
                                                             IdeToolchain         *toolchain);

G_END_DECLS
