/* ide-extension-util.h
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#include <libpeas.h>

G_BEGIN_DECLS

gboolean         ide_extension_util_can_use_plugin (PeasEngine     *engine,
                                                    PeasPluginInfo *plugin_info,
                                                    GType           interface_type,
                                                    const gchar    *key,
                                                    const gchar    *value,
                                                    gint           *priority);
PeasExtensionSet *ide_extension_set_new            (PeasEngine     *engine,
                                                    GType           type,
                                                    const gchar    *first_property,
                                                    ...);
GObject    *ide_extension_new                (PeasEngine     *engine,
                                                    PeasPluginInfo *plugin_info,
                                                    GType           interface_type,
                                                    const gchar    *first_property,
                                                    ...);

G_END_DECLS
