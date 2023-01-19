/* code-result-set.c
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "config.h"

#include "code-query-private.h"
#include "code-result.h"
#include "code-result-set.h"

#define BATCH_SIZE 100

struct _CodeResultSet
{
  GObject        parent_instance;
  GPtrArray     *matched;
  CodeQuery     *query;
  CodeIndex    **indexes;
  DexChannel    *channel;
  DexFuture     *receiver;
  DexScheduler  *scheduler;
  guint          n_indexes;
  guint          in_populate : 1;
  guint          did_populate : 1;
};

static guint
code_result_set_get_n_items (GListModel *model)
{
  return CODE_RESULT_SET (model)->matched->len;
}

static GType
code_result_set_get_item_type (GListModel *model)
{
  return CODE_TYPE_RESULT;
}

static gpointer
code_result_set_get_item (GListModel *model,
                          guint       position)
{
  CodeResultSet *self = CODE_RESULT_SET (model);

  if (position >= self->matched->len)
    return NULL;

  return g_object_ref (g_ptr_array_index (self->matched, position));
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_n_items = code_result_set_get_n_items;
  iface->get_item_type = code_result_set_get_item_type;
  iface->get_item = code_result_set_get_item;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (CodeResultSet, code_result_set, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

enum {
  PROP_0,
  PROP_N_ITEMS,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

CodeResultSet *
code_result_set_new (CodeQuery         *query,
                     CodeIndex * const *indexes,
                     guint              n_indexes)
{
  CodeResultSet *self;

  g_return_val_if_fail (CODE_IS_QUERY (query), NULL);
  g_return_val_if_fail (indexes != NULL || n_indexes == 0, NULL);

  self = g_object_new (CODE_TYPE_RESULT_SET, NULL);
  self->query = g_object_ref (query);
  self->indexes = g_new0 (CodeIndex *, n_indexes);
  self->n_indexes = n_indexes;
  self->channel = dex_channel_new (G_MAXINT);

  for (guint i = 0; i < n_indexes; i++)
    self->indexes[i] = code_index_ref (indexes[i]);

  return self;
}

static void
code_result_set_finalize (GObject *object)
{
  CodeResultSet *self = (CodeResultSet *)object;

  dex_clear (&self->receiver);
  dex_clear (&self->scheduler);

  for (guint i = 0; i < self->n_indexes; i++)
    g_clear_pointer (&self->indexes[i], code_index_unref);

  g_clear_pointer (&self->indexes, g_free);
  g_clear_pointer (&self->matched, g_ptr_array_unref);
  g_clear_object (&self->query);

  G_OBJECT_CLASS (code_result_set_parent_class)->finalize (object);
}

static void
code_result_set_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  CodeResultSet *self = CODE_RESULT_SET (object);

  switch (prop_id)
    {
    case PROP_N_ITEMS:
      g_value_set_uint (value, g_list_model_get_n_items (G_LIST_MODEL (self)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
code_result_set_class_init (CodeResultSetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = code_result_set_finalize;
  object_class->get_property = code_result_set_get_property;

  properties [PROP_N_ITEMS] =
    g_param_spec_uint ("n-items", NULL, NULL, 0, G_MAXUINT, 0,
                       (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
code_result_set_init (CodeResultSet *self)
{
  self->matched = g_ptr_array_new_with_free_func (g_object_unref);
}

static inline const char *
next_document (CodeIndexIter *iters,
               guint          n_iters)
{
  CodeDocument document;

again:
  if (!code_index_iter_next (&iters[0], &document))
    return FALSE;

  for (guint i = 1; i < n_iters; i++)
    {
      if (!code_index_iter_seek_to (&iters[i], document.id))
        goto again;
    }

  return document.path;
}

static DexFuture *
code_result_set_populate_from_index (CodeResultSet *self,
                                     CodeIndex     *index,
                                     const guint   *trigrams,
                                     guint          n_trigrams)
{
  g_autofree CodeIndexIter *freeme = NULL;
  g_autoptr(GPtrArray) futures = NULL;
  g_autoptr(GError) error = NULL;
  CodeIndexIter *iters;
  const char *path;

  g_assert (CODE_IS_RESULT_SET (self));
  g_assert (index != NULL);
  g_assert (trigrams != NULL);
  g_assert (n_trigrams > 0);

  if (n_trigrams < 32)
    iters = g_newa (CodeIndexIter, n_trigrams);
  else
    iters = freeme = g_new (CodeIndexIter, n_trigrams);

  for (guint i = 0; i < n_trigrams; i++)
    {
      CodeTrigram trigram = code_trigram_decode (trigrams[i]);

      if (!code_index_iter_init (&iters[i], index, &trigram))
        return dex_future_new_for_boolean (TRUE);
    }

  futures = g_ptr_array_new_with_free_func (dex_unref);

next_batch:
  for (guint i = 0; i < BATCH_SIZE; i++)
    {
      if (!(path = next_document (iters, n_trigrams)))
        break;

      g_ptr_array_add (futures, _code_query_match (self->query,
                                                   index,
                                                   path,
                                                   self->channel,
                                                   self->scheduler));
    }

  if (futures->len > 0)
    {
      /* Race to completion so that any failure to send to the
       * channel will cause failures to cascade and stop matching
       * additional items.
       */
      if (!dex_await (dex_future_all_racev ((DexFuture **)futures->pdata, futures->len), &error))
        return dex_future_new_for_error (g_steal_pointer (&error));

      g_ptr_array_remove_range (futures, 0, futures->len);
      goto next_batch;
    }

  return dex_future_new_for_boolean (TRUE);
}

static DexFuture *
code_result_set_populate_fiber (gpointer user_data)
{
  CodeResultSet *self = user_data;
  g_autoptr(GPtrArray) futures = NULL;
  g_autofree guint *trigrams = NULL;
  guint n_trigrams = 0;

  g_assert (CODE_IS_RESULT_SET (self));
  g_assert (CODE_IS_QUERY (self->query));
  g_assert (self->indexes != NULL);

  _code_query_get_trigrams (self->query, &trigrams, &n_trigrams);

  if (n_trigrams > 0)
    {
      futures = g_ptr_array_new_with_free_func (dex_unref);

      for (guint i = 0; i < self->n_indexes; i++)
        g_ptr_array_add (futures,
                         code_result_set_populate_from_index (self,
                                                              self->indexes[i],
                                                              trigrams, n_trigrams));

      /* Fail early as soon as we've detected we can no longer send
       * an item to the results channel.
       */
      return dex_future_all_racev ((DexFuture **)futures->pdata, futures->len);
    }

  return dex_future_new_for_boolean (TRUE);
}

static DexFuture *
code_result_set_populate_completed (DexFuture *completed,
                                    gpointer   user_data)
{
  CodeResultSet *self = user_data;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (CODE_IS_RESULT_SET (self));

  self->in_populate = FALSE;
  self->did_populate = TRUE;

  dex_channel_close_send (self->channel);

  return NULL;
}

static DexFuture *
code_result_set_receive_fiber (gpointer user_data)
{
  CodeResultSet *self = user_data;

  g_assert (CODE_IS_RESULT_SET (self));

  for (;;)
    {
      g_autoptr(DexFuture) all = NULL;
      guint n_futures;
      guint position;

      /* If receive_all rejected, bail */
      all = dex_channel_receive_all (self->channel);
      if (!DEX_IS_FUTURE_SET (all))
        break;

      n_futures = dex_future_set_get_size (DEX_FUTURE_SET (all));
      position = self->matched->len;

      for (guint i = 0; i < n_futures; i++)
        {
          const GValue *value = dex_future_set_get_value_at (DEX_FUTURE_SET (all), i, NULL);
          CodeResult *result = value ? g_value_get_object (value) : NULL;

          if (result != NULL)
            g_ptr_array_add (self->matched, g_object_ref (result));
        }

      g_list_model_items_changed (G_LIST_MODEL (self), position, 0, self->matched->len - position);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_N_ITEMS]);

      /* Wait for another batch to come in */
      dex_await (dex_timeout_new_msec (50), NULL);
    }

  return NULL;
}

DexFuture *
code_result_set_populate (CodeResultSet *self,
                          DexScheduler  *scheduler)
{
  DexFuture *ret;

  g_return_val_if_fail (CODE_IS_RESULT_SET (self), NULL);

  if (self->in_populate || self->did_populate)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_INVAL,
                                  "Result set has already populated");

  if (self->n_indexes == 0)
    {
      self->did_populate = TRUE;
      return dex_future_new_for_boolean (TRUE);
    }

  self->in_populate = TRUE;

  self->scheduler = scheduler ? dex_ref (scheduler) : NULL;

  /* Start receiving results from channel on the fiber scheduler
   * for the current thread. This will add them to the result set
   * and emit ::items-changed(position,removed,added) as necessary.
   */
  self->receiver = dex_scheduler_spawn (NULL, 0,
                                        code_result_set_receive_fiber,
                                        g_object_ref (self),
                                        g_object_unref);

  ret = dex_scheduler_spawn (scheduler, 0,
                             code_result_set_populate_fiber,
                             g_object_ref (self),
                             g_object_unref);
  ret = dex_future_finally (ret,
                            code_result_set_populate_completed,
                            g_object_ref (self),
                            g_object_unref);

  return ret;
}

/**
 * code_result_set_populate_async:
 * @self: an #CodeResultSet
 * @scheduler: (nullable): a #DexScheduler or %NULL
 * @cancellable: (nullable): a #GCancellable
 * @callback: a #GAsyncReadyCallback to execute upon completion
 * @user_data: closure data for @callback
 *
 * Schedules @self to be populated with results from indexes.
 *
 * If @scheduler is set, then it will be used to execute the fibers
 * performing the query.
 */
void
code_result_set_populate_async (CodeResultSet       *self,
                                DexScheduler        *scheduler,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_autoptr(DexAsyncResult) result = NULL;

  g_return_if_fail (CODE_IS_RESULT_SET (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (!scheduler || DEX_IS_SCHEDULER (scheduler));

  result = dex_async_result_new (self, cancellable, callback, user_data);
  dex_async_result_await (result, code_result_set_populate (self, scheduler));
}

/**
 * code_result_set_populate_finish:
 * @self: an #CodeResultSet
 * @result: a #GAsyncResult provided to callback
 * @error: a location for a #GError, or %NULL
 *
 * Returns:
 */
gboolean
code_result_set_populate_finish (CodeResultSet  *self,
                                 GAsyncResult   *result,
                                 GError        **error)
{
  g_return_val_if_fail (CODE_IS_RESULT_SET (self), FALSE);
  g_return_val_if_fail (DEX_IS_ASYNC_RESULT (result), FALSE);

  return dex_async_result_propagate_boolean (DEX_ASYNC_RESULT (result), error);
}

void
code_result_set_cancel (CodeResultSet *self)
{
  g_return_if_fail (CODE_IS_RESULT_SET (self));

  /* Just close the send side of the channel so that anything
   * trying to queue a result into it will fail and cause all
   * of the work waiting to fail early.
   */
  dex_channel_close_send (self->channel);
}
