/* ide-configuration-manager.h
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

#ifndef IDE_CONFIGURATION_MANAGER_H
#define IDE_CONFIGURATION_MANAGER_H

#include <gio/gio.h>

#include "ide-object.h"
#include "ide-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_CONFIGURATION_MANAGER (ide_configuration_manager_get_type())

G_DECLARE_FINAL_TYPE (IdeConfigurationManager, ide_configuration_manager, IDE, CONFIGURATION_MANAGER, IdeObject)

IdeConfiguration *ide_configuration_manager_get_current       (IdeConfigurationManager  *self);
void              ide_configuration_manager_set_current       (IdeConfigurationManager  *self,
                                                               IdeConfiguration         *configuration);
IdeConfiguration *ide_configuration_manager_get_configuration (IdeConfigurationManager  *self,
                                                               const gchar              *id);
void              ide_configuration_manager_add               (IdeConfigurationManager  *self,
                                                               IdeConfiguration         *configuration);
void              ide_configuration_manager_remove            (IdeConfigurationManager  *self,
                                                               IdeConfiguration         *configuration);
void              ide_configuration_manager_save_async        (IdeConfigurationManager  *self,
                                                               GCancellable             *cancellable,
                                                               GAsyncReadyCallback       callback,
                                                               gpointer                  user_data);
gboolean          ide_configuration_manager_save_finish       (IdeConfigurationManager  *self,
                                                               GAsyncResult             *result,
                                                               GError                  **error);


G_END_DECLS

#endif /* IDE_CONFIGURATION_MANAGER_H */
