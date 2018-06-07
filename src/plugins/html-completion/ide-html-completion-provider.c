/* ide-html-completion-provider.c
 *
 * Copyright 2014 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "html-completion"

#include "ide-html-completion-provider.h"
#include "ide-html-proposal.h"
#include "ide-html-proposals.h"

struct _IdeHtmlCompletionProvider
{
  GObject parent_instance;
  IdeHtmlProposals *proposals;
};

static void completion_provider_init (IdeCompletionProviderInterface *iface);

G_DEFINE_TYPE_WITH_CODE (IdeHtmlCompletionProvider,
                         ide_html_completion_provider,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_COMPLETION_PROVIDER, completion_provider_init))

static gboolean
in_element (const GtkTextIter *iter)
{
  GtkTextIter copy = *iter;

  /*
   * this is a stupidly simple algorithm. walk backwards, until we reach either
   * <, >, or start of buffer.
   */

  while (gtk_text_iter_backward_char (&copy))
    {
      gunichar ch;

      ch = gtk_text_iter_get_char (&copy);

      if (ch == '/')
        {
          gtk_text_iter_backward_char (&copy);
          ch = gtk_text_iter_get_char (&copy);
          if (ch == '<')
            return IDE_HTML_PROPOSAL_ELEMENT_END;
        }

      if (ch == '>')
        return FALSE;
      else if (ch == '<')
        {
          GtkTextIter end = copy;

          if (gtk_text_iter_forward_char (&end) && '?' == gtk_text_iter_get_char (&end))
            return FALSE;

          return TRUE;
        }
    }

  return FALSE;
}

static gboolean
in_attribute_value (const GtkTextIter *iter,
                    gunichar           looking_for)
{
  GtkTextIter copy = *iter;

  if (!gtk_text_iter_backward_char (&copy))
    return FALSE;

  do
    {
      gunichar ch;

      if (gtk_text_iter_ends_line (&copy))
        return FALSE;

      ch = gtk_text_iter_get_char (&copy);

      if (ch == looking_for)
        {
          gtk_text_iter_backward_char (&copy);
          return (gtk_text_iter_get_char (&copy) == '=');
        }
    }
  while (gtk_text_iter_backward_char (&copy));

  return FALSE;
}

static gboolean
in_attribute_named (const GtkTextIter *iter,
                    const gchar       *name)
{
  GtkTextIter copy = *iter;
  GtkTextIter match_start;
  GtkTextIter match_end;
  gboolean ret = FALSE;

  if (gtk_text_iter_backward_search (&copy, "='",
                                     GTK_TEXT_SEARCH_TEXT_ONLY,
                                     &match_start, &match_end,
                                     NULL) ||
      gtk_text_iter_backward_search (&copy, "=\"",
                                     GTK_TEXT_SEARCH_TEXT_ONLY,
                                     &match_start, &match_end,
                                     NULL))
    {
      GtkTextIter word_begin = match_start;
      gchar *word;

      gtk_text_iter_backward_chars (&word_begin, strlen (name));
      word = gtk_text_iter_get_slice (&word_begin, &match_start);
      ret = (g_strcmp0 (word, name) == 0);
      g_free (word);
    }

  return ret;
}

static IdeHtmlProposalKind
get_mode (const GtkTextIter *iter)
{
  GtkTextIter back;

  g_assert (iter != NULL);

  /*
   * Ignore the = after attribute name.
   */
  back = *iter;
  gtk_text_iter_backward_char (&back);
  if (gtk_text_iter_get_char (&back) == '=')
    return IDE_HTML_PROPOSAL_NONE;

  /*
   * Check for various state inside of element start (<).
   */
  if (in_element (iter))
    {
      GtkTextIter copy = *iter;
      gunichar ch;

      /*
       * If there are no spaces until we reach <, then we are in element name.
       */
      while (gtk_text_iter_backward_char (&copy))
        {
          ch = gtk_text_iter_get_char (&copy);

          if (ch == '/')
            {
              GtkTextIter copy2 = copy;

              gtk_text_iter_backward_char (&copy2);
              if (gtk_text_iter_get_char (&copy2) == '<')
                return IDE_HTML_PROPOSAL_ELEMENT_END;
            }

          if (ch == '<')
            return IDE_HTML_PROPOSAL_ELEMENT_START;

          if (g_unichar_isalnum (ch))
            continue;

          break;
        }

      /*
       * Now check to see if we are in an attribute value.
       */
      if (in_attribute_value (iter, '"') || in_attribute_value (iter, '\''))
        {
          /*
           * If the attribute name is style, then we are in CSS.
           */
          if (in_attribute_named (iter, "style"))
            return IDE_HTML_PROPOSAL_CSS_PROPERTY;

          return IDE_HTML_PROPOSAL_ATTRIBUTE_VALUE;
        }

      /*
       * Not in attribute value, but in element (and not the name). Must be
       * attribute name. But only say so if we have moved past ' or ".
       */
      ch = gtk_text_iter_get_char (&back);
      if (ch != '\'' && ch != '"')
        return IDE_HTML_PROPOSAL_ATTRIBUTE_NAME;
    }

  return IDE_HTML_PROPOSAL_NONE;
}

static gboolean
find_space (gunichar ch,
            gpointer user_data)
{
  return g_unichar_isspace (ch);
}

static gchar *
get_element (const GtkTextIter *iter)
{
  GtkTextIter begin;
  GtkTextIter end;
  GtkTextIter match_begin;
  GtkTextIter match_end;

  g_assert (iter != NULL);

  if (gtk_text_iter_backward_search (iter, "<", GTK_TEXT_SEARCH_TEXT_ONLY, &match_begin, &match_end, NULL))
    {
      end = begin = match_end;

      if (gtk_text_iter_forward_find_char (&end, find_space, NULL, iter))
        return gtk_text_iter_get_slice (&begin, &end);
    }

  return NULL;
}

static void
whereami (IdeCompletionContext  *context,
          IdeHtmlProposalKind   *kind,
          gchar                **element)
{
  GtkTextIter begin, end;

  g_assert (IDE_IS_COMPLETION_CONTEXT (context));
  g_assert (kind != NULL);
  g_assert (element != NULL);

  ide_completion_context_get_bounds (context, &begin, &end);

  *kind = get_mode (&begin);
  *element = NULL;

  switch (*kind)
    {
    case IDE_HTML_PROPOSAL_ELEMENT_START:
    case IDE_HTML_PROPOSAL_ELEMENT_END:
      *element = ide_completion_context_get_word (context);
      break;

    case IDE_HTML_PROPOSAL_ATTRIBUTE_NAME:
      *element = get_element (&begin);
      break;

    case IDE_HTML_PROPOSAL_NONE:
    case IDE_HTML_PROPOSAL_ATTRIBUTE_VALUE:
    case IDE_HTML_PROPOSAL_CSS_PROPERTY:
    default:
      break;
    }
}

static void
ide_html_completion_provider_populate_async (IdeCompletionProvider *provider,
                                             IdeCompletionContext  *context,
                                             GCancellable          *cancellable,
                                             GAsyncReadyCallback    callback,
                                             gpointer               user_data)
{
  IdeHtmlCompletionProvider *self = (IdeHtmlCompletionProvider *)provider;
  g_autoptr(IdeTask) task = NULL;
  IdeHtmlProposalKind kind = 0;
  g_autofree gchar *element = NULL;
  g_autofree gchar *word = NULL;
  g_autofree gchar *casefold = NULL;

  g_assert (IDE_IS_COMPLETION_PROVIDER (provider));
  g_assert (IDE_IS_COMPLETION_CONTEXT (context));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_html_completion_provider_populate_async);

  if (ide_completion_context_is_language (context, "css"))
    kind = IDE_HTML_PROPOSAL_CSS_PROPERTY;
  else
    whereami (context, &kind, &element);

  if ((word = ide_completion_context_get_word (context)))
    casefold = g_utf8_casefold (word, -1);

  if (self->proposals == NULL)
    self->proposals = ide_html_proposals_new ();

  ide_html_proposals_refilter (self->proposals, kind, element, casefold);

  ide_task_return_object (task, g_object_ref (self->proposals));
}

static GListModel *
ide_html_completion_provider_populate_finish (IdeCompletionProvider  *provider,
                                              GAsyncResult           *result,
                                              GError                **error)
{
  g_assert (IDE_IS_HTML_COMPLETION_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_object (IDE_TASK (result), error);
}

static gboolean
ide_html_completion_provider_refilter (IdeCompletionProvider *provider,
                                       IdeCompletionContext  *context,
                                       GListModel            *proposals)
{
  IdeHtmlProposalKind kind = 0;
  g_autofree gchar *element = NULL;
  g_autofree gchar *word = NULL;
  g_autofree gchar *casefold = NULL;

  g_assert (IDE_IS_HTML_COMPLETION_PROVIDER (provider));
  g_assert (IDE_IS_COMPLETION_CONTEXT (context));
  g_assert (IDE_IS_HTML_PROPOSALS (proposals));

  if (ide_completion_context_is_language (context, "css"))
    kind = IDE_HTML_PROPOSAL_CSS_PROPERTY;
  else
    whereami (context, &kind, &element);

  if ((word = ide_completion_context_get_word (context)))
    casefold = g_utf8_casefold (word, -1);

  ide_html_proposals_refilter (IDE_HTML_PROPOSALS (proposals), kind, element, casefold);

  return TRUE;
}

static void
ide_html_completion_provider_activate_proposal (IdeCompletionProvider *provider,
                                                IdeCompletionContext  *context,
                                                IdeCompletionProposal *proposal,
                                                const GdkEventKey     *key)
{
  g_autoptr(IdeSnippet) snippet = NULL;
  IdeHtmlProposal *item = (IdeHtmlProposal *)proposal;
  IdeHtmlProposalKind kind;
  GtkTextBuffer *buffer;
  GtkTextView *view;
  GtkTextIter begin, end;

  g_assert (IDE_IS_HTML_COMPLETION_PROVIDER (provider));
  g_assert (IDE_IS_COMPLETION_CONTEXT (context));
  g_assert (IDE_IS_HTML_PROPOSAL (item));

  snippet = ide_html_proposal_get_snippet (item);
  kind = ide_html_proposal_get_kind (item);

  buffer = ide_completion_context_get_buffer (context);
  view = ide_completion_context_get_view (context);

  gtk_text_buffer_begin_user_action (buffer);
  if (ide_completion_context_get_bounds (context, &begin, &end))
    gtk_text_buffer_delete (buffer, &begin, &end);

  if (kind == IDE_HTML_PROPOSAL_ELEMENT_START && gtk_text_iter_get_char (&begin) != '>')
    {
      g_autoptr(IdeSnippetChunk) chunk1 = ide_snippet_chunk_new ();
      g_autoptr(IdeSnippetChunk) chunk2 = ide_snippet_chunk_new ();

      ide_snippet_chunk_set_tab_stop (chunk1, 0);
      ide_snippet_chunk_set_spec (chunk2, ">");

      ide_snippet_add_chunk (snippet, chunk1);
      ide_snippet_add_chunk (snippet, chunk2);
    }

  if (kind == IDE_HTML_PROPOSAL_CSS_PROPERTY)
    {
      g_autoptr(IdeSnippetChunk) chunk1 = ide_snippet_chunk_new ();
      g_autoptr(IdeSnippetChunk) chunk2 = ide_snippet_chunk_new ();
      g_autoptr(IdeSnippetChunk) chunk3 = ide_snippet_chunk_new ();

      ide_snippet_chunk_set_spec (chunk1, ": ");
      ide_snippet_chunk_set_tab_stop (chunk2, 0);
      ide_snippet_chunk_set_spec (chunk3, ";");

      ide_snippet_add_chunk (snippet, chunk1);
      ide_snippet_add_chunk (snippet, chunk2);
      ide_snippet_add_chunk (snippet, chunk3);
    }

  ide_source_view_push_snippet (IDE_SOURCE_VIEW (view), snippet, &begin);

  gtk_text_buffer_end_user_action (buffer);
}

static void
ide_html_completion_provider_display_proposal (IdeCompletionProvider   *provider,
                                               IdeCompletionListBoxRow *row,
                                               IdeCompletionContext    *context,
                                               const gchar             *typed_text,
                                               IdeCompletionProposal   *proposal)
{
  g_autofree gchar *markup = NULL;
  const gchar *word;
  IdeHtmlProposalKind kind;

  g_assert (IDE_IS_HTML_COMPLETION_PROVIDER (provider));
  g_assert (IDE_IS_COMPLETION_LIST_BOX_ROW (row));
  g_assert (IDE_IS_COMPLETION_CONTEXT (context));
  g_assert (IDE_IS_HTML_PROPOSAL (proposal));

  word = ide_html_proposal_get_word (IDE_HTML_PROPOSAL (proposal));
  markup = ide_completion_fuzzy_highlight (word, typed_text);
  kind = ide_html_proposal_get_kind (IDE_HTML_PROPOSAL (proposal));

  switch (kind)
    {
    case IDE_HTML_PROPOSAL_CSS_PROPERTY:
      /* probably could use something css specific */
      ide_completion_list_box_row_set_icon_name (row, "ui-property-symbolic");
      break;

    case IDE_HTML_PROPOSAL_ELEMENT_START:
    case IDE_HTML_PROPOSAL_ELEMENT_END:
    case IDE_HTML_PROPOSAL_ATTRIBUTE_NAME:
    case IDE_HTML_PROPOSAL_ATTRIBUTE_VALUE:
    case IDE_HTML_PROPOSAL_NONE:
    default:
      ide_completion_list_box_row_set_icon_name (row, NULL);
      break;
    }

  ide_completion_list_box_row_set_left (row, NULL);
  ide_completion_list_box_row_set_right (row, NULL);
  ide_completion_list_box_row_set_center_markup (row, markup);
}

static gint
ide_html_completion_provider_get_priority (IdeCompletionProvider *provider,
                                           IdeCompletionContext  *context)
{
  return 200;
}

static gboolean
in_comment (const GtkTextIter *iter)
{
  GtkTextBuffer *buffer = gtk_text_iter_get_buffer (iter);

  return gtk_source_buffer_iter_has_context_class (GTK_SOURCE_BUFFER (buffer), iter, "string") &&
         gtk_source_buffer_iter_has_context_class (GTK_SOURCE_BUFFER (buffer), iter, "comment");
}

static gboolean
ide_html_completion_provider_is_trigger (IdeCompletionProvider *provider,
                                         const GtkTextIter     *iter,
                                         gunichar               ch)
{
  if (ch == ' ')
    {
      GtkTextIter cur = *iter;
      gunichar prev;

      if (gtk_text_iter_backward_char (&cur) &&
          (prev = gtk_text_iter_get_char (&cur)) &&
          !g_unichar_isspace (prev) &&
          !in_comment (&cur))
        return TRUE;
    }

  return FALSE;
}

static void
ide_html_completion_provider_finalize (GObject *object)
{
  IdeHtmlCompletionProvider *self = (IdeHtmlCompletionProvider *)object;

  g_clear_object (&self->proposals);

  G_OBJECT_CLASS (ide_html_completion_provider_parent_class)->finalize (object);
}

static void
ide_html_completion_provider_class_init (IdeHtmlCompletionProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_html_completion_provider_finalize;
}

static void
ide_html_completion_provider_init (IdeHtmlCompletionProvider *self)
{
}

static void
completion_provider_init (IdeCompletionProviderInterface *iface)
{
  iface->populate_async = ide_html_completion_provider_populate_async;
  iface->populate_finish = ide_html_completion_provider_populate_finish;
  iface->refilter = ide_html_completion_provider_refilter;
  iface->activate_proposal = ide_html_completion_provider_activate_proposal;
  iface->display_proposal = ide_html_completion_provider_display_proposal;
  iface->get_priority = ide_html_completion_provider_get_priority;
  iface->is_trigger = ide_html_completion_provider_is_trigger;
}
