/* ide-clang-completion-provider.c
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "clang-completion-provider"

#include <ide.h>

#include "ide-clang-client.h"
#include "ide-clang-completion-item.h"
#include "ide-clang-completion-provider.h"

struct _IdeClangCompletionProvider
{
  IdeObject      parent_instance;

  GSettings     *settings;
  gchar         *last_line;
  GPtrArray     *last_results;
  gchar         *last_query;
  /*
   * As an optimization, the linked list for result nodes are
   * embedded in the IdeClangCompletionItem structures and we
   * do not allocate them. This is the pointer to the first item
   * in the result set that matches our query. It is not allocated
   * and do not tree to free it or perform g_list_*() operations
   * upon it.
   */
  GList         *head;
  /*
   * We save a weak pointer to the view that performed the request
   * so that we can push a snippet onto the view instead of inserting
   * text into the buffer.
   */
  IdeSourceView *view;
  /*
   * The saved offset used when generating results. This is our position
   * where we moved past all the junk to a stop character (as required
   * by clang).
   */
  guint stop_line;
  guint stop_line_offset;
};

typedef struct
{
  IdeClangCompletionProvider *self;
  GtkSourceCompletionContext *context;
  IdeFile *file;
  GCancellable *cancellable;
  gchar *line;
  gchar *query;
} IdeClangCompletionState;

static void completion_provider_iface_init (GtkSourceCompletionProviderIface *iface);

G_DEFINE_TYPE_EXTENDED (IdeClangCompletionProvider,
                        ide_clang_completion_provider,
                        IDE_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROVIDER,
                                               completion_provider_iface_init)
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_COMPLETION_PROVIDER, NULL))

static void
ide_clang_completion_state_free (IdeClangCompletionState *state)
{
  g_clear_object (&state->self);
  g_clear_object (&state->cancellable);
  g_clear_object (&state->context);
  g_clear_object (&state->file);
  g_clear_pointer (&state->line, g_free);
  g_clear_pointer (&state->query, g_free);
  g_slice_free (IdeClangCompletionState, state);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeClangCompletionState, ide_clang_completion_state_free)

static gint
sort_by_priority (gconstpointer a,
                  gconstpointer b)
{
  const IdeClangCompletionItem *itema = (const IdeClangCompletionItem *)a;
  const IdeClangCompletionItem *itemb = (const IdeClangCompletionItem *)b;

  if (itema->priority < itemb->priority)
    return -1;
  else if (itema->priority > itemb->priority)
    return 1;

  /* If the item is in the result set here, we should have a valid
   * typed_text field because we already scored the completion item.
   */

  return g_strcmp0 (itema->typed_text, itemb->typed_text);
}

static void
ide_clang_completion_provider_sort_by_priority (IdeClangCompletionProvider *self)
{
  g_assert (IDE_IS_CLANG_COMPLETION_PROVIDER (self));

  self->head = g_list_sort (self->head, sort_by_priority);
}

static gchar *
ide_clang_completion_provider_get_name (GtkSourceCompletionProvider *provider)
{
  return g_strdup ("Clang");
}

static gint
ide_clang_completion_provider_get_priority (GtkSourceCompletionProvider *provider)
{
  return IDE_CLANG_COMPLETION_PROVIDER_PRIORITY;
}

static gboolean
ide_clang_completion_provider_match (GtkSourceCompletionProvider *provider,
                                     GtkSourceCompletionContext  *context)
{
  IdeClangCompletionProvider *self = (IdeClangCompletionProvider *)provider;
  GtkSourceCompletionActivation activation;
  GtkTextBuffer *buffer;
  IdeFile *file;
  GtkTextIter iter;

  g_return_val_if_fail (IDE_IS_CLANG_COMPLETION_PROVIDER (self), FALSE);
  g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_CONTEXT (context), FALSE);

  if (!g_settings_get_boolean (self->settings, "clang-autocompletion"))
    return FALSE;

  if (!gtk_source_completion_context_get_iter (context, &iter))
    return FALSE;

  buffer = gtk_text_iter_get_buffer (&iter);
  if (!IDE_IS_BUFFER (buffer) ||
      !(file = ide_buffer_get_file (IDE_BUFFER (buffer))) ||
      ide_file_get_is_temporary (file))
    return FALSE;

  activation = gtk_source_completion_context_get_activation (context);

  if (activation == GTK_SOURCE_COMPLETION_ACTIVATION_INTERACTIVE)
    {
      gunichar ch;

      /* avoid auto completion while in comments, strings, etc */
      if (ide_completion_provider_context_in_comment_or_string (context))
        return FALSE;

      if (gtk_text_iter_starts_line (&iter))
        return FALSE;

      gtk_text_iter_backward_char (&iter);

      ch = gtk_text_iter_get_char (&iter);

      if (!g_unichar_isalnum (ch) && ch != '_')
        return FALSE;
    }

  return TRUE;
}

static gboolean
ide_clang_completion_provider_can_replay (IdeClangCompletionProvider *self,
                                          const gchar                *line)
{
  const gchar *suffix;

  IDE_ENTRY;

  g_assert (IDE_IS_CLANG_COMPLETION_PROVIDER (self));

  if (self->last_results == NULL)
    IDE_RETURN (FALSE);

  if (line == NULL || *line == '\0' || self->last_line == NULL)
    IDE_RETURN (FALSE);

  if (!g_str_has_prefix (line, self->last_line))
    IDE_RETURN (FALSE);

  suffix = line + strlen (self->last_line);

  IDE_TRACE_MSG ("Checking \"%s\" for invalidating characters", suffix);

  for (; *suffix; suffix = g_utf8_next_char (suffix))
    {
      gunichar ch = g_utf8_get_char (suffix);
      if (!g_unichar_isalnum (ch) && (ch != '_'))
        {
          IDE_TRACE_MSG ("contains invaliding characters");
          IDE_RETURN (FALSE);
        }
    }

  IDE_RETURN (TRUE);
}

static void
ide_clang_completion_provider_save_results (IdeClangCompletionProvider *self,
                                            GPtrArray                  *results,
                                            const gchar                *line,
                                            const gchar                *query)
{
  IDE_ENTRY;

  g_assert (IDE_IS_CLANG_COMPLETION_PROVIDER (self));

  g_clear_pointer (&self->last_results, g_ptr_array_unref);
  g_clear_pointer (&self->last_line, g_free);
  g_clear_pointer (&self->last_query, g_free);
  self->head = NULL;

  if (query && !*query)
    query = NULL;

  if (results != NULL)
    {
      self->last_line = g_strdup (line);
      self->last_query = g_strdup (query);
      self->last_results = g_ptr_array_ref (results);
      if (results->len > 0)
        {
          IdeClangCompletionItem *head = g_ptr_array_index (results, 0);
          self->head = &head->link;
        }
    }

  IDE_EXIT;
}

static void
ide_clang_completion_provider_update_links (IdeClangCompletionProvider *self,
                                            GPtrArray                  *results)
{
  IdeClangCompletionItem *item;
  IdeClangCompletionItem *next;
  IdeClangCompletionItem *prev;

  IDE_ENTRY;

  g_assert (IDE_IS_CLANG_COMPLETION_PROVIDER (self));
  g_assert (results != NULL);

  if G_UNLIKELY (results->len == 0)
    {
      self->head = NULL;
      IDE_EXIT;
    }

  /* Unrolling loops for the gentoo crowd */

  item = g_ptr_array_index (results, 0);
  item->link.prev = NULL;
  item->link.next = (results->len == 1) ? NULL : &((IdeClangCompletionItem*)g_ptr_array_index (results, 1))->link;

  self->head = &item->link;

  prev = item;

  for (guint i = 1; i < (results->len - 1); i++)
    {
      item = g_ptr_array_index (results, i);
      next = g_ptr_array_index (results, i + 1);

      item->link.prev = &prev->link;
      item->link.next = &next->link;

      prev = item;
    }

  if (results->len > 1)
    {
      item = g_ptr_array_index (results, results->len - 1);
      item->link.prev = &((IdeClangCompletionItem*)g_ptr_array_index (results, results->len-2))->link;
      item->link.next = NULL;
    }

  IDE_EXIT;
}

static void
ide_clang_completion_provider_refilter (IdeClangCompletionProvider *self,
                                        GPtrArray                  *results,
                                        const gchar                *query)
{
  g_autofree gchar *lower = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_CLANG_COMPLETION_PROVIDER (self));
  g_assert (results != NULL);
  g_assert (query != NULL);

  if (results->len == 0)
    IDE_EXIT;

  IDE_TRACE_MSG ("Filtering with query \"%s\"", query);

  /*
   * By traversing the linked list nodes instead of the array, we allow
   * ourselves to avoid rechecking items we already know filtered.
   * We do need to be mindful of this in case the user backspaced
   * and our list is no longer a continual "deep dive" of matched items.
   */
  if ((self->last_query != NULL) && g_str_has_prefix (query, self->last_query))
    ide_clang_completion_provider_update_links (self, results);

  lower = g_utf8_casefold (query, -1);

  if (!g_str_is_ascii (lower))
    {
      g_warning ("Item filtering requires ascii input.");
      IDE_EXIT;
    }

  for (GList *iter = self->head; iter; iter = iter->next)
    {
      IdeClangCompletionItem *item = iter->data;
      const gchar *typed_text;
      guint priority;

      typed_text = ide_clang_completion_item_get_typed_text (item);

      if (!ide_completion_item_fuzzy_match (typed_text, lower, &priority))
        {
          if (iter->prev != NULL)
            iter->prev->next = iter->next;
          else
            self->head = iter->next;

          if (iter->next != NULL)
            iter->next->prev = iter->prev;

          continue;
        }

      /* Save the generated priority for further sorting */
      item->priority = priority;
    }

  g_free (self->last_query);
  self->last_query = g_strdup (query);

  IDE_EXIT;
}

static void
ide_clang_completion_provider_complete_cb (GObject      *object,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
  IdeClangClient *client = (IdeClangClient *)object;
  g_autoptr(IdeClangCompletionState) state = user_data;
  g_autoptr(GPtrArray) results = NULL;
  g_autoptr(GVariant) vresults = NULL;
  g_autoptr(GError) error = NULL;
  guint n_items;

  IDE_ENTRY;

  g_assert (IDE_IS_CLANG_CLIENT (client));
  g_assert (state != NULL);
  g_assert (IDE_IS_CLANG_COMPLETION_PROVIDER (state->self));
  g_assert (G_IS_CANCELLABLE (state->cancellable));
  g_assert (IDE_IS_FILE (state->file));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (state->context));

  if (!(vresults = ide_clang_client_complete_finish (client, result, &error)))
    {
      g_debug ("%s", error->message);
      if (!g_cancellable_is_cancelled (state->cancellable))
        gtk_source_completion_context_add_proposals (state->context,
                                                     GTK_SOURCE_COMPLETION_PROVIDER (state->self),
                                                     NULL, TRUE);
      IDE_EXIT;
    }

  n_items = (guint)g_variant_n_children (vresults);
  results = g_ptr_array_new_full (n_items, g_object_unref);

  for (guint i = 0; i < n_items; i++)
    g_ptr_array_add (results, ide_clang_completion_item_new (vresults, i));

  ide_clang_completion_provider_save_results (state->self, results, state->line, state->query);
  ide_clang_completion_provider_update_links (state->self, results);

  if (!g_cancellable_is_cancelled (state->cancellable))
    {
      if (results->len > 0)
        {
          if (state->query && *state->query)
            ide_clang_completion_provider_refilter (state->self, results, state->query);
          ide_clang_completion_provider_sort_by_priority (state->self);
          IDE_TRACE_MSG ("%d results returned from clang", results->len);
          gtk_source_completion_context_add_proposals (state->context,
                                                       GTK_SOURCE_COMPLETION_PROVIDER (state->self),
                                                       state->self->head, TRUE);
        }
      else
        {
          IDE_TRACE_MSG ("No results returned from clang");
          gtk_source_completion_context_add_proposals (state->context,
                                                       GTK_SOURCE_COMPLETION_PROVIDER (state->self),
                                                       NULL, TRUE);
        }
    }
  else
    {
      IDE_TRACE_MSG ("Ignoring completions due to cancellation");
    }

  IDE_EXIT;
}

static void
ide_clang_completion_provider_build_flags_cb (GObject      *object,
                                              GAsyncResult *result,
                                              gpointer      user_data)
{
  IdeBuildSystem *build_system = (IdeBuildSystem *)object;
  g_autoptr(IdeClangCompletionState) state = user_data;
  g_autoptr(GError) error = NULL;
  g_auto(GStrv) flags = NULL;
  IdeClangClient *client;
  IdeContext *context;
  GCancellable *cancellable;
  GFile *file;
  GtkTextIter iter;
  guint line;
  guint column;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_SYSTEM (build_system));
  g_assert (state != NULL);
  g_assert (IDE_IS_CLANG_COMPLETION_PROVIDER (state->self));
  g_assert (G_IS_CANCELLABLE (state->cancellable));
  g_assert (IDE_IS_FILE (state->file));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (state->context));

  flags = ide_build_system_get_build_flags_finish (build_system, result, NULL);

  /*
   * It's like we are racing with our future self, which coallesced the
   * compile request into a single delayed query. Use the cancellable
   * to detect if we were cancelled and short-circuit early.
   */
  if (g_cancellable_is_cancelled (state->cancellable))
    IDE_EXIT;

  context = ide_object_get_context (IDE_OBJECT (build_system));
  client = ide_context_get_service_typed (context, IDE_TYPE_CLANG_CLIENT);
  file = ide_file_get_file (state->file);
  cancellable = state->cancellable;

  gtk_source_completion_context_get_iter (state->context, &iter);

  line = gtk_text_iter_get_line (&iter) + 1;
  column = gtk_text_iter_get_line_offset (&iter) + 1;

  ide_clang_client_complete_async (client,
                                   file,
                                   (const gchar * const *)flags,
                                   line,
                                   column,
                                   cancellable,
                                   ide_clang_completion_provider_complete_cb,
                                   g_steal_pointer (&state));

  IDE_EXIT;
}

static void
ide_clang_completion_provider_populate (GtkSourceCompletionProvider *provider,
                                        GtkSourceCompletionContext  *context)
{
  IdeClangCompletionProvider *self = (IdeClangCompletionProvider *)provider;
  IdeClangCompletionState *state;
  GtkSourceCompletionActivation activation;
  g_autoptr(GtkSourceCompletion) completion = NULL;
  g_autofree gchar *line = NULL;
  g_autofree gchar *prefix = NULL;
  GtkTextIter begin;
  GtkTextIter iter;
  GtkTextIter stop;
  GtkTextBuffer *buffer;
  IdeBuildSystem *build_system;
  gunichar ch;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_CLANG_COMPLETION_PROVIDER (self));
  g_return_if_fail (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));

  activation = gtk_source_completion_context_get_activation (context);

  if (!gtk_source_completion_context_get_iter (context, &iter))
    IDE_GOTO (failure);

  buffer = gtk_text_iter_get_buffer (&iter);

  /* Get the line text up to the insertion mark */
  begin = iter;
  gtk_text_iter_set_line_offset (&begin, 0);
  line = gtk_text_iter_get_slice (&begin, &iter);

  /* Move to the just inserted character */
  stop = iter;
  if (!gtk_text_iter_starts_line (&stop))
    gtk_text_iter_backward_char (&stop);

  if (activation == GTK_SOURCE_COMPLETION_ACTIVATION_INTERACTIVE)
    {
      if (gtk_text_iter_get_char (&stop) == ';')
        IDE_GOTO (failure);
    }

  /*
   * Walk backwards to locate the first character after a stop character.
   * A stop character is anything that can't go in a function/type.
   */
  while (!gtk_text_iter_starts_line (&stop) &&
         (ch = gtk_text_iter_get_char (&stop)) &&
         (g_unichar_isalnum (ch) || (ch == '_')) &&
         gtk_text_iter_backward_char (&stop))
    { /* Do nothing */ }

  ch = gtk_text_iter_get_char (&stop);

  /* Move forward if we moved past the last matching char */
  if (!g_unichar_isalnum (ch) &&
      (ch != '_') &&
      (gtk_text_iter_compare (&stop, &iter) < 0))
    gtk_text_iter_forward_char (&stop);

  self->stop_line = gtk_text_iter_get_line (&stop);
  self->stop_line_offset = gtk_text_iter_get_line_offset (&stop);

  prefix = g_strstrip (gtk_text_iter_get_slice (&stop, &iter));

  /*
   * We might be able to reuse the results from our previous query if
   * the buffer is sufficiently similar. If so, possibly just rearrange
   * some things and redisplay those results.
   *
   * However, we always want to perform a new query if ctrl+space was
   * pressed.
   */
  if ((activation != GTK_SOURCE_COMPLETION_ACTIVATION_USER_REQUESTED) &&
      ide_clang_completion_provider_can_replay (self, line))
    {
      IDE_PROBE;

      /*
       * Filter the items that no longer match our query.
       * We save a little state so that we can optimize further
       * passes of this operation by traversing the already filtered
       * linked list instead of all items.
       */
      ide_clang_completion_provider_refilter (self, self->last_results, prefix);
      ide_clang_completion_provider_sort_by_priority (self);
      gtk_source_completion_context_add_proposals (context, provider, self->head, TRUE);

      IDE_EXIT;
    }

#if 0
  /*
   * If we are completed interactively, we only want to activate the clang
   * completion provider if a translation unit is immediatley available.
   * Otherwise, we delay the other completion providers from being visible
   * until after this one has completed. Instead, we'll just queue the load
   * of translation unit for a follow use.
   */
  if (activation == GTK_SOURCE_COMPLETION_ACTIVATION_INTERACTIVE)
    {
      IdeFile *file = ide_buffer_get_file (IDE_BUFFER (buffer));

      tu = ide_clang_service_get_cached_translation_unit (service, file);

      if (tu == NULL)
        {
          ide_clang_service_get_translation_unit_async (service, file, 0, NULL, NULL, NULL);
          gtk_source_completion_context_add_proposals (context, provider, NULL, TRUE);
          IDE_EXIT;
        }
    }
#endif

  /* Save the view so we can push a snippet later */
  g_object_get (context, "completion", &completion, NULL);
  self->view = IDE_SOURCE_VIEW (gtk_source_completion_get_view (completion));

  ide_buffer_sync_to_unsaved_files (IDE_BUFFER (buffer));

  state = g_slice_new0 (IdeClangCompletionState);
  state->self = g_object_ref (self);
  state->context = g_object_ref (context);
  state->file = g_object_ref (ide_buffer_get_file (IDE_BUFFER (buffer)));
  state->cancellable = g_cancellable_new ();
  state->query = prefix, prefix = NULL;
  state->line = line, line = NULL;

  g_signal_connect_object (context,
                           "cancelled",
                           G_CALLBACK (g_cancellable_cancel),
                           state->cancellable,
                           G_CONNECT_SWAPPED);

  build_system = ide_context_get_build_system (ide_object_get_context (IDE_OBJECT (self)));

  ide_build_system_get_build_flags_async (build_system,
                                          state->file,
                                          state->cancellable,
                                          ide_clang_completion_provider_build_flags_cb,
                                          state);

  IDE_EXIT;

failure:
  gtk_source_completion_context_add_proposals (context, provider, NULL, TRUE);

  IDE_EXIT;
}

static gboolean
get_start_iter (GtkSourceCompletionProvider *provider,
                const GtkTextIter           *location,
                GtkSourceCompletionProposal *proposal,
                GtkTextIter                 *iter)
{

  IdeClangCompletionProvider *self = (IdeClangCompletionProvider *)provider;
  GtkTextBuffer *buffer = gtk_text_iter_get_buffer (location);

#if !GTK_CHECK_VERSION(3, 19, 0)
# error "The following requires safety introduced in 3.19.x"
#endif

  gtk_text_buffer_get_iter_at_line_offset (buffer,
                                           iter,
                                           self->stop_line,
                                           self->stop_line_offset);

  if (gtk_text_iter_get_line (iter) != gtk_text_iter_get_line (location))
    return FALSE;

  return TRUE;
}

static gboolean
ide_clang_completion_provider_get_start_iter (GtkSourceCompletionProvider *provider,
                                              GtkSourceCompletionContext  *context,
                                              GtkSourceCompletionProposal *proposal,
                                              GtkTextIter                 *iter)
{
  GtkTextIter location;

  gtk_source_completion_context_get_iter (context, &location);
  return get_start_iter (provider, &location, proposal, iter);
}

static gboolean
ide_clang_completion_provider_activate_proposal (GtkSourceCompletionProvider *provider,
                                                 GtkSourceCompletionProposal *proposal,
                                                 GtkTextIter                 *iter)
{
  IdeClangCompletionProvider *self = (IdeClangCompletionProvider *)provider;
  IdeClangCompletionItem *item = (IdeClangCompletionItem *)proposal;
  g_autoptr(IdeSourceSnippet) snippet = NULL;
  IdeFileSettings *file_settings;
  GtkTextBuffer *buffer;
  IdeFile *file;
  GtkTextIter end;

  IDE_ENTRY;

  g_assert (IDE_IS_CLANG_COMPLETION_PROVIDER (self));
  g_assert (IDE_IS_CLANG_COMPLETION_ITEM (item));

  if (!get_start_iter (provider, iter, proposal, &end))
    IDE_RETURN (FALSE);

  buffer = gtk_text_iter_get_buffer (iter);
  g_assert (IDE_IS_BUFFER (buffer));

  gtk_text_buffer_delete (buffer, iter, &end);

  file = ide_buffer_get_file (IDE_BUFFER (buffer));
  g_assert (IDE_IS_FILE (file));

  file_settings = ide_file_peek_settings (file);
  g_assert (!file_settings || IDE_IS_FILE_SETTINGS (file_settings));

  snippet = ide_clang_completion_item_get_snippet (item, file_settings);

  g_assert (snippet != NULL);
  g_assert (IDE_IS_SOURCE_SNIPPET (snippet));
  g_assert (IDE_IS_SOURCE_VIEW (self->view));

  ide_source_view_push_snippet (self->view, snippet, iter);

  /* ensure @iter is kept valid */
  gtk_text_buffer_get_iter_at_mark (buffer, iter, gtk_text_buffer_get_insert (buffer));

  IDE_RETURN (TRUE);
}

static void
ide_clang_completion_provider_finalize (GObject *object)
{
  IdeClangCompletionProvider *self = (IdeClangCompletionProvider *)object;

  g_clear_pointer (&self->last_results, g_ptr_array_unref);
  g_clear_pointer (&self->last_line, g_free);
  g_clear_pointer (&self->last_query, g_free);
  g_clear_object (&self->settings);

  G_OBJECT_CLASS (ide_clang_completion_provider_parent_class)->finalize (object);
}

static void
ide_clang_completion_provider_class_init (IdeClangCompletionProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_clang_completion_provider_finalize;
}

static void
completion_provider_iface_init (GtkSourceCompletionProviderIface *iface)
{
  iface->activate_proposal = ide_clang_completion_provider_activate_proposal;
  iface->get_name = ide_clang_completion_provider_get_name;
  iface->get_priority = ide_clang_completion_provider_get_priority;
  iface->get_start_iter = ide_clang_completion_provider_get_start_iter;
  iface->match = ide_clang_completion_provider_match;
  iface->populate = ide_clang_completion_provider_populate;
}

static void
ide_clang_completion_provider_init (IdeClangCompletionProvider *self)
{
  IDE_ENTRY;
  self->settings = g_settings_new ("org.gnome.builder.code-insight");
  IDE_EXIT;
}
