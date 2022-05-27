/* ide-config-provider.h
 *
 * Copyright 2016 Matthew Leeds <mleeds@redhat.com>
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

#define IDE_TYPE_CONFIG_PROVIDER (ide_config_provider_get_type ())

IDE_AVAILABLE_IN_ALL
G_DECLARE_INTERFACE (IdeConfigProvider, ide_config_provider, IDE, CONFIG_PROVIDER, IdeObject)

struct _IdeConfigProviderInterface
{
  GTypeInterface parent_iface;

  void     (*added)          (IdeConfigProvider  *self,
                              IdeConfig          *config);
  void     (*removed)        (IdeConfigProvider  *self,
                              IdeConfig          *config);
  void     (*load_async)     (IdeConfigProvider  *self,
                              GCancellable              *cancellable,
                              GAsyncReadyCallback        callback,
                              gpointer                   user_data);
  gboolean (*load_finish)    (IdeConfigProvider  *self,
                              GAsyncResult              *result,
                              GError                   **error);
  void     (*save_async)     (IdeConfigProvider  *self,
                              GCancellable              *cancellable,
                              GAsyncReadyCallback        callback,
                              gpointer                   user_data);
  gboolean (*save_finish)    (IdeConfigProvider  *self,
                              GAsyncResult              *result,
                              GError                   **error);
  void     (*delete)         (IdeConfigProvider  *self,
                              IdeConfig          *config);
  void     (*duplicate)      (IdeConfigProvider  *self,
                              IdeConfig          *config);
  void     (*unload)         (IdeConfigProvider  *self);
};

IDE_AVAILABLE_IN_ALL
void     ide_config_provider_emit_added   (IdeConfigProvider    *self,
                                           IdeConfig            *config);
IDE_AVAILABLE_IN_ALL
void     ide_config_provider_emit_removed (IdeConfigProvider    *self,
                                           IdeConfig            *config);
IDE_AVAILABLE_IN_ALL
void     ide_config_provider_load_async   (IdeConfigProvider    *self,
                                           GCancellable         *cancellable,
                                           GAsyncReadyCallback   callback,
                                           gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gboolean ide_config_provider_load_finish  (IdeConfigProvider    *self,
                                           GAsyncResult         *result,
                                           GError              **error);
IDE_AVAILABLE_IN_ALL
void     ide_config_provider_save_async   (IdeConfigProvider    *self,
                                           GCancellable         *cancellable,
                                           GAsyncReadyCallback   callback,
                                           gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gboolean ide_config_provider_save_finish  (IdeConfigProvider    *self,
                                           GAsyncResult         *result,
                                           GError              **error);
IDE_AVAILABLE_IN_ALL
void     ide_config_provider_delete       (IdeConfigProvider    *self,
                                           IdeConfig            *config);
void     ide_config_provider_duplicate    (IdeConfigProvider    *self,
                                           IdeConfig            *config);
IDE_AVAILABLE_IN_ALL
void     ide_config_provider_unload       (IdeConfigProvider    *self);

G_END_DECLS
