/* ide-pipeline-stage.h
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
#include <libide-io.h>

#include "ide-build-log.h"
#include "ide-foundry-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_PIPELINE_STAGE (ide_pipeline_stage_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdePipelineStage, ide_pipeline_stage, IDE, PIPELINE_STAGE, IdeObject)

struct _IdePipelineStageClass
{
  IdeObjectClass parent_class;

  /**
   * IdePipelineStage::build:
   *
   * This vfunc will be run in a thread by the default
   * IdePipelineStage::build_async() and IdePipelineStage::build_finish()
   * vfuncs.
   *
   * Only use thread-safe API from this function.
   */
  gboolean (*build)          (IdePipelineStage     *self,
                              IdePipeline          *pipeline,
                              GCancellable         *cancellable,
                              GError              **error);

  /**
   * IdePipelineStage::build_async:
   *
   * Asynchronous version of the #IdePipelineStage API. This is the preferred
   * way to subclass #IdePipelineStage.
   */
  void     (*build_async)    (IdePipelineStage     *self,
                              IdePipeline          *pipeline,
                              GCancellable         *cancellable,
                              GAsyncReadyCallback   callback,
                              gpointer              user_data);

  /**
   * IdePipelineStage::build_finish:
   *
   * Completes an asynchronous call to ide_pipeline_stage_build_async().
   *
   * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
   *   Upon failure, the pipeline will be stopped.
   */
  gboolean (*build_finish)   (IdePipelineStage     *self,
                              GAsyncResult         *result,
                              GError              **error);

  /**
   * IdePipelineStage::clean_async:
   * @self: an #IdePipelineStage
   * @pipeline: An #IdePipeline
   * @cancellable: (nullable): a #GCancellable or %NULL
   * @callback: An async callback
   * @user_data: user data for @callback
   *
   * This function will perform the clean operation.
   */
  void     (*clean_async)    (IdePipelineStage     *self,
                              IdePipeline          *pipeline,
                              GCancellable         *cancellable,
                              GAsyncReadyCallback   callback,
                              gpointer              user_data);

  /**
   * IdePipelineStage::clean_finish:
   * @self: an #IdePipelineStage
   * @result: a #GErrorResult
   * @error: A location for a #GError or %NULL.
   *
   * Completes an async operation to ide_pipeline_stage_clean_async().
   *
   * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
   */
  gboolean (*clean_finish)   (IdePipelineStage     *self,
                              GAsyncResult         *result,
                              GError              **error);

  /* Signals */
  void     (*query)          (IdePipelineStage     *self,
                              IdePipeline          *pipeline,
                              GPtrArray            *targets,
                              GCancellable         *cancellable);
  void     (*reap)           (IdePipelineStage     *self,
                              IdeDirectoryReaper   *reaper);
  gboolean (*chain)          (IdePipelineStage     *self,
                              IdePipelineStage     *next);
};

IDE_AVAILABLE_IN_ALL
gboolean     ide_pipeline_stage_get_active       (IdePipelineStage     *self);
IDE_AVAILABLE_IN_ALL
void         ide_pipeline_stage_set_active       (IdePipelineStage     *self,
                                                  gboolean              active);
IDE_AVAILABLE_IN_ALL
const gchar *ide_pipeline_stage_get_name         (IdePipelineStage     *self);
IDE_AVAILABLE_IN_ALL
void         ide_pipeline_stage_set_name         (IdePipelineStage     *self,
                                                  const gchar          *name);
IDE_AVAILABLE_IN_ALL
void         ide_pipeline_stage_log              (IdePipelineStage     *self,
                                                  IdeBuildLogStream     stream,
                                                  const gchar          *message,
                                                  gssize                message_len);
IDE_AVAILABLE_IN_ALL
void         ide_pipeline_stage_log_subprocess   (IdePipelineStage     *self,
                                                  IdeSubprocess        *subprocess);
IDE_AVAILABLE_IN_ALL
void         ide_pipeline_stage_set_log_observer (IdePipelineStage     *self,
                                                  IdeBuildLogObserver   observer,
                                                  gpointer              observer_data,
                                                  GDestroyNotify        observer_data_destroy);
IDE_AVAILABLE_IN_ALL
void         ide_pipeline_stage_set_stdout_path  (IdePipelineStage     *self,
                                                  const gchar          *path);
IDE_AVAILABLE_IN_ALL
const gchar *ide_pipeline_stage_get_stdout_path  (IdePipelineStage     *self);
IDE_AVAILABLE_IN_ALL
gboolean     ide_pipeline_stage_get_completed    (IdePipelineStage     *self);
IDE_AVAILABLE_IN_ALL
void         ide_pipeline_stage_set_completed    (IdePipelineStage     *self,
                                                  gboolean              completed);
IDE_AVAILABLE_IN_ALL
gboolean     ide_pipeline_stage_get_disabled     (IdePipelineStage     *self);
IDE_AVAILABLE_IN_ALL
void         ide_pipeline_stage_set_disabled     (IdePipelineStage     *self,
                                                  gboolean              disabled);
IDE_AVAILABLE_IN_ALL
gboolean     ide_pipeline_stage_get_check_stdout (IdePipelineStage     *self);
IDE_AVAILABLE_IN_ALL
void         ide_pipeline_stage_set_check_stdout (IdePipelineStage     *self,
                                                  gboolean              check_stdout);
IDE_AVAILABLE_IN_ALL
gboolean     ide_pipeline_stage_get_transient    (IdePipelineStage     *self);
IDE_AVAILABLE_IN_ALL
void         ide_pipeline_stage_set_transient    (IdePipelineStage     *self,
                                                  gboolean              transient);
IDE_AVAILABLE_IN_ALL
void         ide_pipeline_stage_build_async      (IdePipelineStage     *self,
                                                  IdePipeline          *pipeline,
                                                  GCancellable         *cancellable,
                                                  GAsyncReadyCallback   callback,
                                                  gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gboolean     ide_pipeline_stage_build_finish     (IdePipelineStage     *self,
                                                  GAsyncResult         *result,
                                                  GError              **error);
IDE_AVAILABLE_IN_ALL
void         ide_pipeline_stage_clean_async      (IdePipelineStage     *self,
                                                  IdePipeline          *pipeline,
                                                  GCancellable         *cancellable,
                                                  GAsyncReadyCallback   callback,
                                                  gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gboolean     ide_pipeline_stage_clean_finish     (IdePipelineStage     *self,
                                                  GAsyncResult         *result,
                                                  GError              **error);
IDE_AVAILABLE_IN_ALL
gboolean     ide_pipeline_stage_chain            (IdePipelineStage     *self,
                                                  IdePipelineStage     *next);
IDE_AVAILABLE_IN_ALL
void         ide_pipeline_stage_pause            (IdePipelineStage     *self);
IDE_AVAILABLE_IN_ALL
void         ide_pipeline_stage_unpause          (IdePipelineStage     *self);
IDE_AVAILABLE_IN_ALL
void         ide_pipeline_stage_emit_reap        (IdePipelineStage     *self,
                                                  IdeDirectoryReaper   *reaper);

G_END_DECLS
