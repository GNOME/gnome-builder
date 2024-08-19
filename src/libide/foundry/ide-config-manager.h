/* ide-config-manager.h
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

#include "ide-foundry-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_CONFIG_MANAGER (ide_config_manager_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeConfigManager, ide_config_manager, IDE, CONFIG_MANAGER, IdeObject)

IDE_AVAILABLE_IN_ALL
IdeConfigManager *ide_config_manager_from_context     (IdeContext           *context);
IDE_AVAILABLE_IN_ALL
IdeConfigManager *ide_config_manager_ref_from_context (IdeContext           *context);
IDE_AVAILABLE_IN_ALL
IdeConfig        *ide_config_manager_get_current      (IdeConfigManager     *self);
IDE_AVAILABLE_IN_ALL
IdeConfig        *ide_config_manager_ref_current      (IdeConfigManager     *self);
IDE_AVAILABLE_IN_ALL
void              ide_config_manager_set_current      (IdeConfigManager     *self,
                                                       IdeConfig            *configuration);
IDE_AVAILABLE_IN_ALL
IdeConfig        *ide_config_manager_get_config       (IdeConfigManager     *self,
                                                       const gchar          *id);
IDE_AVAILABLE_IN_ALL
void              ide_config_manager_duplicate        (IdeConfigManager     *self,
                                                       IdeConfig            *config);
IDE_AVAILABLE_IN_ALL
void              ide_config_manager_delete           (IdeConfigManager     *self,
                                                       IdeConfig            *config);
IDE_AVAILABLE_IN_ALL
void              ide_config_manager_save_async       (IdeConfigManager     *self,
                                                       GCancellable         *cancellable,
                                                       GAsyncReadyCallback   callback,
                                                       gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gboolean          ide_config_manager_save_finish      (IdeConfigManager     *self,
                                                       GAsyncResult         *result,
                                                       GError              **error);
IDE_AVAILABLE_IN_ALL
gboolean          ide_config_manager_get_ready        (IdeConfigManager     *self);
IDE_AVAILABLE_IN_ALL
GMenuModel       *ide_config_manager_get_menu         (IdeConfigManager     *self);
IDE_AVAILABLE_IN_47
void              ide_config_manager_invalidate       (IdeConfigManager     *self);

G_END_DECLS
