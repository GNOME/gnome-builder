/*
 * ide-runtime-provider.h
 *
 * Copyright 2016-2023 Christian Hergert <chergert@redhat.com>
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

#include "ide-pipeline.h"
#include "ide-runtime.h"

G_BEGIN_DECLS

#define IDE_TYPE_RUNTIME_PROVIDER (ide_runtime_provider_get_type())

IDE_AVAILABLE_IN_44
G_DECLARE_DERIVABLE_TYPE (IdeRuntimeProvider, ide_runtime_provider, IDE, RUNTIME_PROVIDER, IdeObject)

struct _IdeRuntimeProviderClass
{
  IdeObjectClass parent_class;

  DexFuture *(*load)              (IdeRuntimeProvider *self);
  DexFuture *(*unload)            (IdeRuntimeProvider *self);
  DexFuture *(*bootstrap_runtime) (IdeRuntimeProvider *self,
                                   IdePipeline        *pipeline);
  gboolean   (*provides)          (IdeRuntimeProvider *self,
                                   const char         *runtime_id);
};

IDE_AVAILABLE_IN_44
DexFuture *ide_runtime_provider_load              (IdeRuntimeProvider *self);
IDE_AVAILABLE_IN_44
DexFuture *ide_runtime_provider_unload            (IdeRuntimeProvider *self);
IDE_AVAILABLE_IN_44
DexFuture *ide_runtime_provider_bootstrap_runtime (IdeRuntimeProvider *self,
                                                   IdePipeline        *pipeline);
IDE_AVAILABLE_IN_44
void       ide_runtime_provider_add               (IdeRuntimeProvider *self,
                                                   IdeRuntime         *runtime);
IDE_AVAILABLE_IN_44
void       ide_runtime_provider_remove            (IdeRuntimeProvider *self,
                                                   IdeRuntime         *runtime);
IDE_AVAILABLE_IN_44
gboolean   ide_runtime_provider_provides          (IdeRuntimeProvider *self,
                                                   const char         *runtime_id);

G_END_DECLS
