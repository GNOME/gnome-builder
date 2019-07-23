/* ide-path.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-path"

#include "config.h"

#include "ide-path.h"

struct _IdePath
{
  GObject    parent_instance;
  GPtrArray *elements;
};

G_DEFINE_TYPE (IdePath, ide_path, G_TYPE_OBJECT)

/**
 * ide_path_new:
 * @elements: (transfer none) (element-type IdePathElement): an array of
 *   elements for the path.
 *
 * Create a new #IdePath using the elements provided.
 *
 * Returns: (transfer full): a newly created #IdePath
 *
 * Since: 3.34
 */
IdePath *
ide_path_new (GPtrArray *elements)
{
  IdePath *self;

  g_return_val_if_fail (elements != NULL, NULL);

  self = g_object_new (IDE_TYPE_PATH, NULL);
  self->elements = g_ptr_array_new_full (elements->len, g_object_unref);
  for (guint i = 0; i < elements->len; i++)
    g_ptr_array_add (self->elements, g_object_ref (g_ptr_array_index (elements, i)));

  return g_steal_pointer (&self);
}

static void
ide_path_finalize (GObject *object)
{
  IdePath *self = (IdePath *)object;

  g_clear_pointer (&self->elements, g_ptr_array_unref);

  G_OBJECT_CLASS (ide_path_parent_class)->finalize (object);
}

static void
ide_path_class_init (IdePathClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_path_finalize;
}

static void
ide_path_init (IdePath *self)
{
}

guint
ide_path_get_n_elements (IdePath *self)
{
  g_return_val_if_fail (IDE_IS_PATH (self), 0);

  return self->elements->len;
}

/**
 * ide_path_get_element:
 * @self: an #IdePath
 * @position: the position of the element
 *
 * Gets the element at the position starting from 0.
 *
 * Returns: (transfer none): an #IdePathElement
 *
 * Since: 3.34
 */
IdePathElement *
ide_path_get_element (IdePath *self,
                      guint    position)
{
  g_return_val_if_fail (IDE_IS_PATH (self), NULL);
  g_return_val_if_fail (position < self->elements->len, NULL);

  return g_ptr_array_index (self->elements, position);
}

gboolean
ide_path_has_prefix (IdePath *self,
                     IdePath *prefix)
{
  g_return_val_if_fail (IDE_IS_PATH (self), FALSE);
  g_return_val_if_fail (IDE_IS_PATH (prefix), FALSE);

  if (prefix->elements->len > self->elements->len)
    return FALSE;

  for (guint i = 0; i < prefix->elements->len; i++)
    {
      IdePathElement *eself = g_ptr_array_index (self->elements, i);
      IdePathElement *pself = g_ptr_array_index (prefix->elements, i);

      if (!ide_path_element_equal (eself, pself))
        return FALSE;
    }

  return TRUE;
}

/**
 * ide_path_get_parent:
 * @self: an #IdePath
 *
 * Gets a new path for the parent of @self.
 *
 * Returns: (transfer full) (nullable): an #IdePath or %NULL if the
 *   path is the root path.
 *
 * Since: 3.34
 */
IdePath *
ide_path_get_parent (IdePath *self)
{
  IdePath *ret;

  g_return_val_if_fail (IDE_IS_PATH (self), NULL);

  if (self->elements->len == 0)
    return NULL;

  ret = g_object_new (IDE_TYPE_PATH, NULL);
  ret->elements = g_ptr_array_new_with_free_func (g_object_unref);

  for (guint i = 0; i < self->elements->len - 1; i++)
    g_ptr_array_add (ret->elements, g_object_ref (g_ptr_array_index (self->elements, i)));

  return g_steal_pointer (&ret);
}

gboolean
ide_path_is_root (IdePath *self)
{
  g_return_val_if_fail (IDE_IS_PATH (self), FALSE);

  return self->elements->len == 0;
}
