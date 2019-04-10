/* ide-vcs-config.c
 *
 * Copyright 2016 Akshaya Kakkilaya <akshaya.kakkilaya@gmail.com>
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

#define G_LOG_DOMAIN "ide-vcs-config"

#include "config.h"

#include "ide-vcs-config.h"
#include "ide-vcs-enums.h"

G_DEFINE_INTERFACE (IdeVcsConfig, ide_vcs_config, IDE_TYPE_OBJECT)

static void
ide_vcs_config_default_init (IdeVcsConfigInterface *iface)
{
}

void
ide_vcs_config_get_config (IdeVcsConfig    *self,
                           IdeVcsConfigType type,
                           GValue          *value)
{
  g_return_if_fail (IDE_IS_VCS_CONFIG (self));

  IDE_VCS_CONFIG_GET_IFACE (self)->get_config (self, type, value);
}

void
ide_vcs_config_set_config (IdeVcsConfig    *self,
                           IdeVcsConfigType type,
                           const GValue    *value)
{
  g_return_if_fail (IDE_IS_VCS_CONFIG (self));

  IDE_VCS_CONFIG_GET_IFACE (self)->set_config (self, type, value);
}
