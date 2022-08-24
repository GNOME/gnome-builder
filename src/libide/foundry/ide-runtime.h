/* ide-runtime.h
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#if !defined (IDE_FOUNDRY_INSIDE) && !defined (IDE_FOUNDRY_COMPILATION)
# error "Only <libide-foundry.h> can be included directly."
#endif

#include <libide-core.h>

#include "ide-foundry-types.h"

G_BEGIN_DECLS

typedef enum
{
  IDE_RUNTIME_ERROR_UNKNOWN = 0,
  IDE_RUNTIME_ERROR_NO_SUCH_RUNTIME,
  IDE_RUNTIME_ERROR_BUILD_FAILED,
  IDE_RUNTIME_ERROR_TARGET_NOT_FOUND,
  IDE_RUNTIME_ERROR_SPAWN_FAILED,
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
  void                    (*prepare_configuration)    (IdeRuntime           *self,
                                                       IdeConfig            *config);
  void                    (*prepare_to_run)           (IdeRuntime           *self,
                                                       IdePipeline          *pipeline,
                                                       IdeRunContext        *run_context);
  void                    (*prepare_to_build)         (IdeRuntime           *self,
                                                       IdePipeline          *pipeline,
                                                       IdeRunContext        *run_context);
  GFile                  *(*translate_file)           (IdeRuntime           *self,
                                                       GFile                *file);
  gchar                 **(*get_system_include_dirs)  (IdeRuntime           *self);
  IdeTriplet             *(*get_triplet)              (IdeRuntime           *self);
  gboolean                (*supports_toolchain)       (IdeRuntime           *self,
                                                       IdeToolchain         *toolchain);

  /*< private >*/
  gpointer _reserved[12];
};

IDE_AVAILABLE_IN_ALL
GQuark                  ide_runtime_error_quark              (void) G_GNUC_CONST;
IDE_AVAILABLE_IN_ALL
gboolean                ide_runtime_contains_program_in_path (IdeRuntime      *self,
                                                              const gchar     *program,
                                                              GCancellable    *cancellable);
IDE_AVAILABLE_IN_ALL
void                    ide_runtime_prepare_to_run           (IdeRuntime      *self,
                                                              IdePipeline     *pipeline,
                                                              IdeRunContext   *run_context);
IDE_AVAILABLE_IN_ALL
void                    ide_runtime_prepare_to_build         (IdeRuntime      *self,
                                                              IdePipeline     *pipeline,
                                                              IdeRunContext   *run_context);
IDE_AVAILABLE_IN_ALL
void                    ide_runtime_prepare_configuration    (IdeRuntime      *self,
                                                              IdeConfig       *configuration);
IDE_AVAILABLE_IN_ALL
IdeRuntime             *ide_runtime_new                      (const gchar     *id,
                                                              const gchar     *title);
IDE_AVAILABLE_IN_ALL
const gchar            *ide_runtime_get_id                   (IdeRuntime      *self);
IDE_AVAILABLE_IN_ALL
void                    ide_runtime_set_id                   (IdeRuntime      *self,
                                                              const gchar     *id);
IDE_AVAILABLE_IN_ALL
const gchar            *ide_runtime_get_short_id             (IdeRuntime      *self);
IDE_AVAILABLE_IN_ALL
void                    ide_runtime_set_short_id             (IdeRuntime      *self,
                                                              const gchar     *short_id);
IDE_AVAILABLE_IN_ALL
const gchar            *ide_runtime_get_category             (IdeRuntime      *self);
IDE_AVAILABLE_IN_ALL
void                    ide_runtime_set_category             (IdeRuntime      *self,
                                                              const gchar     *category);
IDE_AVAILABLE_IN_ALL
const gchar            *ide_runtime_get_display_name         (IdeRuntime      *self);
IDE_AVAILABLE_IN_ALL
void                    ide_runtime_set_display_name         (IdeRuntime      *self,
                                                              const gchar     *display_name);
IDE_AVAILABLE_IN_ALL
const gchar            *ide_runtime_get_name                 (IdeRuntime      *self);
IDE_AVAILABLE_IN_ALL
void                    ide_runtime_set_name                 (IdeRuntime      *self,
                                                              const gchar     *name);
IDE_AVAILABLE_IN_ALL
GFile                  *ide_runtime_translate_file           (IdeRuntime      *self,
                                                              GFile           *file);
IDE_AVAILABLE_IN_ALL
gchar                 **ide_runtime_get_system_include_dirs  (IdeRuntime      *self);
IDE_AVAILABLE_IN_ALL
gchar                  *ide_runtime_get_arch                 (IdeRuntime      *self);
IDE_AVAILABLE_IN_ALL
IdeTriplet             *ide_runtime_get_triplet              (IdeRuntime      *self);
IDE_AVAILABLE_IN_ALL
gboolean                ide_runtime_supports_toolchain       (IdeRuntime      *self,
                                                              IdeToolchain    *toolchain);
IDE_AVAILABLE_IN_ALL
const char             *ide_runtime_get_icon_name            (IdeRuntime      *self);
IDE_AVAILABLE_IN_ALL
void                    ide_runtime_set_icon_name            (IdeRuntime      *self,
                                                              const char      *icon_name);

G_END_DECLS
