/* ide-xml-schema-cache-entry.c
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

#define G_LOG_DOMAIN "ide-xml-schema-cache-entry"

#include "ide-xml-schema-cache-entry.h"

G_DEFINE_BOXED_TYPE (IdeXmlSchemaCacheEntry,
                     ide_xml_schema_cache_entry,
                     ide_xml_schema_cache_entry_ref,
                     ide_xml_schema_cache_entry_unref)

IdeXmlSchemaCacheEntry *
ide_xml_schema_cache_entry_new (void)
{
  IdeXmlSchemaCacheEntry *self;

  self = g_slice_new0 (IdeXmlSchemaCacheEntry);
  self->ref_count = 1;

  return self;
}

IdeXmlSchemaCacheEntry *
ide_xml_schema_cache_entry_new_full (GBytes      *content,
                                     const gchar *error_message)
{
  IdeXmlSchemaCacheEntry *self;

  g_assert ((content != NULL && error_message == NULL) ||
            (content == NULL && error_message != NULL));

  self = ide_xml_schema_cache_entry_new ();

  if (content != NULL)
    self->content = g_bytes_ref (content);

  if (error_message != NULL)
    self->error_message = g_strdup (error_message);

  return self;
}

IdeXmlSchemaCacheEntry *
ide_xml_schema_cache_entry_copy (IdeXmlSchemaCacheEntry *self)
{
  IdeXmlSchemaCacheEntry *copy;

  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count, NULL);

  copy = ide_xml_schema_cache_entry_new ();

  if (self->content != NULL)
    copy->content = g_bytes_ref (self->content);

  if (self->error_message != NULL)
    copy->error_message = g_strdup (self->error_message);

  if (self->file != NULL)
    copy->file = g_object_ref (self->file);

  copy->kind = self->kind;
  copy->state = self->state;
  copy->line = self->line;
  copy->col = self->col;
  copy->mtime = self->mtime;

  return copy;
}

static void
ide_xml_schema_cache_entry_free (IdeXmlSchemaCacheEntry *self)
{
  g_assert (self != NULL);
  g_assert (self->ref_count == 0);

  g_clear_pointer (&self->content, g_bytes_unref);
  g_clear_object (&self->file);
  g_clear_pointer (&self->error_message, g_free);

  g_slice_free (IdeXmlSchemaCacheEntry, self);
}

IdeXmlSchemaCacheEntry *
ide_xml_schema_cache_entry_ref (IdeXmlSchemaCacheEntry *self)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count > 0, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

void
ide_xml_schema_cache_entry_unref (IdeXmlSchemaCacheEntry *self)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (self->ref_count > 0);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    ide_xml_schema_cache_entry_free (self);
}
