/* ide-extension-set-adapter.h
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

#if !defined (IDE_PLUGINS_INSIDE) && !defined (IDE_PLUGINS_COMPILATION)
# error "Only <libide-plugins.h> can be included directly."
#endif

#include <libpeas.h>

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_EXTENSION_SET_ADAPTER (ide_extension_set_adapter_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeExtensionSetAdapter, ide_extension_set_adapter, IDE, EXTENSION_SET_ADAPTER, IdeObject)

typedef void (*IdeExtensionSetAdapterForeachFunc) (IdeExtensionSetAdapter *set,
                                                   PeasPluginInfo         *plugin_info,
                                                   GObject          *extension,
                                                   gpointer                user_data);

IDE_AVAILABLE_IN_ALL
IdeExtensionSetAdapter *ide_extension_set_adapter_new                 (IdeObject                         *parent,
                                                                       PeasEngine                        *engine,
                                                                       GType                              interface_type,
                                                                       const gchar                       *key,
                                                                       const gchar                       *value);
IDE_AVAILABLE_IN_ALL
PeasEngine             *ide_extension_set_adapter_get_engine          (IdeExtensionSetAdapter            *self);
IDE_AVAILABLE_IN_ALL
GType                   ide_extension_set_adapter_get_interface_type  (IdeExtensionSetAdapter            *self);
IDE_AVAILABLE_IN_ALL
const gchar            *ide_extension_set_adapter_get_key             (IdeExtensionSetAdapter            *self);
IDE_AVAILABLE_IN_ALL
void                    ide_extension_set_adapter_set_key             (IdeExtensionSetAdapter            *self,
                                                                       const gchar                       *key);
IDE_AVAILABLE_IN_ALL
const gchar            *ide_extension_set_adapter_get_value           (IdeExtensionSetAdapter            *self);
IDE_AVAILABLE_IN_ALL
void                    ide_extension_set_adapter_set_value           (IdeExtensionSetAdapter            *self,
                                                                       const gchar                       *value);
IDE_AVAILABLE_IN_ALL
guint                   ide_extension_set_adapter_get_n_extensions    (IdeExtensionSetAdapter            *self);
IDE_AVAILABLE_IN_ALL
void                    ide_extension_set_adapter_foreach             (IdeExtensionSetAdapter            *self,
                                                                       IdeExtensionSetAdapterForeachFunc  foreach_func,
                                                                       gpointer                           user_data);
IDE_AVAILABLE_IN_ALL
void                    ide_extension_set_adapter_foreach_by_priority (IdeExtensionSetAdapter            *self,
                                                                       IdeExtensionSetAdapterForeachFunc  foreach_func,
                                                                       gpointer                           user_data);
IDE_AVAILABLE_IN_ALL
GObject          *ide_extension_set_adapter_get_extension       (IdeExtensionSetAdapter            *self,
                                                                       PeasPluginInfo                    *plugin_info);

G_END_DECLS
