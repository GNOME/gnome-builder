/* ide-config-view-addin.h
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

#pragma once

#include <dazzle.h>
#include <libide-core.h>
#include <libide-foundry.h>

G_BEGIN_DECLS

#define IDE_TYPE_CONFIG_VIEW_ADDIN (ide_config_view_addin_get_type())

IDE_AVAILABLE_IN_3_32
G_DECLARE_INTERFACE (IdeConfigViewAddin, ide_config_view_addin, IDE, CONFIG_VIEW_ADDIN, GObject)

struct _IdeConfigViewAddinInterface
{
  GTypeInterface parent_iface;

  void (*load) (IdeConfigViewAddin *self,
                DzlPreferences     *preferences,
                IdeConfig          *configuration);
};

IDE_AVAILABLE_IN_3_32
void ide_config_view_addin_load (IdeConfigViewAddin *self,
                                 DzlPreferences     *preferences,
                                 IdeConfig          *configuration);

G_END_DECLS
