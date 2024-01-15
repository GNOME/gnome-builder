/*
 * ide-runtime-provider.c
 *
 * Copyright 2016-2023 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-runtime-provider"

#include "config.h"

#include <glib/gi18n.h>

#include "ide-runtime-provider.h"

#include "ide-pair-private.h"

typedef struct
{
  GListStore *runtimes;
  DexFuture  *loaded;
} IdeRuntimeProviderPrivate;

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (IdeRuntimeProvider, ide_runtime_provider, IDE_TYPE_OBJECT,
                                  G_ADD_PRIVATE (IdeRuntimeProvider)
                                  G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static DexFuture *
ide_runtime_provider_real_load (IdeRuntimeProvider *provider)
{
  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "Not supported");
}

static DexFuture *
ide_runtime_provider_real_unload (IdeRuntimeProvider *provider)
{
  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "Not supported");
}

static DexFuture *
ide_runtime_provider_real_bootstrap_runtime (IdeRuntimeProvider *provider,
                                             IdePipeline        *pipeline)
{
  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "Not supported");
}

static void
ide_runtime_provider_items_changed_cb (IdeRuntimeProvider *self,
                                       guint               position,
                                       guint               removed,
                                       guint               added,
                                       GListModel         *model)
{
  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_RUNTIME_PROVIDER (self));
  g_assert (G_IS_LIST_STORE (model));

  g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);

  IDE_EXIT;
}

static void
ide_runtime_provider_destroy (IdeObject *object)
{
  IdeRuntimeProvider *self = (IdeRuntimeProvider *)object;
  IdeRuntimeProviderPrivate *priv = ide_runtime_provider_get_instance_private (self);

  IDE_ENTRY;

  dex_clear (&priv->loaded);

  g_list_store_remove_all (G_LIST_STORE (priv->runtimes));

  IDE_OBJECT_CLASS (ide_runtime_provider_parent_class)->destroy (object);

  IDE_EXIT;
}

static void
ide_runtime_provider_finalize (GObject *object)
{
  IdeRuntimeProvider *self = (IdeRuntimeProvider *)object;
  IdeRuntimeProviderPrivate *priv = ide_runtime_provider_get_instance_private (self);

  g_clear_object (&priv->runtimes);

  G_OBJECT_CLASS (ide_runtime_provider_parent_class)->finalize (object);
}

static void
ide_runtime_provider_class_init (IdeRuntimeProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  object_class->finalize = ide_runtime_provider_finalize;

  i_object_class->destroy = ide_runtime_provider_destroy;

  klass->load = ide_runtime_provider_real_load;
  klass->unload = ide_runtime_provider_real_unload;
  klass->bootstrap_runtime = ide_runtime_provider_real_bootstrap_runtime;
}

static void
ide_runtime_provider_init (IdeRuntimeProvider *self)
{
  IdeRuntimeProviderPrivate *priv = ide_runtime_provider_get_instance_private (self);

  priv->runtimes = g_list_store_new (IDE_TYPE_RUNTIME);

  g_signal_connect_object (priv->runtimes,
                           "items-changed",
                           G_CALLBACK (ide_runtime_provider_items_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

void
ide_runtime_provider_add (IdeRuntimeProvider *self,
                          IdeRuntime         *runtime)
{
  IdeRuntimeProviderPrivate *priv = ide_runtime_provider_get_instance_private (self);

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_RUNTIME_PROVIDER (self));
  g_return_if_fail (IDE_IS_RUNTIME (runtime));

  ide_object_debug (self,
                    _("Discovered runtime “%s”"),
                    ide_runtime_get_id (runtime));

  ide_object_append (IDE_OBJECT (self), IDE_OBJECT (runtime));
  g_list_store_append (priv->runtimes, runtime);

  IDE_EXIT;
}

void
ide_runtime_provider_remove (IdeRuntimeProvider *self,
                             IdeRuntime         *runtime)
{
  IdeRuntimeProviderPrivate *priv = ide_runtime_provider_get_instance_private (self);
  guint n_items;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_RUNTIME_PROVIDER (self));
  g_return_if_fail (IDE_IS_RUNTIME (runtime));

  ide_object_debug (self,
                    _("Removing runtime “%s”"),
                    ide_runtime_get_id (runtime));

  n_items = g_list_model_get_n_items (G_LIST_MODEL (priv->runtimes));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(IdeRuntime) item = g_list_model_get_item (G_LIST_MODEL (priv->runtimes), i);

      if (runtime == item)
        {
          g_list_store_remove (priv->runtimes, i);
          ide_object_destroy (IDE_OBJECT (runtime));
          break;
        }
    }

  IDE_EXIT;
}

/**
 * ide_runtime_provider_load:
 * @self: a #IdeRuntimeProvider
 *
 * Returns: (transfer full): a #DexFuture
 */
DexFuture *
ide_runtime_provider_load (IdeRuntimeProvider *self)
{
  IdeRuntimeProviderPrivate *priv = ide_runtime_provider_get_instance_private (self);

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_RUNTIME_PROVIDER (self), NULL);

  if (priv->loaded == NULL)
    priv->loaded = IDE_RUNTIME_PROVIDER_GET_CLASS (self)->load (self);

  IDE_RETURN (dex_ref (priv->loaded));
}

static DexFuture *
ide_runtime_provider_unload_cb (DexFuture *completed,
                                gpointer   user_data)
{
  IdeRuntimeProvider *self = user_data;
  g_autoptr(DexFuture) ret = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_RUNTIME_PROVIDER (self));

  ret = IDE_RUNTIME_PROVIDER_GET_CLASS (self)->unload (self);

  IDE_RETURN (g_steal_pointer (&ret));
}

/**
 * ide_runtime_provider_unload:
 * @self: a #IdeRuntimeProvider
 *
 * Returns: (transfer full): a #DexFuture
 */
DexFuture *
ide_runtime_provider_unload (IdeRuntimeProvider *self)
{
  IdeRuntimeProviderPrivate *priv = ide_runtime_provider_get_instance_private (self);
  DexFuture *ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_RUNTIME_PROVIDER (self), NULL);

  /* Don't unload until we've finished loading */
  ret = dex_future_finally (dex_ref (priv->loaded),
                            ide_runtime_provider_unload_cb,
                            g_object_ref (self),
                            g_object_unref);

  IDE_RETURN (ret);
}

static DexFuture *
ide_runtime_provider_bootstrap_runtime_cb (DexFuture *future,
                                           gpointer   user_data)
{
  IdePair *pair = user_data;
  g_autoptr(DexFuture) ret = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (DEX_IS_FUTURE (future));
  g_assert (pair != NULL);
  g_assert (IDE_IS_RUNTIME_PROVIDER (pair->a));
  g_assert (IDE_IS_PIPELINE (pair->b));

  ret = IDE_RUNTIME_PROVIDER_GET_CLASS (pair->a)->bootstrap_runtime (pair->a, pair->b);

  IDE_RETURN (g_steal_pointer (&ret));
}

/**
 * ide_runtime_provider_bootstrap_runtime:
 * @self: an #IdeRuntimeProvider
 * @pipeline: an #IdePipeline
 *
 * Locates and installs the necessary runtime for @pipeline if possible.
 *
 * The future must either resolve with an #IdeRuntime or reject with error.
 *
 * Returns: (transfer full): a #DexFuture that resolves an #IdeRuntime or
 *   rejects with error.
 */
DexFuture *
ide_runtime_provider_bootstrap_runtime (IdeRuntimeProvider *self,
                                        IdePipeline        *pipeline)
{
  IdeRuntimeProviderPrivate *priv = ide_runtime_provider_get_instance_private (self);
  g_autoptr(DexFuture) ret = NULL;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_RUNTIME_PROVIDER (self), NULL);
  g_return_val_if_fail (IDE_IS_PIPELINE (pipeline), NULL);

  /* Don't bootstrap until we've finished loading */
  ret = dex_future_then (dex_ref (priv->loaded),
                         ide_runtime_provider_bootstrap_runtime_cb,
                         ide_pair_new (self, pipeline),
                         (GDestroyNotify) ide_pair_unref);

  IDE_RETURN (g_steal_pointer (&ret));
}

gboolean
ide_runtime_provider_provides (IdeRuntimeProvider *self,
                               const char         *runtime_id)
{
  g_return_val_if_fail (IDE_IS_RUNTIME_PROVIDER (self), FALSE);
  g_return_val_if_fail (runtime_id != NULL, FALSE);

  if (IDE_RUNTIME_PROVIDER_GET_CLASS (self)->provides)
    return IDE_RUNTIME_PROVIDER_GET_CLASS (self)->provides (self, runtime_id);

  return FALSE;
}

static GType
ide_runtime_provider_get_item_type (GListModel *model)
{
  return IDE_TYPE_RUNTIME;
}

static guint
ide_runtime_provider_get_n_items (GListModel *model)
{
  IdeRuntimeProvider *self = IDE_RUNTIME_PROVIDER (model);
  IdeRuntimeProviderPrivate *priv = ide_runtime_provider_get_instance_private (self);

  if (priv->runtimes != NULL)
    return g_list_model_get_n_items (G_LIST_MODEL (priv->runtimes));

  return 0;
}

static gpointer
ide_runtime_provider_get_item (GListModel *model,
                               guint       position)
{
  IdeRuntimeProvider *self = IDE_RUNTIME_PROVIDER (model);
  IdeRuntimeProviderPrivate *priv = ide_runtime_provider_get_instance_private (self);

  if (priv->runtimes != NULL)
    return g_list_model_get_item (G_LIST_MODEL (priv->runtimes), position);

  return NULL;
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_n_items = ide_runtime_provider_get_n_items;
  iface->get_item = ide_runtime_provider_get_item;
  iface->get_item_type = ide_runtime_provider_get_item_type;
}
