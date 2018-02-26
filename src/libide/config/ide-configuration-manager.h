/* ide-configuration-manager.h
 *
 * Copyright © 2016 Christian Hergert <chergert@redhat.com>
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

#include "ide-version-macros.h"

#include "ide-object.h"
#include "ide-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_CONFIGURATION_MANAGER (ide_configuration_manager_get_type())

G_DECLARE_FINAL_TYPE (IdeConfigurationManager, ide_configuration_manager, IDE, CONFIGURATION_MANAGER, IdeObject)

IDE_AVAILABLE_IN_ALL
IdeConfiguration *ide_configuration_manager_get_current       (IdeConfigurationManager  *self);
IDE_AVAILABLE_IN_ALL
void              ide_configuration_manager_set_current       (IdeConfigurationManager  *self,
                                                               IdeConfiguration         *configuration);
IDE_AVAILABLE_IN_ALL
IdeConfiguration *ide_configuration_manager_get_configuration (IdeConfigurationManager  *self,
                                                               const gchar              *id);
IDE_AVAILABLE_IN_3_28
void              ide_configuration_manager_duplicate         (IdeConfigurationManager  *self,
                                                               IdeConfiguration         *config);
IDE_AVAILABLE_IN_3_28
void              ide_configuration_manager_delete            (IdeConfigurationManager  *self,
                                                               IdeConfiguration         *config);
IDE_AVAILABLE_IN_ALL
void              ide_configuration_manager_save_async        (IdeConfigurationManager  *self,
                                                               GCancellable             *cancellable,
                                                               GAsyncReadyCallback       callback,
                                                               gpointer                  user_data);
IDE_AVAILABLE_IN_ALL
gboolean          ide_configuration_manager_save_finish       (IdeConfigurationManager  *self,
                                                               GAsyncResult             *result,
                                                               GError                  **error);
IDE_AVAILABLE_IN_3_28
gboolean          ide_configuration_manager_get_ready         (IdeConfigurationManager  *self);

G_END_DECLS
