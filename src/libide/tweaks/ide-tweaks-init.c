/* ide-tweaks-init.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-tweaks-init"

#include "config.h"

#include "libide-tweaks.h"

#include "ide-tweaks-init.h"
#include "ide-tweaks-resources.h"

void
_ide_tweaks_init (void)
{
  g_resources_register (ide_tweaks_get_resource ());

  g_type_ensure (IDE_TYPE_TWEAKS);
  g_type_ensure (IDE_TYPE_TWEAKS_EXTERNAL);
  g_type_ensure (IDE_TYPE_TWEAKS_FACTORY);
  g_type_ensure (IDE_TYPE_TWEAKS_GROUP);
  g_type_ensure (IDE_TYPE_TWEAKS_ITEM);
  g_type_ensure (IDE_TYPE_TWEAKS_PAGE);
  g_type_ensure (IDE_TYPE_TWEAKS_RADIO);
  g_type_ensure (IDE_TYPE_TWEAKS_SECTION);
  g_type_ensure (IDE_TYPE_TWEAKS_SETTINGS);
  g_type_ensure (IDE_TYPE_TWEAKS_SWITCH);
  g_type_ensure (IDE_TYPE_TWEAKS_VARIABLE);
  g_type_ensure (IDE_TYPE_TWEAKS_WIDGET);
  g_type_ensure (IDE_TYPE_TWEAKS_WINDOW);
}
