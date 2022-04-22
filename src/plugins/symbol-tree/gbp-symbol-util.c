/* gbp-symbol-util.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-symbol-util"

#include "config.h"

#include <libide-threading.h>

#include "gbp-symbol-util.h"

typedef struct
{
  GPtrArray   *resolvers;
  IdeBuffer   *buffer;
  IdeLocation *location;
} FindNearestScope;

typedef struct
{
  GPtrArray *resolvers;
  IdeBuffer *buffer;
  GFile     *file;
  GBytes    *contents;
} GetSymbolTree;

static void
get_symbol_tree_free (GetSymbolTree *data)
{
  g_assert (data != NULL);
  g_assert (data->resolvers != NULL);
  g_assert (data->buffer != NULL);
  g_assert (IDE_IS_BUFFER (data->buffer));
  g_assert (G_IS_FILE (data->file));
  g_assert (data->contents != NULL);

  g_clear_pointer (&data->resolvers, g_ptr_array_unref);
  g_clear_pointer (&data->buffer, ide_buffer_release);
  g_clear_pointer (&data->contents, g_bytes_unref);
  g_clear_object (&data->file);
  g_slice_free (GetSymbolTree, data);
}

static void
find_nearest_scope_free (FindNearestScope *data)
{
  g_assert (data != NULL);
  g_assert (data->resolvers != NULL);
  g_assert (data->buffer != NULL);
  g_assert (IDE_IS_BUFFER (data->buffer));
  g_assert (IDE_IS_LOCATION (data->location));

  g_clear_pointer (&data->resolvers, g_ptr_array_unref);
  g_clear_pointer (&data->buffer, ide_buffer_release);
  g_clear_object (&data->location);
  g_slice_free (FindNearestScope, data);
}

static void
gbp_symbol_find_nearest_scope_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  IdeSymbolResolver *resolver = (IdeSymbolResolver *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(IdeSymbol) symbol = NULL;
  g_autoptr(GError) error = NULL;
  FindNearestScope *data;

  IDE_ENTRY;

  g_assert (IDE_IS_SYMBOL_RESOLVER (resolver));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if ((symbol = ide_symbol_resolver_find_nearest_scope_finish (resolver, result, &error)))
    {
      ide_task_return_object (task, g_steal_pointer (&symbol));
      IDE_EXIT;
    }

  data = ide_task_get_task_data (task);

  g_assert (data != NULL);
  g_assert (data->resolvers != NULL);
  g_assert (data->resolvers->len > 0);
  g_assert (IDE_IS_LOCATION (data->location));
  g_assert (IDE_IS_BUFFER (data->buffer));

  g_ptr_array_remove_index (data->resolvers,
                            data->resolvers->len - 1);

  if (data->resolvers->len == 0)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "No resolvers could locate the nearest scope");
      IDE_EXIT;
    }

  ide_symbol_resolver_find_nearest_scope_async (g_ptr_array_index (data->resolvers, data->resolvers->len - 1),
                                                data->location,
                                                ide_task_get_cancellable (task),
                                                gbp_symbol_find_nearest_scope_cb,
                                                g_object_ref (task));

  IDE_EXIT;
}

void
gbp_symbol_find_nearest_scope_async (IdeBuffer           *buffer,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GPtrArray) resolvers = NULL;
  FindNearestScope *data;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_BUFFER (buffer));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (buffer, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_symbol_find_nearest_scope_async);

  resolvers = ide_buffer_get_symbol_resolvers (buffer);
  IDE_PTR_ARRAY_SET_FREE_FUNC (resolvers, g_object_unref);

  if (resolvers == NULL || resolvers->len == 0)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "No symbol resolvers available");
      IDE_EXIT;
    }

  data = g_slice_new0 (FindNearestScope);
  data->resolvers = g_steal_pointer (&resolvers);
  data->buffer = ide_buffer_hold (buffer);
  data->location = ide_buffer_get_insert_location (buffer);
  ide_task_set_task_data (task, data, find_nearest_scope_free);

  ide_symbol_resolver_find_nearest_scope_async (g_ptr_array_index (data->resolvers, data->resolvers->len - 1),
                                                data->location,
                                                cancellable,
                                                gbp_symbol_find_nearest_scope_cb,
                                                g_steal_pointer (&task));

  IDE_EXIT;
}

IdeSymbol *
gbp_symbol_find_nearest_scope_finish (IdeBuffer     *buffer,
                                      GAsyncResult  *result,
                                      GError       **error)
{
  IdeSymbol *ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_BUFFER (buffer), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  ret = ide_task_propagate_object (IDE_TASK (result), error);

  g_return_val_if_fail (!ret || IDE_IS_SYMBOL (ret), NULL);

  IDE_RETURN (ret);
}

static void
gbp_symbol_get_symbol_tree_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  IdeSymbolResolver *resolver = (IdeSymbolResolver *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(IdeSymbolTree) tree = NULL;
  g_autoptr(GError) error = NULL;
  GetSymbolTree *data;

  IDE_ENTRY;

  g_assert (IDE_IS_SYMBOL_RESOLVER (resolver));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if ((tree = ide_symbol_resolver_get_symbol_tree_finish (resolver, result, &error)))
    {
      ide_task_return_object (task, g_steal_pointer (&tree));
      IDE_EXIT;
    }

  data = ide_task_get_task_data (task);

  g_assert (data != NULL);
  g_assert (data->resolvers != NULL);
  g_assert (data->resolvers->len > 0);
  g_assert (IDE_IS_BUFFER (data->buffer));
  g_assert (G_IS_FILE (data->file));
  g_assert (data->contents != NULL);

  g_ptr_array_remove_index (data->resolvers,
                            data->resolvers->len - 1);

  if (data->resolvers->len == 0)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "No resolvers could create a symbol tree");
      IDE_EXIT;
    }

  ide_symbol_resolver_get_symbol_tree_async (g_ptr_array_index (data->resolvers, data->resolvers->len - 1),
                                             data->file,
                                             data->contents,
                                             ide_task_get_cancellable (task),
                                             gbp_symbol_get_symbol_tree_cb,
                                             g_object_ref (task));

  IDE_EXIT;
}

void
gbp_symbol_get_symbol_tree_async (IdeBuffer           *buffer,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GPtrArray) resolvers = NULL;
  g_autoptr(GBytes) contents = NULL;
  GetSymbolTree *data;
  GFile *file;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_BUFFER (buffer));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (buffer, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_symbol_get_symbol_tree_async);

  file = ide_buffer_get_file (buffer);
  contents = ide_buffer_dup_content (buffer);

  if (file == NULL || contents == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "Symbol resolvers require both a file and buffer contents");
      IDE_EXIT;
    }

  resolvers = ide_buffer_get_symbol_resolvers (buffer);
  IDE_PTR_ARRAY_SET_FREE_FUNC (resolvers, g_object_unref);

  if (resolvers == NULL || resolvers->len == 0)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "No symbol resolvers available");
      IDE_EXIT;
    }

  data = g_slice_new0 (GetSymbolTree);
  data->resolvers = g_steal_pointer (&resolvers);
  data->buffer = ide_buffer_hold (buffer);
  data->file = g_object_ref (file);
  data->contents = g_steal_pointer (&contents);
  ide_task_set_task_data (task, data, get_symbol_tree_free);

  ide_symbol_resolver_get_symbol_tree_async (g_ptr_array_index (data->resolvers, data->resolvers->len - 1),
                                             data->file,
                                             data->contents,
                                             cancellable,
                                             gbp_symbol_get_symbol_tree_cb,
                                             g_steal_pointer (&task));

  IDE_EXIT;
}

IdeSymbolTree *
gbp_symbol_get_symbol_tree_finish (IdeBuffer     *buffer,
                                   GAsyncResult  *result,
                                   GError       **error)
{
  IdeSymbolTree *ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_BUFFER (buffer), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  ret = ide_task_propagate_object (IDE_TASK (result), error);

  g_return_val_if_fail (!ret || IDE_IS_SYMBOL_TREE (ret), NULL);

  IDE_RETURN (ret);
}
