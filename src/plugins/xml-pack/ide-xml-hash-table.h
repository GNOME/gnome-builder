/* ide-xml-hash-table.h
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

#include <libxml/xmlstring.h>

#include <glib-object.h>

G_BEGIN_DECLS

#define IDE_TYPE_XML_HASH_TABLE (ide_xml_hash_table_get_type())

typedef struct _IdeXmlHashTable IdeXmlHashTable;

struct _IdeXmlHashTable
{
  volatile gint   ref_count;

  GHashTable     *table;
  GDestroyNotify  free_func;
};

typedef void (*IdeXmlHashTableScanFunc)      (const gchar *name,
                                              gpointer     value,
                                              gpointer     data);

typedef void (*IdeXmlHashTableArrayScanFunc) (const gchar *name,
                                              GPtrArray   *array,
                                              gpointer     data);

GType                ide_xml_hash_table_get_type    (void);
IdeXmlHashTable     *ide_xml_hash_table_new         (GDestroyNotify                free_func);
gboolean             ide_xml_hash_table_add         (IdeXmlHashTable              *self,
                                                     const gchar                  *name,
                                                     gpointer                      data);
IdeXmlHashTable     *ide_xml_hash_table_copy        (IdeXmlHashTable              *self);
void                 ide_xml_hash_table_array_scan  (IdeXmlHashTable              *self,
                                                     IdeXmlHashTableArrayScanFunc  func,
                                                     gpointer                      data);
void                 ide_xml_hash_table_full_scan   (IdeXmlHashTable              *self,
                                                     IdeXmlHashTableScanFunc       func,
                                                     gpointer                      data);
GPtrArray           *ide_xml_hash_table_lookup      (IdeXmlHashTable              *self,
                                                     const gchar                  *name);
IdeXmlHashTable     *ide_xml_hash_table_ref         (IdeXmlHashTable              *self);
void                 ide_xml_hash_table_unref       (IdeXmlHashTable              *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeXmlHashTable, ide_xml_hash_table_unref)

G_END_DECLS
