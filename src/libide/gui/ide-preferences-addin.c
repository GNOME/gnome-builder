/* ide-preferences-addin.c
 *
 * Copyright 2015-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-preferences-addin"

#include "config.h"

#include "ide-preferences-addin.h"

G_DEFINE_INTERFACE (IdePreferencesAddin, ide_preferences_addin, G_TYPE_OBJECT)

static void
ide_preferences_addin_default_init (IdePreferencesAddinInterface *iface)
{
}

/**
 * ide_preferences_addin_load:
 * @self: An #IdePreferencesAddin.
 * @preferences: The preferences container implementation.
 *
 * This interface method is called when a preferences addin is initialized. It
 * could be initialized from multiple preferences implementations, so consumers
 * should use the #DzlPreferences interface to add their preferences controls
 * to the container.
 *
 * Such implementations might include a preferences dialog window, or a
 * preferences widget which could be rendered as a perspective.
 *
 * Since: 3.32
 */
void
ide_preferences_addin_load (IdePreferencesAddin *self,
                            DzlPreferences      *preferences)
{
  g_return_if_fail (IDE_IS_PREFERENCES_ADDIN (self));
  g_return_if_fail (DZL_IS_PREFERENCES (preferences));

  if (IDE_PREFERENCES_ADDIN_GET_IFACE (self)->load)
    IDE_PREFERENCES_ADDIN_GET_IFACE (self)->load (self, preferences);
}

/**
 * ide_preferences_addin_unload:
 * @self: An #IdePreferencesAddin.
 * @preferences: The preferences container implementation.
 *
 * This interface method is called when the preferences addin should remove all
 * controls added to @preferences. This could happen during desctruction of
 * @preferences, or when the plugin is unloaded.
 *
 * Since: 3.32
 */
void
ide_preferences_addin_unload (IdePreferencesAddin *self,
                              DzlPreferences      *preferences)
{
  g_return_if_fail (IDE_IS_PREFERENCES_ADDIN (self));
  g_return_if_fail (DZL_IS_PREFERENCES (preferences));

  if (IDE_PREFERENCES_ADDIN_GET_IFACE (self)->unload)
    IDE_PREFERENCES_ADDIN_GET_IFACE (self)->unload (self, preferences);
}
