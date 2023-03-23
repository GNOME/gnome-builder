/* ide-extension-adapter.h
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

#define IDE_TYPE_EXTENSION_ADAPTER (ide_extension_adapter_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeExtensionAdapter, ide_extension_adapter, IDE, EXTENSION_ADAPTER, IdeObject)

IDE_AVAILABLE_IN_ALL
IdeExtensionAdapter *ide_extension_adapter_new                (IdeObject           *parent,
                                                               PeasEngine          *engine,
                                                               GType                interface_type,
                                                               const gchar         *key,
                                                               const gchar         *value);
IDE_AVAILABLE_IN_ALL
PeasEngine          *ide_extension_adapter_get_engine         (IdeExtensionAdapter *self);
IDE_AVAILABLE_IN_ALL
gpointer             ide_extension_adapter_get_extension      (IdeExtensionAdapter *self);
IDE_AVAILABLE_IN_ALL
GType                ide_extension_adapter_get_interface_type (IdeExtensionAdapter *self);
IDE_AVAILABLE_IN_ALL
const gchar         *ide_extension_adapter_get_key            (IdeExtensionAdapter *self);
IDE_AVAILABLE_IN_ALL
void                 ide_extension_adapter_set_key            (IdeExtensionAdapter *self,
                                                               const gchar         *key);
IDE_AVAILABLE_IN_ALL
const gchar         *ide_extension_adapter_get_value          (IdeExtensionAdapter *self);
IDE_AVAILABLE_IN_ALL
void                 ide_extension_adapter_set_value          (IdeExtensionAdapter *self,
                                                               const gchar         *value);

G_END_DECLS
