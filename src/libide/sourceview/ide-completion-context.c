/* ide-completion-context.c
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

#define G_LOG_DOMAIN "ide-completion-context"

#include "config.h"

#include <dazzle.h>
#include <libide-threading.h>
#include <string.h>

#include "ide-completion.h"
#include "ide-completion-context.h"
#include "ide-completion-private.h"
#include "ide-completion-proposal.h"
#include "ide-completion-provider.h"

struct _IdeCompletionContext
{
  GObject parent_instance;

  IdeCompletion *completion;

  GArray *providers;

  GtkTextMark *begin_mark;
  GtkTextMark *end_mark;

  IdeCompletionActivation activation;

  guint busy : 1;
  guint has_populated : 1;
  guint empty : 1;
};

typedef struct
{
  IdeCompletionProvider *provider;
  GCancellable          *cancellable;
  GListModel            *results;
  GError                *error;
  gulong                 items_changed_handler;
} ProviderInfo;

typedef struct
{
  guint n_active;
} CompleteTaskData;

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (IdeCompletionContext, ide_completion_context, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

enum {
  PROP_0,
  PROP_BUSY,
  PROP_COMPLETION,
  PROP_EMPTY,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];
static GQuark provider_quark;

static void
clear_provider_info (gpointer data)
{
  ProviderInfo *info = data;

  if (info->items_changed_handler != 0)
    {
      g_signal_handler_disconnect (info->results, info->items_changed_handler);
      info->items_changed_handler = 0;
    }

  g_clear_object (&info->provider);
  g_clear_object (&info->cancellable);
  g_clear_object (&info->results);
  g_clear_error (&info->error);
}

static gint
compare_provider_info (gconstpointer a,
                       gconstpointer b,
                       gpointer      user_data)
{
  IdeCompletionContext *self = user_data;
  const ProviderInfo *info_a = a;
  const ProviderInfo *info_b = b;

  return ide_completion_provider_get_priority (info_a->provider, self) -
         ide_completion_provider_get_priority (info_b->provider, self);
}

static void
complete_task_data_free (gpointer data)
{
  CompleteTaskData *task_data = data;

  g_slice_free (CompleteTaskData, task_data);
}

static void
ide_completion_context_update_empty (IdeCompletionContext *self)
{
  gboolean empty = TRUE;

  g_assert (IDE_IS_COMPLETION_CONTEXT (self));

  for (guint i = 0; i < self->providers->len; i++)
    {
      const ProviderInfo *info = &g_array_index (self->providers, ProviderInfo, i);

      if (info->results != NULL && g_list_model_get_n_items (info->results) > 0)
        {
          empty = FALSE;
          break;
        }
    }

  if (self->empty != empty)
    {
      self->empty = empty;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_EMPTY]);
    }
}

static void
ide_completion_context_mark_failed (IdeCompletionContext  *self,
                                    IdeCompletionProvider *provider,
                                    const GError          *error)
{
  g_assert (IDE_IS_COMPLETION_CONTEXT (self));
  g_assert (IDE_IS_COMPLETION_PROVIDER (provider));
  g_assert (error != NULL);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) ||
      g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED))
    return;

  for (guint i = 0; i < self->providers->len; i++)
    {
      ProviderInfo *info = &g_array_index (self->providers, ProviderInfo, i);

      if (info->provider == provider)
        {
          if (error != info->error)
            {
              g_clear_error (&info->error);
              info->error = g_error_copy (error);
            }
          break;
        }
    }
}

static void
ide_completion_context_dispose (GObject *object)
{
  IdeCompletionContext *self = (IdeCompletionContext *)object;

  g_clear_pointer (&self->providers, g_array_unref);
  g_clear_object (&self->completion);

  if (self->begin_mark != NULL)
    {
      gtk_text_buffer_delete_mark (gtk_text_mark_get_buffer (self->begin_mark), self->begin_mark);
      g_clear_object (&self->begin_mark);
    }

  if (self->end_mark != NULL)
    {
      gtk_text_buffer_delete_mark (gtk_text_mark_get_buffer (self->end_mark), self->end_mark);
      g_clear_object (&self->end_mark);
    }

  G_OBJECT_CLASS (ide_completion_context_parent_class)->dispose (object);
}

static void
ide_completion_context_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  IdeCompletionContext *self = IDE_COMPLETION_CONTEXT (object);

  switch (prop_id)
    {
    case PROP_BUSY:
      g_value_set_boolean (value, ide_completion_context_get_busy (self));
      break;

    case PROP_COMPLETION:
      g_value_set_object (value, ide_completion_context_get_completion (self));
      break;

    case PROP_EMPTY:
      g_value_set_boolean (value, ide_completion_context_is_empty (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_completion_context_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  IdeCompletionContext *self = IDE_COMPLETION_CONTEXT (object);

  switch (prop_id)
    {
    case PROP_COMPLETION:
      self->completion = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_completion_context_class_init (IdeCompletionContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_completion_context_dispose;
  object_class->get_property = ide_completion_context_get_property;
  object_class->set_property = ide_completion_context_set_property;

  /**
   * IdeCompletionContext:busy:
   *
   * The "busy" property is %TRUE while the completion context is
   * populating completion proposals.
   *
   * Since: 3.32
   */
  properties [PROP_BUSY] =
    g_param_spec_boolean ("busy",
                          "Busy",
                          "Is the completion context busy populating",
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * IdeCompletionContext:empty:
   *
   * The "empty" property is %TRUE when there are no results.
   *
   * It will be notified when the first result is added or the last
   * result is removed.
   *
   * Since: 3.32
   */
  properties [PROP_EMPTY] =
    g_param_spec_boolean ("empty",
                          "Empty",
                          "If the context has no results",
                          TRUE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * IdeCompletionContext:completion:
   *
   * The "completion" is the #IdeCompletion that was used to create the context.
   *
   * Since: 3.32
   */
  properties [PROP_COMPLETION] =
    g_param_spec_object ("completion",
                         "Completion",
                         "Completion",
                         IDE_TYPE_COMPLETION,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  provider_quark = g_quark_from_static_string ("IDE_COMPLETION_PROPOSAL_PROVIDER");
}

static void
ide_completion_context_init (IdeCompletionContext *self)
{
  self->empty = TRUE;

  self->providers = g_array_new (FALSE, FALSE, sizeof (ProviderInfo));
  g_array_set_clear_func (self->providers, clear_provider_info);
}

void
_ide_completion_context_add_provider (IdeCompletionContext  *self,
                                      IdeCompletionProvider *provider)
{
  ProviderInfo info = {0};

  g_return_if_fail (IDE_IS_COMPLETION_CONTEXT (self));
  g_return_if_fail (IDE_IS_COMPLETION_PROVIDER (provider));
  g_return_if_fail (self->has_populated == FALSE);

  info.provider = g_object_ref (provider);
  info.cancellable = g_cancellable_new ();
  info.results = NULL;

  g_array_append_val (self->providers, info);
  g_array_sort_with_data (self->providers, compare_provider_info, self);
}

void
_ide_completion_context_remove_provider (IdeCompletionContext  *self,
                                         IdeCompletionProvider *provider)
{
  g_return_if_fail (IDE_IS_COMPLETION_CONTEXT (self));
  g_return_if_fail (IDE_IS_COMPLETION_PROVIDER (provider));
  g_return_if_fail (self->has_populated == FALSE);

  for (guint i = 0; i < self->providers->len; i++)
    {
      const ProviderInfo *info = &g_array_index (self->providers, ProviderInfo, i);

      if (info->provider == provider)
        {
          g_array_remove_index (self->providers, i);
          return;
        }
    }

  g_warning ("No such provider <%s %p> in context",
             G_OBJECT_TYPE_NAME (provider), provider);
}

static void
ide_completion_context_items_changed_cb (IdeCompletionContext  *self,
                                         guint                  position,
                                         guint                  removed,
                                         guint                  added,
                                         GListModel            *results)
{
  guint real_position = 0;

  g_assert (IDE_IS_COMPLETION_CONTEXT (self));
  g_assert (G_IS_LIST_MODEL (results));

  if (removed == 0 && added == 0)
    return;

  for (guint i = 0; i < self->providers->len; i++)
    {
      ProviderInfo *info = &g_array_index (self->providers, ProviderInfo, i);

      if (info->results == results)
        {
          g_list_model_items_changed (G_LIST_MODEL (self),
                                      real_position + position,
                                      removed,
                                      added);
          break;
        }

      if (info->results != NULL)
        real_position += g_list_model_get_n_items (info->results);
    }

  ide_completion_context_update_empty (self);
}

/**
 * ide_completion_context_set_proposals_for_provider:
 * @self: an #IdeCompletionContext
 * @provider: an #IdeCompletionProvider
 * @results: (nullable): a #GListModel or %NULL
 *
 * This function allows providers to update their results for a context
 * outside of a call to ide_completion_provider_populate_async(). This
 * can be used to immediately return results for a provider while it does
 * additional asynchronous work. Doing so will allow the completions to
 * update while the operation is in progress.
 *
 * Since: 3.32
 */
void
ide_completion_context_set_proposals_for_provider (IdeCompletionContext  *self,
                                                   IdeCompletionProvider *provider,
                                                   GListModel            *results)
{
  guint position = 0;

  g_assert (IDE_IS_COMPLETION_CONTEXT (self));
  g_assert (IDE_IS_COMPLETION_PROVIDER (provider));
  g_assert (!results || G_IS_LIST_MODEL (results));

  for (guint i = 0; i < self->providers->len; i++)
    {
      ProviderInfo *info = &g_array_index (self->providers, ProviderInfo, i);

      if (info->provider == provider)
        {
          guint n_removed = 0;
          guint n_added = 0;

          if (info->results == results)
            return;

          if (info->results != NULL)
            n_removed = g_list_model_get_n_items (info->results);

          if (results != NULL)
            n_added = g_list_model_get_n_items (results);

          if (info->items_changed_handler != 0)
            {
              g_signal_handler_disconnect (info->results, info->items_changed_handler);
              info->items_changed_handler = 0;
            }

          g_set_object (&info->results, results);

          if (info->results != NULL)
            info->items_changed_handler =
              g_signal_connect_object (info->results,
                                       "items-changed",
                                       G_CALLBACK (ide_completion_context_items_changed_cb),
                                       self,
                                       G_CONNECT_SWAPPED);

          g_list_model_items_changed (G_LIST_MODEL (self), position, n_removed, n_added);

          break;
        }

      if (info->results != NULL)
        position += g_list_model_get_n_items (info->results);
    }

  ide_completion_context_update_empty (self);
}

static void
ide_completion_context_populate_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  IdeCompletionProvider *provider = (IdeCompletionProvider *)object;
  IdeCompletionContext *self;
  CompleteTaskData *task_data;
  g_autoptr(GListModel) results = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_COMPLETION_PROVIDER (provider));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  g_assert (IDE_IS_COMPLETION_CONTEXT (self));

  task_data = ide_task_get_task_data (task);
  g_assert (task_data != NULL);

  if (!(results = ide_completion_provider_populate_finish (provider, result, &error)))
    ide_completion_context_mark_failed (self, provider, error);
  else
    ide_completion_context_set_proposals_for_provider (self, provider, results);

  task_data->n_active--;

  ide_completion_context_update_empty (self);

  if (task_data->n_active == 0)
    ide_task_return_boolean (task, TRUE);
}

static void
ide_completion_context_notify_complete_cb (IdeCompletionContext *self,
                                           GParamSpec           *pspec,
                                           IdeTask                *task)
{
  g_assert (IDE_IS_COMPLETION_CONTEXT (self));
  g_assert (IDE_IS_TASK (task));

  self->busy = FALSE;
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BUSY]);
}

/**
 * _ide_completion_context_complete_async:
 * @self: a #IdeCompletionContext
 * @activation: how we are being activated
 * @iter: a #GtkTextIter
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: (nullable): a callback or %NULL
 * @user_data: closure data for @callback
 *
 * Asynchronously requests that the completion context load proposals
 * from the registered providers.
 *
 * Since: 3.32
 */
void
_ide_completion_context_complete_async (IdeCompletionContext    *self,
                                        IdeCompletionActivation  activation,
                                        const GtkTextIter       *begin,
                                        const GtkTextIter       *end,
                                        GCancellable            *cancellable,
                                        GAsyncReadyCallback      callback,
                                        gpointer                 user_data)
{
  g_autoptr(IdeTask) task = NULL;
  CompleteTaskData *task_data;
  GtkTextBuffer *buffer;
  guint n_items;

  g_return_if_fail (IDE_IS_COMPLETION_CONTEXT (self));
  g_return_if_fail (self->has_populated == FALSE);
  g_return_if_fail (self->begin_mark == NULL);
  g_return_if_fail (self->end_mark == NULL);
  g_return_if_fail (begin != NULL);
  g_return_if_fail (end != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  self->activation = activation;
  self->has_populated = TRUE;
  self->busy = TRUE;

  buffer = ide_completion_get_buffer (self->completion);

  self->begin_mark = gtk_text_buffer_create_mark (buffer, NULL, begin, TRUE);
  g_object_ref (self->begin_mark);

  self->end_mark = gtk_text_buffer_create_mark (buffer, NULL, end, FALSE);
  g_object_ref (self->end_mark);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, _ide_completion_context_complete_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  task_data = g_slice_new0 (CompleteTaskData);
  task_data->n_active = self->providers->len;
  ide_task_set_task_data (task, task_data, complete_task_data_free);

  /* Always notify of busy completion, whether we fail or not */
  g_signal_connect_object (task,
                           "notify::completed",
                           G_CALLBACK (ide_completion_context_notify_complete_cb),
                           self,
                           G_CONNECT_SWAPPED);

  for (guint i = 0; i < self->providers->len; i++)
    {
      const ProviderInfo *info = &g_array_index (self->providers, ProviderInfo, i);

      dzl_cancellable_chain (info->cancellable, cancellable);
      ide_completion_provider_populate_async (info->provider,
                                              self,
                                              info->cancellable,
                                              ide_completion_context_populate_cb,
                                              g_object_ref (task));
    }

  /* Providers may adjust their position based on our new marks */
  n_items = g_list_model_get_n_items (G_LIST_MODEL (self));
  g_array_sort_with_data (self->providers, compare_provider_info, self);
  g_list_model_items_changed (G_LIST_MODEL (self), 0, n_items, n_items);

  if (task_data->n_active == 0)
      ide_task_return_boolean (task, TRUE);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BUSY]);
}

/**
 * _ide_completion_context_complete_finish:
 * @self: an #IdeCompletionContext
 * @result: a #GAsyncResult provided to callback
 * @error: a location for a #GError, or %NULL
 *
 * Completes an asynchronous request to populate proposals.
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set
 *
 * Since: 3.32
 */
gboolean
_ide_completion_context_complete_finish (IdeCompletionContext  *self,
                                         GAsyncResult          *result,
                                         GError               **error)
{
  g_return_val_if_fail (IDE_IS_COMPLETION_CONTEXT (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

/**
 * ide_completion_context_get_busy:
 *
 * Gets the "busy" property. This is set to %TRUE while the completion
 * context is actively fetching proposals from the #IdeCompletionProvider
 * that were registered with ide_completion_context_add_provider().
 *
 * Returns: %TRUE if the context is busy
 *
 * Since: 3.32
 */
gboolean
ide_completion_context_get_busy (IdeCompletionContext *self)
{
  g_return_val_if_fail (IDE_IS_COMPLETION_CONTEXT (self), FALSE);

  return self->busy;
}

static GType
ide_completion_context_get_item_type (GListModel *model)
{
  return IDE_TYPE_COMPLETION_PROPOSAL;
}

static guint
ide_completion_context_get_n_items (GListModel *model)
{
  IdeCompletionContext *self = (IdeCompletionContext *)model;
  guint count = 0;

  g_assert (IDE_IS_COMPLETION_CONTEXT (self));

  for (guint i = 0; i < self->providers->len; i++)
    {
      const ProviderInfo *info = &g_array_index (self->providers, ProviderInfo, i);

      if (info->results != NULL)
        count += g_list_model_get_n_items (info->results);
    }

  return count;
}

gboolean
ide_completion_context_get_item_full (IdeCompletionContext   *self,
                                      guint                   position,
                                      IdeCompletionProvider **provider,
                                      IdeCompletionProposal **proposal)
{
  g_return_val_if_fail (IDE_IS_COMPLETION_CONTEXT (self), FALSE);

  if (provider != NULL)
    *provider = NULL;

  if (proposal != NULL)
    *proposal = NULL;

  for (guint i = 0; i < self->providers->len; i++)
    {
      const ProviderInfo *info = &g_array_index (self->providers, ProviderInfo, i);
      guint n_items;

      if (info->results == NULL)
        continue;

      n_items = g_list_model_get_n_items (info->results);

      if (position >= n_items)
        {
          position -= n_items;
          continue;
        }

      if (provider != NULL)
        *provider = g_object_ref (info->provider);

      if (proposal != NULL)
        *proposal = g_list_model_get_item (info->results, position);

      return TRUE;
    }

  return FALSE;
}

static gpointer
ide_completion_context_get_item (GListModel *model,
                                 guint       position)
{
  IdeCompletionContext *self = (IdeCompletionContext *)model;
  g_autoptr(IdeCompletionProposal) proposal = NULL;

  g_assert (IDE_IS_COMPLETION_CONTEXT (self));

  if (ide_completion_context_get_item_full (self, position, NULL, &proposal))
    return g_steal_pointer (&proposal);

  return NULL;
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = ide_completion_context_get_item_type;
  iface->get_item = ide_completion_context_get_item;
  iface->get_n_items = ide_completion_context_get_n_items;
}

/**
 * ide_completion_context_get_bounds:
 * @self: an #IdeCompletionContext
 * @begin: (out) (optional): a #GtkTextIter
 * @end: (out) (optional): a #GtkTextIter
 *
 * Gets the bounds for the completion, which is the beginning of the
 * current word (taking break characters into account) to the current
 * insertion cursor.
 *
 * If @begin is non-%NULL, it will be set to the start position of the
 * current word being completed.
 *
 * If @end is non-%NULL, it will be set to the insertion cursor for the
 * current word being completed.
 *
 * Returns: %TRUE if the marks are still valid and @begin or @end was set.
 *
 * Since: 3.32
 */
gboolean
ide_completion_context_get_bounds (IdeCompletionContext *self,
                                   GtkTextIter          *begin,
                                   GtkTextIter          *end)
{
  GtkTextBuffer *buffer;

  g_return_val_if_fail (IDE_IS_COMPLETION_CONTEXT (self), FALSE);
  g_return_val_if_fail (self->completion != NULL, FALSE);
  g_return_val_if_fail (begin != NULL || end != NULL, FALSE);

  buffer = ide_completion_get_buffer (self->completion);

  g_return_val_if_fail (buffer != NULL, FALSE);

  if (begin != NULL)
    memset (begin, 0, sizeof *begin);

  if (end != NULL)
    memset (end, 0, sizeof *end);

  if (self->begin_mark == NULL)
    {
      /* Try to give some sort of valid iter */
      gtk_text_buffer_get_selection_bounds (buffer, begin, end);
      return FALSE;
    }

  g_assert (GTK_IS_TEXT_MARK (self->begin_mark));
  g_assert (GTK_IS_TEXT_MARK (self->end_mark));
  g_assert (GTK_IS_TEXT_BUFFER (buffer));

  if (begin != NULL)
    gtk_text_buffer_get_iter_at_mark (buffer, begin, self->begin_mark);

  if (end != NULL)
    gtk_text_buffer_get_iter_at_mark (buffer, end, self->end_mark);

  return TRUE;
}

/**
 * ide_completion_context_get_completion:
 * @self: an #IdeCompletionContext
 *
 * Gets the #IdeCompletion that created the context.
 *
 * Returns: (transfer none) (nullable): an #IdeCompletion or %NULL
 *
 * Since: 3.32
 */
IdeCompletion *
ide_completion_context_get_completion (IdeCompletionContext *self)
{
  g_return_val_if_fail (IDE_IS_COMPLETION_CONTEXT (self), NULL);

  return self->completion;
}

IdeCompletionContext *
_ide_completion_context_new (IdeCompletion *completion)
{
  g_return_val_if_fail (IDE_IS_COMPLETION (completion), NULL);

  return g_object_new (IDE_TYPE_COMPLETION_CONTEXT,
                       "completion", completion,
                       NULL);
}

/**
 * ide_completion_context_is_empty:
 * @self: a #IdeCompletionContext
 *
 * Checks if any proposals have been provided to the context.
 *
 * Out of convenience, this function will return %TRUE if @self is %NULL.
 *
 * Returns: %TRUE if there are no proposals in the context
 *
 * Since: 3.32
 */
gboolean
ide_completion_context_is_empty (IdeCompletionContext *self)
{
  g_return_val_if_fail (!self || IDE_IS_COMPLETION_CONTEXT (self), FALSE);

  return self ? self->empty : TRUE;
}

/**
 * ide_completion_context_get_start_iter:
 * @self: a #IdeCompletionContext
 * @iter: (out): a location for a #GtkTextIter
 *
 * Gets the iter for the start of the completion.
 *
 * Returns:
 *
 * Since: 3.32
 */
gboolean
ide_completion_context_get_start_iter (IdeCompletionContext *self,
                                       GtkTextIter          *iter)
{
  g_return_val_if_fail (IDE_IS_COMPLETION_CONTEXT (self), FALSE);
  g_return_val_if_fail (self->completion != NULL, FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  if (self->begin_mark != NULL)
    {
      GtkTextBuffer *buffer = gtk_text_mark_get_buffer (self->begin_mark);
      gtk_text_buffer_get_iter_at_mark (buffer, iter, self->begin_mark);
      return TRUE;
    }

  return FALSE;
}

/**
 * ide_completion_context_get_word:
 * @self: a #IdeCompletionContext
 *
 * Gets the word that is being completed up to the position of the insert mark.
 *
 * Returns: (transfer full): a string containing the current word
 *
 * Since: 3.32
 */
gchar *
ide_completion_context_get_word (IdeCompletionContext *self)
{
  GtkTextIter begin, end;

  g_return_val_if_fail (IDE_IS_COMPLETION_CONTEXT (self), NULL);

  ide_completion_context_get_bounds (self, &begin, &end);
  return gtk_text_iter_get_slice (&begin, &end);
}

gboolean
_ide_completion_context_can_refilter (IdeCompletionContext *self,
                                      const GtkTextIter    *begin,
                                      const GtkTextIter    *end)
{
  GtkTextIter old_begin;
  GtkTextIter old_end;

  g_return_val_if_fail (IDE_IS_COMPLETION_CONTEXT (self), FALSE);
  g_return_val_if_fail (begin != NULL, FALSE);
  g_return_val_if_fail (end != NULL, FALSE);

  ide_completion_context_get_bounds (self, &old_begin, &old_end);

  if (gtk_text_iter_equal (&old_begin, begin))
    {
      /*
       * TODO: We can probably get smarter about this by asking all of
       * the providers if they can refilter the new word (and only reload
       * the data for those that cannot.
       *
       * Also, we might want to deal with that by copying the context
       * into a new context and query using that.
       */
      if (gtk_text_iter_compare (&old_end, end) <= 0)
        {
          GtkTextBuffer *buffer = gtk_text_iter_get_buffer (begin);

          gtk_text_buffer_move_mark (buffer, self->begin_mark, begin);
          gtk_text_buffer_move_mark (buffer, self->end_mark, end);

          return TRUE;
        }
    }

  return FALSE;
}

/**
 * ide_completion_context_get_buffer:
 * @self: an #IdeCompletionContext
 *
 * Gets the underlying buffer used by the context.
 *
 * This is a convenience function to get the buffer via the #IdeCompletion
 * property.
 *
 * Returns: (transfer none) (nullable): a #GtkTextBuffer or %NULL
 *
 * Since: 3.32
 */
GtkTextBuffer *
ide_completion_context_get_buffer (IdeCompletionContext *self)
{
  g_return_val_if_fail (IDE_IS_COMPLETION_CONTEXT (self), NULL);

  if (self->completion != NULL)
    return ide_completion_get_buffer (self->completion);

  return NULL;
}

/**
 * ide_completion_context_get_view:
 * @self: a #IdeCompletionContext
 *
 * Gets the text view for the context.
 *
 * Returns: (nullable) (transfer none): a #GtkTextView or %NULL
 *
 * Since: 3.32
 */
GtkTextView *
ide_completion_context_get_view (IdeCompletionContext *self)
{
  g_return_val_if_fail (IDE_IS_COMPLETION_CONTEXT (self), NULL);

  if (self->completion != NULL)
    return GTK_TEXT_VIEW (ide_completion_get_view (self->completion));

  return NULL;
}

void
_ide_completion_context_refilter (IdeCompletionContext *self)
{
  g_return_if_fail (IDE_IS_COMPLETION_CONTEXT (self));

  for (guint i = 0; i < self->providers->len; i++)
    {
      const ProviderInfo *info = &g_array_index (self->providers, ProviderInfo, i);

      if (info->error != NULL)
        continue;

      if (info->results == NULL)
        continue;

      ide_completion_provider_refilter (info->provider, self, info->results);
    }
}

gboolean
_ide_completion_context_iter_invalidates (IdeCompletionContext *self,
                                          const GtkTextIter    *iter)
{
  GtkTextIter begin, end;
  GtkTextBuffer *buffer;

  g_assert (!self || IDE_IS_COMPLETION_CONTEXT (self));
  g_assert (iter != NULL);

  if (self == NULL)
    return FALSE;

  buffer = gtk_text_iter_get_buffer (iter);

  gtk_text_buffer_get_iter_at_mark (buffer, &begin, self->begin_mark);
  gtk_text_buffer_get_iter_at_mark (buffer, &end, self->end_mark);

  return gtk_text_iter_compare (&begin, iter) <= 0 &&
         gtk_text_iter_compare (&end, iter) >= 0;
}

/**
 * ide_completion_context_get_line_text:
 * @self: a #IdeCompletionContext
 *
 * This is a convenience helper to get the line text up until the insertion
 * cursor for the current completion.
 *
 * Returns: a newly allocated string
 *
 * Since: 3.32
 */
gchar *
ide_completion_context_get_line_text (IdeCompletionContext *self)
{
  GtkTextIter begin, end;

  g_return_val_if_fail (IDE_IS_COMPLETION_CONTEXT (self), NULL);

  ide_completion_context_get_bounds (self, &begin, &end);
  gtk_text_iter_set_line_offset (&begin, 0);
  return gtk_text_iter_get_slice (&begin, &end);
}

/**
 * ide_completion_context_get_language:
 * @self: a #IdeCompletionContext
 *
 * Gets the language identifier which can be useful for providers that support
 * multiple languages.
 *
 * Returns: (nullable): a language identifier or %NULL
 *
 * Since: 3.32
 */
const gchar *
ide_completion_context_get_language (IdeCompletionContext *self)
{
  GtkTextBuffer *buffer;
  GtkSourceLanguage *language;

  g_return_val_if_fail (IDE_IS_COMPLETION_CONTEXT (self), NULL);

  if (!(buffer = ide_completion_context_get_buffer (self)))
    return NULL;

  if (!(language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (buffer))))
    return NULL;

  return gtk_source_language_get_id (language);
}

/**
 * ide_completion_context_is_language:
 * @self: a #IdeCompletionContext
 *
 * Helper to check the language of the underlying buffer.
 *
 * Returns: %TRUE if @language matches; otherwise %FALSE.
 *
 * Since: 3.32
 */
gboolean
ide_completion_context_is_language (IdeCompletionContext *self,
                                    const gchar          *language)
{
  g_return_val_if_fail (IDE_IS_COMPLETION_CONTEXT (self), FALSE);

  return g_strcmp0 (language, ide_completion_context_get_language (self)) == 0;
}

/**
 * ide_completion_context_get_activation:
 * @self: a #IdeCompletionContext
 *
 * Gets the mode for which the context was activated.
 *
 * Since: 3.32
 */
IdeCompletionActivation
ide_completion_context_get_activation (IdeCompletionContext *self)
{
  g_return_val_if_fail (IDE_IS_COMPLETION_CONTEXT (self), 0);

  return self->activation;
}
