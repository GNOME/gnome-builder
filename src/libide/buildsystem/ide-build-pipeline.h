/* ide-build-pipeline.h
 *
 * Copyright © 2016 Christian Hergert <chergert@redhat.com>
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
#include <vte/vte.h>

#include "ide-types.h"
#include "ide-version-macros.h"

#include "buildsystem/ide-build-log.h"
#include "buildsystem/ide-build-stage.h"
#include "config/ide-configuration.h"
#include "runtimes/ide-runtime.h"
#include "subprocess/ide-subprocess-launcher.h"

G_BEGIN_DECLS

#define IDE_TYPE_BUILD_PIPELINE     (ide_build_pipeline_get_type())
#define IDE_BUILD_PHASE_MASK        (0xFFFFFF)
#define IDE_BUILD_PHASE_WHENCE_MASK (IDE_BUILD_PHASE_BEFORE | IDE_BUILD_PHASE_AFTER)

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

G_DECLARE_FINAL_TYPE (IdeBuildPipeline, ide_build_pipeline, IDE, BUILD_PIPELINE, IdeObject)

IDE_AVAILABLE_IN_3_28
gboolean               ide_build_pipeline_is_ready            (IdeBuildPipeline       *self);
IDE_AVAILABLE_IN_ALL
gboolean               ide_build_pipeline_get_busy            (IdeBuildPipeline       *self);
IDE_AVAILABLE_IN_ALL
IdeConfiguration      *ide_build_pipeline_get_configuration   (IdeBuildPipeline       *self);
IDE_AVAILABLE_IN_3_28
IdeDevice             *ide_build_pipeline_get_device          (IdeBuildPipeline       *self);
IDE_AVAILABLE_IN_3_28
IdeRuntime            *ide_build_pipeline_get_runtime         (IdeBuildPipeline       *self);
IDE_AVAILABLE_IN_ALL
const gchar           *ide_build_pipeline_get_builddir        (IdeBuildPipeline       *self);
IDE_AVAILABLE_IN_ALL
const gchar           *ide_build_pipeline_get_srcdir          (IdeBuildPipeline       *self);
IDE_AVAILABLE_IN_3_28
const gchar           *ide_build_pipeline_get_arch            (IdeBuildPipeline       *self);
IDE_AVAILABLE_IN_3_28
const gchar           *ide_build_pipeline_get_kernel          (IdeBuildPipeline       *self);
IDE_AVAILABLE_IN_3_28
const gchar           *ide_build_pipeline_get_system          (IdeBuildPipeline       *self);
IDE_AVAILABLE_IN_3_28
const gchar           *ide_build_pipeline_get_system_type     (IdeBuildPipeline       *self);
IDE_AVAILABLE_IN_ALL
gchar                 *ide_build_pipeline_get_message         (IdeBuildPipeline       *self);
IDE_AVAILABLE_IN_ALL
IdeBuildPhase          ide_build_pipeline_get_phase           (IdeBuildPipeline       *self);
IDE_AVAILABLE_IN_ALL
gboolean               ide_build_pipeline_get_can_export      (IdeBuildPipeline       *self);
IDE_AVAILABLE_IN_3_28
VtePty                *ide_build_pipeline_get_pty             (IdeBuildPipeline       *self);
IDE_AVAILABLE_IN_ALL
IdeSubprocessLauncher *ide_build_pipeline_create_launcher     (IdeBuildPipeline       *self,
                                                               GError                **error);
IDE_AVAILABLE_IN_ALL
gchar                 *ide_build_pipeline_build_srcdir_path   (IdeBuildPipeline       *self,
                                                               const gchar            *first_part,
                                                               ...) G_GNUC_NULL_TERMINATED;
IDE_AVAILABLE_IN_ALL
gchar                 *ide_build_pipeline_build_builddir_path (IdeBuildPipeline       *self,
                                                               const gchar            *first_part,
                                                               ...) G_GNUC_NULL_TERMINATED;
IDE_AVAILABLE_IN_ALL
void                   ide_build_pipeline_invalidate_phase    (IdeBuildPipeline       *self,
                                                               IdeBuildPhase           phases);
IDE_AVAILABLE_IN_ALL
gboolean               ide_build_pipeline_request_phase       (IdeBuildPipeline       *self,
                                                               IdeBuildPhase           phase);
IDE_AVAILABLE_IN_ALL
guint                  ide_build_pipeline_connect             (IdeBuildPipeline       *self,
                                                               IdeBuildPhase           phase,
                                                               gint                    priority,
                                                               IdeBuildStage          *stage);
IDE_AVAILABLE_IN_ALL
guint                  ide_build_pipeline_connect_launcher    (IdeBuildPipeline       *self,
                                                               IdeBuildPhase           phase,
                                                               gint                    priority,
                                                               IdeSubprocessLauncher  *launcher);
IDE_AVAILABLE_IN_ALL
void                   ide_build_pipeline_disconnect          (IdeBuildPipeline       *self,
                                                               guint                   stage_id);
IDE_AVAILABLE_IN_ALL
IdeBuildStage         *ide_build_pipeline_get_stage_by_id     (IdeBuildPipeline       *self,
                                                               guint                   stage_id);
IDE_AVAILABLE_IN_ALL
guint                  ide_build_pipeline_add_log_observer    (IdeBuildPipeline       *self,
                                                               IdeBuildLogObserver     observer,
                                                               gpointer                observer_data,
                                                               GDestroyNotify          observer_data_destroy);
IDE_AVAILABLE_IN_ALL
gboolean               ide_build_pipeline_remove_log_observer (IdeBuildPipeline       *self,
                                                               guint                   observer_id);
IDE_AVAILABLE_IN_ALL
void                   ide_build_pipeline_emit_diagnostic     (IdeBuildPipeline       *self,
                                                               IdeDiagnostic          *diagnostic);
IDE_AVAILABLE_IN_ALL
guint                  ide_build_pipeline_add_error_format    (IdeBuildPipeline       *self,
                                                               const gchar            *regex,
                                                               GRegexCompileFlags      flags);
IDE_AVAILABLE_IN_ALL
gboolean               ide_build_pipeline_remove_error_format (IdeBuildPipeline       *self,
                                                               guint                   error_format_id);
IDE_AVAILABLE_IN_3_28
void                   ide_build_pipeline_build_async         (IdeBuildPipeline       *self,
                                                               IdeBuildPhase           phase,
                                                               GCancellable           *cancellable,
                                                               GAsyncReadyCallback     callback,
                                                               gpointer                user_data);
IDE_AVAILABLE_IN_3_28
gboolean               ide_build_pipeline_build_finish        (IdeBuildPipeline       *self,
                                                               GAsyncResult           *result,
                                                               GError                **error);
IDE_AVAILABLE_IN_ALL
void                   ide_build_pipeline_execute_async       (IdeBuildPipeline       *self,
                                                               GCancellable           *cancellable,
                                                               GAsyncReadyCallback     callback,
                                                               gpointer                user_data);
IDE_AVAILABLE_IN_ALL
gboolean               ide_build_pipeline_execute_finish      (IdeBuildPipeline       *self,
                                                               GAsyncResult           *result,
                                                               GError                **error);
IDE_AVAILABLE_IN_ALL
void                   ide_build_pipeline_foreach_stage       (IdeBuildPipeline       *self,
                                                               GFunc                   stage_callback,
                                                               gpointer                user_data);
IDE_AVAILABLE_IN_ALL
void                   ide_build_pipeline_clean_async         (IdeBuildPipeline       *self,
                                                               IdeBuildPhase           phase,
                                                               GCancellable           *cancellable,
                                                               GAsyncReadyCallback     callback,
                                                               gpointer                user_data);
IDE_AVAILABLE_IN_ALL
gboolean               ide_build_pipeline_clean_finish        (IdeBuildPipeline       *self,
                                                               GAsyncResult           *result,
                                                               GError                **error);
IDE_AVAILABLE_IN_ALL
void                   ide_build_pipeline_rebuild_async       (IdeBuildPipeline       *self,
                                                               IdeBuildPhase           phase,
                                                               GCancellable           *cancellable,
                                                               GAsyncReadyCallback     callback,
                                                               gpointer                user_data);
IDE_AVAILABLE_IN_ALL
gboolean               ide_build_pipeline_rebuild_finish      (IdeBuildPipeline       *self,
                                                               GAsyncResult           *result,
                                                               GError                **error);
IDE_AVAILABLE_IN_3_28
void                   ide_build_pipeline_attach_pty          (IdeBuildPipeline       *self,
                                                               IdeSubprocessLauncher  *launcher);
IDE_AVAILABLE_IN_3_28
gboolean               ide_build_pipeline_has_configured      (IdeBuildPipeline       *self);
IDE_AVAILABLE_IN_3_28
IdeBuildPhase          ide_build_pipeline_get_requested_phase (IdeBuildPipeline       *self);
IDE_AVAILABLE_IN_3_28
gboolean               ide_build_pipeline_is_native           (IdeBuildPipeline       *self);

G_END_DECLS
