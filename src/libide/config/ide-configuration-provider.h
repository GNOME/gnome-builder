/* ide-configuration-provider.h
 *
 * Copyright 2016 Matthew Leeds <mleeds@redhat.com>
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
 */

#pragma once

#include <gio/gio.h>

#include "ide-types.h"
#include "ide-version-macros.h"

G_BEGIN_DECLS

#define IDE_TYPE_CONFIGURATION_PROVIDER (ide_configuration_provider_get_type ())

G_DECLARE_INTERFACE (IdeConfigurationProvider, ide_configuration_provider, IDE, CONFIGURATION_PROVIDER, IdeObject)

struct _IdeConfigurationProviderInterface
{
  GTypeInterface parent_iface;

  void     (*added)          (IdeConfigurationProvider  *self,
                              IdeConfiguration          *config);
  void     (*removed)        (IdeConfigurationProvider  *self,
                              IdeConfiguration          *config);
  void     (*load_async)     (IdeConfigurationProvider  *self,
                              GCancellable              *cancellable,
                              GAsyncReadyCallback        callback,
                              gpointer                   user_data);
  gboolean (*load_finish)    (IdeConfigurationProvider  *self,
                              GAsyncResult              *result,
                              GError                   **error);
  void     (*save_async)     (IdeConfigurationProvider  *self,
                              GCancellable              *cancellable,
                              GAsyncReadyCallback        callback,
                              gpointer                   user_data);
  gboolean (*save_finish)    (IdeConfigurationProvider  *self,
                              GAsyncResult              *result,
                              GError                   **error);
  void     (*delete)         (IdeConfigurationProvider  *self,
                              IdeConfiguration          *config);
  void     (*duplicate)      (IdeConfigurationProvider  *self,
                              IdeConfiguration          *config);
  void     (*unload)         (IdeConfigurationProvider  *self);
};

IDE_AVAILABLE_IN_3_28
void     ide_configuration_provider_emit_added    (IdeConfigurationProvider  *self,
                                                   IdeConfiguration          *config);
IDE_AVAILABLE_IN_3_28
void     ide_configuration_provider_emit_removed  (IdeConfigurationProvider  *self,
                                                   IdeConfiguration          *config);
IDE_AVAILABLE_IN_3_28
void     ide_configuration_provider_load_async    (IdeConfigurationProvider  *self,
                                                   GCancellable              *cancellable,
                                                   GAsyncReadyCallback        callback,
                                                   gpointer                   user_data);
IDE_AVAILABLE_IN_3_28
gboolean ide_configuration_provider_load_finish   (IdeConfigurationProvider  *self,
                                                   GAsyncResult              *result,
                                                   GError                   **error);
IDE_AVAILABLE_IN_3_28
void     ide_configuration_provider_save_async    (IdeConfigurationProvider  *self,
                                                   GCancellable              *cancellable,
                                                   GAsyncReadyCallback        callback,
                                                   gpointer                   user_data);
IDE_AVAILABLE_IN_3_28
gboolean ide_configuration_provider_save_finish   (IdeConfigurationProvider  *self,
                                                   GAsyncResult              *result,
                                                   GError                   **error);
IDE_AVAILABLE_IN_3_28
void     ide_configuration_provider_delete        (IdeConfigurationProvider  *self,
                                                   IdeConfiguration          *config);
void     ide_configuration_provider_duplicate     (IdeConfigurationProvider  *self,
                                                   IdeConfiguration          *config);
IDE_AVAILABLE_IN_3_28
void     ide_configuration_provider_unload        (IdeConfigurationProvider  *self);

G_END_DECLS
