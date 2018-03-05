/* ide-deploy-strategy.h
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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

#include "ide-object.h"

G_BEGIN_DECLS

#define IDE_TYPE_DEPLOY_STRATEGY (ide_deploy_strategy_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeDeployStrategy, ide_deploy_strategy, IDE, DEPLOY_STRATEGY, IdeObject)

struct _IdeDeployStrategyClass
{
  IdeObjectClass parent;

  void     (*load_async)    (IdeDeployStrategy     *self,
                             IdeBuildPipeline      *pipeline,
                             GCancellable          *cancellable,
                             GAsyncReadyCallback    callback,
                             gpointer               user_data);
  gboolean (*load_finish)   (IdeDeployStrategy     *self,
                             GAsyncResult          *result,
                             GError               **error);
  void     (*deploy_async)  (IdeDeployStrategy     *self,
                             IdeBuildPipeline      *pipeline,
                             GFileProgressCallback  progress,
                             gpointer               progress_data,
                             GDestroyNotify         progress_data_destroy,
                             GCancellable          *cancellable,
                             GAsyncReadyCallback    callback,
                             gpointer               user_data);
  gboolean (*deploy_finish) (IdeDeployStrategy     *self,
                             GAsyncResult          *result,
                             GError               **error);

  gpointer _reserved[16];
};

void     ide_deploy_strategy_load_async    (IdeDeployStrategy      *self,
                                            IdeBuildPipeline       *pipeline,
                                            GCancellable           *cancellable,
                                            GAsyncReadyCallback     callback,
                                            gpointer                user_data);
gboolean ide_deploy_strategy_load_finish   (IdeDeployStrategy      *self,
                                            GAsyncResult           *result,
                                            GError                **error);
void     ide_deploy_strategy_deploy_async  (IdeDeployStrategy      *self,
                                            IdeBuildPipeline       *pipeline,
                                            GFileProgressCallback   progress,
                                            gpointer                progress_data,
                                            GDestroyNotify          progress_data_destroy,
                                            GCancellable           *cancellable,
                                            GAsyncReadyCallback     callback,
                                            gpointer                user_data);
gboolean ide_deploy_strategy_deploy_finish (IdeDeployStrategy      *self,
                                            GAsyncResult           *result,
                                            GError                **error);

G_END_DECLS
