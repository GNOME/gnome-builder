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

#include <vte/vte.h>

#include <libide-core.h>
#include <libide-code.h>
#include <libide-threading.h>

#include "ide-build-log.h"
#include "ide-foundry-types.h"
#include "ide-pipeline-phase.h"

G_BEGIN_DECLS

#define IDE_TYPE_PIPELINE              (ide_pipeline_get_type())
#define IDE_PIPELINE_PHASE_MASK        (0xFFFFFF)
#define IDE_PIPELINE_PHASE_WHENCE_MASK (IDE_PIPELINE_PHASE_BEFORE | IDE_PIPELINE_PHASE_AFTER)
#define IDE_BUILD_ERROR                (ide_build_error_quark())

typedef enum
{
  IDE_BUILD_ERROR_UNKNOWN = 0,
  IDE_BUILD_ERROR_BROKEN,
  IDE_BUILD_ERROR_NOT_LOADED,
  IDE_BUILD_ERROR_NEEDS_REBUILD,
} IdeBuildError;

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdePipeline, ide_pipeline, IDE, PIPELINE, IdeObject)

IDE_AVAILABLE_IN_ALL
GQuark                 ide_build_error_quark                 (void) G_GNUC_CONST;
IDE_AVAILABLE_IN_44
char                  *ide_pipeline_dup_arch                 (IdePipeline            *self);
IDE_AVAILABLE_IN_ALL
gboolean               ide_pipeline_is_native                (IdePipeline            *self);
IDE_AVAILABLE_IN_ALL
gboolean               ide_pipeline_is_ready                 (IdePipeline            *self);
IDE_AVAILABLE_IN_ALL
gboolean               ide_pipeline_get_busy                 (IdePipeline            *self);
IDE_AVAILABLE_IN_ALL
IdeConfig             *ide_pipeline_get_config               (IdePipeline            *self);
IDE_AVAILABLE_IN_ALL
IdeDevice             *ide_pipeline_get_device               (IdePipeline            *self);
IDE_AVAILABLE_IN_ALL
IdeDeviceInfo         *ide_pipeline_get_device_info          (IdePipeline            *self);
IDE_AVAILABLE_IN_ALL
IdeTriplet            *ide_pipeline_get_host_triplet         (IdePipeline            *self);
IDE_AVAILABLE_IN_ALL
IdeRuntime            *ide_pipeline_get_runtime              (IdePipeline            *self);
IDE_AVAILABLE_IN_ALL
IdeToolchain          *ide_pipeline_get_toolchain            (IdePipeline            *self);
IDE_AVAILABLE_IN_ALL
IdeToolchain          *ide_pipeline_ref_toolchain            (IdePipeline            *self);
IDE_AVAILABLE_IN_ALL
const gchar           *ide_pipeline_get_builddir             (IdePipeline            *self);
IDE_AVAILABLE_IN_ALL
const gchar           *ide_pipeline_get_srcdir               (IdePipeline            *self);
IDE_AVAILABLE_IN_ALL
gchar                 *ide_pipeline_get_message              (IdePipeline            *self);
IDE_AVAILABLE_IN_ALL
IdePipelinePhase       ide_pipeline_get_phase                (IdePipeline            *self);
IDE_AVAILABLE_IN_ALL
gboolean               ide_pipeline_get_can_export           (IdePipeline            *self);
IDE_AVAILABLE_IN_ALL
VtePty                *ide_pipeline_get_pty                  (IdePipeline            *self);
IDE_AVAILABLE_IN_ALL
IdeSubprocessLauncher *ide_pipeline_create_launcher          (IdePipeline            *self,
                                                              GError                **error);
IDE_AVAILABLE_IN_ALL
IdeRunContext         *ide_pipeline_create_run_context       (IdePipeline            *self,
                                                              IdeRunCommand          *run_command);
IDE_AVAILABLE_IN_ALL
void                   ide_pipeline_prepare_run_context      (IdePipeline            *self,
                                                              IdeRunContext          *run_context);
IDE_AVAILABLE_IN_ALL
gchar                 *ide_pipeline_build_srcdir_path        (IdePipeline            *self,
                                                              const gchar            *first_part,
                                                              ...) G_GNUC_NULL_TERMINATED;
IDE_AVAILABLE_IN_ALL
gchar                 *ide_pipeline_build_builddir_path      (IdePipeline            *self,
                                                              const gchar            *first_part,
                                                              ...) G_GNUC_NULL_TERMINATED;
IDE_AVAILABLE_IN_ALL
void                   ide_pipeline_invalidate_phase         (IdePipeline            *self,
                                                              IdePipelinePhase        phases);
IDE_AVAILABLE_IN_ALL
gboolean               ide_pipeline_request_phase            (IdePipeline            *self,
                                                              IdePipelinePhase        phase);
IDE_AVAILABLE_IN_ALL
guint                  ide_pipeline_attach                   (IdePipeline            *self,
                                                              IdePipelinePhase        phase,
                                                              gint                    priority,
                                                              IdePipelineStage       *stage);
IDE_AVAILABLE_IN_ALL
guint                  ide_pipeline_attach_launcher          (IdePipeline            *self,
                                                              IdePipelinePhase        phase,
                                                              gint                    priority,
                                                              IdeSubprocessLauncher  *launcher);
IDE_AVAILABLE_IN_ALL
guint                  ide_pipeline_attach_command           (IdePipeline            *self,
                                                              IdePipelinePhase        phase,
                                                              gint                    priority,
                                                              IdeRunCommand          *run_command);
IDE_AVAILABLE_IN_ALL
void                   ide_pipeline_detach                   (IdePipeline            *self,
                                                              guint                   stage_id);
IDE_AVAILABLE_IN_ALL
IdePipelineStage      *ide_pipeline_get_stage_by_id          (IdePipeline            *self,
                                                              guint                   stage_id);
IDE_AVAILABLE_IN_ALL
guint                  ide_pipeline_add_log_observer         (IdePipeline            *self,
                                                              IdeBuildLogObserver     observer,
                                                              gpointer                observer_data,
                                                              GDestroyNotify          observer_data_destroy);
IDE_AVAILABLE_IN_ALL
gboolean               ide_pipeline_remove_log_observer      (IdePipeline            *self,
                                                              guint                   observer_id);
IDE_AVAILABLE_IN_ALL
void                   ide_pipeline_emit_diagnostic          (IdePipeline            *self,
                                                              IdeDiagnostic          *diagnostic);
IDE_AVAILABLE_IN_ALL
guint                  ide_pipeline_add_error_format         (IdePipeline            *self,
                                                              const gchar            *regex,
                                                              GRegexCompileFlags      flags);
IDE_AVAILABLE_IN_ALL
gboolean               ide_pipeline_remove_error_format      (IdePipeline            *self,
                                                              guint                   error_format_id);
IDE_AVAILABLE_IN_ALL
void                   ide_pipeline_build_async              (IdePipeline            *self,
                                                              IdePipelinePhase        phase,
                                                              GCancellable           *cancellable,
                                                              GAsyncReadyCallback     callback,
                                                              gpointer                user_data);
IDE_AVAILABLE_IN_ALL
gboolean               ide_pipeline_build_finish             (IdePipeline            *self,
                                                              GAsyncResult           *result,
                                                              GError                **error);
IDE_AVAILABLE_IN_ALL
void                   ide_pipeline_build_targets_async      (IdePipeline            *self,
                                                              IdePipelinePhase        phase,
                                                              GPtrArray              *targets,
                                                              GCancellable           *cancellable,
                                                              GAsyncReadyCallback     callback,
                                                              gpointer                user_data);
IDE_AVAILABLE_IN_ALL
gboolean               ide_pipeline_build_targets_finish     (IdePipeline            *self,
                                                              GAsyncResult           *result,
                                                              GError                **error);
IDE_AVAILABLE_IN_ALL
void                   ide_pipeline_foreach_stage            (IdePipeline            *self,
                                                              GFunc                   stage_callback,
                                                              gpointer                user_data);
IDE_AVAILABLE_IN_ALL
void                   ide_pipeline_clean_async              (IdePipeline            *self,
                                                              IdePipelinePhase        phase,
                                                              GCancellable           *cancellable,
                                                              GAsyncReadyCallback     callback,
                                                              gpointer                user_data);
IDE_AVAILABLE_IN_ALL
gboolean               ide_pipeline_clean_finish             (IdePipeline            *self,
                                                              GAsyncResult           *result,
                                                              GError                **error);
IDE_AVAILABLE_IN_ALL
void                   ide_pipeline_rebuild_async            (IdePipeline            *self,
                                                              IdePipelinePhase        phase,
                                                              GPtrArray              *targets,
                                                              GCancellable           *cancellable,
                                                              GAsyncReadyCallback     callback,
                                                              gpointer                user_data);
IDE_AVAILABLE_IN_ALL
gboolean               ide_pipeline_rebuild_finish           (IdePipeline            *self,
                                                              GAsyncResult           *result,
                                                              GError                **error);
IDE_AVAILABLE_IN_ALL
void                   ide_pipeline_attach_pty               (IdePipeline            *self,
                                                              IdeSubprocessLauncher  *launcher);
IDE_AVAILABLE_IN_ALL
gboolean               ide_pipeline_has_configured           (IdePipeline            *self);
IDE_AVAILABLE_IN_ALL
IdePipelinePhase       ide_pipeline_get_requested_phase      (IdePipeline            *self);
IDE_AVAILABLE_IN_ALL
gboolean               ide_pipeline_contains_program_in_path (IdePipeline            *self,
                                                              const gchar            *name,
                                                              GCancellable           *cancellable);
IDE_AVAILABLE_IN_ALL
IdeDeployStrategy     *ide_pipeline_get_deploy_strategy      (IdePipeline            *self);

G_END_DECLS
