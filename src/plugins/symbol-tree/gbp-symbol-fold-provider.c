/*
 * gbp-symbol-fold-provider.c
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

#include <libdex.h>

#include <libide-threading.h>

#include "gbp-symbol-fold-provider.h"

struct _GbpSymbolFoldProvider
{
  IdeFoldProvider parent_instance;
};

typedef struct
{
  IdeBuffer *buffer;
  GPtrArray *resolvers;
  GFile     *file;
  GBytes    *bytes;
} ListRegions;

G_DEFINE_FINAL_TYPE (GbpSymbolFoldProvider, gbp_symbol_fold_provider, IDE_TYPE_FOLD_PROVIDER)

static void
list_regions_free (ListRegions *state)
{
  g_clear_pointer (&state->resolvers, g_ptr_array_unref);
  g_clear_pointer (&state->bytes, g_bytes_unref);
  g_clear_object (&state->buffer);
  g_clear_object (&state->file);
  g_free (state);
}

static DexFuture *
gbp_symbol_fold_provider_list_regions_fiber (gpointer user_data)
{
  ListRegions *state = user_data;
  g_autoptr(IdeSymbolTree) tree = NULL;

  g_assert (state != NULL);
  g_assert (IDE_IS_BUFFER (state->buffer));

  if (state->resolvers == NULL || state->resolvers->len == 0)
    goto return_empty;

  for (guint i = 0; i < state->resolvers->len; i++)
    {
      IdeSymbolResolver *resolver = g_ptr_array_index (state->resolvers, i);

      if ((tree = dex_await_object (ide_symbol_resolver_get_symbol_tree (resolver, state->file, state->bytes), NULL)))
        break;
    }

  if (tree != NULL)
    {
      g_autoptr(IdeFoldRegionsBuilder) builder = ide_fold_regions_builder_new (GTK_TEXT_BUFFER (state->buffer));
      g_autoptr(GPtrArray) futures = g_ptr_array_new_with_free_func (dex_unref);
      guint n_children = ide_symbol_tree_get_n_children (tree, NULL);

      /* TODO: Recursive nodes */

      for (guint nth = 0; nth < n_children; nth++)
        {
          g_autoptr(IdeSymbolNode) node = ide_symbol_tree_get_nth_child (tree, NULL, nth);

          /* TODO: Need a location range for the symbol */

          if (node != NULL)
            g_ptr_array_add (futures, ide_symbol_node_get_location (node));
        }

      if (futures->len > 0)
        dex_await (dex_future_allv ((DexFuture **)futures->pdata, futures->len), NULL);

      for (guint i = 0; i < futures->len; i++)
        {
          DexFuture *future = g_ptr_array_index (futures, i);
          const GValue *value = dex_future_get_value (future, NULL);

          if (value != NULL && G_VALUE_HOLDS (value, IDE_TYPE_LOCATION))
            {
              IdeLocation *location = g_value_get_object (value);

              g_print ("Line: %u Offset: %u\n",
                       ide_location_get_line (location),
                       ide_location_get_line_offset (location));
            }
        }

      return dex_future_new_take_object (ide_fold_regions_builder_build (builder));
    }

return_empty:
  return dex_future_new_take_object (g_object_new (IDE_TYPE_FOLD_REGIONS, NULL));
}

static void
gbp_symbol_fold_provider_list_regions_async (IdeFoldProvider     *provider,
                                             IdeBuffer           *buffer,
                                             GCancellable        *cancellable,
                                             GAsyncReadyCallback  callback,
                                             gpointer             user_data)
{
  GbpSymbolFoldProvider *self = (GbpSymbolFoldProvider *)provider;
  g_autoptr(DexAsyncResult) result = NULL;
  ListRegions *state;

  IDE_ENTRY;

  g_assert (GBP_IS_SYMBOL_FOLD_PROVIDER (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  state = g_new0 (ListRegions, 1);
  state->resolvers = ide_buffer_get_symbol_resolvers (buffer);
  state->buffer = g_object_ref (buffer);
  state->bytes = ide_buffer_dup_content (buffer);
  state->file = g_object_ref (ide_buffer_get_file (buffer));

  g_ptr_array_set_free_func (state->resolvers, g_object_unref);

  result = dex_async_result_new (self, cancellable, callback, user_data);
  dex_async_result_await (result,
                          dex_scheduler_spawn (NULL, 0,
                                               gbp_symbol_fold_provider_list_regions_fiber,
                                               state,
                                               (GDestroyNotify)list_regions_free));

  IDE_EXIT;
}

static IdeFoldRegions *
gbp_symbol_fold_provider_list_regions_finish (IdeFoldProvider  *provider,
                                              GAsyncResult     *result,
                                              GError          **error)
{
  IdeFoldRegions *ret;

  IDE_ENTRY;

  g_assert (GBP_IS_SYMBOL_FOLD_PROVIDER (provider));
  g_assert (DEX_IS_ASYNC_RESULT (result));

  ret = dex_async_result_propagate_pointer (DEX_ASYNC_RESULT (result), error);

  IDE_RETURN (ret);
}

static void
gbp_symbol_fold_provider_destroy (IdeObject *object)
{
  IDE_OBJECT_CLASS (gbp_symbol_fold_provider_parent_class)->destroy (object);
}

static void
gbp_symbol_fold_provider_class_init (GbpSymbolFoldProviderClass *klass)
{
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);
  IdeFoldProviderClass *fold_provider_class = IDE_FOLD_PROVIDER_CLASS (klass);

  i_object_class->destroy = gbp_symbol_fold_provider_destroy;

  fold_provider_class->list_regions_async = gbp_symbol_fold_provider_list_regions_async;
  fold_provider_class->list_regions_finish = gbp_symbol_fold_provider_list_regions_finish;
}

static void
gbp_symbol_fold_provider_init (GbpSymbolFoldProvider *self)
{
}
