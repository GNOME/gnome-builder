/* ide-marked-content.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-marked-content"

#include "config.h"

#include "ide-marked-content.h"

#define IDE_MARKED_CONTENT_MAGIC 0x81124633

struct _IdeMarkedContent
{
  guint          magic;
  IdeMarkedKind  kind;
  GBytes        *data;
  volatile gint  ref_count;
};

G_DEFINE_BOXED_TYPE (IdeMarkedContent,
                     ide_marked_content,
                     ide_marked_content_ref,
                     ide_marked_content_unref)

/**
 * ide_marked_content_new:
 * @content: a #GBytes containing the markup
 * @kind: an #IdeMakredKind describing the markup kind
 *
 * Creates a new #IdeMarkedContent using the bytes provided.
 *
 * Returns: (transfer full): an #IdeMarkedContent
 */
IdeMarkedContent *
ide_marked_content_new (GBytes        *content,
                        IdeMarkedKind  kind)
{
  IdeMarkedContent *self;

  g_return_val_if_fail (content != NULL, NULL);

  self = g_slice_new0 (IdeMarkedContent);
  self->magic = IDE_MARKED_CONTENT_MAGIC;
  self->ref_count = 1;
  self->data = g_bytes_ref (content);
  self->kind = kind;

  return self;
}

/**
 * ide_marked_content_new_plaintext:
 * @plaintext: (nullable): a string containing the plaintext
 *
 * Creates a new #IdeMarkedContent of type %IDE_MARKED_KIND_PLAINTEXT
 * with the contents of @string.
 *
 * Returns: (transfer full): an #IdeMarkedContent
 */
IdeMarkedContent *
ide_marked_content_new_plaintext (const gchar *plaintext)
{
  if (plaintext == NULL)
    plaintext = "";

  return ide_marked_content_new_from_data (plaintext, -1, IDE_MARKED_KIND_PLAINTEXT);
}

/**
 * ide_marked_content_new_from_data:
 * @data: the data for the content
 * @len: the length of the data, or -1 to strlen() @data
 * @kind: the kind of markup
 *
 * Creates a new #IdeMarkedContent from the provided data.
 *
 * Returns: (transfer full): an #IdeMarkedContent
 */
IdeMarkedContent *
ide_marked_content_new_from_data (const gchar   *data,
                                  gssize         len,
                                  IdeMarkedKind  kind)
{
  g_autoptr(GBytes) bytes = NULL;

  if (len < 0)
    len = strlen (data);

  bytes = g_bytes_new (data, len);

  return ide_marked_content_new (bytes, kind);
}

/**
 * ide_marked_content_ref:
 * @self: an #IdeMarkedContent
 *
 * Increments the reference count of @self by one.
 *
 * When a #IdeMarkedContent reaches a reference count of zero, by using
 * ide_marked_content_unref(), it will be freed.
 *
 * Returns: (transfer full): @self with the reference count incremented
 */
IdeMarkedContent *
ide_marked_content_ref (IdeMarkedContent *self)
{
  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (self->magic == IDE_MARKED_CONTENT_MAGIC, NULL);
  g_return_val_if_fail (self->ref_count > 0, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

/**
 * ide_marked_content_unref:
 * @self: an #IdeMarkedContent
 *
 * Decrements the reference count of @self by one.
 *
 * When the reference count of @self reaches zero, it will be freed.
 */
void
ide_marked_content_unref (IdeMarkedContent *self)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (self->magic == IDE_MARKED_CONTENT_MAGIC);
  g_return_if_fail (self->ref_count > 0);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    {
      self->magic = 0;
      self->kind = 0;
      g_clear_pointer (&self->data, g_bytes_unref);
      g_slice_free (IdeMarkedContent, self);
    }
}

/**
 * ide_marked_content_get_kind:
 * @self: an #IdeMarkedContent
 *
 * Gets the kind of markup that @self contains.
 *
 * This is used to display the content appropriately.
 *
 * Returns:
 */
IdeMarkedKind
ide_marked_content_get_kind (IdeMarkedContent *self)
{
  g_return_val_if_fail (self != NULL, 0);
  g_return_val_if_fail (self->magic == IDE_MARKED_CONTENT_MAGIC, 0);
  g_return_val_if_fail (self->ref_count > 0, 0);

  return self->kind;
}

/**
 * ide_marked_content_get_bytes:
 *
 * Gets the bytes for the marked content.
 *
 * Returns: (transfer none): a #GBytes
 */
GBytes *
ide_marked_content_get_bytes (IdeMarkedContent *self)
{
  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (self->magic == IDE_MARKED_CONTENT_MAGIC, NULL);
  g_return_val_if_fail (self->ref_count > 0, NULL);

  return self->data;
}

/**
 * ide_marked_content_as_string:
 * @self: a #IdeMarkedContent
 * @len: (out) (optional): Location to store the length of the returned strings in bytes, or %NULL
 *
 * Gets the contents of the marked content as a C string.
 *
 * Returns: (transfer none) (nullable): the content as a string or %NULL
 */
const gchar *
ide_marked_content_as_string (IdeMarkedContent *self,
                              gsize            *len)
{
  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (self->magic == IDE_MARKED_CONTENT_MAGIC, NULL);
  g_return_val_if_fail (self->ref_count > 0, NULL);

  if (self->data != NULL)
    {
      const gchar *result;
      gsize length;

      if ((result = g_bytes_get_data (self->data, &length)))
        {
          if (len != NULL)
            *len = length;

          return result;
        }
    }

  return NULL;
}
