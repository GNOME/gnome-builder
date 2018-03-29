/* ide-machine-config-name.c
 *
 * Copyright (C) 2018 Corentin Noël <corentin.noel@collabora.com>
 * Copyright (C) 2018 Collabora Ltd.
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

#include <glib-object.h>

#include "ide-version-macros.h"

G_BEGIN_DECLS

#define IDE_TYPE_MACHINE_CONFIG_NAME (ide_machine_config_name_get_type())

typedef struct _IdeMachineConfigName IdeMachineConfigName;

IDE_AVAILABLE_IN_3_30
GType                  ide_machine_config_name_get_type             (void);
IDE_AVAILABLE_IN_3_30
IdeMachineConfigName  *ide_machine_config_name_new                  (const gchar           *full_name);
IDE_AVAILABLE_IN_3_30
IdeMachineConfigName  *ide_machine_config_name_new_from_system      (void);
IDE_AVAILABLE_IN_3_30
IdeMachineConfigName  *ide_machine_config_name_new_with_triplet     (const gchar           *cpu,
                                                                     const gchar           *kernel,
                                                                     const gchar           *operating_system);
IDE_AVAILABLE_IN_3_30
IdeMachineConfigName  *ide_machine_config_name_new_with_quadruplet  (const gchar           *cpu,
                                                                     const gchar           *vendor,
                                                                     const gchar           *kernel,
                                                                     const gchar           *operating_system);
IDE_AVAILABLE_IN_3_30
IdeMachineConfigName  *ide_machine_config_name_ref                  (IdeMachineConfigName  *self);
IDE_AVAILABLE_IN_3_30
void                   ide_machine_config_name_unref                (IdeMachineConfigName  *self);
IDE_AVAILABLE_IN_3_30
const gchar           *ide_machine_config_name_get_full_name        (IdeMachineConfigName  *self);
IDE_AVAILABLE_IN_3_30
const gchar           *ide_machine_config_name_get_cpu              (IdeMachineConfigName  *self);
IDE_AVAILABLE_IN_3_30
const gchar           *ide_machine_config_name_get_vendor           (IdeMachineConfigName  *self);
IDE_AVAILABLE_IN_3_30
const gchar           *ide_machine_config_name_get_kernel           (IdeMachineConfigName  *self);
IDE_AVAILABLE_IN_3_30
const gchar           *ide_machine_config_name_get_operating_system (IdeMachineConfigName  *self);
IDE_AVAILABLE_IN_3_30
gboolean               ide_machine_config_name_is_system            (IdeMachineConfigName  *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeMachineConfigName, ide_machine_config_name_unref)

G_END_DECLS
