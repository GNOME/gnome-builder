/* gbp-top-match-completion-provider.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-top-match-completion-provider"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-core.h>

#include <gtksourceview/gtksource.h>

#include "gbp-top-match-completion-model.h"
#include "gbp-top-match-completion-proposal.h"
#include "gbp-top-match-completion-provider.h"

struct _GbpTopMatchCompletionProvider
{
  GObject parent_instance;
};

static void completion_provider_iface_init (GtkSourceCompletionProviderInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpTopMatchCompletionProvider, gbp_top_match_completion_provider, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROVIDER, completion_provider_iface_init))

static void
gbp_top_match_completion_provider_dispose (GObject *object)
{
  G_OBJECT_CLASS (gbp_top_match_completion_provider_parent_class)->dispose (object);
}

static void
gbp_top_match_completion_provider_class_init (GbpTopMatchCompletionProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gbp_top_match_completion_provider_dispose;
}

static void
gbp_top_match_completion_provider_init (GbpTopMatchCompletionProvider *self)
{
}

static int
gbp_top_match_completion_provider_get_priority (GtkSourceCompletionProvider *self,
                                                GtkSourceCompletionContext  *context)
{
  return G_MAXINT;
}

static GListModel *
gbp_top_match_completion_provider_populate (GtkSourceCompletionProvider  *provider,
                                            GtkSourceCompletionContext   *context,
                                            GError                      **error)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GTK_SOURCE_IS_COMPLETION_PROVIDER (provider));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));

  return G_LIST_MODEL (gbp_top_match_completion_model_new (context));
}

static void
gbp_top_match_completion_provider_display (GtkSourceCompletionProvider *provider,
                                           GtkSourceCompletionContext  *context,
                                           GtkSourceCompletionProposal *proposalptr,
                                           GtkSourceCompletionCell     *cell)
{
  GbpTopMatchCompletionProposal *proposal = (GbpTopMatchCompletionProposal *)proposalptr;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_TOP_MATCH_COMPLETION_PROVIDER (provider));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));
  g_assert (GBP_IS_TOP_MATCH_COMPLETION_PROPOSAL (proposal));
  g_assert (GTK_SOURCE_IS_COMPLETION_CELL (cell));

  if (gtk_source_completion_cell_get_column (cell) == GTK_SOURCE_COMPLETION_COLUMN_ICON)
    gtk_source_completion_cell_set_icon_name (cell, "completion-top-match-symbolic");
  else
    gtk_source_completion_provider_display (proposal->provider, context, proposal->proposal, cell);
}

static void
gbp_top_match_completion_provider_activate (GtkSourceCompletionProvider *provider,
                                            GtkSourceCompletionContext  *context,
                                            GtkSourceCompletionProposal *proposalptr)
{
  GbpTopMatchCompletionProposal *proposal = (GbpTopMatchCompletionProposal *)proposalptr;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_TOP_MATCH_COMPLETION_PROVIDER (provider));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));
  g_assert (GBP_IS_TOP_MATCH_COMPLETION_PROPOSAL (proposal));

  gtk_source_completion_provider_activate (proposal->provider, context, proposal->proposal);
}

static void
gbp_top_match_completion_provider_refilter (GtkSourceCompletionProvider *provider,
                                            GtkSourceCompletionContext  *context,
                                            GListModel                  *model)
{
  g_autofree char *word = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_TOP_MATCH_COMPLETION_PROVIDER (provider));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));
  g_assert (GBP_IS_TOP_MATCH_COMPLETION_MODEL (model));

  word = gtk_source_completion_context_get_word (context);
  gbp_top_match_completion_model_set_typed_text (GBP_TOP_MATCH_COMPLETION_MODEL (model), word);
}

static gboolean
gbp_top_match_completion_provider_key_activates (GtkSourceCompletionProvider *provider,
                                                 GtkSourceCompletionContext  *context,
                                                 GtkSourceCompletionProposal *proposalptr,
                                                 guint                        keyval,
                                                 GdkModifierType              state)
{
  GbpTopMatchCompletionProposal *proposal = (GbpTopMatchCompletionProposal *)proposalptr;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_TOP_MATCH_COMPLETION_PROVIDER (provider));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));
  g_assert (GBP_IS_TOP_MATCH_COMPLETION_PROPOSAL (proposal));

  return gtk_source_completion_provider_key_activates (proposal->provider, context, proposal->proposal, keyval, state);
}

static GPtrArray *
gbp_top_match_completion_provider_list_alternates (GtkSourceCompletionProvider *provider,
                                                   GtkSourceCompletionContext  *context,
                                                   GtkSourceCompletionProposal *proposalptr)
{
  GbpTopMatchCompletionProposal *proposal = (GbpTopMatchCompletionProposal *)proposalptr;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_TOP_MATCH_COMPLETION_PROVIDER (provider));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));
  g_assert (GBP_IS_TOP_MATCH_COMPLETION_PROPOSAL (proposal));

  return gtk_source_completion_provider_list_alternates (proposal->provider, context, proposal->proposal);
}

static void
completion_provider_iface_init (GtkSourceCompletionProviderInterface *iface)
{
  iface->activate = gbp_top_match_completion_provider_activate;
  iface->display = gbp_top_match_completion_provider_display;
  iface->get_priority = gbp_top_match_completion_provider_get_priority;
  iface->key_activates = gbp_top_match_completion_provider_key_activates;
  iface->populate = gbp_top_match_completion_provider_populate;
  iface->refilter = gbp_top_match_completion_provider_refilter;
  iface->list_alternates = gbp_top_match_completion_provider_list_alternates;
}
