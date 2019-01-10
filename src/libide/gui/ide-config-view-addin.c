/* ide-config-view-addin.c
 *
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

#define G_LOG_DOMAIN "ide-config-view-addin"

#include "config.h"

#include "ide-config-view-addin.h"

G_DEFINE_INTERFACE (IdeConfigViewAddin, ide_config_view_addin, G_TYPE_OBJECT)

static void
ide_config_view_addin_default_init (IdeConfigViewAddinInterface *iface)
{
}

void
ide_config_view_addin_load (IdeConfigViewAddin *self,
                            DzlPreferences     *preferences,
                            IdeConfig   *configuration)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_CONFIG_VIEW_ADDIN (self));
  g_return_if_fail (DZL_IS_PREFERENCES (preferences));
  g_return_if_fail (IDE_IS_CONFIG (configuration));

  if (IDE_CONFIG_VIEW_ADDIN_GET_IFACE (self)->load)
    IDE_CONFIG_VIEW_ADDIN_GET_IFACE (self)->load (self, preferences, configuration);
}
