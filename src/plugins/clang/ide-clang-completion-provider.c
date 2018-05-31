/* ide-clang-completion-provider.c
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

#define G_LOG_DOMAIN "ide-clang-completion-provider"

#include "ide-clang-client.h"
#include "ide-clang-completion-item.h"
#include "ide-clang-completion-provider.h"
#include "ide-clang-proposals.h"

struct _IdeClangCompletionProvider
{
  IdeObject          parent_instance;
  IdeClangClient    *client;
  IdeClangProposals *proposals;
};

static gint
ide_clang_completion_provider_get_priority (IdeCompletionProvider *provider)
{
  return 200;
}

static gboolean
ide_clang_completion_provider_is_trigger (IdeCompletionProvider *provider,
                                          const GtkTextIter     *iter,
                                          gunichar               ch)
{
  if (ch == '.' || ch == '(')
    return TRUE;

  if (ch == '>')
    {
      GtkTextIter copy = *iter;

      if (gtk_text_iter_backward_chars (&copy, 2))
        return gtk_text_iter_get_char (&copy) == '-';
    }

  return FALSE;
}

static gboolean
ide_clang_completion_provider_key_activates (IdeCompletionProvider *provider,
                                             IdeCompletionProposal *proposal,
                                             const GdkEventKey     *key)
{
  IdeClangCompletionItem *item = IDE_CLANG_COMPLETION_ITEM (proposal);

  /* We add suffix ; if pressed */
  if (key->keyval == GDK_KEY_semicolon)
    return TRUE;

  /* Try to dereference field/variable */
  if (item->kind == IDE_SYMBOL_FIELD || item->kind == IDE_SYMBOL_VARIABLE)
    return key->keyval == GDK_KEY_period;

  /* Open parens for function */
  if (item->kind == IDE_SYMBOL_FUNCTION)
    return key->keyval == GDK_KEY_parenleft;

  return FALSE;

}

static void
ide_clang_completion_provider_activate_proposal (IdeCompletionProvider *provider,
                                                 IdeCompletionContext  *context,
                                                 IdeCompletionProposal *proposal,
                                                 const GdkEventKey     *key)
{
  IdeClangCompletionProvider *self = (IdeClangCompletionProvider *)provider;
  g_autoptr(IdeSourceSnippet) snippet = NULL;
  IdeClangCompletionItem *item;
  GtkTextBuffer *buffer;
  GtkTextView *view;
  IdeFile *file;
  GtkTextIter begin, end;

  g_assert (IDE_IS_CLANG_COMPLETION_PROVIDER (self));
  g_assert (IDE_IS_COMPLETION_CONTEXT (context));
  g_assert (IDE_IS_CLANG_COMPLETION_ITEM (proposal));

  buffer = ide_completion_context_get_buffer (context);
  view = ide_completion_context_get_view (context);

  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (IDE_IS_SOURCE_VIEW (view));

  file = ide_buffer_get_file (IDE_BUFFER (buffer));
  g_assert (IDE_IS_FILE (file));

  gtk_text_buffer_begin_user_action (buffer);

  if (ide_completion_context_get_bounds (context, &begin, &end))
    gtk_text_buffer_delete (buffer, &begin, &end);

  item = IDE_CLANG_COMPLETION_ITEM (proposal);
  snippet = ide_clang_completion_item_get_snippet (item, ide_file_peek_settings (file));

  /*
   * If we are completing field or variable types, we might want to add
   * a . or -> to the snippet based on the input character.
   */
  if (item->kind == IDE_SYMBOL_FIELD || item->kind == IDE_SYMBOL_VARIABLE)
    {
      if (key->keyval == GDK_KEY_period || key->keyval == GDK_KEY_minus)
        {
          g_autoptr(IdeSourceSnippetChunk) chunk = ide_source_snippet_chunk_new ();
          if (strchr (item->return_type, '*'))
            ide_source_snippet_chunk_set_spec (chunk, "->");
          else
            ide_source_snippet_chunk_set_spec (chunk, ".");
          ide_source_snippet_add_chunk (snippet, chunk);
        }
    }

  if (key->keyval == GDK_KEY_semicolon)
    {
      g_autoptr(IdeSourceSnippetChunk) chunk = ide_source_snippet_chunk_new ();
      ide_source_snippet_chunk_set_spec (chunk, ";");
      ide_source_snippet_add_chunk (snippet, chunk);
    }

  ide_source_view_push_snippet (IDE_SOURCE_VIEW (view), snippet, &begin);

  gtk_text_buffer_end_user_action (buffer);
}

static gboolean
ide_clang_completion_provider_refilter (IdeCompletionProvider *provider,
                                        IdeCompletionContext  *context,
                                        GListModel            *proposals)
{
  IdeClangCompletionProvider *self = (IdeClangCompletionProvider *)provider;

  g_assert (IDE_IS_CLANG_COMPLETION_PROVIDER (self));
  g_assert (IDE_IS_COMPLETION_CONTEXT (context));
  g_assert (G_IS_LIST_MODEL (proposals));

  if (self->proposals != NULL)
    {
      g_autofree gchar *word = NULL;
      GtkTextIter begin, end;

      ide_completion_context_get_bounds (context, &begin, &end);
      word = gtk_text_iter_get_slice (&begin, &end);
      ide_clang_proposals_refilter (self->proposals, word);

      return TRUE;
    }

  return FALSE;
}

static gchar *
ide_clang_completion_provider_get_title (IdeCompletionProvider *provider)
{
  return g_strdup ("Clang");
}

static void
ide_clang_completion_provider_load (IdeCompletionProvider *provider,
                                    IdeContext            *context)
{
  IdeClangCompletionProvider *self = (IdeClangCompletionProvider *)provider;
  IdeClangClient *client;

  g_assert (IDE_IS_CLANG_COMPLETION_PROVIDER (self));
  g_assert (IDE_IS_CONTEXT (context));

  client = ide_context_get_service_typed (context, IDE_TYPE_CLANG_CLIENT);
  g_set_object (&self->client, client);
}

static void
ide_clang_completion_provider_populate_cb (GObject      *object,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
  IdeClangProposals *proposals = (IdeClangProposals *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_CLANG_PROPOSALS (proposals));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_clang_proposals_populate_finish (proposals, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_pointer (task, g_object_ref (proposals), g_object_unref);
}

static void
ide_clang_completion_provider_populate_async (IdeCompletionProvider  *provider,
                                              IdeCompletionContext   *context,
                                              GCancellable           *cancellable,
                                              GListModel            **proposals,
                                              GAsyncReadyCallback     callback,
                                              gpointer                user_data)
{
  IdeClangCompletionProvider *self = (IdeClangCompletionProvider *)provider;
  g_autoptr(IdeTask) task = NULL;
  g_autofree gchar *word = NULL;
  GtkTextIter begin, end;

  g_assert (IDE_IS_CLANG_COMPLETION_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_clang_completion_provider_populate_async);

  if (ide_completion_context_get_bounds (context, &begin, &end))
    word = gtk_text_iter_get_slice (&begin, &end);

  if (self->proposals == NULL)
    self->proposals = ide_clang_proposals_new (self->client);

  *proposals = g_object_ref (G_LIST_MODEL (self->proposals));

  ide_clang_proposals_populate_async (self->proposals,
                                      &begin,
                                      word,
                                      cancellable,
                                      ide_clang_completion_provider_populate_cb,
                                      g_steal_pointer (&task));
}

static GListModel *
ide_clang_completion_provider_populate_finish (IdeCompletionProvider  *provider,
                                               GAsyncResult           *result,
                                               GError                **error)
{
  g_assert (IDE_IS_CLANG_COMPLETION_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static void
ide_clang_completion_provider_display_proposal (IdeCompletionProvider   *provider,
                                                IdeCompletionListBoxRow *row,
                                                IdeCompletionContext    *context,
                                                const gchar             *typed_text,
                                                IdeCompletionProposal   *proposal)
{
  IdeClangCompletionItem *item = IDE_CLANG_COMPLETION_ITEM (proposal);
  g_autofree gchar *markup = NULL;
  g_autofree gchar *highlight = NULL;

  highlight = ide_completion_item_fuzzy_highlight (item->typed_text, typed_text);
  ide_completion_list_box_row_set_icon_name (row, item->icon_name);
  ide_completion_list_box_row_set_left (row, item->return_type);
  markup = g_strdup_printf ("%s%s<span fgalpha='32767'>%s</span>",
                            highlight,
                            item->params ? " " : "",
                            item->params ?: "");
  ide_completion_list_box_row_set_center_markup (row, markup);
}

static void
provider_iface_init (IdeCompletionProviderInterface *iface)
{
  iface->load = ide_clang_completion_provider_load;
  iface->get_priority = ide_clang_completion_provider_get_priority;
  iface->is_trigger = ide_clang_completion_provider_is_trigger;
  iface->key_activates = ide_clang_completion_provider_key_activates;
  iface->activate_proposal = ide_clang_completion_provider_activate_proposal;
  iface->refilter = ide_clang_completion_provider_refilter;
  iface->get_title = ide_clang_completion_provider_get_title;
  iface->populate_async = ide_clang_completion_provider_populate_async;
  iface->populate_finish = ide_clang_completion_provider_populate_finish;
  iface->display_proposal = ide_clang_completion_provider_display_proposal;
}

G_DEFINE_TYPE_WITH_CODE (IdeClangCompletionProvider, ide_clang_completion_provider, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_COMPLETION_PROVIDER, provider_iface_init))

static void
ide_clang_completion_provider_dispose (GObject *object)
{
  IdeClangCompletionProvider *self = (IdeClangCompletionProvider *)object;

  if (self->proposals != NULL)
    ide_clang_proposals_clear (self->proposals);

  g_clear_object (&self->client);
  g_clear_object (&self->proposals);

  G_OBJECT_CLASS (ide_clang_completion_provider_parent_class)->dispose (object);
}

static void
ide_clang_completion_provider_class_init (IdeClangCompletionProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_clang_completion_provider_dispose;
}

static void
ide_clang_completion_provider_init (IdeClangCompletionProvider *self)
{
}
