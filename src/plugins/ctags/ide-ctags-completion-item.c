/* ide-ctags-completion-item.c
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-ctags-completion-item"

#include "config.h"

#include <glib/gi18n.h>

#include "ide-ctags-completion-item.h"
#include "ide-ctags-index.h"
#include "ide-ctags-results.h"

static char *
ide_ctags_completion_item_get_typed_text (GtkSourceCompletionProposal *proposal)
{
  return g_strdup (IDE_CTAGS_COMPLETION_ITEM (proposal)->entry->name);
}

static void
proposal_iface_init (GtkSourceCompletionProposalInterface *iface)
{
  iface->get_typed_text = ide_ctags_completion_item_get_typed_text;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeCtagsCompletionItem,
                               ide_ctags_completion_item,
                               G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROPOSAL, proposal_iface_init))

IdeCtagsCompletionItem *
ide_ctags_completion_item_new (IdeCtagsResults          *results,
                               const IdeCtagsIndexEntry *entry)
{
  IdeCtagsCompletionItem *self;

  /*
   * we hold a reference to results so that we can ensure that
   * a reference to the index that contains @entry is maintained.
   * (as indexes are never unref'd from @results until finalized).
   */

  self = g_object_new (IDE_TYPE_CTAGS_COMPLETION_ITEM, NULL);
  self->results = g_object_ref (results);
  self->entry = entry;

  return self;
}

static void
ide_ctags_completion_item_finalize (GObject *object)
{
  G_OBJECT_CLASS (ide_ctags_completion_item_parent_class)->finalize (object);
}

static void
ide_ctags_completion_item_class_init (IdeCtagsCompletionItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_ctags_completion_item_finalize;
}

static void
ide_ctags_completion_item_init (IdeCtagsCompletionItem *self)
{
}

gboolean
ide_ctags_completion_item_is_function (IdeCtagsCompletionItem *self)
{
  g_return_val_if_fail (IDE_IS_CTAGS_COMPLETION_ITEM (self), FALSE);
  g_return_val_if_fail (self->entry != NULL, FALSE);

  return self->entry->kind == IDE_CTAGS_INDEX_ENTRY_FUNCTION;
}

GtkSourceSnippet *
ide_ctags_completion_item_get_snippet (IdeCtagsCompletionItem *self,
                                       IdeFileSettings        *file_settings)
{
  g_autoptr(GtkSourceSnippet) ret = NULL;
  g_autoptr(GtkSourceSnippetChunk) chunk1 = NULL;

  g_return_val_if_fail (IDE_IS_CTAGS_COMPLETION_ITEM (self), NULL);
  g_return_val_if_fail (!file_settings || IDE_IS_FILE_SETTINGS (file_settings), NULL);

  ret = gtk_source_snippet_new (NULL, NULL);

  chunk1 = gtk_source_snippet_chunk_new ();
  gtk_source_snippet_chunk_set_spec (chunk1, self->entry->name);
  gtk_source_snippet_add_chunk (ret, chunk1);

  if (ide_ctags_completion_item_is_function (self))
    {
      g_autoptr(GtkSourceSnippetChunk) chunk2 = gtk_source_snippet_chunk_new ();
      g_autoptr(GtkSourceSnippetChunk) chunk3 = gtk_source_snippet_chunk_new ();
      g_autoptr(GtkSourceSnippetChunk) chunk4 = gtk_source_snippet_chunk_new ();
      IdeSpacesStyle style = IDE_SPACES_STYLE_BEFORE_LEFT_PAREN;

      if (file_settings != NULL)
        style = ide_file_settings_get_spaces_style (file_settings);

      if (style & IDE_SPACES_STYLE_BEFORE_LEFT_PAREN)
        gtk_source_snippet_chunk_set_spec (chunk2, " (");
      else
        gtk_source_snippet_chunk_set_spec (chunk2, "(");

      gtk_source_snippet_chunk_set_focus_position (chunk3, 0);
      gtk_source_snippet_chunk_set_spec (chunk4, ")");

      gtk_source_snippet_add_chunk (ret, chunk2);
      gtk_source_snippet_add_chunk (ret, chunk3);
      gtk_source_snippet_add_chunk (ret, chunk4);
    }

  return g_steal_pointer (&ret);
}
