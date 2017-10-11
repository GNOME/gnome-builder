/* ide-extension-adapter.h
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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
 */

#pragma once

#include <libpeas/peas.h>

#include "ide-object.h"

G_BEGIN_DECLS

#define IDE_TYPE_EXTENSION_ADAPTER (ide_extension_adapter_get_type())

G_DECLARE_FINAL_TYPE (IdeExtensionAdapter, ide_extension_adapter, IDE, EXTENSION_ADAPTER, IdeObject)

IdeExtensionAdapter *ide_extension_adapter_new                (IdeContext          *context,
                                                               PeasEngine          *engine,
                                                               GType                interface_type,
                                                               const gchar         *key,
                                                               const gchar         *value);
PeasEngine          *ide_extension_adapter_get_engine         (IdeExtensionAdapter *self);
gpointer             ide_extension_adapter_get_extension      (IdeExtensionAdapter *self);
GType                ide_extension_adapter_get_interface_type (IdeExtensionAdapter *self);
const gchar         *ide_extension_adapter_get_key            (IdeExtensionAdapter *self);
void                 ide_extension_adapter_set_key            (IdeExtensionAdapter *self,
                                                               const gchar         *key);
const gchar         *ide_extension_adapter_get_value          (IdeExtensionAdapter *self);
void                 ide_extension_adapter_set_value          (IdeExtensionAdapter *self,
                                                               const gchar         *value);

G_END_DECLS
