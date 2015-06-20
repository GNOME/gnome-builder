/* ide-clang-completion-provider.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-clang-completion"

#include <devhelp/dh-assistant-view.h>
#include <devhelp/dh-book-manager.h>
#include <glib/gi18n.h>

#include "ide-buffer.h"
#include "ide-clang-completion-item.h"
#include "ide-clang-completion-provider.h"
#include "ide-clang-service.h"
#include "ide-clang-translation-unit.h"
#include "ide-context.h"
#include "ide-debug.h"
#include "ide-file.h"
#include "ide-source-snippet.h"
#include "ide-source-view.h"

#define MAX_COMPLETION_ITEMS 200


struct _IdeClangCompletionProvider
{
  GObject parent_instance;

  IdeSourceView *view;
  GPtrArray     *last_results;
  GtkWidget     *assistant;
  GSettings     *settings;
};

typedef struct
{
  GCancellable                *cancellable;
  GtkSourceCompletionProvider *provider;
  GtkSourceCompletionContext  *context;
  GFile                       *file;
} AddProposalsState;

static void completion_provider_iface_init (GtkSourceCompletionProviderIface *);

G_DEFINE_TYPE_EXTENDED (IdeClangCompletionProvider,
                        ide_clang_completion_provider,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROVIDER,
                                               completion_provider_iface_init))

static DhBookManager *
get_book_manager (void)
{
  static DhBookManager *instance;

  if (instance == NULL)
    {
      instance = dh_book_manager_new ();
      dh_book_manager_populate (instance);
    }

  return instance;
}

static void
add_proposals_state_free (AddProposalsState *state)
{
  g_signal_handlers_disconnect_by_func (state->context,
                                        G_CALLBACK (g_cancellable_cancel),
                                        state->cancellable);

  g_clear_object (&state->provider);
  g_clear_object (&state->context);
  g_clear_object (&state->file);
  g_clear_object (&state->cancellable);
  g_free (state);
}

static gboolean
stop_on_predicate (gunichar ch,
                   gpointer data)
{
  switch (ch)
    {
    case '_':
      return FALSE;

    case ')':
    case '(':
    case '&':
    case '*':
    case '{':
    case '}':
    case ' ':
    case '\t':
    case '[':
    case ']':
    case '=':
    case '"':
    case '\'':
      return TRUE;

    default:
      return !g_unichar_isalnum (ch);
    }
}

static gboolean
matches (IdeClangCompletionItem *item,
         const gchar            *word)
{
  const gchar *typed_text;

  typed_text = ide_clang_completion_item_get_typed_text (item);
  return !!strstr (typed_text, word);
}

static gchar *
get_word (const GtkTextIter *location)
{
  GtkTextIter iter = *location;
  GtkTextBuffer *buffer;
  GtkTextIter end;

  end = iter;
  buffer = gtk_text_iter_get_buffer (&iter);

  if (!gtk_text_iter_backward_find_char (&iter, stop_on_predicate, NULL, NULL))
    return gtk_text_buffer_get_text (buffer, &iter, &end, TRUE);

  gtk_text_iter_forward_char (&iter);

  return gtk_text_iter_get_text (&iter, &end);
}

static GList *
filter_list (GPtrArray   *ar,
             const gchar *word)
{
  g_autoptr(GPtrArray) matched = NULL;
  GList *ret = NULL;
  gsize i;

  matched = g_ptr_array_new ();

  for (i = 0; i < ar->len; i++)
    {
      IdeClangCompletionItem *item;

      item = g_ptr_array_index (ar, i);
      if (matches (item, word))
        {
          g_ptr_array_add (matched, item);

          /*
           * FIXME:
           *
           * We should be a bit more intelligent about which items we accept.
           * The results don't come to us in "most important" order.
           */
          if (G_UNLIKELY (matched->len == MAX_COMPLETION_ITEMS))
            break;
        }
    }

  for (i = 0; i < matched->len; i++)
    ret = g_list_prepend (ret, g_ptr_array_index (matched, i));

  return ret;
}

static void
ide_clang_completion_provider_finalize (GObject *object)
{
  IdeClangCompletionProvider *self = (IdeClangCompletionProvider *)object;

  g_clear_pointer (&self->last_results, g_ptr_array_unref);
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
ide_clang_completion_provider_init (IdeClangCompletionProvider *self)
{
  self->settings = g_settings_new ("org.gnome.builder.code-insight");
}

static gchar *
ide_clang_completion_provider_get_name (GtkSourceCompletionProvider *provider)
{
  return g_strdup (_("Clang"));
}

static void
ide_clang_completion_provider_complete_cb (GObject      *object,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
  IdeClangTranslationUnit *tu = (IdeClangTranslationUnit *)object;
  IdeClangCompletionProvider *self;
  AddProposalsState *state = user_data;
  g_autofree gchar *word = NULL;
  g_autoptr(GPtrArray) ar = NULL;
  GtkTextIter iter;
  GError *error = NULL;
  GList *filtered = NULL;

  self = (IdeClangCompletionProvider *)state->provider;

  ar = ide_clang_translation_unit_code_complete_finish (tu, result, &error);

  if (!ar)
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
      goto failure;
    }

  if (self->last_results != NULL)
    g_ptr_array_unref (self->last_results);
  self->last_results = g_ptr_array_ref (ar);

  gtk_source_completion_context_get_iter (state->context, &iter);
  word = get_word (&iter);

  IDE_TRACE_MSG ("Current word: %s", word ?: "(null)");

  if (word)
    filtered = filter_list (ar, word);

failure:
  if (!g_cancellable_is_cancelled (state->cancellable))
    gtk_source_completion_context_add_proposals (state->context, state->provider, filtered, TRUE);

  g_list_free (filtered);
  add_proposals_state_free (state);
}

static void
ide_clang_completion_provider_tu_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  IdeClangService *service = (IdeClangService *)object;
  g_autoptr(IdeClangTranslationUnit) tu = NULL;
  AddProposalsState *state = user_data;
  GError *error = NULL;
  GtkTextIter iter;

  g_assert (IDE_IS_CLANG_SERVICE (service));
  g_assert (state);
  g_assert (IDE_IS_CLANG_COMPLETION_PROVIDER (state->provider));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (state->context));
  g_assert (G_IS_FILE (state->file));

  tu = ide_clang_service_get_translation_unit_finish (service, result, &error);

  if (!tu)
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
      goto cleanup;
    }

  if (!gtk_source_completion_context_get_iter (state->context, &iter))
    goto cleanup;

  ide_clang_translation_unit_code_complete_async (tu,
                                                  state->file,
                                                  &iter,
                                                  NULL,
                                                  ide_clang_completion_provider_complete_cb,
                                                  state);

  return;

cleanup:
  if (!g_cancellable_is_cancelled (state->cancellable))
    gtk_source_completion_context_add_proposals (state->context, state->provider, NULL, TRUE);
  add_proposals_state_free (state);
}

static void
ide_clang_completion_provider_populate (GtkSourceCompletionProvider *provider,
                                        GtkSourceCompletionContext  *context)
{
  IdeClangCompletionProvider *self = (IdeClangCompletionProvider *)provider;
  AddProposalsState *state;
  IdeClangService *service;
  GtkSourceCompletion *completion;
  g_autofree gchar *word = NULL;
  GtkSourceView *view;
  GtkTextBuffer *buffer;
  IdeContext *icontext;
  GtkTextIter iter;
  IdeFile *file;

  g_assert (IDE_IS_CLANG_COMPLETION_PROVIDER (self));

  if (!g_settings_get_boolean (self->settings, "clang-autocompletion"))
    goto failure;

  if (!gtk_source_completion_context_get_iter (context, &iter))
    goto failure;

  word = get_word (&iter);
  if (!word || !word [0] || !word [1])
    goto failure;

  buffer = gtk_text_iter_get_buffer (&iter);
  g_assert (IDE_IS_BUFFER (buffer));

  /* stash the view for later */
  if (G_UNLIKELY (!self->view))
    {
      g_object_get (context, "completion", &completion, NULL);
      g_assert (GTK_SOURCE_IS_COMPLETION (completion));
      view = gtk_source_completion_get_view (completion);
      g_assert (IDE_IS_SOURCE_VIEW (view));
      g_assert ((self->view == NULL) || (self->view == (IdeSourceView *)view));
      self->view = IDE_SOURCE_VIEW (view);
      g_clear_object (&completion);
    }

  file = ide_buffer_get_file (IDE_BUFFER (buffer));
  if (file == NULL)
    goto failure;

  g_assert (IDE_IS_FILE (file));

  icontext = ide_buffer_get_context (IDE_BUFFER (buffer));
  if (icontext == NULL)
    goto failure;

  g_assert (IDE_IS_CONTEXT (icontext));

  service = ide_context_get_service_typed (icontext, IDE_TYPE_CLANG_SERVICE);
  g_assert (IDE_IS_CLANG_SERVICE (service));

  state = g_new0 (AddProposalsState, 1);
  state->provider = g_object_ref (provider);
  state->context = g_object_ref (context);
  state->file = g_object_ref (ide_file_get_file (file));
  state->cancellable = g_cancellable_new ();

  g_signal_connect_swapped (context,
                            "cancelled",
                            G_CALLBACK (g_cancellable_cancel),
                            state->cancellable);

  ide_clang_service_get_translation_unit_async (service,
                                                file,
                                                0,
                                                state->cancellable,
                                                ide_clang_completion_provider_tu_cb,
                                                state);

  return;

failure:
  gtk_source_completion_context_add_proposals (context, provider, NULL, TRUE);
}

static gboolean
get_start_iter (GtkSourceCompletionProvider *provider,
                const GtkTextIter           *location,
                GtkSourceCompletionProposal *proposal,
                GtkTextIter                 *iter)
{
  IdeClangCompletionItem *item = (IdeClangCompletionItem *)proposal;
  const gchar *typed_text = ide_clang_completion_item_get_typed_text (item);
  g_autofree gchar *text = g_strdup (typed_text);
  gint len = g_utf8_strlen (typed_text ?: "", -1);
  GtkTextIter begin;
  GtkTextIter end;
  guint offset;

  end = begin = *location;

  offset = gtk_text_iter_get_offset (&end);

  if (offset >= len)
    {
      gchar *textptr = g_utf8_offset_to_pointer (text, len);
      gchar *prevptr;
      GtkTextIter match_start;
      GtkTextIter match_end;

      gtk_text_iter_set_offset (&begin, offset - len);

      while (*text)
        {
          if (gtk_text_iter_forward_search (&begin, text, GTK_TEXT_SEARCH_TEXT_ONLY, &match_start, &match_end, &end))
            {
              *iter = match_start;
              return TRUE;
            }

          prevptr = textptr;
          textptr = g_utf8_find_prev_char (text, textptr);
          *prevptr = '\0';
        }
    }

  return FALSE;
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
  IdeSourceSnippet *snippet;
  GtkTextBuffer *buffer;
  GtkTextIter end;

  IDE_ENTRY;

  g_assert (IDE_IS_CLANG_COMPLETION_PROVIDER (self));
  g_assert (IDE_IS_CLANG_COMPLETION_ITEM (item));

  if (!get_start_iter (provider, iter, proposal, &end))
    return FALSE;

  buffer = gtk_text_iter_get_buffer (iter);
  gtk_text_buffer_delete (buffer, iter, &end);

  snippet = ide_clang_completion_item_get_snippet (item);

  g_assert (snippet != NULL);
  g_assert (IDE_IS_SOURCE_SNIPPET (snippet));
  g_assert (IDE_IS_SOURCE_VIEW (self->view));

  ide_source_view_push_snippet (self->view, snippet);

  IDE_RETURN (TRUE);
}

static gint
ide_clang_completion_provider_get_interactive_delay (GtkSourceCompletionProvider *provider)
{
  return -1;
}

static void
ide_clang_completion_provider_update_info (GtkSourceCompletionProvider *provider,
                                           GtkSourceCompletionProposal *proposal,
                                           GtkSourceCompletionInfo     *info)
{
  IdeClangCompletionProvider *self = (IdeClangCompletionProvider *)provider;
  IdeClangCompletionItem *item = (IdeClangCompletionItem *)proposal;
  const gchar *typed_text;

  typed_text = ide_clang_completion_item_get_typed_text (item);
  dh_assistant_view_search (DH_ASSISTANT_VIEW (self->assistant), typed_text);

  if (info)
    gtk_widget_show (GTK_WIDGET (info));
}

static GtkWidget *
ide_clang_completion_provider_get_info_widget (GtkSourceCompletionProvider *provider,
                                               GtkSourceCompletionProposal *proposal)
{
  IdeClangCompletionProvider *self = (IdeClangCompletionProvider *)provider;

  if (self->assistant == NULL)
    {
      DhBookManager *book_manager;

      book_manager = get_book_manager ();
      self->assistant = dh_assistant_view_new ();
      dh_assistant_view_set_book_manager (DH_ASSISTANT_VIEW (self->assistant), book_manager);
    }

  ide_clang_completion_provider_update_info (provider, proposal, NULL);
  gtk_widget_show (self->assistant);

  gtk_widget_set_size_request (self->assistant, 300, 200);

  return self->assistant;
}

static gint
ide_clang_completion_provider_get_priority (GtkSourceCompletionProvider *provider)
{
  return IDE_CLANG_COMPLETION_PROVIDER_PRIORITY;
}

static void
completion_provider_iface_init (GtkSourceCompletionProviderIface *iface)
{
  iface->activate_proposal = ide_clang_completion_provider_activate_proposal;
  iface->get_interactive_delay = ide_clang_completion_provider_get_interactive_delay;
  iface->get_name = ide_clang_completion_provider_get_name;
  iface->get_start_iter = ide_clang_completion_provider_get_start_iter;
  iface->populate = ide_clang_completion_provider_populate;
  iface->get_info_widget = ide_clang_completion_provider_get_info_widget;
  iface->update_info = ide_clang_completion_provider_update_info;
  iface->get_priority = ide_clang_completion_provider_get_priority;
}
