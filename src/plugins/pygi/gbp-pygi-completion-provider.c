/* gbp-pygi-completion-provider.c
 *
 * Copyright 2015 Elad Alfassa <elad@fedoraproject.org>
 * Copyright 2015-2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-pygi-completion-provider"

#include "config.h"

#include <gtksourceview/gtksource.h>

#include <libide-threading.h>

#include "gbp-pygi-completion-provider.h"
#include "gbp-pygi-proposal.h"
#include "gbp-pygi-proposals.h"

struct _GbpPygiCompletionProvider
{
  GObject parent_instance;
};

static void
gbp_pygi_completion_provider_populate_async (GtkSourceCompletionProvider *provider,
                                             GtkSourceCompletionContext  *context,
                                             GCancellable                *cancellable,
                                             GAsyncReadyCallback          callback,
                                             gpointer                     user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GbpPygiProposals) results = NULL;
  g_autofree char *line_text = NULL;
  g_autofree char *word = NULL;
  GtkTextIter begin, end;

  g_assert (GBP_IS_PYGI_COMPLETION_PROVIDER (provider));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (provider, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_pygi_completion_provider_populate_async);

  gtk_source_completion_context_get_bounds (context, &begin, &end);
  gtk_text_iter_set_line_offset (&begin, 0);
  line_text = g_strstrip (gtk_text_iter_get_slice (&begin, &end));

  if (!g_str_has_prefix (line_text, "from gi.repository import"))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "Not supported");
      return;
    }

  word = gtk_source_completion_context_get_word (context);
  results = gbp_pygi_proposals_new ();
  gbp_pygi_proposals_filter (results, word);

  ide_task_return_object (task, g_steal_pointer (&results));
}

static GListModel *
gbp_pygi_completion_provider_populate_finish (GtkSourceCompletionProvider  *provider,
                                              GAsyncResult                 *result,
                                              GError                      **error)
{
  g_assert (GBP_IS_PYGI_COMPLETION_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_object (IDE_TASK (result), error);
}

static void
gbp_pygi_completion_provider_display (GtkSourceCompletionProvider *provider,
                                      GtkSourceCompletionContext  *context,
                                      GtkSourceCompletionProposal *proposal,
                                      GtkSourceCompletionCell     *cell)
{
  g_assert (GBP_IS_PYGI_COMPLETION_PROVIDER (provider));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));
  g_assert (GBP_IS_PYGI_PROPOSAL (proposal));
  g_assert (GTK_SOURCE_IS_COMPLETION_CELL (cell));

  gbp_pygi_proposal_display (GBP_PYGI_PROPOSAL (proposal), context, cell);
}

static void
gbp_pygi_completion_provider_activate (GtkSourceCompletionProvider *provider,
                                       GtkSourceCompletionContext  *context,
                                       GtkSourceCompletionProposal *proposal)
{
  GtkSourceBuffer *buffer;
  GtkTextIter begin, end;

  g_assert (GBP_IS_PYGI_COMPLETION_PROVIDER (provider));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));
  g_assert (GBP_IS_PYGI_PROPOSAL (proposal));

  buffer = gtk_source_completion_context_get_buffer (context);

  gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (buffer));
  if (gtk_source_completion_context_get_bounds (context, &begin, &end))
    gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &begin, &end);
  gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer),
                          &begin,
                          gbp_pygi_proposal_get_name (GBP_PYGI_PROPOSAL (proposal)),
                          -1);
  gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (buffer));
}

static void
gbp_pygi_completion_provider_refilter (GtkSourceCompletionProvider *provider,
                                       GtkSourceCompletionContext  *context,
                                       GListModel                  *model)
{
  g_autofree char *word = NULL;

  g_assert (GBP_IS_PYGI_COMPLETION_PROVIDER (provider));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));
  g_assert (GBP_IS_PYGI_PROPOSALS (model));

  word = gtk_source_completion_context_get_word (context);
  gbp_pygi_proposals_filter (GBP_PYGI_PROPOSALS (model), word);
}

static void
completion_provider_iface_init (GtkSourceCompletionProviderInterface *iface)
{
  iface->populate_async = gbp_pygi_completion_provider_populate_async;
  iface->populate_finish = gbp_pygi_completion_provider_populate_finish;
  iface->display = gbp_pygi_completion_provider_display;
  iface->activate = gbp_pygi_completion_provider_activate;
  iface->refilter = gbp_pygi_completion_provider_refilter;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpPygiCompletionProvider, gbp_pygi_completion_provider, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROVIDER, completion_provider_iface_init))

static void
gbp_pygi_completion_provider_class_init (GbpPygiCompletionProviderClass *klass)
{
}

static void
gbp_pygi_completion_provider_init (GbpPygiCompletionProvider *self)
{
}
