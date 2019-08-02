/* ide-pipeline.h
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
#include "ide-config.h"
#include "ide-pipeline-stage.h"
#include "ide-runtime.h"
#include "ide-triplet.h"

G_BEGIN_DECLS

#define IDE_TYPE_PIPELINE              (ide_pipeline_get_type())
#define IDE_PIPELINE_PHASE_MASK        (0xFFFFFF)
#define IDE_PIPELINE_PHASE_WHENCE_MASK (IDE_PIPELINE_PHASE_BEFORE | IDE_PIPELINE_PHASE_AFTER)
#define IDE_BUILD_ERROR                (ide_build_error_quark())

typedef enum
{
  IDE_PIPELINE_PHASE_NONE         = 0,
  IDE_PIPELINE_PHASE_PREPARE      = 1 << 0,
  IDE_PIPELINE_PHASE_DOWNLOADS    = 1 << 1,
  IDE_PIPELINE_PHASE_DEPENDENCIES = 1 << 2,
  IDE_PIPELINE_PHASE_AUTOGEN      = 1 << 3,
  IDE_PIPELINE_PHASE_CONFIGURE    = 1 << 4,
  IDE_PIPELINE_PHASE_BUILD        = 1 << 6,
  IDE_PIPELINE_PHASE_INSTALL      = 1 << 7,
  IDE_PIPELINE_PHASE_COMMIT       = 1 << 8,
  IDE_PIPELINE_PHASE_EXPORT       = 1 << 9,
  IDE_PIPELINE_PHASE_FINAL        = 1 << 10,
  IDE_PIPELINE_PHASE_BEFORE       = 1 << 28,
  IDE_PIPELINE_PHASE_AFTER        = 1 << 29,
  IDE_PIPELINE_PHASE_FINISHED     = 1 << 30,
  IDE_PIPELINE_PHASE_FAILED       = 1 << 31,
} IdePipelinePhase;

typedef enum
{
  IDE_BUILD_ERROR_UNKNOWN = 0,
  IDE_BUILD_ERROR_BROKEN,
  IDE_BUILD_ERROR_NOT_LOADED,
  IDE_BUILD_ERROR_NEEDS_REBUILD,
} IdeBuildError;

IDE_AVAILABLE_IN_3_32
G_DECLARE_FINAL_TYPE (IdePipeline, ide_pipeline, IDE, PIPELINE, IdeObject)

IDE_AVAILABLE_IN_3_32
GQuark                 ide_build_error_quark                 (void) G_GNUC_CONST;
IDE_AVAILABLE_IN_3_32
gchar                 *ide_pipeline_get_arch                 (IdePipeline            *self);
IDE_AVAILABLE_IN_3_32
gboolean               ide_pipeline_is_native                (IdePipeline            *self);
IDE_AVAILABLE_IN_3_32
gboolean               ide_pipeline_is_ready                 (IdePipeline            *self);
IDE_AVAILABLE_IN_3_32
gboolean               ide_pipeline_get_busy                 (IdePipeline            *self);
IDE_AVAILABLE_IN_3_32
IdeConfig             *ide_pipeline_get_config               (IdePipeline            *self);
IDE_AVAILABLE_IN_3_32
IdeDevice             *ide_pipeline_get_device               (IdePipeline            *self);
IDE_AVAILABLE_IN_3_32
IdeDeviceInfo         *ide_pipeline_get_device_info          (IdePipeline            *self);
IDE_AVAILABLE_IN_3_32
IdeTriplet            *ide_pipeline_get_host_triplet         (IdePipeline            *self);
IDE_AVAILABLE_IN_3_32
IdeRuntime            *ide_pipeline_get_runtime              (IdePipeline            *self);
IDE_AVAILABLE_IN_3_32
IdeToolchain          *ide_pipeline_get_toolchain            (IdePipeline            *self);
IDE_AVAILABLE_IN_3_32
IdeToolchain          *ide_pipeline_ref_toolchain            (IdePipeline            *self);
IDE_AVAILABLE_IN_3_32
const gchar           *ide_pipeline_get_builddir             (IdePipeline            *self);
IDE_AVAILABLE_IN_3_32
const gchar           *ide_pipeline_get_srcdir               (IdePipeline            *self);
IDE_AVAILABLE_IN_3_32
gchar                 *ide_pipeline_get_message              (IdePipeline            *self);
IDE_AVAILABLE_IN_3_32
IdePipelinePhase       ide_pipeline_get_phase                (IdePipeline            *self);
IDE_AVAILABLE_IN_3_32
gboolean               ide_pipeline_get_can_export           (IdePipeline            *self);
IDE_AVAILABLE_IN_3_32
VtePty                *ide_pipeline_get_pty                  (IdePipeline            *self);
IDE_AVAILABLE_IN_3_32
IdeSubprocessLauncher *ide_pipeline_create_launcher          (IdePipeline            *self,
                                                              GError                **error);
IDE_AVAILABLE_IN_3_32
gchar                 *ide_pipeline_build_srcdir_path        (IdePipeline            *self,
                                                              const gchar            *first_part,
                                                              ...) G_GNUC_NULL_TERMINATED;
IDE_AVAILABLE_IN_3_32
gchar                 *ide_pipeline_build_builddir_path      (IdePipeline            *self,
                                                              const gchar            *first_part,
                                                              ...) G_GNUC_NULL_TERMINATED;
IDE_AVAILABLE_IN_3_32
void                   ide_pipeline_invalidate_phase         (IdePipeline            *self,
                                                              IdePipelinePhase        phases);
IDE_AVAILABLE_IN_3_32
gboolean               ide_pipeline_request_phase            (IdePipeline            *self,
                                                              IdePipelinePhase        phase);
IDE_AVAILABLE_IN_3_32
guint                  ide_pipeline_attach                   (IdePipeline            *self,
                                                              IdePipelinePhase        phase,
                                                              gint                    priority,
                                                              IdePipelineStage       *stage);
IDE_AVAILABLE_IN_3_32
guint                  ide_pipeline_attach_launcher          (IdePipeline            *self,
                                                              IdePipelinePhase        phase,
                                                              gint                    priority,
                                                              IdeSubprocessLauncher  *launcher);
IDE_AVAILABLE_IN_3_32
void                   ide_pipeline_detach                   (IdePipeline            *self,
                                                              guint                   stage_id);
IDE_AVAILABLE_IN_3_32
IdePipelineStage      *ide_pipeline_get_stage_by_id          (IdePipeline            *self,
                                                              guint                   stage_id);
IDE_AVAILABLE_IN_3_32
guint                  ide_pipeline_add_log_observer         (IdePipeline            *self,
                                                              IdeBuildLogObserver     observer,
                                                              gpointer                observer_data,
                                                              GDestroyNotify          observer_data_destroy);
IDE_AVAILABLE_IN_3_32
gboolean               ide_pipeline_remove_log_observer      (IdePipeline            *self,
                                                              guint                   observer_id);
IDE_AVAILABLE_IN_3_32
void                   ide_pipeline_emit_diagnostic          (IdePipeline            *self,
                                                              IdeDiagnostic          *diagnostic);
IDE_AVAILABLE_IN_3_32
guint                  ide_pipeline_add_error_format         (IdePipeline            *self,
                                                              const gchar            *regex,
                                                              GRegexCompileFlags      flags);
IDE_AVAILABLE_IN_3_32
gboolean               ide_pipeline_remove_error_format      (IdePipeline            *self,
                                                              guint                   error_format_id);
IDE_AVAILABLE_IN_3_32
void                   ide_pipeline_build_async              (IdePipeline            *self,
                                                              IdePipelinePhase        phase,
                                                              GCancellable           *cancellable,
                                                              GAsyncReadyCallback     callback,
                                                              gpointer                user_data);
IDE_AVAILABLE_IN_3_32
gboolean               ide_pipeline_build_finish             (IdePipeline            *self,
                                                              GAsyncResult           *result,
                                                              GError                **error);
IDE_AVAILABLE_IN_3_32
void                   ide_pipeline_build_targets_async      (IdePipeline            *self,
                                                              IdePipelinePhase        phase,
                                                              GPtrArray              *targets,
                                                              GCancellable           *cancellable,
                                                              GAsyncReadyCallback     callback,
                                                              gpointer                user_data);
IDE_AVAILABLE_IN_3_32
gboolean               ide_pipeline_build_targets_finish     (IdePipeline            *self,
                                                              GAsyncResult           *result,
                                                              GError                **error);
IDE_AVAILABLE_IN_3_32
void                   ide_pipeline_foreach_stage            (IdePipeline            *self,
                                                              GFunc                   stage_callback,
                                                              gpointer                user_data);
IDE_AVAILABLE_IN_3_32
void                   ide_pipeline_clean_async              (IdePipeline            *self,
                                                              IdePipelinePhase        phase,
                                                              GCancellable           *cancellable,
                                                              GAsyncReadyCallback     callback,
                                                              gpointer                user_data);
IDE_AVAILABLE_IN_3_32
gboolean               ide_pipeline_clean_finish             (IdePipeline            *self,
                                                              GAsyncResult           *result,
                                                              GError                **error);
IDE_AVAILABLE_IN_3_32
void                   ide_pipeline_rebuild_async            (IdePipeline            *self,
                                                              IdePipelinePhase        phase,
                                                              GPtrArray              *targets,
                                                              GCancellable           *cancellable,
                                                              GAsyncReadyCallback     callback,
                                                              gpointer                user_data);
IDE_AVAILABLE_IN_3_32
gboolean               ide_pipeline_rebuild_finish           (IdePipeline            *self,
                                                              GAsyncResult           *result,
                                                              GError                **error);
IDE_AVAILABLE_IN_3_32
void                   ide_pipeline_attach_pty               (IdePipeline            *self,
                                                              IdeSubprocessLauncher  *launcher);
IDE_AVAILABLE_IN_3_32
gboolean               ide_pipeline_has_configured           (IdePipeline            *self);
IDE_AVAILABLE_IN_3_32
IdePipelinePhase       ide_pipeline_get_requested_phase      (IdePipeline            *self);
IDE_AVAILABLE_IN_3_34
gboolean               ide_pipeline_contains_program_in_path (IdePipeline            *self,
                                                              const gchar            *name,
                                                              GCancellable           *cancellable);

G_END_DECLS
