/* ide-xml-tree-builder-utils-private.h
 *
 * Copyright 2017 Sebastien Lafargue <slafargue@gnome.org>
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

#include <glib.h>
#include <libide-code.h>

#include "ide-xml-schema-cache-entry.h"
#include "ide-xml-symbol-node.h"
#include "ide-xml-validator.h"

G_BEGIN_DECLS

const gchar  *get_schema_kind_string     (IdeXmlSchemaKind    kind);
gchar        *get_schema_url             (const gchar        *data);
const gchar  *list_get_attribute         (const guchar      **attributes,
                                          const gchar        *name);
void          print_node                 (IdeXmlSymbolNode   *node,
                                          guint               depth);

G_END_DECLS
