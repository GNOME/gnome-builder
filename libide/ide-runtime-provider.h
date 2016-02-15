/* ide-runtime-provider.h
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#ifndef IDE_RUNTIME_PROVIDER_H
#define IDE_RUNTIME_PROVIDER_H

#include <gio/gio.h>

#include "ide-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_RUNTIME_PROVIDER (ide_runtime_provider_get_type ())

G_DECLARE_INTERFACE (IdeRuntimeProvider, ide_runtime_provider, IDE, RUNTIME_PROVIDER, GObject)

struct _IdeRuntimeProviderInterface
{
  GTypeInterface parent;

  void   (*load)         (IdeRuntimeProvider *self,
                          IdeRuntimeManager  *manager);
  void   (*unload)       (IdeRuntimeProvider *self,
                          IdeRuntimeManager  *manager);
};

void ide_runtime_provider_load   (IdeRuntimeProvider *self,
                                  IdeRuntimeManager  *manager);
void ide_runtime_provider_unload (IdeRuntimeProvider *self,
                                  IdeRuntimeManager  *manager);

G_END_DECLS

#endif /* IDE_RUNTIME_PROVIDER_H */
