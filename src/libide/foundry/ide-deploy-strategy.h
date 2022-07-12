/* ide-deploy-strategy.h
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define IDE_TYPE_DEPLOY_STRATEGY (ide_deploy_strategy_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeDeployStrategy, ide_deploy_strategy, IDE, DEPLOY_STRATEGY, IdeObject)

struct _IdeDeployStrategyClass
{
  IdeObjectClass parent;

  void           (*load_async)           (IdeDeployStrategy      *self,
                                          IdePipeline            *pipeline,
                                          GCancellable           *cancellable,
                                          GAsyncReadyCallback     callback,
                                          gpointer                user_data);
  gboolean       (*load_finish)          (IdeDeployStrategy      *self,
                                          GAsyncResult           *result,
                                          int                    *priority,
                                          GError                **error);
  void           (*deploy_async)         (IdeDeployStrategy      *self,
                                          IdePipeline            *pipeline,
                                          GFileProgressCallback   progress,
                                          gpointer                progress_data,
                                          GDestroyNotify          progress_data_destroy,
                                          GCancellable           *cancellable,
                                          GAsyncReadyCallback     callback,
                                          gpointer                user_data);
  gboolean       (*deploy_finish)        (IdeDeployStrategy      *self,
                                          GAsyncResult           *result,
                                          GError                **error);
  void           (*prepare_run_context)  (IdeDeployStrategy      *self,
                                          IdePipeline            *pipeline,
                                          IdeRunContext          *run_context);
};

IDE_AVAILABLE_IN_ALL
void     ide_deploy_strategy_load_async          (IdeDeployStrategy      *self,
                                                  IdePipeline            *pipeline,
                                                  GCancellable           *cancellable,
                                                  GAsyncReadyCallback     callback,
                                                  gpointer                user_data);
IDE_AVAILABLE_IN_ALL
gboolean ide_deploy_strategy_load_finish         (IdeDeployStrategy      *self,
                                                  GAsyncResult           *result,
                                                  int                    *priority,
                                                  GError                **error);
IDE_AVAILABLE_IN_ALL
void     ide_deploy_strategy_deploy_async        (IdeDeployStrategy      *self,
                                                  IdePipeline            *pipeline,
                                                  GFileProgressCallback   progress,
                                                  gpointer                progress_data,
                                                  GDestroyNotify          progress_data_destroy,
                                                  GCancellable           *cancellable,
                                                  GAsyncReadyCallback     callback,
                                                  gpointer                user_data);
IDE_AVAILABLE_IN_ALL
gboolean ide_deploy_strategy_deploy_finish       (IdeDeployStrategy      *self,
                                                  GAsyncResult           *result,
                                                  GError                **error);
IDE_AVAILABLE_IN_ALL
void     ide_deploy_strategy_prepare_run_context (IdeDeployStrategy      *self,
                                                  IdePipeline            *pipeline,
                                                  IdeRunContext          *run_context);

G_END_DECLS
