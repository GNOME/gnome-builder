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

static void
gchararray_to_variant (const GValue *src,
                       GValue       *dest)
{
  const char *str;

  if ((str = g_value_get_string (src)))
    g_value_take_variant (dest, g_variant_take_ref (g_variant_new_string (str)));
}

void
_ide_tweaks_init (void)
{
  g_resources_register (ide_tweaks_get_resource ());

  g_type_ensure (IDE_TYPE_TWEAKS);
  g_type_ensure (IDE_TYPE_TWEAKS_ADDIN);
  g_type_ensure (IDE_TYPE_TWEAKS_BINDING);
  g_type_ensure (IDE_TYPE_TWEAKS_CAPTION);
  g_type_ensure (IDE_TYPE_TWEAKS_CHOICE);
  g_type_ensure (IDE_TYPE_TWEAKS_COMBO);
  g_type_ensure (IDE_TYPE_TWEAKS_DIRECTORY);
  g_type_ensure (IDE_TYPE_TWEAKS_ENTRY);
  g_type_ensure (IDE_TYPE_TWEAKS_FACTORY);
  g_type_ensure (IDE_TYPE_TWEAKS_FONT);
  g_type_ensure (IDE_TYPE_TWEAKS_GROUP);
  g_type_ensure (IDE_TYPE_TWEAKS_INFO);
  g_type_ensure (IDE_TYPE_TWEAKS_ITEM);
  g_type_ensure (IDE_TYPE_TWEAKS_PAGE);
  g_type_ensure (IDE_TYPE_TWEAKS_PASSWORD);
  g_type_ensure (IDE_TYPE_TWEAKS_PROPERTY);
  g_type_ensure (IDE_TYPE_TWEAKS_RADIO);
  g_type_ensure (IDE_TYPE_TWEAKS_SECTION);
  g_type_ensure (IDE_TYPE_TWEAKS_SETTING);
  g_type_ensure (IDE_TYPE_TWEAKS_SPIN);
  g_type_ensure (IDE_TYPE_TWEAKS_SWITCH);
  g_type_ensure (IDE_TYPE_TWEAKS_WIDGET);
  g_type_ensure (IDE_TYPE_TWEAKS_WINDOW);

  if (!g_value_type_transformable (G_TYPE_STRING, G_TYPE_VARIANT))
    g_value_register_transform_func (G_TYPE_STRING, G_TYPE_VARIANT, gchararray_to_variant);
}
