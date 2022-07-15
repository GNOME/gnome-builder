/* gbp-word-completion-provider.c
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

#define G_LOG_DOMAIN "gbp-word-completion-provider"

#include "config.h"

#include <libide-sourceview.h>

#include "gbp-word-completion-provider.h"
#include "gbp-word-proposal.h"
#include "gbp-word-proposals.h"

struct _GbpWordCompletionProvider
{
  GObject           parent_instance;
  GbpWordProposals *proposals;
};

static void completion_provider_iface_init (GtkSourceCompletionProviderInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpWordCompletionProvider, gbp_word_completion_provider, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROVIDER, completion_provider_iface_init))

static void
gbp_word_completion_provider_finalize (GObject *object)
{
  GbpWordCompletionProvider *self = (GbpWordCompletionProvider *)object;

  g_clear_object (&self->proposals);

  G_OBJECT_CLASS (gbp_word_completion_provider_parent_class)->finalize (object);
}

static void
gbp_word_completion_provider_class_init (GbpWordCompletionProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_word_completion_provider_finalize;
}

static void
gbp_word_completion_provider_init (GbpWordCompletionProvider *self)
{
}

static void
gbp_word_completion_provider_refilter (GtkSourceCompletionProvider *provider,
                                       GtkSourceCompletionContext  *context,
                                       GListModel                  *model)
{
  g_autofree gchar *casefold = NULL;
  g_autofree gchar *word = NULL;

  g_assert (GBP_IS_WORD_COMPLETION_PROVIDER (provider));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));
  g_assert (GBP_IS_WORD_PROPOSALS (model));

  if ((word = gtk_source_completion_context_get_word (context)))
    casefold = g_utf8_casefold (word, -1);

  gbp_word_proposals_refilter (GBP_WORD_PROPOSALS (model), casefold);
}

static void
gbp_word_completion_provider_populate_cb (GObject      *object,
                                          GAsyncResult *result,
                                          gpointer      user_data)
{
  GbpWordProposals *proposals = (GbpWordProposals *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (GBP_IS_WORD_PROPOSALS (proposals));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!gbp_word_proposals_populate_finish (proposals, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_object (task, g_object_ref (proposals));
}

static void
gbp_word_completion_provider_populate_async (GtkSourceCompletionProvider *provider,
                                             GtkSourceCompletionContext  *context,
                                             GCancellable                *cancellable,
                                             GAsyncReadyCallback          callback,
                                             gpointer                     user_data)
{
  GbpWordCompletionProvider *self = (GbpWordCompletionProvider *)provider;
  GtkSourceCompletionActivation activation;
  g_autoptr(IdeTask) task = NULL;

  g_assert (GBP_IS_WORD_COMPLETION_PROVIDER (self));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_word_completion_provider_populate_async);

  if (self->proposals == NULL)
    self->proposals = gbp_word_proposals_new ();

  /*
   * Avoid extra processing unless the user requested completion, since
   * scanning the buffer is rather expensive.
   */
  activation = gtk_source_completion_context_get_activation (context);
  if (activation != GTK_SOURCE_COMPLETION_ACTIVATION_USER_REQUESTED)
    {
      gbp_word_proposals_clear (self->proposals);
      ide_task_return_boolean (task, TRUE);
      return;
    }

  gtk_source_completion_context_set_proposals_for_provider (context,
                                                            provider,
                                                            G_LIST_MODEL (self->proposals));

  gbp_word_proposals_populate_async (self->proposals,
                                     context,
                                     cancellable,
                                     gbp_word_completion_provider_populate_cb,
                                     g_steal_pointer (&task));
}

static GListModel *
gbp_word_completion_provider_populate_finish (GtkSourceCompletionProvider  *provider,
                                              GAsyncResult           *result,
                                              GError                **error)
{
  g_assert (GBP_IS_WORD_COMPLETION_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_object (IDE_TASK (result), error);
}

static void
gbp_word_completion_provider_display (GtkSourceCompletionProvider *provider,
                                      GtkSourceCompletionContext  *context,
                                      GtkSourceCompletionProposal *proposal,
                                      GtkSourceCompletionCell     *cell)
{
  GtkSourceCompletionColumn column;

  g_assert (GBP_IS_WORD_COMPLETION_PROVIDER (provider));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));
  g_assert (GTK_SOURCE_IS_COMPLETION_PROPOSAL (proposal));
  g_assert (GTK_SOURCE_IS_COMPLETION_CELL (cell));

  column = gtk_source_completion_cell_get_column (cell);

  switch (column)
    {
    case GTK_SOURCE_COMPLETION_COLUMN_ICON:
      gtk_source_completion_cell_set_icon_name (cell, "completion-word-symbolic");
      break;

    case GTK_SOURCE_COMPLETION_COLUMN_TYPED_TEXT:
      {
        const char *word = gbp_word_proposal_get_word (GBP_WORD_PROPOSAL (proposal));
        g_autofree char *typed_text = gtk_source_completion_context_get_word (context);
        g_autoptr(PangoAttrList) attrs = gtk_source_completion_fuzzy_highlight (word, typed_text);
        gtk_source_completion_cell_set_text_with_attributes (cell, word, attrs);
        break;
      }

    case GTK_SOURCE_COMPLETION_COLUMN_BEFORE:
    case GTK_SOURCE_COMPLETION_COLUMN_AFTER:
    case GTK_SOURCE_COMPLETION_COLUMN_COMMENT:
    case GTK_SOURCE_COMPLETION_COLUMN_DETAILS:
    default:
      gtk_source_completion_cell_set_text (cell, NULL);
      break;
    }
}

static void
gbp_word_completion_provider_activate (GtkSourceCompletionProvider *provider,
                                       GtkSourceCompletionContext  *context,
                                       GtkSourceCompletionProposal *proposal)
{
  GtkSourceBuffer *buffer;
  const gchar *word;
  GtkTextIter begin, end;

  g_assert (GBP_IS_WORD_COMPLETION_PROVIDER (provider));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));
  g_assert (GBP_IS_WORD_PROPOSAL (proposal));

  buffer = gtk_source_completion_context_get_buffer (context);
  word = gbp_word_proposal_get_word (GBP_WORD_PROPOSAL (proposal));

  gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (buffer));
  if (gtk_source_completion_context_get_bounds (context, &begin, &end))
    gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &begin, &end);
  gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &begin, word, -1);
  gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (buffer));
}

static gint
gbp_word_completion_provider_get_priority (GtkSourceCompletionProvider *provider,
                                           GtkSourceCompletionContext  *context)
{
  return -10000;
}

static void
completion_provider_iface_init (GtkSourceCompletionProviderInterface *iface)
{
  iface->populate_async = gbp_word_completion_provider_populate_async;
  iface->populate_finish = gbp_word_completion_provider_populate_finish;
  iface->display = gbp_word_completion_provider_display;
  iface->activate = gbp_word_completion_provider_activate;
  iface->refilter = gbp_word_completion_provider_refilter;
  iface->get_priority = gbp_word_completion_provider_get_priority;
}
