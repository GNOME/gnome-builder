/* ide-source-location.c
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-source-location"

#include "config.h"

#include <dazzle.h>

#include "ide-context.h"

#include "files/ide-file.h"
#include "diagnostics/ide-source-location.h"

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

DZL_DEFINE_COUNTER (instances, "IdeSourceLocation", "Instances", "Number of IdeSourceLocation")

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
      DZL_COUNTER_DEC (instances);
    }
}

/**
 * ide_source_location_get_offset:
 *
 * Retrieves the character offset within the file.
 *
 * Returns: a #guint containing the character offset within the file.
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
 * Returns: a #guint containing the target line.
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
 * Returns: a #guint containing the offset within the line.
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
  ret->line = MIN (G_MAXINT, line);
  ret->line_offset = MIN (G_MAXINT, line_offset);
  ret->offset = offset;

  DZL_COUNTER_INC (instances);

  return ret;
}

/**
 * ide_source_location_get_uri:
 * @self: (in): an #IdeSourceLocation.
 *
 * Returns: (transfer full): A newly allocated #IdeUri.
 */
IdeUri *
ide_source_location_get_uri (IdeSourceLocation *self)
{
  GFile *file;
  IdeUri *ret;
  gchar *fragment;

  g_return_val_if_fail (self != NULL, NULL);

  file = ide_file_get_file (self->file);
  ret = ide_uri_new_from_file (file);
  fragment = g_strdup_printf ("L%u_%u", self->line, self->line_offset);
  ide_uri_set_fragment (ret, fragment);
  g_free (fragment);

  return ret;
}

gint
ide_source_location_compare (const IdeSourceLocation *a,
                             const IdeSourceLocation *b)
{
  gint ret;

  g_assert (a != NULL);
  g_assert (b != NULL);

  if (a->file && b->file)
    {
      if (0 != (ret = ide_file_compare (a->file, b->file)))
        return ret;
    }
  else if (a->file)
    return -1;
  else if (b->file)
    return 1;

  if (0 != (ret = (gint)a->line - (gint)b->line))
    return ret;

  return (gint)a->line_offset - (gint)b->line_offset;
}

guint
ide_source_location_hash (IdeSourceLocation *self)
{
  return ide_file_hash (self->file) ^ g_int_hash (&self->line) ^ g_int_hash (&self->line_offset);
}

IdeSourceLocation *
ide_source_location_new_for_path (IdeContext  *context,
                                  const gchar *path,
                                  guint        line,
                                  guint        line_offset)
{
  g_autoptr(IdeFile) ifile = NULL;

  g_return_val_if_fail (!context || IDE_IS_CONTEXT (context), NULL);

  ifile = ide_file_new_for_path (context, path);

  return ide_source_location_new (ifile, line, line_offset, 0);
}

/**
 * ide_source_location_to_variant:
 * @self: a #IdeSourceLocation
 *
 * Serializes the location into a variant that can be used to transport
 * across IPC boundaries.
 *
 * This function will never return a variant with a floating reference.
 *
 * Returns: (transfer full): a #GVariant
 */
GVariant *
ide_source_location_to_variant (const IdeSourceLocation *self)
{
  GVariantDict dict;

  g_return_val_if_fail (self != NULL, NULL);

  g_variant_dict_init (&dict, NULL);

  if (self->file != NULL)
    {
      GFile *gfile = ide_file_get_file (self->file);
      g_autofree gchar *uri = g_file_get_uri (gfile);

      g_variant_dict_insert (&dict, "uri", "s", uri);
    }

  g_variant_dict_insert (&dict, "line", "u", self->line);
  g_variant_dict_insert (&dict, "line-offset", "u", self->line_offset);
  g_variant_dict_insert (&dict, "offset", "u", self->offset);

  return g_variant_ref_sink (g_variant_dict_end (&dict));
}
