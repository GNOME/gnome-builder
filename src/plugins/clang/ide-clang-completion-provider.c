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
#include "ide-clang-proposals.h"

#include "sourceview/ide-text-iter.h"

struct _IdeClangCompletionProvider
{
  IdeObject parent_instance;
  /*
   * We keep a copy of settings so that we can ignore completion requests
   * if the user has specifically disabled completion.
   */
  GSettings *settings;
  /*
   * We save a weak pointer to the view that performed the request
   * so that we can push a snippet onto the view instead of inserting
   * text into the buffer.
   */
  IdeSourceView *view;
  /*
   * The proposals helper that manages generating results.
   */
  IdeClangProposals *proposals;
};

typedef struct
{
  IdeClangCompletionProvider *self;
  GtkSourceCompletionContext *context;
  GCancellable               *cancellable;
} Populate;

static void completion_provider_iface_init (GtkSourceCompletionProviderIface *);
static void populate_free                   (Populate *);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (Populate, populate_free);

G_DEFINE_TYPE_EXTENDED (IdeClangCompletionProvider,
                        ide_clang_completion_provider,
                        IDE_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROVIDER,
                                               completion_provider_iface_init)
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_COMPLETION_PROVIDER, NULL))

static void
populate_free (Populate *p)
{
  g_clear_object (&p->self);
  g_clear_object (&p->context);
  g_clear_object (&p->cancellable);
  g_slice_free (Populate, p);
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
  g_autofree gchar *word = NULL;
  GtkTextIter iter;
  GtkTextBuffer *buffer;
  IdeFile *file;

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
  if (activation == GTK_SOURCE_COMPLETION_ACTIVATION_USER_REQUESTED)
    return TRUE;

  if (!(word = _ide_text_iter_current_symbol (&iter, NULL)))
    return FALSE;

  return TRUE;
}

static void
ide_clang_completion_provider_populate_cb (GObject      *object,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
  IdeClangProposals *proposals = (IdeClangProposals *)object;
  g_autoptr(Populate) state = user_data;
  g_autoptr(GError) error = NULL;
  const GList *results = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_CLANG_PROPOSALS (proposals));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (state != NULL);
  g_assert (IDE_IS_CLANG_COMPLETION_PROVIDER (state->self));
  g_assert (G_IS_CANCELLABLE (state->cancellable));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (state->context));

  if (ide_clang_proposals_populate_finish (proposals, result, NULL))
    results = ide_clang_proposals_get_list (proposals);

  if (!g_cancellable_is_cancelled (state->cancellable))
    gtk_source_completion_context_add_proposals (state->context,
                                                 GTK_SOURCE_COMPLETION_PROVIDER (state->self),
                                                 (GList *)results,
                                                 TRUE);
  IDE_EXIT;
}

static void
ide_clang_completion_provider_populate (GtkSourceCompletionProvider *provider,
                                        GtkSourceCompletionContext  *context)
{
  IdeClangCompletionProvider *self = (IdeClangCompletionProvider *)provider;
  GtkSourceCompletionActivation activation;
  g_autoptr(GtkSourceCompletion) completion = NULL;
  Populate *state;
  GtkTextIter iter;
  gboolean user_requested;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_CLANG_COMPLETION_PROVIDER (self));
  g_return_if_fail (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));

  activation = gtk_source_completion_context_get_activation (context);
  user_requested = activation == GTK_SOURCE_COMPLETION_ACTIVATION_USER_REQUESTED;

  if (!gtk_source_completion_context_get_iter (context, &iter))
    IDE_GOTO (failure);

  g_object_get (context,
                "completion", &completion,
                NULL);

  self->view = IDE_SOURCE_VIEW (gtk_source_completion_get_view (completion));

  if (self->proposals == NULL)
    {
      IdeContext *ctx = ide_object_get_context (IDE_OBJECT (self));
      IdeClangClient *client = ide_context_get_service_typed (ctx, IDE_TYPE_CLANG_CLIENT);

      self->proposals = ide_clang_proposals_new (client);
    }

  state = g_slice_new0 (Populate);
  state->self = g_object_ref (self);
  state->cancellable = g_cancellable_new ();
  state->context = g_object_ref (context);

  g_signal_connect_object (state->context,
                           "cancelled",
                           G_CALLBACK (g_cancellable_cancel),
                           state->cancellable,
                           G_CONNECT_SWAPPED);

  ide_clang_proposals_populate_async (self->proposals,
                                      &iter,
                                      user_requested,
                                      state->cancellable,
                                      ide_clang_completion_provider_populate_cb,
                                      state);

  IDE_EXIT;

failure:
  gtk_source_completion_context_add_proposals (context, provider, NULL, TRUE);

  IDE_EXIT;
}

static gboolean
ide_clang_completion_provider_get_start_iter (GtkSourceCompletionProvider *provider,
                                              GtkSourceCompletionContext  *context,
                                              GtkSourceCompletionProposal *proposal,
                                              GtkTextIter                 *iter)
{
  g_autofree gchar *word = NULL;
  GtkTextIter begin;
  GtkTextIter end;

  if (gtk_source_completion_context_get_iter (context, &end) &&
      (word = _ide_text_iter_current_symbol (&end, &begin)))
    {
      *iter = begin;
      return TRUE;
    }

  return FALSE;
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
  g_autofree gchar *word = NULL;
  GtkTextBuffer *buffer;
  GtkTextIter begin;
  IdeFile *file;

  IDE_ENTRY;

  g_assert (IDE_IS_CLANG_COMPLETION_PROVIDER (self));
  g_assert (IDE_IS_CLANG_COMPLETION_ITEM (item));

  if (!(word = _ide_text_iter_current_symbol (iter, &begin)))
    IDE_RETURN (FALSE);

  buffer = gtk_text_iter_get_buffer (iter);
  g_assert (IDE_IS_BUFFER (buffer));

  gtk_text_buffer_delete (buffer, &begin, iter);

  file = ide_buffer_get_file (IDE_BUFFER (buffer));
  g_assert (IDE_IS_FILE (file));

  file_settings = ide_file_peek_settings (file);
  g_assert (!file_settings || IDE_IS_FILE_SETTINGS (file_settings));

  snippet = ide_clang_completion_item_get_snippet (item, file_settings);

  g_assert (snippet != NULL);
  g_assert (IDE_IS_SOURCE_SNIPPET (snippet));
  g_assert (IDE_IS_SOURCE_VIEW (self->view));

  ide_source_view_push_snippet (self->view, snippet, &begin);

  /* ensure @iter is kept valid */
  gtk_text_buffer_get_iter_at_mark (buffer, iter, gtk_text_buffer_get_insert (buffer));

  IDE_RETURN (TRUE);
}

static void
ide_clang_completion_provider_finalize (GObject *object)
{
  IdeClangCompletionProvider *self = (IdeClangCompletionProvider *)object;

  g_clear_object (&self->proposals);
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
  self->settings = g_settings_new ("org.gnome.builder.code-insight");
}
