/* ide-omni-bar-addin.c
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

#define G_LOG_DOMAIN "ide-omni-bar-addin"

#include "config.h"

#include "ide-omni-bar-addin.h"

/**
 * SECTION:ide-omni-bar-addin
 * @title: IdeOmniBarAddin
 * @short_description: addins to extend the #IdeOmniBar
 *
 * The #IdeOmniBarAddin allows plugins to extend how the #IdeOmniBar
 * works. They can add additional components such as buttons, or more
 * information to the popover.
 *
 * See #IdeOmniBar for information about what you can alter.
 */

G_DEFINE_INTERFACE (IdeOmniBarAddin, ide_omni_bar_addin, G_TYPE_OBJECT)

static void
ide_omni_bar_addin_default_init (IdeOmniBarAddinInterface *iface)
{
}

/**
 * ide_omni_bar_addin_load:
 * @self: an #IdeOmniBarAddin
 * @omni_bar: an #IdeOmniBar
 *
 * Requests that the #IdeOmniBarAddin initialize, possibly modifying
 * @omni_bar as necessary.
 */
void
ide_omni_bar_addin_load (IdeOmniBarAddin *self,
                         IdeOmniBar      *omni_bar)
{
  g_return_if_fail (IDE_IS_OMNI_BAR_ADDIN (self));
  g_return_if_fail (IDE_IS_OMNI_BAR (omni_bar));

  if (IDE_OMNI_BAR_ADDIN_GET_IFACE (self)->load)
    IDE_OMNI_BAR_ADDIN_GET_IFACE (self)->load (self, omni_bar);
}

/**
 * ide_omni_bar_addin_unload:
 * @self: an #IdeOmniBarAddin
 * @omni_bar: an #IdeOmniBar
 *
 * Requests that the #IdeOmniBarAddin shutdown, possibly modifying
 * @omni_bar as necessary to return it to the original state before
 * the addin was loaded.
 */
void
ide_omni_bar_addin_unload (IdeOmniBarAddin *self,
                           IdeOmniBar      *omni_bar)
{
  g_return_if_fail (IDE_IS_OMNI_BAR_ADDIN (self));
  g_return_if_fail (IDE_IS_OMNI_BAR (omni_bar));

  if (IDE_OMNI_BAR_ADDIN_GET_IFACE (self)->unload)
    IDE_OMNI_BAR_ADDIN_GET_IFACE (self)->unload (self, omni_bar);
}
