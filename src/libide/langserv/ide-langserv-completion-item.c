/* ide-langserv-completion-item.c
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

#define G_LOG_DOMAIN "ide-langserv-completion-item"

#include <jsonrpc-glib.h>

#include "ide-debug.h"

#include "completion/ide-completion.h"
#include "langserv/ide-langserv-completion-item.h"
#include "langserv/ide-langserv-util.h"
#include "snippets/ide-snippet-chunk.h"
#include "symbols/ide-symbol.h"

struct _IdeLangservCompletionItem
{
  GObject parent_instance;
  GVariant *variant;
  const gchar *label;
  const gchar *detail;
  guint kind;
};

G_DEFINE_TYPE_WITH_CODE (IdeLangservCompletionItem, ide_langserv_completion_item, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_COMPLETION_PROPOSAL, NULL))

static void
ide_langserv_completion_item_finalize (GObject *object)
{
  IdeLangservCompletionItem *self = (IdeLangservCompletionItem *)object;

  g_clear_pointer (&self->variant, g_variant_unref);

  G_OBJECT_CLASS (ide_langserv_completion_item_parent_class)->finalize (object);
}

static void
ide_langserv_completion_item_class_init (IdeLangservCompletionItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_langserv_completion_item_finalize;
}

static void
ide_langserv_completion_item_init (IdeLangservCompletionItem *self)
{
}

IdeLangservCompletionItem *
ide_langserv_completion_item_new (GVariant *variant)
{
  g_autoptr(GVariant) unboxed = NULL;
  IdeLangservCompletionItem *self;
  gint64 kind = 0;

  g_return_val_if_fail (variant != NULL, NULL);

  if (g_variant_is_of_type (variant, G_VARIANT_TYPE_VARIANT))
    variant = unboxed = g_variant_get_variant (variant);

  self = g_object_new (IDE_TYPE_LANGSERV_COMPLETION_ITEM, NULL);
  self->variant = g_variant_ref_sink (variant);

  g_variant_lookup (variant, "label", "&s", &self->label);
  g_variant_lookup (variant, "detail", "&s", &self->detail);

  if (JSONRPC_MESSAGE_PARSE (variant, "kind", JSONRPC_MESSAGE_GET_INT64 (&kind)))
    self->kind = ide_langserv_decode_completion_kind (kind);

  return self;
}

gchar *
ide_langserv_completion_item_get_markup (IdeLangservCompletionItem *self,
                                         const gchar               *typed_text)
{
  g_return_val_if_fail (IDE_IS_LANGSERV_COMPLETION_ITEM (self), NULL);

  return ide_completion_fuzzy_highlight (self->label, typed_text);
}

const gchar *
ide_langserv_completion_item_get_return_type (IdeLangservCompletionItem *self)
{
  g_return_val_if_fail (IDE_IS_LANGSERV_COMPLETION_ITEM (self), NULL);

  /* TODO: How do we get this from langserv? */

  return NULL;
}

const gchar *
ide_langserv_completion_item_get_icon_name (IdeLangservCompletionItem *self)
{
  g_return_val_if_fail (IDE_IS_LANGSERV_COMPLETION_ITEM (self), NULL);

  return ide_symbol_kind_get_icon_name (self->kind);
}

const gchar *
ide_langserv_completion_item_get_detail (IdeLangservCompletionItem *self)
{
  g_return_val_if_fail (IDE_IS_LANGSERV_COMPLETION_ITEM (self), NULL);

  return self->detail;
}

IdeSnippet *
ide_langserv_completion_item_get_snippet (IdeLangservCompletionItem *self)
{
  g_autoptr(IdeSnippet) snippet = NULL;
  g_autoptr(IdeSnippetChunk) chunk = NULL;

  g_return_val_if_fail (IDE_IS_LANGSERV_COMPLETION_ITEM (self), NULL);

  snippet = ide_snippet_new (NULL, NULL);
  chunk = ide_snippet_chunk_new ();
  ide_snippet_chunk_set_spec (chunk, self->label);
  ide_snippet_add_chunk (snippet, chunk);

  return g_steal_pointer (&snippet);
}
