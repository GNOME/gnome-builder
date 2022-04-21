/* ide-xml-hash-table.c
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

#include <libide-code.h>

#include "ide-xml-hash-table.h"

G_DEFINE_BOXED_TYPE (IdeXmlHashTable, ide_xml_hash_table, ide_xml_hash_table_ref, ide_xml_hash_table_unref)

IdeXmlHashTable *
ide_xml_hash_table_new (GDestroyNotify free_func)
{
  IdeXmlHashTable *self;

  g_return_val_if_fail (free_func != NULL, NULL);

  self = g_slice_new0 (IdeXmlHashTable);
  self->ref_count = 1;

  self->free_func = free_func;
  self->table = g_hash_table_new_full (g_str_hash,
                                       g_str_equal,
                                       g_free,
                                       (GDestroyNotify)g_ptr_array_unref);

  return self;
}

void
ide_xml_hash_table_array_scan (IdeXmlHashTable              *self,
                               IdeXmlHashTableArrayScanFunc  func,
                               gpointer                      data)
{
  GHashTableIter iter;
  gpointer key, value;

  g_return_if_fail (self != NULL);
  g_return_if_fail (func != NULL);
  g_return_if_fail (data != NULL);

  g_hash_table_iter_init (&iter, self->table);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      GPtrArray *array = (GPtrArray *)value;
      const gchar *name = (const gchar *)key;

      func (name, array, data);
    }
}

void
ide_xml_hash_table_full_scan (IdeXmlHashTable         *self,
                              IdeXmlHashTableScanFunc  func,
                              gpointer                 data)
{
  GHashTableIter iter;
  gpointer key, value;

  g_return_if_fail (self != NULL);
  g_return_if_fail (func != NULL);
  g_return_if_fail (data != NULL);

  g_hash_table_iter_init (&iter, self->table);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      GPtrArray *array = (GPtrArray *)value;
      const gchar *name = (const gchar *)key;

      for (gint i = 0; i < array->len; ++i)
        {
          gpointer content = g_ptr_array_index (array, i);

          func (name, content, data);
        }
    }
}

gboolean
ide_xml_hash_table_add (IdeXmlHashTable *self,
                        const gchar     *name,
                        gpointer         data)
{
  GPtrArray *array;

  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (!ide_str_empty0 (name), FALSE);
  g_return_val_if_fail (data != NULL, FALSE);

  if (NULL == (array = g_hash_table_lookup (self->table, name)))
    {
      array = g_ptr_array_new_with_free_func (self->free_func);
      g_hash_table_insert (self->table, g_strdup (name), array);
    }
  else
    {
      for (gint i = 0; i < array->len; ++i)
        {
          if (data == g_ptr_array_index (array, i))
            return FALSE;
        }
    }

  g_ptr_array_add (array, data);

  return TRUE;
}

GPtrArray *
ide_xml_hash_table_lookup (IdeXmlHashTable *self,
                           const gchar     *name)
{
  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (!ide_str_empty0 (name), NULL);

  return g_hash_table_lookup (self->table, name);
}

static void
ide_xml_hash_table_free (IdeXmlHashTable *self)
{
  g_assert (self);
  g_assert_cmpint (self->ref_count, ==, 0);

  g_hash_table_unref (self->table);

  g_slice_free (IdeXmlHashTable, self);
}

IdeXmlHashTable *
ide_xml_hash_table_ref (IdeXmlHashTable *self)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count > 0, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

void
ide_xml_hash_table_unref (IdeXmlHashTable *self)
{
  g_return_if_fail (self);
  g_return_if_fail (self->ref_count > 0);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    ide_xml_hash_table_free (self);
}
