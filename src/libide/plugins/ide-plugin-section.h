/* ide-plugin-section.h
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

#pragma once

#if !defined (IDE_PLUGINS_INSIDE) && !defined (IDE_PLUGINS_COMPILATION)
# error "Only <libide-plugins.h> can be included directly."
#endif

#include <libide-core.h>

#include "ide-plugin.h"

G_BEGIN_DECLS

#define IDE_TYPE_PLUGIN_SECTION (ide_plugin_section_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdePluginSection, ide_plugin_section, IDE, PLUGIN_SECTION, GObject)

IDE_AVAILABLE_IN_ALL
const char *ide_plugin_section_get_id      (IdePluginSection *self);
IDE_AVAILABLE_IN_ALL
GListModel *ide_plugin_section_get_plugins (IdePluginSection *self);

G_END_DECLS
