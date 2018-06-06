/* ide-clang-proposals.c
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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
 */

#define G_LOG_DOMAIN "ide-clang-proposals"

#include "ide-clang-completion-item.h"
#include "ide-clang-proposals.h"

#include "sourceview/ide-text-iter.h"

struct _IdeClangProposals
{
  GObject parent_instance;

  /*
   * @cancellable contains a GCancellable that we can use to cancel an
   * in-flight completion request. This generally happens if we determine
   * that we cannot re-use results from an in-flight request due to the
   * user typing new characters that break the request.
   */
  GCancellable *cancellable;

  /*
   * @client is our wrapper around the subprocess providing clang services
   * to this process. We will query it using ide_clang_client_complete_async()
   * to get a new set of results. We try to avoid calling that too much and
   * favor client-side filtering because the process is rather slow.
   */
  IdeClangClient *client;

  /*
   * The most recent GVariant we received from the peer.
   */
  GVariant *results;

  /*
   * Instead of inflating GObjects for each of our matches, we instead keep
   * an index for all of the items that match the current result set. When
   * the new result variant is returned, this is set to an array that is 1:1
   * of the variant indexes or filtered based on the typed_text to only include
   * the items that match).
   */
  GArray *match_indexes;

  /*
   * The word we are trying to filter. If we are waiting on a previous query
   * to finish, this might change before the result has come back from clang.
   * We post-filter requests based on the filter once we receive it.
   */
  gchar *filter;

  /*
   * @line is the line we last performed a completion request upon. We cannot
   * reuse results that are on a different line or are not a continuation of
   * the query that we previously made.
   */
  gint line;

  /*
   * @line_offset is the offset for the beginning of our previous query. This
   * is generally the position of the first-character of the query. So if we
   * start with a completion upon "gtk_|" (where | is the cursor), we would
   * store the position of "g" so that determining if we can reuse the results
   * is simply a check to ensure only symbol characters are used between the
   * start position (g) and the new cursor location.
   */
  gint line_offset;

  /*
   * Each time we request a new query, we increment this monotonic integer.
   * That allows us to check when getting the result to see if the query we
   * made is still the most active.
   *
   * This is necessary because if we continue to type while we are trying to
   * start a completion, we might want to continue using the previous query
   * but post-process the results using our more specific filter.
   */
  guint query_id;

  /*
   * If we are currently performing a query, we can push tasks here to be
   * completed when the results come in.
   */
  GQueue queued_tasks;
};

typedef struct
{
  IdeClangClient *client;
  GFile          *file;
  guint           line;
  guint           column;
  guint           query_id;
} Query;

typedef struct
{
  guint index;
  guint priority;
  const gchar *keyword;
} Item;

enum {
  PROP_0,
  PROP_CLIENT,
  N_PROPS
};

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (IdeClangProposals, ide_clang_proposals, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static GParamSpec *properties [N_PROPS];

static void
query_free (gpointer data)
{
  Query *q = data;

  g_clear_object (&q->client);
  g_clear_object (&q->file);
  g_slice_free (Query, q);
}

static inline gboolean
is_symbol_char (gunichar ch)
{
  return g_unichar_isalnum (ch) || ch == '_';
}

static void
ide_clang_proposals_finalize (GObject *object)
{
  IdeClangProposals *self = (IdeClangProposals *)object;

  g_assert (self->queued_tasks.head == NULL);
  g_assert (self->queued_tasks.tail == NULL);
  g_assert (self->queued_tasks.length == 0);

  g_clear_object (&self->client);
  g_clear_object (&self->cancellable);
  g_clear_pointer (&self->filter, g_free);
  g_clear_pointer (&self->match_indexes, g_array_unref);
  g_clear_pointer (&self->results, g_variant_unref);

  G_OBJECT_CLASS (ide_clang_proposals_parent_class)->finalize (object);
}

static void
ide_clang_proposals_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  IdeClangProposals *self = IDE_CLANG_PROPOSALS (object);

  switch (prop_id)
    {
    case PROP_CLIENT:
      g_value_set_object (value, self->client);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_clang_proposals_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  IdeClangProposals *self = IDE_CLANG_PROPOSALS (object);

  switch (prop_id)
    {
    case PROP_CLIENT:
      self->client = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_clang_proposals_class_init (IdeClangProposalsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_clang_proposals_finalize;
  object_class->get_property = ide_clang_proposals_get_property;
  object_class->set_property = ide_clang_proposals_set_property;

  properties [PROP_CLIENT] =
    g_param_spec_object ("client",
                         "Client",
                         "The client to the clang worker process",
                         IDE_TYPE_CLANG_CLIENT,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  
  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_clang_proposals_init (IdeClangProposals *self)
{
  self->line = -1;
  self->line_offset = -1;
  self->match_indexes = g_array_new (FALSE, FALSE, sizeof (Item));
}

void
ide_clang_proposals_clear (IdeClangProposals *self)
{
  GList *list;
  guint old_len;

  g_assert (IDE_IS_CLANG_CLIENT (self));

  self->line = -1;
  self->line_offset = -1;

  ide_clear_string (&self->filter);

  if ((old_len = self->match_indexes->len))
    g_array_remove_range (self->match_indexes, 0, self->match_indexes->len);

  list = g_steal_pointer (&self->queued_tasks.head);
  self->queued_tasks.head = NULL;
  self->queued_tasks.tail = NULL;
  self->queued_tasks.length = 0;

  g_list_model_items_changed (G_LIST_MODEL (self), 0, old_len, 0);

  for (const GList *iter = list; iter != NULL; iter = iter->next)
    {
      g_autoptr(IdeTask) task = iter->data;
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_CANCELLED,
                                 "Completion request was invalidated");
    }

  g_list_free (list);
}

IdeClangProposals *
ide_clang_proposals_new (IdeClangClient *client)
{
  g_return_val_if_fail (IDE_IS_CLANG_CLIENT (client), NULL);

  return g_object_new (IDE_TYPE_CLANG_PROPOSALS,
                       "client", client,
                       NULL);
}

IdeClangClient *
ide_clang_proposals_get_client (IdeClangProposals *self)
{
  g_return_val_if_fail (IDE_IS_CLANG_PROPOSALS (self), NULL);

  return self->client;
}

static gint
sort_by_priority (gconstpointer a,
                  gconstpointer b)
{
  const Item *ai = a;
  const Item *bi = b;

  return (gint)ai->priority - (gint)bi->priority;
}

static void
ide_clang_proposals_do_refilter (IdeClangProposals *self,
                                 gboolean           fast_refilter)
{
  g_autofree gchar *folded = NULL;
  guint old_len = 0;
  guint n_items;

  g_assert (IDE_IS_CLANG_PROPOSALS (self));

  old_len = self->match_indexes->len;

  if (self->filter != NULL)
    folded = g_utf8_casefold (self->filter, -1);

  if (fast_refilter)
    {
      for (guint i = old_len; i > 0; i--)
        {
          Item *item = &g_array_index (self->match_indexes, Item, i - 1);
          const gchar *keyword = item->keyword;

          if (keyword == NULL)
            {
              g_autoptr(GVariant) child = g_variant_get_child_value (self->results, item->index);
              if (!g_variant_lookup (child, "keyword", "&s", &keyword))
                keyword = NULL;
            }

          if (keyword == NULL ||
              !ide_completion_fuzzy_match (keyword, folded, &item->priority))
            g_array_remove_index_fast (self->match_indexes, i - 1);
        }

      g_array_sort (self->match_indexes, sort_by_priority);

      g_list_model_items_changed (G_LIST_MODEL (self), 0, old_len, self->match_indexes->len);

      return;
    }

  if (old_len > 0)
    g_array_remove_range (self->match_indexes, 0, old_len);

  n_items = self->results ? g_variant_n_children (self->results) : 0;

  if (self->filter == NULL || self->filter[0] == 0)
    {
      for (guint i = 0; i < n_items; i++)
        {
          Item item = { i, i };
          g_array_append_val (self->match_indexes, item);
        }
    }
  else if (self->results != NULL)
    {
      GVariantIter iter;
      GVariant *value;
      guint index = 0;

      g_variant_iter_init (&iter, self->results);

      while ((value = g_variant_iter_next_value (&iter)))
        {
          const gchar *typed_text;

          /*
           * We get a typed_text pointer into the larger buffer, which is
           * kept around for the lifetime of @variant (so that we don't need
           * to copy more strings.
           */

          if (g_variant_lookup (value, "keyword", "&s", &typed_text))
            {
              Item item = { index, 0, typed_text };

              if (ide_completion_fuzzy_match (typed_text, folded, &item.priority))
                g_array_append_val (self->match_indexes, item);
            }

          g_variant_unref (value);

          index++;
        }

      g_array_sort (self->match_indexes, sort_by_priority);
    }

  g_list_model_items_changed (G_LIST_MODEL (self), 0, old_len, self->match_indexes->len);
}

static void
ide_clang_proposals_flush (IdeClangProposals *self,
                           GVariant          *results,
                           const GError      *error)
{
  GList *list;

  g_assert (IDE_IS_CLANG_PROPOSALS (self));
  g_assert (results != NULL || error != NULL);

  if (results != self->results)
    {
      g_clear_pointer (&self->results, g_variant_unref);
      self->results = g_variant_ref (results);
    }

  ide_clang_proposals_do_refilter (self, FALSE);

  list = g_steal_pointer (&self->queued_tasks.head);
  self->queued_tasks.head = NULL;
  self->queued_tasks.tail = NULL;
  self->queued_tasks.length = 0;

  for (const GList *iter = list; iter != NULL; iter = iter->next)
    {
      g_autoptr(IdeTask) task = iter->data;

      if (error != NULL)
        ide_task_return_error (task, g_error_copy (error));
      else
        ide_task_return_boolean (task, TRUE);
    }

  g_list_free (list);
}

static void
ide_clang_proposals_query_complete_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  IdeClangClient *client = (IdeClangClient *)object;
  g_autoptr(GVariant) variant = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_CLANG_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  variant = ide_clang_client_complete_finish (client, result, &error);

  if (error != NULL)
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_pointer (task,
                             g_steal_pointer (&variant),
                             (GDestroyNotify)g_variant_unref);
}

static void
ide_clang_proposals_query_build_flags_cb (GObject      *object,
                                          GAsyncResult *result,
                                          gpointer      user_data)
{
  IdeBuildSystem *build_system = (IdeBuildSystem *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_auto(GStrv) flags = NULL;
  GCancellable *cancellable;
  Query *query;

  g_assert (IDE_IS_BUILD_SYSTEM (build_system));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  flags = ide_build_system_get_build_flags_finish (build_system, result, &error);

  if (ide_task_return_error_if_cancelled (task))
    return;

  query = ide_task_get_task_data (task);
  cancellable = ide_task_get_cancellable (task);

  g_assert (query != NULL);
  g_assert (G_IS_FILE (query->file));
  g_assert (IDE_IS_CLANG_CLIENT (query->client));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  ide_clang_client_complete_async (query->client,
                                   query->file,
                                   (const gchar * const *)flags,
                                   query->line,
                                   query->column,
                                   cancellable,
                                   ide_clang_proposals_query_complete_cb,
                                   g_steal_pointer (&task));
}

static void
ide_clang_proposals_query_async (IdeClangProposals   *self,
                                 IdeFile             *file,
                                 guint                line,
                                 guint                column,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  IdeBuildSystem *build_system;
  IdeContext *context;
  Query *q;

  g_assert (IDE_IS_CLANG_PROPOSALS (self));
  g_assert (IDE_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  context = ide_object_get_context (IDE_OBJECT (self->client));
  build_system = ide_context_get_build_system (context);

  q = g_slice_new0 (Query);
  q->client = g_object_ref (self->client);
  q->file = g_object_ref (ide_file_get_file (file));
  q->line = line;
  q->column = column;
  q->query_id = ++self->query_id;

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_clang_proposals_query_async);
  ide_task_set_task_data (task, q, query_free);

  ide_build_system_get_build_flags_async (build_system,
                                          file,
                                          cancellable,
                                          ide_clang_proposals_query_build_flags_cb,
                                          g_steal_pointer (&task));
}

static GVariant *
ide_clang_proposals_query_finish (IdeClangProposals  *self,
                                  GAsyncResult       *result,
                                  GError            **error)
{
  g_autoptr(GVariant) ret = NULL;

  g_return_val_if_fail (IDE_IS_CLANG_PROPOSALS (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  if ((ret = ide_task_propagate_pointer (IDE_TASK (result), error)))
    {
      Query *query = ide_task_get_task_data (IDE_TASK (result));

      if (query->query_id != self->query_id)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_CANCELLED,
                       "Query is no longer valid");
          return NULL;
        }
    }

  return g_steal_pointer (&ret);
}

static void
query_subprocess_cb (GObject      *object,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  IdeClangProposals *self = (IdeClangProposals *)object;
  g_autoptr(GVariant) ret = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_CLANG_PROPOSALS (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (user_data == NULL);

  ret = ide_clang_proposals_query_finish (self, result, &error);

  ide_clang_proposals_flush (self, ret, error);

  IDE_EXIT;
}

void
ide_clang_proposals_populate_async (IdeClangProposals   *self,
                                    const GtkTextIter   *iter,
                                    const gchar         *word,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  g_autoptr(GCancellable) prev_cancellable = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autofree gchar *slice = NULL;
  GtkTextBuffer *buffer;
  GtkTextIter begin;
  GtkTextIter previous;
  IdeFile *file;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_CLANG_PROPOSALS (self));
  g_return_if_fail (iter != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  buffer = gtk_text_iter_get_buffer (iter);
  g_assert (IDE_IS_BUFFER (buffer));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_clang_proposals_populate_async);
  ide_task_set_task_data (task, g_object_ref (buffer), g_object_unref);

  begin = *iter;

  if (self->line < 0 || self->line_offset < 0)
    IDE_GOTO (query_client);

  gtk_text_buffer_get_iter_at_line_offset (buffer, &previous, self->line, self->line_offset);

  /*
   * If we are not at the same starting position and any previous query, then
   * we cannot reuse those results. We have to requery first.
   */
  if (!gtk_text_iter_equal (&previous, &begin))
    IDE_GOTO (query_client);

  /*
   * At this point, we know we can refilter results. However, we may not have
   * have received those yet from the subprocess. If that is the case, queue
   * any follow-up work until that happens.
   */
  if (!g_queue_is_empty (&self->queued_tasks))
    {
      ide_set_string (&self->filter, word);
      g_queue_push_tail (&self->queued_tasks, g_steal_pointer (&task));
      IDE_EXIT;
    }

  /* Unlikely, but is this the exact same query as before? */
  if (dzl_str_equal0 (self->filter, word))
    {
      ide_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  /*
   * Now we know we have results and can refilter them. If the current word
   * contains the previous word as a prefix, then we can simply refilter using
   * the linked-list (rather than reset from the array).
   */
  if (self->filter == NULL || (word && g_str_has_prefix (word, self->filter)))
    {
      ide_set_string (&self->filter, word);
      ide_clang_proposals_do_refilter (self, TRUE);
      ide_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  /*
   * So we can reuse the results, but since the user backspaced we have to
   * clear the linked list and update by walking the whole array.
   */
  ide_set_string (&self->filter, word);
  ide_clang_proposals_do_refilter (self, FALSE);
  ide_task_return_boolean (task, TRUE);

  IDE_EXIT;


query_client:

  file = ide_buffer_get_file (IDE_BUFFER (buffer));

  prev_cancellable = g_steal_pointer (&self->cancellable);
  self->cancellable = g_cancellable_new ();

  self->line = gtk_text_iter_get_line (&begin);
  self->line_offset = gtk_text_iter_get_line_offset (&begin);

  g_queue_push_tail (&self->queued_tasks, g_steal_pointer (&task));

  ide_set_string (&self->filter, word);

  ide_clang_proposals_query_async (self,
                                   file,
                                   self->line + 1,
                                   self->line_offset + 1,
                                   self->cancellable,
                                   query_subprocess_cb,
                                   NULL);

  g_cancellable_cancel (prev_cancellable);

  IDE_EXIT;
}

gboolean
ide_clang_proposals_populate_finish (IdeClangProposals  *self,
                                     GAsyncResult       *result,
                                     GError            **error)
{
  g_return_val_if_fail (IDE_IS_CLANG_PROPOSALS (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

void
ide_clang_proposals_refilter (IdeClangProposals *self,
                              const gchar       *word)
{
  gboolean fast_refilter;

  g_assert (IDE_IS_CLANG_PROPOSALS (self));

  fast_refilter = self->filter && word && g_str_has_prefix (word, self->filter);
  ide_set_string (&self->filter, word);
  ide_clang_proposals_do_refilter (self, fast_refilter);
}

static guint
ide_clang_proposals_get_n_items (GListModel *model)
{
  return IDE_CLANG_PROPOSALS (model)->match_indexes->len;
}

static GType
ide_clang_proposals_get_item_type (GListModel *model)
{
  return IDE_TYPE_CLANG_COMPLETION_ITEM;
}

static gpointer
ide_clang_proposals_get_item (GListModel *model,
                              guint       position)
{
  IdeClangProposals *self = IDE_CLANG_PROPOSALS (model);
  Item *item = &g_array_index (self->match_indexes, Item, position);
  g_autoptr(GVariant) child = g_variant_get_child_value (self->results, item->index);
  const gchar *keyword = NULL;

  g_variant_lookup (child, "keyword", "&s", &keyword);

  return ide_clang_completion_item_new (self->results, item->index, keyword);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item = ide_clang_proposals_get_item;
  iface->get_item_type = ide_clang_proposals_get_item_type;
  iface->get_n_items = ide_clang_proposals_get_n_items;
}
