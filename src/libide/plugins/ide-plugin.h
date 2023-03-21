/* ide-plugin.h
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

#include <libpeas.h>

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_PLUGIN (ide_plugin_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdePlugin, ide_plugin, IDE, PLUGIN, GObject)

IDE_AVAILABLE_IN_ALL
const char     *ide_plugin_get_id          (IdePlugin *self);
IDE_AVAILABLE_IN_ALL
const char     *ide_plugin_get_category    (IdePlugin *self);
IDE_AVAILABLE_IN_ALL
const char     *ide_plugin_get_category_id (IdePlugin *self);
IDE_AVAILABLE_IN_ALL
const char     *ide_plugin_get_description (IdePlugin *self);
IDE_AVAILABLE_IN_ALL
PeasPluginInfo *ide_plugin_get_info        (IdePlugin *self);
IDE_AVAILABLE_IN_ALL
const char     *ide_plugin_get_name        (IdePlugin *self);
IDE_AVAILABLE_IN_ALL
const char     *ide_plugin_get_section     (IdePlugin *self);

G_END_DECLS
