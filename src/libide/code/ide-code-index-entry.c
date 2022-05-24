/* ide-code-index-entry.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
 * Copyright 2017 Anoop Chandu <anoopchandu96@gmail.com>
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

#define G_LOG_DOMAIN "ide-code-index-entry"

#include "config.h"

#include "ide-code-index-entry.h"

/**
 * SECTION:ide-code-index-entry
 * @title: IdeCodeIndexEntry
 * @short_description: information about code index entry
 *
 * The #IdeCodeIndexEntry structure contains information about something to be
 * indexed in the code index. It is an immutable data object so that it can be
 * passed between threads where data is indexed. Plugins should use
 * #IdeCodeIndexEntryBuilder to create index entries.
 */

struct _IdeCodeIndexEntry
{
  gchar          *key;
  gchar          *name;

  IdeSymbolKind   kind;
  IdeSymbolFlags  flags;

  guint           begin_line;
  guint           begin_line_offset;
  guint           end_line;
  guint           end_line_offset;
};

struct _IdeCodeIndexEntryBuilder
{
  IdeCodeIndexEntry entry;
};

G_DEFINE_BOXED_TYPE (IdeCodeIndexEntry,
                     ide_code_index_entry,
                     ide_code_index_entry_copy,
                     ide_code_index_entry_free)
G_DEFINE_BOXED_TYPE (IdeCodeIndexEntryBuilder,
                     ide_code_index_entry_builder,
                     ide_code_index_entry_builder_copy,
                     ide_code_index_entry_builder_free)

void
ide_code_index_entry_free (IdeCodeIndexEntry *self)
{
  if (self != NULL)
    {
      g_clear_pointer (&self->name, g_free);
      g_clear_pointer (&self->key, g_free);
      g_slice_free (IdeCodeIndexEntry, self);
    }
}

IdeCodeIndexEntry *
ide_code_index_entry_copy (const IdeCodeIndexEntry *self)
{
  IdeCodeIndexEntry *copy = NULL;

  if (self != NULL)
    {
      copy = g_slice_dup (IdeCodeIndexEntry, self);
      copy->name = g_strdup (self->name);
      copy->key = g_strdup (self->key);
    }

  return copy;
}

const gchar *
ide_code_index_entry_get_key (const IdeCodeIndexEntry *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->key;
}

const gchar *
ide_code_index_entry_get_name (const IdeCodeIndexEntry *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->name;
}

IdeSymbolKind
ide_code_index_entry_get_kind (const IdeCodeIndexEntry *self)
{
  g_return_val_if_fail (self != NULL, IDE_SYMBOL_KIND_NONE);

  return self->kind;
}

IdeSymbolFlags
ide_code_index_entry_get_flags (const IdeCodeIndexEntry *self)
{
  g_return_val_if_fail (self != NULL, IDE_SYMBOL_FLAGS_NONE);

  return self->flags;
}

/**
 * ide_code_index_entry_get_range:
 * @self: a #IdeCodeIndexEntry
 * @begin_line: (out): first line
 * @begin_line_offset: (out): first line offset
 * @end_line: (out): last line
 * @end_line_offset: (out): last line offset
 */
void
ide_code_index_entry_get_range (const IdeCodeIndexEntry *self,
                                guint                   *begin_line,
                                guint                   *begin_line_offset,
                                guint                   *end_line,
                                guint                   *end_line_offset)
{
  g_return_if_fail (self != NULL);

  if (begin_line != NULL)
    *begin_line = self->begin_line;

  if (begin_line_offset != NULL)
    *begin_line_offset = self->begin_line_offset;

  if (end_line != NULL)
    *end_line = self->end_line;

  if (end_line_offset != NULL)
    *end_line_offset = self->end_line_offset;
}

IdeCodeIndexEntryBuilder *
ide_code_index_entry_builder_new (void)
{
  return g_slice_new0 (IdeCodeIndexEntryBuilder);
}

void
ide_code_index_entry_builder_free (IdeCodeIndexEntryBuilder *builder)
{
  if (builder != NULL)
    {
      g_clear_pointer (&builder->entry.key, g_free);
      g_clear_pointer (&builder->entry.name, g_free);
      g_slice_free (IdeCodeIndexEntryBuilder, builder);
    }
}

void
ide_code_index_entry_builder_set_range (IdeCodeIndexEntryBuilder *builder,
                                        guint                     begin_line,
                                        guint                     begin_line_offset,
                                        guint                     end_line,
                                        guint                     end_line_offset)
{
  g_return_if_fail (builder != NULL);

  builder->entry.begin_line = begin_line;
  builder->entry.begin_line_offset = begin_line_offset;
  builder->entry.end_line = end_line;
  builder->entry.end_line_offset = end_line_offset;
}

void
ide_code_index_entry_builder_set_name (IdeCodeIndexEntryBuilder *builder,
                                       const gchar              *name)
{
  g_return_if_fail (builder != NULL);

  if (name != builder->entry.name)
    {
      g_free (builder->entry.name);
      builder->entry.name = g_strdup (name);
    }
}

void
ide_code_index_entry_builder_set_key (IdeCodeIndexEntryBuilder *builder,
                                      const gchar              *key)
{
  g_return_if_fail (builder != NULL);

  if (key != builder->entry.key)
    {
      g_free (builder->entry.key);
      builder->entry.key = g_strdup (key);
    }
}

void
ide_code_index_entry_builder_set_flags (IdeCodeIndexEntryBuilder *builder,
                                        IdeSymbolFlags            flags)
{
  g_return_if_fail (builder != NULL);

  builder->entry.flags = flags;
}

void
ide_code_index_entry_builder_set_kind (IdeCodeIndexEntryBuilder *builder,
                                       IdeSymbolKind             kind)
{
  g_return_if_fail (builder != NULL);

  builder->entry.kind = kind;
}

/**
 * ide_code_index_entry_builder_build:
 * @builder: a #IdeCodeIndexEntryBuilder
 *
 * Creates an immutable #IdeCodeIndexEntry from the builder content.
 *
 * Returns: (transfer full): an #IdeCodeIndexEntry
 */
IdeCodeIndexEntry *
ide_code_index_entry_builder_build (IdeCodeIndexEntryBuilder *builder)
{
  g_return_val_if_fail (builder != NULL, NULL);

  return ide_code_index_entry_copy (&builder->entry);
}

/**
 * ide_code_index_entry_builder_copy:
 * @builder: a #IdeCodeIndexEntryBuilder
 *
 * Returns: (transfer full): a deep copy of @builder
 */
IdeCodeIndexEntryBuilder *
ide_code_index_entry_builder_copy (IdeCodeIndexEntryBuilder *builder)
{
  IdeCodeIndexEntryBuilder *copy;

  copy = g_slice_dup (IdeCodeIndexEntryBuilder, builder);
  copy->entry.key = g_strdup (builder->entry.key);
  copy->entry.name = g_strdup (builder->entry.name);

  return copy;
}
