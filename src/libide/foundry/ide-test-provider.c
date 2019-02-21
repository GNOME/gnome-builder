/* ide-test-provider.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-test-provider"

#include "config.h"

#include "ide-pipeline.h"
#include "ide-test-provider.h"
#include "ide-test-private.h"

typedef struct
{
  GPtrArray *items;
  guint loading : 1;
} IdeTestProviderPrivate;

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (IdeTestProvider, ide_test_provider, IDE_TYPE_OBJECT,
                                  G_ADD_PRIVATE (IdeTestProvider)
                                  G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

enum {
  PROP_0,
  PROP_LOADING,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
ide_test_provider_real_run_async (IdeTestProvider     *self,
                                  IdeTest             *test,
                                  IdePipeline         *pipeline,
                                  VtePty              *pty,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  g_task_report_new_error (self, callback, user_data,
                           ide_test_provider_real_run_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "%s is missing test runner implementation",
                           G_OBJECT_TYPE_NAME (self));
}

static gboolean
ide_test_provider_real_run_finish (IdeTestProvider  *self,
                                   GAsyncResult     *result,
                                   GError          **error)
{
  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
ide_test_provider_dispose (GObject *object)
{
  IdeTestProvider *self = (IdeTestProvider *)object;
  IdeTestProviderPrivate *priv = ide_test_provider_get_instance_private (self);

  if (priv->items != NULL && priv->items->len > 0)
    {
      guint len = priv->items->len;

      g_ptr_array_remove_range (priv->items, 0, len);
      g_list_model_items_changed (G_LIST_MODEL (self), 0, len, 0);
    }

  g_clear_pointer (&priv->items, g_ptr_array_unref);

  G_OBJECT_CLASS (ide_test_provider_parent_class)->dispose (object);
}

static void
ide_test_provider_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  IdeTestProvider *self = IDE_TEST_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_LOADING:
      g_value_set_boolean (value, ide_test_provider_get_loading (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_test_provider_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  IdeTestProvider *self = IDE_TEST_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_LOADING:
      ide_test_provider_set_loading (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_test_provider_class_init (IdeTestProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_test_provider_dispose;
  object_class->get_property = ide_test_provider_get_property;
  object_class->set_property = ide_test_provider_set_property;

  klass->run_async = ide_test_provider_real_run_async;
  klass->run_finish = ide_test_provider_real_run_finish;

  properties [PROP_LOADING] =
    g_param_spec_boolean ("loading",
                          "Loading",
                          "If the provider is loading tests",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_test_provider_init (IdeTestProvider *self)
{
  IdeTestProviderPrivate *priv = ide_test_provider_get_instance_private (self);

  priv->items = g_ptr_array_new_with_free_func (g_object_unref);
}

static GType
ide_test_provider_get_item_type (GListModel *model)
{
  return IDE_TYPE_TEST;
}

static guint
ide_test_provider_get_n_items (GListModel *model)
{
  IdeTestProvider *self = (IdeTestProvider *)model;
  IdeTestProviderPrivate *priv = ide_test_provider_get_instance_private (self);

  g_assert (IDE_IS_TEST_PROVIDER (self));

  return priv->items ? priv->items->len : 0;
}

static gpointer
ide_test_provider_get_item (GListModel *model,
                            guint       position)
{
  IdeTestProvider *self = (IdeTestProvider *)model;
  IdeTestProviderPrivate *priv = ide_test_provider_get_instance_private (self);

  g_assert (IDE_IS_TEST_PROVIDER (self));

  if (priv->items != NULL)
    {
      if (position < priv->items->len)
        return g_object_ref (g_ptr_array_index (priv->items, position));
    }

  return NULL;
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item = ide_test_provider_get_item;
  iface->get_n_items = ide_test_provider_get_n_items;
  iface->get_item_type = ide_test_provider_get_item_type;
}

void
ide_test_provider_add (IdeTestProvider *self,
                       IdeTest         *test)
{
  IdeTestProviderPrivate *priv = ide_test_provider_get_instance_private (self);

  g_return_if_fail (IDE_IS_TEST_PROVIDER (self));
  g_return_if_fail (IDE_IS_TEST (test));

  if (priv->items != NULL)
    {
      g_ptr_array_add (priv->items, g_object_ref (test));
      _ide_test_set_provider (test, self);
      g_list_model_items_changed (G_LIST_MODEL (self), priv->items->len - 1, 0, 1);
    }
}

void
ide_test_provider_remove (IdeTestProvider *self,
                          IdeTest         *test)
{
  IdeTestProviderPrivate *priv = ide_test_provider_get_instance_private (self);

  g_return_if_fail (IDE_IS_TEST_PROVIDER (self));
  g_return_if_fail (IDE_IS_TEST (test));

  if (priv->items != NULL)
    {
      for (guint i = 0; i < priv->items->len; i++)
        {
          IdeTest *element = g_ptr_array_index (priv->items, i);

          if (element == test)
            {
              _ide_test_set_provider (test, NULL);
              g_ptr_array_remove_index (priv->items, i);
              g_list_model_items_changed (G_LIST_MODEL (self), i, 1, 0);
              break;
            }
        }
    }
}

void
ide_test_provider_clear (IdeTestProvider *self)
{
  IdeTestProviderPrivate *priv = ide_test_provider_get_instance_private (self);
  g_autoptr(GPtrArray) ar = NULL;

  g_return_if_fail (IDE_IS_TEST_PROVIDER (self));

  ar = priv->items;
  priv->items = g_ptr_array_new_with_free_func (g_object_unref);

  for (guint i = 0; i < ar->len; i++)
    {
      IdeTest *test = g_ptr_array_index (ar, i);
      _ide_test_set_provider (test, NULL);
    }

  g_list_model_items_changed (G_LIST_MODEL (self), 0, ar->len, 0);
}

void
ide_test_provider_run_async (IdeTestProvider     *self,
                             IdeTest             *test,
                             IdePipeline         *pipeline,
                             VtePty              *pty,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  g_return_if_fail (IDE_IS_TEST_PROVIDER (self));
  g_return_if_fail (IDE_IS_TEST (test));
  g_return_if_fail (IDE_IS_PIPELINE (pipeline));
  g_return_if_fail (!pty || VTE_IS_PTY (pty));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_TEST_PROVIDER_GET_CLASS (self)->run_async (self,
                                                 test,
                                                 pipeline,
                                                 pty,
                                                 cancellable,
                                                 callback,
                                                 user_data);
}

gboolean
ide_test_provider_run_finish (IdeTestProvider  *self,
                              GAsyncResult     *result,
                              GError          **error)
{
  g_return_val_if_fail (IDE_IS_TEST_PROVIDER (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_TEST_PROVIDER_GET_CLASS (self)->run_finish (self, result, error);
}

gboolean
ide_test_provider_get_loading (IdeTestProvider *self)
{
  IdeTestProviderPrivate *priv = ide_test_provider_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TEST_PROVIDER (self), FALSE);

  return priv->loading;
}

void
ide_test_provider_set_loading (IdeTestProvider *self,
                               gboolean         loading)
{
  IdeTestProviderPrivate *priv = ide_test_provider_get_instance_private (self);

  g_return_if_fail (IDE_IS_TEST_PROVIDER (self));

  loading = !!loading;

  if (priv->loading != loading)
    {
      priv->loading = loading;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LOADING]);
    }
}

/**
 * ide_test_provider_reload:
 * @self: a #IdeTestProvider
 *
 * Requests the test provider reloads the tests.
 *
 * Since: 3.32
 */
void
ide_test_provider_reload (IdeTestProvider *self)
{
  g_return_if_fail (IDE_IS_TEST_PROVIDER (self));

  if (IDE_TEST_PROVIDER_GET_CLASS (self)->reload)
    IDE_TEST_PROVIDER_GET_CLASS (self)->reload (self);
}
