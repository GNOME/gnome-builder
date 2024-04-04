/*
 * manuals-gom.h
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

#include <gom/gom.h>
#include <libdex.h>

G_BEGIN_DECLS

static inline void
gom_adapter_open_cb (GObject      *object,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  GomAdapter *adapter = (GomAdapter *)object;
  g_autoptr(DexPromise) promise = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (GOM_IS_ADAPTER (adapter));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (DEX_IS_PROMISE (promise));

  if (gom_adapter_open_finish (adapter, result, &error))
    dex_promise_resolve_boolean (promise, TRUE);
  else
    dex_promise_reject (promise, g_steal_pointer (&error));
}

static inline DexFuture *
gom_adapter_open (GomAdapter *adapter,
                  const char *uri)
{
  DexPromise *future = dex_promise_new ();
  gom_adapter_open_async (adapter,
                          uri,
                          gom_adapter_open_cb,
                          dex_ref (future));
  return DEX_FUTURE (future);
}

static inline void
gom_adapter_close_cb (GObject      *object,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  GomAdapter *adapter = (GomAdapter *)object;
  g_autoptr(DexPromise) promise = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (GOM_IS_ADAPTER (adapter));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (DEX_IS_PROMISE (promise));

  if (gom_adapter_close_finish (adapter, result, &error))
    dex_promise_resolve_boolean (promise, TRUE);
  else
    dex_promise_reject (promise, g_steal_pointer (&error));
}

static inline DexFuture *
gom_adapter_close (GomAdapter *adapter)
{
  DexPromise *future = dex_promise_new ();
  gom_adapter_close_async (adapter,
                           gom_adapter_close_cb,
                           dex_ref (future));
  return DEX_FUTURE (future);
}

static inline void
gom_repository_automatic_migrate_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  GomRepository *repository = (GomRepository *)object;
  g_autoptr(DexPromise) promise = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (GOM_IS_REPOSITORY (repository));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (DEX_IS_PROMISE (promise));

  if (gom_repository_automatic_migrate_finish (repository, result, &error))
    dex_promise_resolve_boolean (promise, TRUE);
  else
    dex_promise_reject (promise, g_steal_pointer (&error));
}

static inline DexFuture *
gom_repository_automatic_migrate (GomRepository *repository,
                                  guint          version,
                                  GList         *object_types)
{
  DexPromise *future = dex_promise_new ();
  gom_repository_automatic_migrate_async (repository,
                                          version,
                                          object_types,
                                          gom_repository_automatic_migrate_cb,
                                          dex_ref (future));
  return DEX_FUTURE (future);
}

static inline void
gom_repository_find_cb (GObject      *object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  GomRepository *repository = (GomRepository *)object;
  g_autoptr(DexPromise) promise = user_data;
  g_autoptr(GomResourceGroup) resource_group = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (GOM_IS_REPOSITORY (repository));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (DEX_IS_PROMISE (promise));

  if (!(resource_group = gom_repository_find_finish (repository, result, &error)))
    dex_promise_reject (promise, g_steal_pointer (&error));
  else
    dex_promise_resolve_object (promise, g_steal_pointer (&resource_group));
}

static inline DexFuture *
gom_repository_find (GomRepository *repository,
                     GType          resource_type,
                     GomFilter     *filter)
{
  DexPromise *future = dex_promise_new ();
  gom_repository_find_async (repository,
                             resource_type,
                             filter,
                             gom_repository_find_cb,
                             dex_ref (future));
  return DEX_FUTURE (future);
}

static inline DexFuture *
gom_repository_find_sorted (GomRepository *repository,
                            GType          resource_type,
                            GomFilter     *filter,
                            GomSorting    *sorting)
{
  DexPromise *future = dex_promise_new ();
  gom_repository_find_sorted_async (repository,
                                    resource_type,
                                    filter,
                                    sorting,
                                    gom_repository_find_cb,
                                    dex_ref (future));
  return DEX_FUTURE (future);
}

static inline void
gom_resource_group_fetch_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  GomResourceGroup *resource_group = (GomResourceGroup *)object;
  g_autoptr(GomResourceGroup) hold = NULL;
  g_autoptr(DexPromise) promise = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (GOM_IS_RESOURCE_GROUP (resource_group));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (DEX_IS_PROMISE (promise));

  hold = g_object_ref (resource_group);

  if (!gom_resource_group_fetch_finish (resource_group, result, &error))
    dex_promise_reject (promise, g_steal_pointer (&error));
  else
    dex_promise_resolve_object (promise, g_steal_pointer (&hold));
}

static inline DexFuture *
gom_resource_group_fetch (GomResourceGroup *resource_group,
                          guint             index,
                          guint             count)
{
  DexPromise *future = dex_promise_new ();
  gom_resource_group_fetch_async (resource_group,
                                  index,
                                  count,
                                  gom_resource_group_fetch_cb,
                                  dex_ref (future));
  return DEX_FUTURE (future);
}

typedef struct _GomResourceGroupFetchItem
{
  GomResourceGroup *group;
  guint index;
} GomResourceGroupFetchItem;

static inline void
gom_resource_group_fetch_item_free (GomResourceGroupFetchItem *item)
{
  g_clear_object (&item->group);
  g_free (item);
}

static DexFuture *
gom_resource_group_fetch_item_cb (DexFuture *completed,
                                  gpointer   user_data)
{
  GomResourceGroupFetchItem *fetch = user_data;

  return dex_future_new_take_object (gom_resource_group_get_index (fetch->group, fetch->index));
}

static inline DexFuture *
gom_resource_group_fetch_item (GomResourceGroup *resource_group,
                               guint             index)
{
  GomResourceGroupFetchItem *fetch;
  DexFuture *future;

  fetch = g_new0 (GomResourceGroupFetchItem, 1);
  fetch->group = g_object_ref (resource_group);
  fetch->index = index;

  future = gom_resource_group_fetch (resource_group, index, 1);
  future = dex_future_then (future,
                            gom_resource_group_fetch_item_cb,
                            fetch,
                            (GDestroyNotify)gom_resource_group_fetch_item_free);

  return future;
}

static inline void
gom_resource_save_cb (GObject      *object,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  GomResource *resource = (GomResource *)object;
  g_autoptr(DexPromise) promise = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (GOM_IS_RESOURCE (resource));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (DEX_IS_PROMISE (promise));

  if (gom_resource_save_finish (resource, result, &error))
    dex_promise_resolve_boolean (promise, TRUE);
  else
    dex_promise_reject (promise, g_steal_pointer (&error));
}

static inline DexFuture *
gom_resource_save (GomResource *resource)
{
  DexPromise *future = dex_promise_new ();
  gom_resource_save_async (resource,
                           gom_resource_save_cb,
                           dex_ref (future));
  return DEX_FUTURE (future);
}

static inline void
gom_resource_delete_cb (GObject      *object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  GomResource *resource = (GomResource *)object;
  g_autoptr(DexPromise) promise = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (GOM_IS_RESOURCE (resource));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (DEX_IS_PROMISE (promise));

  if (gom_resource_delete_finish (resource, result, &error))
    dex_promise_resolve_boolean (promise, TRUE);
  else
    dex_promise_reject (promise, g_steal_pointer (&error));
}

static inline DexFuture *
gom_resource_delete (GomResource *resource)
{
  DexPromise *future = dex_promise_new ();
  gom_resource_delete_async (resource,
                           gom_resource_delete_cb,
                           dex_ref (future));
  return DEX_FUTURE (future);
}

static inline void
gom_resource_group_delete_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  GomResourceGroup *resource_group = (GomResourceGroup *)object;
  g_autoptr(DexPromise) promise = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (GOM_IS_RESOURCE_GROUP (resource_group));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (DEX_IS_PROMISE (promise));

  if (!gom_resource_group_delete_finish (resource_group, result, &error))
    dex_promise_reject (promise, g_steal_pointer (&error));
  else
    dex_promise_resolve_boolean (promise, TRUE);
}

static inline DexFuture *
gom_resource_group_delete (GomResourceGroup *resource_group)
{
  DexPromise *future = dex_promise_new ();
  gom_resource_group_delete_async (resource_group,
                                   gom_resource_group_delete_cb,
                                   dex_ref (future));
  return DEX_FUTURE (future);
}

static inline void
gom_resource_group_write_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  GomResourceGroup *resource_group = (GomResourceGroup *)object;
  g_autoptr(DexPromise) promise = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (GOM_IS_RESOURCE_GROUP (resource_group));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (DEX_IS_PROMISE (promise));

  if (!gom_resource_group_write_finish (resource_group, result, &error))
    dex_promise_reject (promise, g_steal_pointer (&error));
  else
    dex_promise_resolve_boolean (promise, TRUE);
}

static inline DexFuture *
gom_resource_group_write (GomResourceGroup *resource_group)
{
  DexPromise *future = dex_promise_new ();
  gom_resource_group_write_async (resource_group,
                                  gom_resource_group_write_cb,
                                  dex_ref (future));
  return DEX_FUTURE (future);
}

G_END_DECLS
