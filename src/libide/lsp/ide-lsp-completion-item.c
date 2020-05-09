/* ide-lsp-completion-item.c
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

#define G_LOG_DOMAIN "ide-lsp-completion-item"

#include "config.h"

#include <libide-sourceview.h>
#include <jsonrpc-glib.h>

#include "ide-lsp-completion-item.h"
#include "ide-lsp-util.h"

struct _IdeLspCompletionItem
{
  GObject parent_instance;
  GVariant *variant;
  const gchar *label;
  const gchar *detail;
  guint kind;
};

G_DEFINE_TYPE_WITH_CODE (IdeLspCompletionItem, ide_lsp_completion_item, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_COMPLETION_PROPOSAL, NULL))

static void
ide_lsp_completion_item_finalize (GObject *object)
{
  IdeLspCompletionItem *self = (IdeLspCompletionItem *)object;

  g_clear_pointer (&self->variant, g_variant_unref);

  G_OBJECT_CLASS (ide_lsp_completion_item_parent_class)->finalize (object);
}

static void
ide_lsp_completion_item_class_init (IdeLspCompletionItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_lsp_completion_item_finalize;
}

static void
ide_lsp_completion_item_init (IdeLspCompletionItem *self)
{
}

IdeLspCompletionItem *
ide_lsp_completion_item_new (GVariant *variant)
{
  g_autoptr(GVariant) unboxed = NULL;
  IdeLspCompletionItem *self;
  gint64 kind = 0;

  g_return_val_if_fail (variant != NULL, NULL);

  if (g_variant_is_of_type (variant, G_VARIANT_TYPE_VARIANT))
    variant = unboxed = g_variant_get_variant (variant);

  self = g_object_new (IDE_TYPE_LSP_COMPLETION_ITEM, NULL);
  self->variant = g_variant_ref_sink (variant);

  g_variant_lookup (variant, "label", "&s", &self->label);
  g_variant_lookup (variant, "detail", "&s", &self->detail);

  if (JSONRPC_MESSAGE_PARSE (variant, "kind", JSONRPC_MESSAGE_GET_INT64 (&kind)))
    self->kind = ide_lsp_decode_completion_kind (kind);

  return self;
}

gchar *
ide_lsp_completion_item_get_markup (IdeLspCompletionItem *self,
                                    const gchar          *typed_text)
{
  g_return_val_if_fail (IDE_IS_LSP_COMPLETION_ITEM (self), NULL);

  return ide_completion_fuzzy_highlight (self->label, typed_text);
}

const gchar *
ide_lsp_completion_item_get_return_type (IdeLspCompletionItem *self)
{
  g_return_val_if_fail (IDE_IS_LSP_COMPLETION_ITEM (self), NULL);

  /* TODO: How do we get this from lsp? */

  return NULL;
}

const gchar *
ide_lsp_completion_item_get_icon_name (IdeLspCompletionItem *self)
{
  g_return_val_if_fail (IDE_IS_LSP_COMPLETION_ITEM (self), NULL);

  return ide_symbol_kind_get_icon_name (self->kind);
}

const gchar *
ide_lsp_completion_item_get_detail (IdeLspCompletionItem *self)
{
  g_return_val_if_fail (IDE_IS_LSP_COMPLETION_ITEM (self), NULL);

  return self->detail;
}

/**
 * ide_lsp_completion_item_get_snippet:
 * @self: a #IdeLspCompletionItem
 *
 * Creates a new snippet for the completion item to be inserted into
 * the document.
 *
 * Returns: (transfer full): an #IdeSnippet
 *
 * Since: 3.30
 */
IdeSnippet *
ide_lsp_completion_item_get_snippet (IdeLspCompletionItem *self)
{
  g_autoptr(IdeSnippet) snippet = NULL;
  g_autoptr(IdeSnippetChunk) plainchunk = NULL;
  const gchar *snippet_text = NULL;
  const gchar *snippet_new_text = NULL;
  const gchar *text;
  gint64 format = 0;

  g_return_val_if_fail (IDE_IS_LSP_COMPLETION_ITEM (self), NULL);

  text = self->label;

  if (JSONRPC_MESSAGE_PARSE (self->variant,
                             "insertTextFormat", JSONRPC_MESSAGE_GET_INT64 (&format)))
    {
      JSONRPC_MESSAGE_PARSE (self->variant, "insertText", JSONRPC_MESSAGE_GET_STRING (&snippet_text));
      JSONRPC_MESSAGE_PARSE (self->variant, "textEdit", "{", "newText", JSONRPC_MESSAGE_GET_STRING (&snippet_new_text), "}");
      if (format == 2 && snippet_new_text != NULL)
        {
          g_autoptr(GError) error = NULL;

          if ((snippet = ide_snippet_parser_parse_one (snippet_new_text, -1, &error)))
            return g_steal_pointer (&snippet);

          g_warning ("Failed to parse snippet: %s: %s",
                     error->message, snippet_text);
        }
      else if (format == 2 && snippet_text != NULL)
        {
          g_autoptr(GError) error = NULL;

          if ((snippet = ide_snippet_parser_parse_one (snippet_text, -1, &error)))
            return g_steal_pointer (&snippet);

          g_warning ("Failed to parse snippet: %s: %s",
                     error->message, snippet_text);
        }
    }

  snippet = ide_snippet_new (NULL, NULL);
  plainchunk = ide_snippet_chunk_new ();
  ide_snippet_chunk_set_text (plainchunk, text);
  ide_snippet_chunk_set_text_set (plainchunk, TRUE);
  ide_snippet_add_chunk (snippet, plainchunk);

  return g_steal_pointer (&snippet);
}
