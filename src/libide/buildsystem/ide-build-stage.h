/* ide-build-stage.h
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

#include <dazzle.h>
#include <gio/gio.h>

#include "ide-version-macros.h"

#include "ide-types.h"
#include "ide-object.h"

#include "buildsystem/ide-build-log.h"

G_BEGIN_DECLS

#define IDE_TYPE_BUILD_STAGE (ide_build_stage_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeBuildStage, ide_build_stage, IDE, BUILD_STAGE, IdeObject)

struct _IdeBuildStageClass
{
  IdeObjectClass parent_class;

  /**
   * IdeBuildStage::execute:
   *
   * This vfunc will be run in a thread by the default
   * IdeBuildStage::execute_async() and IdeBuildStage::execute_finish()
   * vfuncs.
   *
   * Only use thread-safe API from this function.
   */
  gboolean (*execute)        (IdeBuildStage        *self,
                              IdeBuildPipeline     *pipeline,
                              GCancellable         *cancellable,
                              GError              **error);

  /**
   * IdeBuildStage::execute_async:
   *
   * Asynchronous version of the #IdeBuildStage API. This is the preferred
   * way to subclass #IdeBuildStage.
   */
  void     (*execute_async)  (IdeBuildStage        *self,
                              IdeBuildPipeline     *pipeline,
                              GCancellable         *cancellable,
                              GAsyncReadyCallback   callback,
                              gpointer              user_data);

  /**
   * IdeBuildStage::execute_finish:
   *
   * Completes an asynchronous call to ide_build_stage_execute_async().
   *
   * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
   *   Upon failure, the pipeline will be stopped.
   */
  gboolean (*execute_finish) (IdeBuildStage        *self,
                              GAsyncResult         *result,
                              GError              **error);

  /**
   * IdeBuildStage::clean_async:
   * @self: an #IdeBuildStage
   * @pipeline: An #IdeBuildPipeline
   * @cancellable: (nullable): a #GCancellable or %NULL
   * @callback: An async callback
   * @user_data: user data for @callback
   *
   * This function will perform the clean operation.
   */
  void     (*clean_async)    (IdeBuildStage        *self,
                              IdeBuildPipeline     *pipeline,
                              GCancellable         *cancellable,
                              GAsyncReadyCallback   callback,
                              gpointer              user_data);

  /**
   * IdeBuildStage::clean_finish:
   * @self: an #IdeBuildStage
   * @result: a #GErrorResult
   * @error: A location for a #GError or %NULL.
   *
   * Completes an async operation to ide_build_stage_clean_async().
   *
   * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
   */
  gboolean (*clean_finish)   (IdeBuildStage        *self,
                              GAsyncResult         *result,
                              GError              **error);

  /**
   * IdeBuildStage::query:
   * @self: An #IdeBuildStage
   * @pipeline: An #IdeBuildPipeline
   * @cancellable: (nullable): a #GCancellable or %NULL
   *
   * The #IdeBuildStage::query signal is emitted to request that the
   * build stage update its completed stage from any external resources.
   *
   * This can be useful if you want to use an existing build stage instances
   * and use a signal to pause forward progress until an external system
   * has been checked.
   *
   * For example, in a signal handler, you may call ide_build_stage_pause()
   * and perform an external operation. Forward progress of the stage will
   * be paused until a matching number of ide_build_stage_unpause() calls
   * have been made.
   */
  void     (*query)          (IdeBuildStage        *self,
                              IdeBuildPipeline     *pipeline,
                              GCancellable         *cancellable);

  /**
   * IdeBuildStage::reap:
   * @self: An #IdeBuildStage
   * @reaper: An #DzlDirectoryReaper
   *
   * This signal is emitted when a request to rebuild the project has
   * occurred. This allows build stages to ensure that certain files are
   * removed from the system. For example, an autotools build stage might
   * request that "configure" is removed so that autogen.sh will be executed
   * as part of the next build.
   */
  void     (*reap)           (IdeBuildStage        *self,
                              DzlDirectoryReaper   *reaper);


  /**
   * IdeBuildStage:chain:
   *
   * We might want to be able to "chain" multiple stages into a single stage
   * so that we can avoid duplicate work. For example, if we have a "make"
   * stage immediately follwed by a "make install" stage, it does not make
   * sense to perform them both individually.
   *
   * Returns: %TRUE if @next's work was chained into @self for the next
   *    execution of the pipeline.
   */
  gboolean (*chain)          (IdeBuildStage        *self,
                              IdeBuildStage        *next);

  /*< private >*/
  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
  gpointer _reserved5;
  gpointer _reserved6;
  gpointer _reserved7;
  gpointer _reserved8;
  gpointer _reserved9;
  gpointer _reserved10;
  gpointer _reserved11;
  gpointer _reserved12;
};

IDE_AVAILABLE_IN_3_28
gboolean       ide_build_stage_get_active       (IdeBuildStage        *self);
IDE_AVAILABLE_IN_3_28
void           ide_build_stage_set_active       (IdeBuildStage        *self,
                                                 gboolean              active);
IDE_AVAILABLE_IN_ALL
const gchar   *ide_build_stage_get_name         (IdeBuildStage        *self);
IDE_AVAILABLE_IN_ALL
void           ide_build_stage_set_name         (IdeBuildStage        *self,
                                                 const gchar          *name);
IDE_AVAILABLE_IN_ALL
void           ide_build_stage_log              (IdeBuildStage        *self,
                                                 IdeBuildLogStream     stream,
                                                 const gchar          *message,
                                                 gssize                message_len);
IDE_AVAILABLE_IN_ALL
void           ide_build_stage_log_subprocess   (IdeBuildStage        *self,
                                                 IdeSubprocess        *subprocess);
IDE_AVAILABLE_IN_ALL
void           ide_build_stage_set_log_observer (IdeBuildStage        *self,
                                                 IdeBuildLogObserver   observer,
                                                 gpointer              observer_data,
                                                 GDestroyNotify        observer_data_destroy);
IDE_AVAILABLE_IN_ALL
void           ide_build_stage_set_stdout_path  (IdeBuildStage        *self,
                                                 const gchar          *path);
IDE_AVAILABLE_IN_ALL
const gchar   *ide_build_stage_get_stdout_path  (IdeBuildStage        *self);
IDE_AVAILABLE_IN_ALL
gboolean       ide_build_stage_get_completed    (IdeBuildStage        *self);
IDE_AVAILABLE_IN_ALL
void           ide_build_stage_set_completed    (IdeBuildStage        *self,
                                                 gboolean              completed);
IDE_AVAILABLE_IN_ALL
gboolean       ide_build_stage_get_disabled     (IdeBuildStage        *self);
IDE_AVAILABLE_IN_ALL
void           ide_build_stage_set_disabled     (IdeBuildStage        *self,
                                                 gboolean              disabled);
IDE_AVAILABLE_IN_ALL
gboolean       ide_build_stage_get_check_stdout (IdeBuildStage        *self);
IDE_AVAILABLE_IN_ALL
void           ide_build_stage_set_check_stdout (IdeBuildStage        *self,
                                                 gboolean              check_stdout);
IDE_AVAILABLE_IN_ALL
gboolean       ide_build_stage_get_transient    (IdeBuildStage        *self);
IDE_AVAILABLE_IN_ALL
void           ide_build_stage_set_transient    (IdeBuildStage        *self,
                                                 gboolean              transient);
IDE_AVAILABLE_IN_ALL
void           ide_build_stage_execute_async    (IdeBuildStage        *self,
                                                 IdeBuildPipeline     *pipeline,
                                                 GCancellable         *cancellable,
                                                 GAsyncReadyCallback   callback,
                                                 gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gboolean       ide_build_stage_execute_finish   (IdeBuildStage        *self,
                                                 GAsyncResult         *result,
                                                 GError              **error);
IDE_AVAILABLE_IN_ALL
void           ide_build_stage_clean_async      (IdeBuildStage        *self,
                                                 IdeBuildPipeline     *pipeline,
                                                 GCancellable         *cancellable,
                                                 GAsyncReadyCallback   callback,
                                                 gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gboolean       ide_build_stage_clean_finish     (IdeBuildStage        *self,
                                                 GAsyncResult         *result,
                                                 GError              **error);
IDE_AVAILABLE_IN_ALL
gboolean       ide_build_stage_chain            (IdeBuildStage        *self,
                                                 IdeBuildStage        *next);
IDE_AVAILABLE_IN_ALL
void           ide_build_stage_pause            (IdeBuildStage        *self);
IDE_AVAILABLE_IN_ALL
void           ide_build_stage_unpause          (IdeBuildStage        *self);
IDE_AVAILABLE_IN_ALL
void           ide_build_stage_emit_reap        (IdeBuildStage        *self,
                                                 DzlDirectoryReaper   *reaper);

G_END_DECLS
