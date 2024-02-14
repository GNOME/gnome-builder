/* ide-clang-completion-provider.c
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

#define G_LOG_DOMAIN "ide-clang-completion-provider"

#include <libide-sourceview.h>

#include "ide-clang-client.h"
#include "ide-clang-completion-item.h"
#include "ide-clang-completion-provider.h"
#include "ide-clang-proposals.h"

struct _IdeClangCompletionProvider
{
  IdeObject          parent_instance;

  IdeClangClient    *client;
  IdeClangProposals *proposals;

  char              *word;
  char              *refilter_word;

  guint              activation_keyval;
  GdkModifierType    activation_state;

  guint              loaded : 1;
};

static gboolean
is_field_access (GtkSourceCompletionContext *context)
{
  GtkTextIter begin, end;

  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));

  gtk_source_completion_context_get_bounds (context, &begin, &end);

  if (gtk_text_iter_backward_char (&begin))
    {
      if (gtk_text_iter_get_char (&begin) == '>')
        {
          if (gtk_text_iter_backward_char (&begin))
            {
              if (gtk_text_iter_get_char (&begin) == '-')
                return TRUE;
            }
        }

      if (gtk_text_iter_get_char (&begin) == '.')
        return TRUE;
    }

  return FALSE;
}

static gint
ide_clang_completion_provider_get_priority (GtkSourceCompletionProvider *provider,
                                            GtkSourceCompletionContext  *context)
{
  return 2000;
}

static gboolean
ide_clang_completion_provider_is_trigger (GtkSourceCompletionProvider *provider,
                                          const GtkTextIter     *iter,
                                          gunichar               ch)
{
  GtkTextBuffer *buffer = gtk_text_iter_get_buffer (iter);

  if (gtk_source_buffer_iter_has_context_class (GTK_SOURCE_BUFFER (buffer), iter, "comment") ||
      gtk_source_buffer_iter_has_context_class (GTK_SOURCE_BUFFER (buffer), iter, "string"))
    return FALSE;

  if (ch == '.' || ch == '(')
    return TRUE;

  if (ch == ':')
    {
      GtkTextIter copy = *iter;

      if (gtk_text_iter_backward_chars (&copy, 2))
        return gtk_text_iter_get_char (&copy) == ':';
    }

  if (ch == '>')
    {
      GtkTextIter copy = *iter;

      if (gtk_text_iter_backward_chars (&copy, 2))
        return gtk_text_iter_get_char (&copy) == '-';
    }

  return FALSE;
}

static gboolean
ide_clang_completion_provider_key_activates (GtkSourceCompletionProvider *provider,
                                             GtkSourceCompletionContext  *context,
                                             GtkSourceCompletionProposal *proposal,
                                             guint                        keyval,
                                             GdkModifierType              state)
{
  IdeClangCompletionProvider *self = (IdeClangCompletionProvider *)provider;
  IdeClangCompletionItem *item = IDE_CLANG_COMPLETION_ITEM (proposal);

  g_assert (IDE_IS_CLANG_COMPLETION_PROVIDER (self));
  g_assert (IDE_IS_CLANG_COMPLETION_ITEM (item));

  if ((state & (GDK_SHIFT_MASK|GDK_CONTROL_MASK)) == 0)
    {
      if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter)
        goto store_and_activate;
    }

  /* Try to dereference field/variable */
  if (item->kind == IDE_SYMBOL_KIND_FIELD || item->kind == IDE_SYMBOL_KIND_VARIABLE)
    {
      if (keyval == GDK_KEY_period || keyval == GDK_KEY_minus)
        goto store_and_activate;
    }

#if 0
  /* We add suffix ; if pressed */
  if (keyval == GDK_KEY_semicolon)
    goto store_and_activate;
#endif

#if 0
  /* Open parens for function */
  if (item->kind == IDE_SYMBOL_FUNCTION)
    return keyval == GDK_KEY_parenleft;
#endif

  self->activation_keyval = 0;
  self->activation_state = 0;

  return FALSE;

store_and_activate:
  self->activation_keyval = keyval;
  self->activation_state = state;

  return TRUE;
}

static gboolean
ends_with_string (GtkSourceSnippet *snippet,
                  const char       *string)
{
  const char *spec = "";
  guint n_chunks;
  int pos;

  g_assert (GTK_SOURCE_IS_SNIPPET (snippet));

  if (!(n_chunks = gtk_source_snippet_get_n_chunks (snippet)))
    return FALSE;

  pos = n_chunks - 1;

  while (ide_str_empty0 (spec) && pos >= 0)
    {
      GtkSourceSnippetChunk *chunk = gtk_source_snippet_get_nth_chunk (snippet, pos);
      spec = gtk_source_snippet_chunk_get_text (chunk);
      pos--;
    }

  return g_str_has_suffix (spec, string);
}

static void
ide_clang_completion_provider_activate (GtkSourceCompletionProvider *provider,
                                        GtkSourceCompletionContext  *context,
                                        GtkSourceCompletionProposal *proposal)
{
  IdeClangCompletionProvider *self = (IdeClangCompletionProvider *)provider;
  IdeClangCompletionItem *item = (IdeClangCompletionItem *)proposal;
  g_autofree gchar *word = NULL;
  g_autoptr(GtkSourceSnippet) snippet = NULL;
  IdeFileSettings *file_settings;
  GtkSourceBuffer *buffer;
  GtkSourceView *view;
  GtkTextIter begin, end;
  guint n_chunks;

  g_assert (IDE_IS_CLANG_COMPLETION_PROVIDER (self));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));
  g_assert (IDE_IS_CLANG_COMPLETION_ITEM (item));

  buffer = gtk_source_completion_context_get_buffer (context);
  view = gtk_source_completion_context_get_view (context);

  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (IDE_IS_SOURCE_VIEW (view));

  file_settings = ide_buffer_get_file_settings (IDE_BUFFER (buffer));

  /*
   * If the typed text matches the typed text of the item, and the user
   * it enter, then just skip the result and instead insert a newline.
   */
  if (self->activation_keyval == GDK_KEY_Return ||
      self->activation_keyval == GDK_KEY_KP_Enter)
    {
      if ((word = gtk_source_completion_context_get_word (context)))
        {
          if (ide_str_equal0 (word, item->typed_text))
            {
              gtk_source_completion_context_get_bounds (context, &begin, &end);
              gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &end, "\n", -1);

              return;
            }
        }
    }

  gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (buffer));

  if (gtk_source_completion_context_get_bounds (context, &begin, &end))
    gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &begin, &end);

  snippet = ide_clang_completion_item_get_snippet (item, file_settings);
  n_chunks = gtk_source_snippet_get_n_chunks (snippet);

  /* Check the last snippet chunk and see if it matches our current
   * position so we can omit it.
   */
  if (n_chunks > 0)
    {
      GtkSourceSnippetChunk *chunk;
      const gchar *text;
      GtkTextIter limit;

      chunk = gtk_source_snippet_get_nth_chunk (snippet, n_chunks-1);
      text = gtk_source_snippet_chunk_get_text (chunk);
      limit = end;

      if (text != NULL)
        {
          gtk_text_iter_forward_chars (&limit, g_utf8_strlen (text, -1));

          if (gtk_text_iter_get_line (&limit) != gtk_text_iter_get_line (&end))
            {
              limit = end;
              if (!gtk_text_iter_ends_line (&limit))
                gtk_text_iter_forward_to_line_end (&limit);
            }

          ide_text_util_remove_common_prefix (&end, text);
          begin = end;
        }
    }

  /*
   * If we are completing field or variable types, we might want to convert
   * a . to a -> if it's a pointer type.
   */
  if (item->kind == IDE_SYMBOL_KIND_FIELD || item->kind == IDE_SYMBOL_KIND_VARIABLE)
    {
      if (self->activation_keyval == GDK_KEY_period)
        {
          g_autoptr(GtkSourceSnippetChunk) chunk = gtk_source_snippet_chunk_new ();
          if (strchr (item->return_type, '*'))
            gtk_source_snippet_chunk_set_spec (chunk, "->");
          else
            gtk_source_snippet_chunk_set_spec (chunk, ".");
          gtk_source_snippet_add_chunk (snippet, chunk);
        }
    }

  /* If we're in a path context, remove everything within it because
   * clang will try to replace it and the trailing ".
   */
  if (gtk_source_buffer_iter_has_context_class (buffer, &begin, "path") &&
      gtk_source_buffer_iter_has_context_class (buffer, &end, "path"))
    {
      gunichar ch;

      while (gtk_text_iter_backward_char (&begin) &&
             !gtk_text_iter_starts_line (&begin) &&
             (ch = gtk_text_iter_get_char (&begin)) &&
             ch != '/' && ch != '<' && ch != '"')
        { /* Do Nothing */ }

      end = begin;
      gtk_source_buffer_iter_forward_to_context_class_toggle (buffer, &end, "path");

      if (gtk_text_iter_get_char (&begin) == '"' ||
          gtk_text_iter_get_char (&begin) == '<')
        gtk_text_iter_forward_char (&begin);

      gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &begin, &end);
    }

  if (self->activation_keyval == GDK_KEY_semicolon &&
      !ends_with_string (snippet, ";"))
    {
      g_autoptr(GtkSourceSnippetChunk) chunk = gtk_source_snippet_chunk_new ();
      gtk_source_snippet_chunk_set_spec (chunk, ";");
      gtk_source_snippet_add_chunk (snippet, chunk);
    }

  gtk_source_view_push_snippet (view, snippet, &begin);

  gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (buffer));
}

static void
ide_clang_completion_provider_refilter (GtkSourceCompletionProvider *provider,
                                        GtkSourceCompletionContext  *context,
                                        GListModel            *proposals)
{
  IdeClangCompletionProvider *self = (IdeClangCompletionProvider *)provider;

  IDE_ENTRY;

  g_assert (IDE_IS_CLANG_COMPLETION_PROVIDER (self));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));
  g_assert (G_IS_LIST_MODEL (proposals));

  if (self->proposals != NULL)
    {
      GtkTextIter begin, end;

      g_clear_pointer (&self->refilter_word, g_free);

      gtk_source_completion_context_get_bounds (context, &begin, &end);
      self->refilter_word = gtk_text_iter_get_slice (&begin, &end);
      ide_clang_proposals_refilter (self->proposals, self->refilter_word);

      IDE_EXIT;
    }

  IDE_EXIT;
}

static gchar *
ide_clang_completion_provider_get_title (GtkSourceCompletionProvider *provider)
{
  return g_strdup ("Clang");
}

static void
ide_clang_completion_provider_load (IdeClangCompletionProvider *self)
{
  g_autoptr(IdeClangClient) client = NULL;
  g_autoptr(IdeContext) context = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_CLANG_COMPLETION_PROVIDER (self));
  g_assert (self->loaded == FALSE);

  self->loaded = TRUE;

  context = ide_object_ref_context (IDE_OBJECT (self));
  client = ide_object_ensure_child_typed (IDE_OBJECT (context), IDE_TYPE_CLANG_CLIENT);

  g_set_object (&self->client, client);

  IDE_EXIT;
}

static void
ide_clang_completion_provider_populate_cb (GObject      *object,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
  IdeClangProposals *proposals = (IdeClangProposals *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_CLANG_PROPOSALS (proposals));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_clang_proposals_populate_finish (proposals, result, &error))
    {
      IDE_TRACE_MSG ("Clang completion failed: %s", error->message);
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  ide_task_return_pointer (task, g_object_ref (proposals), g_object_unref);

  IDE_EXIT;
}

static gboolean
can_first_symbol_char (gunichar ch)
{
  if (ch == 0)
    return TRUE;

  if (g_unichar_isdigit (ch))
    return FALSE;

  return ch == '_' || g_unichar_isalpha (ch);
}

static void
ide_clang_completion_provider_populate_async (GtkSourceCompletionProvider *provider,
                                              GtkSourceCompletionContext  *context,
                                              GCancellable                *cancellable,
                                              GAsyncReadyCallback          callback,
                                              gpointer                     user_data)
{
  IdeClangCompletionProvider *self = (IdeClangCompletionProvider *)provider;
  g_autoptr(IdeTask) task = NULL;
  GtkTextIter begin, end;

  IDE_ENTRY;

  g_assert (IDE_IS_CLANG_COMPLETION_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (!self->loaded)
    ide_clang_completion_provider_load (self);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_clang_completion_provider_populate_async);

  g_clear_pointer (&self->refilter_word, g_free);
  g_clear_pointer (&self->word, g_free);

  if (gtk_source_completion_context_get_bounds (context, &begin, &end))
    self->word = gtk_text_iter_get_slice (&begin, &end);

  if (self->proposals == NULL)
    self->proposals = ide_clang_proposals_new (self->client);

  /* If the first character is not a symbol character, then just
   * avoid any sort of completion work now.
   */
  if (self->word && !can_first_symbol_char (g_utf8_get_char (self->word)))
    {
      ide_clang_proposals_clear (self->proposals);
      ide_task_return_pointer (task,
                               g_object_ref (self->proposals),
                               g_object_unref);
      IDE_EXIT;
    }

  /* Deliver results immediately until our updated results come in. Often what
   * the user wants will be in the previous list too, and that can drop the
   * latency a bit.
   */
  if (!is_field_access (context))
    gtk_source_completion_context_set_proposals_for_provider (context,
                                                              provider,
                                                              G_LIST_MODEL (self->proposals));

  ide_clang_proposals_populate_async (self->proposals,
                                      &begin,
                                      self->word,
                                      cancellable,
                                      ide_clang_completion_provider_populate_cb,
                                      g_steal_pointer (&task));

  IDE_EXIT;
}

static GListModel *
ide_clang_completion_provider_populate_finish (GtkSourceCompletionProvider  *provider,
                                               GAsyncResult                 *result,
                                               GError                      **error)
{
  GListModel *ret;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CLANG_COMPLETION_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
ide_clang_completion_provider_display (GtkSourceCompletionProvider   *provider,
                                       GtkSourceCompletionContext    *context,
                                       GtkSourceCompletionProposal   *proposal,
                                       GtkSourceCompletionCell       *cell)
{
  IdeClangCompletionProvider *self = (IdeClangCompletionProvider *)provider;
  IdeClangCompletionItem *item = IDE_CLANG_COMPLETION_ITEM (proposal);
  const char *typed_text;

  g_assert (IDE_IS_CLANG_COMPLETION_PROVIDER (self));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));
  g_assert (IDE_IS_CLANG_COMPLETION_ITEM (item));
  g_assert (GTK_SOURCE_IS_COMPLETION_CELL (cell));

  if (self->refilter_word)
    typed_text = self->refilter_word;
  else
    typed_text = self->word;

  ide_clang_completion_item_display (item, cell, typed_text);
}

static void
provider_iface_init (GtkSourceCompletionProviderInterface *iface)
{
  iface->get_priority = ide_clang_completion_provider_get_priority;
  iface->is_trigger = ide_clang_completion_provider_is_trigger;
  iface->key_activates = ide_clang_completion_provider_key_activates;
  iface->activate = ide_clang_completion_provider_activate;
  iface->refilter = ide_clang_completion_provider_refilter;
  iface->get_title = ide_clang_completion_provider_get_title;
  iface->populate_async = ide_clang_completion_provider_populate_async;
  iface->populate_finish = ide_clang_completion_provider_populate_finish;
  iface->display = ide_clang_completion_provider_display;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeClangCompletionProvider, ide_clang_completion_provider, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROVIDER, provider_iface_init))

static void
ide_clang_completion_provider_destroy (IdeObject *object)
{
  IdeClangCompletionProvider *self = (IdeClangCompletionProvider *)object;

  if (self->proposals != NULL)
    ide_clang_proposals_clear (self->proposals);

  g_clear_object (&self->client);
  g_clear_object (&self->proposals);

  g_clear_pointer (&self->word, g_free);
  g_clear_pointer (&self->refilter_word, g_free);

  IDE_OBJECT_CLASS (ide_clang_completion_provider_parent_class)->destroy (object);
}

static void
ide_clang_completion_provider_class_init (IdeClangCompletionProviderClass *klass)
{
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  i_object_class->destroy = ide_clang_completion_provider_destroy;
}

static void
ide_clang_completion_provider_init (IdeClangCompletionProvider *self)
{
}
