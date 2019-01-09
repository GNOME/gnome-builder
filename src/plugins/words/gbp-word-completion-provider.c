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
  GObject parent_instance;
  GbpWordProposals *proposals;
};

static void completion_provider_iface_init (IdeCompletionProviderInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GbpWordCompletionProvider, gbp_word_completion_provider, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_COMPLETION_PROVIDER, completion_provider_iface_init))

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

static gboolean
gbp_word_completion_provider_refilter (IdeCompletionProvider *provider,
                                       IdeCompletionContext  *context,
                                       GListModel            *model)
{
  g_autofree gchar *casefold = NULL;
  g_autofree gchar *word = NULL;

  g_assert (GBP_IS_WORD_COMPLETION_PROVIDER (provider));
  g_assert (IDE_IS_COMPLETION_CONTEXT (context));
  g_assert (GBP_IS_WORD_PROPOSALS (model));

  if ((word = ide_completion_context_get_word (context)))
    casefold = g_utf8_casefold (word, -1);

  gbp_word_proposals_refilter (GBP_WORD_PROPOSALS (model), casefold);

  return TRUE;
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
gbp_word_completion_provider_populate_async (IdeCompletionProvider *provider,
                                             IdeCompletionContext  *context,
                                             GCancellable          *cancellable,
                                             GAsyncReadyCallback    callback,
                                             gpointer               user_data)
{
  GbpWordCompletionProvider *self = (GbpWordCompletionProvider *)provider;
  IdeCompletionActivation activation;
  g_autoptr(IdeTask) task = NULL;

  g_assert (GBP_IS_WORD_COMPLETION_PROVIDER (self));
  g_assert (IDE_IS_COMPLETION_CONTEXT (context));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_word_completion_provider_populate_async);

  if (self->proposals == NULL)
    self->proposals = gbp_word_proposals_new ();

  /*
   * Avoid extra processing unless the user requested completion, since
   * scanning the buffer is rather expensive.
   */
  activation = ide_completion_context_get_activation (context);
  if (activation != IDE_COMPLETION_USER_REQUESTED)
    {
      gbp_word_proposals_clear (self->proposals);
      ide_task_return_boolean (task, TRUE);
      return;
    }

  ide_completion_context_set_proposals_for_provider (context,
                                                     provider,
                                                     G_LIST_MODEL (self->proposals));

  gbp_word_proposals_populate_async (self->proposals,
                                     context,
                                     cancellable,
                                     gbp_word_completion_provider_populate_cb,
                                     g_steal_pointer (&task));
}

static GListModel *
gbp_word_completion_provider_populate_finish (IdeCompletionProvider  *provider,
                                              GAsyncResult           *result,
                                              GError                **error)
{
  g_assert (GBP_IS_WORD_COMPLETION_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_object (IDE_TASK (result), error);
}

static void
gbp_word_completion_provider_display_proposal (IdeCompletionProvider   *provider,
                                               IdeCompletionListBoxRow *row,
                                               IdeCompletionContext    *context,
                                               const gchar             *typed_text,
                                               IdeCompletionProposal   *proposal)
{
  g_autofree gchar *markup = NULL;
  const gchar *word;

  g_assert (GBP_IS_WORD_COMPLETION_PROVIDER (provider));
  g_assert (IDE_IS_COMPLETION_LIST_BOX_ROW (row));
  g_assert (IDE_IS_COMPLETION_CONTEXT (context));
  g_assert (IDE_IS_COMPLETION_PROPOSAL (proposal));

  word = gbp_word_proposal_get_word (GBP_WORD_PROPOSAL (proposal));
  markup = ide_completion_fuzzy_highlight (word, typed_text);

  ide_completion_list_box_row_set_icon_name (row, "completion-word-symbolic");
  ide_completion_list_box_row_set_left (row, NULL);
  ide_completion_list_box_row_set_center_markup (row, markup);
  ide_completion_list_box_row_set_right (row, NULL);
}

static void
gbp_word_completion_provider_activate_proposal (IdeCompletionProvider *provider,
                                                IdeCompletionContext  *context,
                                                IdeCompletionProposal *proposal,
                                                const GdkEventKey     *key)
{
  GtkTextIter begin, end;
  GtkTextBuffer *buffer;
  const gchar *word;

  g_assert (GBP_IS_WORD_COMPLETION_PROVIDER (provider));
  g_assert (IDE_IS_COMPLETION_CONTEXT (context));
  g_assert (GBP_IS_WORD_PROPOSAL (proposal));
  g_assert (key != NULL);

  buffer = ide_completion_context_get_buffer (context);
  word = gbp_word_proposal_get_word (GBP_WORD_PROPOSAL (proposal));

  gtk_text_buffer_begin_user_action (buffer);
  if (ide_completion_context_get_bounds (context, &begin, &end))
    gtk_text_buffer_delete (buffer, &begin, &end);
  gtk_text_buffer_insert (buffer, &begin, word, -1);
  gtk_text_buffer_end_user_action (buffer);
}

static gint
gbp_word_completion_provider_get_priority (IdeCompletionProvider *provider,
                                           IdeCompletionContext  *context)
{
  return 1000;
}

static void
completion_provider_iface_init (IdeCompletionProviderInterface *iface)
{
  iface->populate_async = gbp_word_completion_provider_populate_async;
  iface->populate_finish = gbp_word_completion_provider_populate_finish;
  iface->display_proposal = gbp_word_completion_provider_display_proposal;
  iface->activate_proposal = gbp_word_completion_provider_activate_proposal;
  iface->refilter = gbp_word_completion_provider_refilter;
  iface->get_priority = gbp_word_completion_provider_get_priority;
}
