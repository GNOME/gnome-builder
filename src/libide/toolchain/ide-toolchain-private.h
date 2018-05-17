/* ide-toolchain-private.h
 *
 * Copyright 2018 Collabora Ltd.
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
 * Authors: Corentin Noël <corentin.noel@collabora.com>
 */

#pragma once

#include "buildsystem/ide-build-pipeline.h"
#include "toolchain/ide-toolchain-manager.h"

G_BEGIN_DECLS

void     _ide_toolchain_manager_prepare_async  (IdeToolchainManager  *self,
                                                IdeBuildPipeline     *pipeline,
                                                GCancellable         *cancellable,
                                                GAsyncReadyCallback   callback,
                                                gpointer              user_data);
gboolean _ide_toolchain_manager_prepare_finish (IdeToolchainManager  *self,
                                                GAsyncResult         *result,
                                                GError              **error);

G_END_DECLS
