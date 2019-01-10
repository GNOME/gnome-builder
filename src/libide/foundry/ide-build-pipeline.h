/* ide-build-pipeline.h
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
#include <libide-code.h>
#include <libide-threading.h>
#include <vte/vte.h>

#include "ide-foundry-types.h"

#include "ide-build-log.h"
#include "ide-build-stage.h"
#include "ide-config.h"
#include "ide-runtime.h"
#include "ide-triplet.h"

G_BEGIN_DECLS

#define IDE_TYPE_BUILD_PIPELINE     (ide_build_pipeline_get_type())
#define IDE_BUILD_PHASE_MASK        (0xFFFFFF)
#define IDE_BUILD_PHASE_WHENCE_MASK (IDE_BUILD_PHASE_BEFORE | IDE_BUILD_PHASE_AFTER)
#define IDE_BUILD_ERROR             (ide_build_error_quark())

typedef enum
{
  IDE_BUILD_PHASE_NONE         = 0,
  IDE_BUILD_PHASE_PREPARE      = 1 << 0,
  IDE_BUILD_PHASE_DOWNLOADS    = 1 << 1,
  IDE_BUILD_PHASE_DEPENDENCIES = 1 << 2,
  IDE_BUILD_PHASE_AUTOGEN      = 1 << 3,
  IDE_BUILD_PHASE_CONFIGURE    = 1 << 4,
  IDE_BUILD_PHASE_BUILD        = 1 << 6,
  IDE_BUILD_PHASE_INSTALL      = 1 << 7,
  IDE_BUILD_PHASE_COMMIT       = 1 << 8,
  IDE_BUILD_PHASE_EXPORT       = 1 << 9,
  IDE_BUILD_PHASE_FINAL        = 1 << 10,
  IDE_BUILD_PHASE_BEFORE       = 1 << 28,
  IDE_BUILD_PHASE_AFTER        = 1 << 29,
  IDE_BUILD_PHASE_FINISHED     = 1 << 30,
  IDE_BUILD_PHASE_FAILED       = 1 << 31,
} IdeBuildPhase;

typedef enum
{
  IDE_BUILD_ERROR_UNKNOWN = 0,
  IDE_BUILD_ERROR_BROKEN,
  IDE_BUILD_ERROR_NOT_LOADED,
  IDE_BUILD_ERROR_NEEDS_REBUILD,
} IdeBuildError;

IDE_AVAILABLE_IN_3_32
G_DECLARE_FINAL_TYPE (IdeBuildPipeline, ide_build_pipeline, IDE, BUILD_PIPELINE, IdeObject)

IDE_AVAILABLE_IN_3_32
GQuark                 ide_build_error_quark                      (void) G_GNUC_CONST;
IDE_AVAILABLE_IN_3_32
gboolean               ide_build_pipeline_is_native               (IdeBuildPipeline       *self);
IDE_AVAILABLE_IN_3_32
gboolean               ide_build_pipeline_is_ready                (IdeBuildPipeline       *self);
IDE_AVAILABLE_IN_3_32
gboolean               ide_build_pipeline_get_busy                (IdeBuildPipeline       *self);
IDE_AVAILABLE_IN_3_32
IdeConfig      *ide_build_pipeline_get_configuration       (IdeBuildPipeline       *self);
IDE_AVAILABLE_IN_3_32
IdeDevice             *ide_build_pipeline_get_device              (IdeBuildPipeline       *self);
IDE_AVAILABLE_IN_3_32
IdeTriplet            *ide_build_pipeline_get_host_triplet        (IdeBuildPipeline       *self);
IDE_AVAILABLE_IN_3_32
IdeRuntime            *ide_build_pipeline_get_runtime             (IdeBuildPipeline       *self);
IDE_AVAILABLE_IN_3_32
IdeToolchain          *ide_build_pipeline_get_toolchain           (IdeBuildPipeline       *self);
IDE_AVAILABLE_IN_3_32
IdeToolchain          *ide_build_pipeline_ref_toolchain           (IdeBuildPipeline       *self);
IDE_AVAILABLE_IN_3_32
const gchar           *ide_build_pipeline_get_builddir            (IdeBuildPipeline       *self);
IDE_AVAILABLE_IN_3_32
const gchar           *ide_build_pipeline_get_srcdir              (IdeBuildPipeline       *self);
IDE_AVAILABLE_IN_3_32
gchar                 *ide_build_pipeline_get_message             (IdeBuildPipeline       *self);
IDE_AVAILABLE_IN_3_32
IdeBuildPhase          ide_build_pipeline_get_phase               (IdeBuildPipeline       *self);
IDE_AVAILABLE_IN_3_32
gboolean               ide_build_pipeline_get_can_export          (IdeBuildPipeline       *self);
IDE_AVAILABLE_IN_3_32
VtePty                *ide_build_pipeline_get_pty                 (IdeBuildPipeline       *self);
IDE_AVAILABLE_IN_3_32
IdeSubprocessLauncher *ide_build_pipeline_create_launcher         (IdeBuildPipeline       *self,
                                                                   GError                **error);
IDE_AVAILABLE_IN_3_32
gchar                 *ide_build_pipeline_build_srcdir_path       (IdeBuildPipeline       *self,
                                                                   const gchar            *first_part,
                                                                   ...) G_GNUC_NULL_TERMINATED;
IDE_AVAILABLE_IN_3_32
gchar                 *ide_build_pipeline_build_builddir_path     (IdeBuildPipeline       *self,
                                                                   const gchar            *first_part,
                                                                   ...) G_GNUC_NULL_TERMINATED;
IDE_AVAILABLE_IN_3_32
void                   ide_build_pipeline_invalidate_phase        (IdeBuildPipeline       *self,
                                                                   IdeBuildPhase           phases);
IDE_AVAILABLE_IN_3_32
gboolean               ide_build_pipeline_request_phase           (IdeBuildPipeline       *self,
                                                                   IdeBuildPhase           phase);
IDE_AVAILABLE_IN_3_32
guint                  ide_build_pipeline_attach                 (IdeBuildPipeline       *self,
                                                                   IdeBuildPhase           phase,
                                                                   gint                    priority,
                                                                   IdeBuildStage          *stage);
IDE_AVAILABLE_IN_3_32
guint                  ide_build_pipeline_attach_launcher        (IdeBuildPipeline       *self,
                                                                   IdeBuildPhase           phase,
                                                                   gint                    priority,
                                                                   IdeSubprocessLauncher  *launcher);
IDE_AVAILABLE_IN_3_32
void                   ide_build_pipeline_detach              (IdeBuildPipeline       *self,
                                                                   guint                   stage_id);
IDE_AVAILABLE_IN_3_32
IdeBuildStage         *ide_build_pipeline_get_stage_by_id         (IdeBuildPipeline       *self,
                                                                   guint                   stage_id);
IDE_AVAILABLE_IN_3_32
guint                  ide_build_pipeline_add_log_observer        (IdeBuildPipeline       *self,
                                                                   IdeBuildLogObserver     observer,
                                                                   gpointer                observer_data,
                                                                   GDestroyNotify          observer_data_destroy);
IDE_AVAILABLE_IN_3_32
gboolean               ide_build_pipeline_remove_log_observer     (IdeBuildPipeline       *self,
                                                                   guint                   observer_id);
IDE_AVAILABLE_IN_3_32
void                   ide_build_pipeline_emit_diagnostic         (IdeBuildPipeline       *self,
                                                                   IdeDiagnostic          *diagnostic);
IDE_AVAILABLE_IN_3_32
guint                  ide_build_pipeline_add_error_format        (IdeBuildPipeline       *self,
                                                                   const gchar            *regex,
                                                                   GRegexCompileFlags      flags);
IDE_AVAILABLE_IN_3_32
gboolean               ide_build_pipeline_remove_error_format     (IdeBuildPipeline       *self,
                                                                   guint                   error_format_id);
IDE_AVAILABLE_IN_3_32
void                   ide_build_pipeline_build_async             (IdeBuildPipeline       *self,
                                                                   IdeBuildPhase           phase,
                                                                   GCancellable           *cancellable,
                                                                   GAsyncReadyCallback     callback,
                                                                   gpointer                user_data);
IDE_AVAILABLE_IN_3_32
gboolean               ide_build_pipeline_build_finish            (IdeBuildPipeline       *self,
                                                                   GAsyncResult           *result,
                                                                   GError                **error);
IDE_AVAILABLE_IN_3_32
void                   ide_build_pipeline_build_targets_async     (IdeBuildPipeline       *self,
                                                                   IdeBuildPhase           phase,
                                                                   GPtrArray              *targets,
                                                                   GCancellable           *cancellable,
                                                                   GAsyncReadyCallback     callback,
                                                                   gpointer                user_data);
IDE_AVAILABLE_IN_3_32
gboolean               ide_build_pipeline_build_targets_finish    (IdeBuildPipeline       *self,
                                                                   GAsyncResult           *result,
                                                                   GError                **error);
IDE_AVAILABLE_IN_3_32
void                   ide_build_pipeline_execute_async           (IdeBuildPipeline       *self,
                                                                   GCancellable           *cancellable,
                                                                   GAsyncReadyCallback     callback,
                                                                   gpointer                user_data);
IDE_AVAILABLE_IN_3_32
gboolean               ide_build_pipeline_execute_finish          (IdeBuildPipeline       *self,
                                                                   GAsyncResult           *result,
                                                                   GError                **error);
IDE_AVAILABLE_IN_3_32
void                   ide_build_pipeline_foreach_stage           (IdeBuildPipeline       *self,
                                                                   GFunc                   stage_callback,
                                                                   gpointer                user_data);
IDE_AVAILABLE_IN_3_32
void                   ide_build_pipeline_clean_async             (IdeBuildPipeline       *self,
                                                                   IdeBuildPhase           phase,
                                                                   GCancellable           *cancellable,
                                                                   GAsyncReadyCallback     callback,
                                                                   gpointer                user_data);
IDE_AVAILABLE_IN_3_32
gboolean               ide_build_pipeline_clean_finish            (IdeBuildPipeline       *self,
                                                                   GAsyncResult           *result,
                                                                   GError                **error);
IDE_AVAILABLE_IN_3_32
void                   ide_build_pipeline_rebuild_async           (IdeBuildPipeline       *self,
                                                                   IdeBuildPhase           phase,
                                                                   GPtrArray              *targets,
                                                                   GCancellable           *cancellable,
                                                                   GAsyncReadyCallback     callback,
                                                                   gpointer                user_data);
IDE_AVAILABLE_IN_3_32
gboolean               ide_build_pipeline_rebuild_finish          (IdeBuildPipeline       *self,
                                                                   GAsyncResult           *result,
                                                                   GError                **error);
IDE_AVAILABLE_IN_3_32
void                   ide_build_pipeline_attach_pty              (IdeBuildPipeline       *self,
                                                                   IdeSubprocessLauncher  *launcher);
IDE_AVAILABLE_IN_3_32
gboolean               ide_build_pipeline_has_configured          (IdeBuildPipeline       *self);
IDE_AVAILABLE_IN_3_32
IdeBuildPhase          ide_build_pipeline_get_requested_phase     (IdeBuildPipeline       *self);

G_END_DECLS
