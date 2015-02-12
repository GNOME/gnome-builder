/* ide-source-location.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ide-file.h"
#include "ide-source-location.h"

G_DEFINE_BOXED_TYPE (IdeSourceLocation, ide_source_location,
                     ide_source_location_ref, ide_source_location_unref)

struct _IdeSourceLocation
{
  volatile gint  ref_count;
  guint          line;
  guint          line_offset;
  guint          offset;
  IdeFile       *file;
};

/**
 * ide_source_location_ref:
 *
 * Increments the reference count of @self by one.
 *
 * Returns: (transfer full): self
 */
IdeSourceLocation *
ide_source_location_ref (IdeSourceLocation *self)
{
  g_return_val_if_fail (self->ref_count > 0, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

/**
 * ide_source_location_unref:
 *
 * Decrements the reference count of @self by one. If the reference count
 * reaches zero, then the structure is freed.
 */
void
ide_source_location_unref (IdeSourceLocation *self)
{
  g_return_if_fail (self->ref_count > 0);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    {
      g_clear_object (&self->file);
      g_slice_free (IdeSourceLocation, self);
    }
}

/**
 * ide_source_location_get_offset:
 *
 * Retrieves the character offset within the file.
 *
 * Returns: A #guint containing the character offset within the file.
 */
guint
ide_source_location_get_offset (IdeSourceLocation *self)
{
  g_return_val_if_fail (self, 0);

  return self->offset;
}

/**
 * ide_source_location_get_line:
 *
 * Retrieves the target line number starting from 0.
 *
 * Returns: A #guint containing the target line.
 */
guint
ide_source_location_get_line (IdeSourceLocation *self)
{
  g_return_val_if_fail (self, 0);

  return self->line;
}

/**
 * ide_source_location_get_line_offset:
 *
 * Retrieves the character offset within the line.
 *
 * Returns: A #guint containing the offset within the line.
 */
guint
ide_source_location_get_line_offset (IdeSourceLocation *self)
{
  g_return_val_if_fail (self, 0);

  return self->line_offset;
}

/**
 * ide_source_location_get_file:
 *
 * The file represented by this source location.
 *
 * Returns: (transfer none): An #IdeFile.
 */
IdeFile *
ide_source_location_get_file (IdeSourceLocation *self)
{
  g_return_val_if_fail (self, NULL);

  return self->file;
}

/**
 * ide_source_location_new:
 * @file: an #IdeFile
 * @line: the line number starting from zero
 * @line_offset: the character offset within the line
 * @offset: the character offset in the file
 *
 * Creates a new #IdeSourceLocation, using the file, line, column, and character
 * offset provided.
 *
 * Returns: (transfer full): A newly allocated #IdeSourceLocation.
 */
IdeSourceLocation *
ide_source_location_new (IdeFile *file,
                         guint    line,
                         guint    line_offset,
                         guint    offset)
{
  IdeSourceLocation *ret;

  g_return_val_if_fail (IDE_IS_FILE (file), NULL);

  ret = g_slice_new0 (IdeSourceLocation);
  ret->ref_count = 1;
  ret->file = g_object_ref (file);
  ret->line = line;
  ret->line_offset = line_offset;
  ret->offset = offset;

  return ret;
}
