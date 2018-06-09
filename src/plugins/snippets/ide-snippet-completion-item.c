/* ide-snippet-completion-item.c
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

#define G_LOG_DOMAIN "ide-snippet-completion-item"

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

  if (!ide_snippet_parser_load_from_data (parser, self->info->begin, self->info->len, &error))
    {
      g_message ("Failed to parse snippet: %s", error->message);
      return ide_snippet_new (NULL, NULL);
    }

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

  return ide_snippet_new (NULL, NULL);
}

const IdeSnippetInfo *
ide_snippet_completion_item_get_info (IdeSnippetCompletionItem *self)
{
  g_return_val_if_fail (IDE_IS_SNIPPET_COMPLETION_ITEM (self), NULL);

  return self->info;
}
