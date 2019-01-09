/* ide-xml-schema-cache-entry.h
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

#include <gio/gio.h>
#include <glib.h>

#include "ide-xml-schema.h"

G_BEGIN_DECLS

#define IDE_TYPE_XML_SCHEMA_CACHE_ENTRY (ide_xml_schema_cache_entry_get_type())

typedef struct _IdeXmlSchemaCacheEntry IdeXmlSchemaCacheEntry;

typedef enum
{
  SCHEMA_KIND_NONE,
  SCHEMA_KIND_DTD,
  SCHEMA_KIND_RNG,
  SCHEMA_KIND_XML_SCHEMA,
} IdeXmlSchemaKind;

typedef enum
{
  SCHEMA_STATE_NONE,
  SCHEMA_STATE_WRONG_FILE_TYPE,
  SCHEMA_STATE_CANT_LOAD,
  SCHEMA_STATE_CANT_VALIDATE,
  SCHEMA_STATE_CANT_PARSE,
  SCHEMA_STATE_PARSED
} IdeXmlSchemaState;

struct _IdeXmlSchemaCacheEntry
{
  volatile gint      ref_count;

  GFile             *file;
  GBytes            *content;
  IdeXmlSchema      *schema;
  gchar             *error_message;
  IdeXmlSchemaKind   kind;
  IdeXmlSchemaState  state;
  gint32             line;
  gint32             col;
  guint64            mtime;
};

GType                       ide_xml_schema_cache_entry_get_type      (void);
IdeXmlSchemaCacheEntry     *ide_xml_schema_cache_entry_new           (void);
IdeXmlSchemaCacheEntry     *ide_xml_schema_cache_entry_new_full      (GBytes                 *content,
                                                                      const gchar            *error_message);
IdeXmlSchemaCacheEntry     *ide_xml_schema_cache_entry_copy          (IdeXmlSchemaCacheEntry *self);
IdeXmlSchemaCacheEntry     *ide_xml_schema_cache_entry_ref           (IdeXmlSchemaCacheEntry *self);
void                        ide_xml_schema_cache_entry_unref         (IdeXmlSchemaCacheEntry *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeXmlSchemaCacheEntry, ide_xml_schema_cache_entry_unref)

G_END_DECLS
