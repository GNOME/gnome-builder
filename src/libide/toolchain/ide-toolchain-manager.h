/* ide-toolchain-manager.h
 *
 * Copyright 2018 Corentin Noël <corentin.noel@collabora.com>
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
 */

#pragma once

#include "ide-object.h"
#include "ide-version-macros.h"

#include "toolchain/ide-toolchain.h"

G_BEGIN_DECLS

#define IDE_TYPE_TOOLCHAIN_MANAGER (ide_toolchain_manager_get_type())

IDE_AVAILABLE_IN_3_30
G_DECLARE_FINAL_TYPE (IdeToolchainManager, ide_toolchain_manager, IDE, TOOLCHAIN_MANAGER, IdeObject)

IDE_AVAILABLE_IN_3_30
IdeToolchain *ide_toolchain_manager_get_toolchain (IdeToolchainManager  *self,
                                                   const gchar          *id);
G_END_DECLS
