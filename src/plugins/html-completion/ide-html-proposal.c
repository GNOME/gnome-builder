/* ide-html-proposal.c
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

#include "config.h"

#define G_LOG_DOMAIN "ide-html-proposal"

#include "ide-html-proposal.h"

struct _IdeHtmlProposal
{
  GObject parent_instance;
  const gchar *word;
  IdeHtmlProposalKind kind;
};

G_DEFINE_TYPE_WITH_CODE (IdeHtmlProposal, ide_html_proposal, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_COMPLETION_PROPOSAL, NULL))

static void
ide_html_proposal_class_init (IdeHtmlProposalClass *klass)
{
}

static void
ide_html_proposal_init (IdeHtmlProposal *self)
{
}

IdeHtmlProposal *
ide_html_proposal_new (const gchar         *word,
                       IdeHtmlProposalKind  kind)
{
  IdeHtmlProposal *self;

  /* word is an internal string */

  self = g_object_new (IDE_TYPE_HTML_PROPOSAL, NULL);
  self->word = word;
  self->kind = kind;

  return self;
}

IdeSnippet *
ide_html_proposal_get_snippet (IdeHtmlProposal *self)
{
  g_autoptr(IdeSnippet) snippet = NULL;
  g_autoptr(IdeSnippetChunk) chunk = NULL;

  g_return_val_if_fail (IDE_IS_HTML_PROPOSAL (self), NULL);

  snippet = ide_snippet_new (NULL, NULL);
  chunk = ide_snippet_chunk_new ();
  ide_snippet_chunk_set_spec (chunk, self->word);
  ide_snippet_add_chunk (snippet, chunk);

  return g_steal_pointer (&snippet);
}

const gchar *
ide_html_proposal_get_word (IdeHtmlProposal *self)
{
  return self->word;
}

IdeHtmlProposalKind
ide_html_proposal_get_kind (IdeHtmlProposal *self)
{
  return self->kind;
}
