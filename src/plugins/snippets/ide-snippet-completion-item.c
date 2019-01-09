/* ide-snippet-completion-item.c
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

#define G_LOG_DOMAIN "ide-snippet-completion-item"

#include "config.h"

#include <glib/gi18n.h>

#include "ide-snippet-completion-item.h"

struct _IdeSnippetCompletionItem
{
  GObject               parent_instance;
  IdeSnippetStorage    *storage;
  const IdeSnippetInfo *info;
};

G_DEFINE_TYPE_WITH_CODE (IdeSnippetCompletionItem, ide_snippet_completion_item, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_COMPLETION_PROPOSAL, NULL))

static void
ide_snippet_completion_item_finalize (GObject *object)
{
  IdeSnippetCompletionItem *self = (IdeSnippetCompletionItem *)object;

  self->info = NULL;
  g_clear_object (&self->storage);

  G_OBJECT_CLASS (ide_snippet_completion_item_parent_class)->finalize (object);
}

static void
ide_snippet_completion_item_class_init (IdeSnippetCompletionItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_snippet_completion_item_finalize;
}

static void
ide_snippet_completion_item_init (IdeSnippetCompletionItem *self)
{
}

IdeSnippetCompletionItem *
ide_snippet_completion_item_new (IdeSnippetStorage    *storage,
                                 const IdeSnippetInfo *info)
{
  IdeSnippetCompletionItem *self;

  self = g_object_new (IDE_TYPE_SNIPPET_COMPLETION_ITEM, NULL);
  self->storage = g_object_ref (storage);
  self->info = info;

  return self;
}

IdeSnippet *
ide_snippet_completion_item_get_snippet (IdeSnippetCompletionItem *self,
                                         const gchar              *language)
{
  g_autoptr(IdeSnippetParser) parser = NULL;
  g_autoptr(GError) error = NULL;
  GList *items;

  g_return_val_if_fail (IDE_IS_SNIPPET_COMPLETION_ITEM (self), NULL);
  g_return_val_if_fail (self->info != NULL, NULL);
  g_return_val_if_fail (self->info->begin != NULL, NULL);

  parser = ide_snippet_parser_new ();

  if (!ide_snippet_parser_load_from_data (parser, self->info->default_lang, self->info->begin, self->info->len, &error))
    goto failure;

  items = ide_snippet_parser_get_snippets (parser);

  /*
   * We might have parsed snippets for the other languages too, so we need to
   * make sure we the proper one for the current language.
   */
  for (const GList *iter = items; iter != NULL; iter = iter->next)
    {
      IdeSnippet *snippet = iter->data;
      const gchar *lang_id = ide_snippet_get_language (snippet);

      if (g_strcmp0 (lang_id, language) == 0)
        return g_object_ref (snippet);
    }

failure:
    {
      g_autoptr(IdeSnippet) snippet = NULL;
      g_autoptr(IdeSnippetChunk) chunk = NULL;
      g_autofree gchar *failed_text = NULL;

      failed_text = g_strdup_printf (_("Failed to parse snippet “%s”"), self->info->name);

      snippet = ide_snippet_new (NULL, NULL);
      chunk = ide_snippet_chunk_new ();
      ide_snippet_chunk_set_text (chunk, failed_text);
      ide_snippet_chunk_set_text_set (chunk, TRUE);
      ide_snippet_add_chunk (snippet, chunk);

      return g_steal_pointer (&snippet);
    }
}

const IdeSnippetInfo *
ide_snippet_completion_item_get_info (IdeSnippetCompletionItem *self)
{
  g_return_val_if_fail (IDE_IS_SNIPPET_COMPLETION_ITEM (self), NULL);

  return self->info;
}
