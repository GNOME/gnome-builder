/*
 * manuals-gio.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#include "manuals-gio.h"

typedef struct _ListChildrenTyped
{
  GFile     *file;
  char      *attributes;
  GFileType  file_type;
} ListChildrenTyped;

static void
list_children_typed_free (gpointer data)
{
  ListChildrenTyped *state = data;

  g_clear_object (&state->file);
  g_clear_pointer (&state->attributes, g_free);
  g_free (state);
}

static DexFuture *
manuals_list_children_typed_fiber (gpointer user_data)
{
  ListChildrenTyped *state = user_data;
  g_autoptr(GFileEnumerator) enumerator = NULL;
  g_autoptr(GPtrArray) ar = NULL;
  g_autoptr(GError) error = NULL;
  GList *list;

  g_assert (state != NULL);
  g_assert (G_IS_FILE (state->file));

  if (!(enumerator = dex_await_object (dex_file_enumerate_children (state->file,
                                                                    state->attributes,
                                                                    G_FILE_QUERY_INFO_NONE,
                                                                    G_PRIORITY_DEFAULT),
                                       &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  ar = g_ptr_array_new_with_free_func (g_object_unref);

  while ((list = dex_await_boxed (dex_file_enumerator_next_files (enumerator,
                                                                  100,
                                                                  G_PRIORITY_DEFAULT),
                                  &error)))
    {
      for (const GList *iter = list; iter; iter = iter->next)
        {
          GFileInfo *info = iter->data;
          GFileType file_type = g_file_info_get_file_type (info);

          if (file_type == state->file_type)
            g_ptr_array_add (ar, iter->data);
          else
            g_object_unref (iter->data);
        }

      g_list_free (list);
    }

  if (error != NULL)
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_take_boxed (G_TYPE_PTR_ARRAY, g_steal_pointer (&ar));
}

DexFuture *
manuals_list_children_typed (GFile      *file,
                             GFileType   file_type,
                             const char *attributes)
{
  ListChildrenTyped *state;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  state = g_new0 (ListChildrenTyped, 1);
  state->file = g_object_ref (file);
  state->file_type = file_type;

  if (attributes == NULL)
    state->attributes = g_strdup (G_FILE_ATTRIBUTE_STANDARD_NAME","G_FILE_ATTRIBUTE_STANDARD_TYPE);
  else
    state->attributes = g_strdup_printf ("%s,%s,%s",
                                         G_FILE_ATTRIBUTE_STANDARD_NAME,
                                         G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                         attributes);

  return dex_scheduler_spawn (NULL, 0,
                              manuals_list_children_typed_fiber,
                              state,
                              list_children_typed_free);
}
