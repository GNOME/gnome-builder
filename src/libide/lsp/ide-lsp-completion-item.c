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

#include <gtksourceview/gtksource.h>

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

static char *
ide_lsp_completion_item_get_typed_text (GtkSourceCompletionProposal *proposal)
{
  return g_strdup (IDE_LSP_COMPLETION_ITEM (proposal)->label);
}

static void
proposal_iface_init (GtkSourceCompletionProposalInterface *iface)
{
  iface->get_typed_text = ide_lsp_completion_item_get_typed_text;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeLspCompletionItem, ide_lsp_completion_item, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROPOSAL, proposal_iface_init))

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

void
ide_lsp_completion_item_display (IdeLspCompletionItem    *self,
                                 GtkSourceCompletionCell *cell,
                                 const char              *typed_text)
{
  GtkSourceCompletionColumn column;

  g_return_if_fail (IDE_IS_LSP_COMPLETION_ITEM (self));
  g_return_if_fail (GTK_SOURCE_IS_COMPLETION_CELL (cell));

  column = gtk_source_completion_cell_get_column (cell);

  switch (column)
    {
    case GTK_SOURCE_COMPLETION_COLUMN_ICON:
      gtk_source_completion_cell_set_icon_name (cell, ide_lsp_completion_item_get_icon_name (self));
      break;

    case GTK_SOURCE_COMPLETION_COLUMN_TYPED_TEXT:
      {
        PangoAttrList *attrs;

        attrs = gtk_source_completion_fuzzy_highlight (self->label, typed_text);
        gtk_source_completion_cell_set_text_with_attributes (cell, self->label, attrs);
        pango_attr_list_unref (attrs);

        break;
      }

    case GTK_SOURCE_COMPLETION_COLUMN_COMMENT:
      if (self->detail != NULL && self->detail[0] != 0)
        {
          const char *endptr = strchr (self->detail, '\n');

          if (endptr == NULL)
            {
              gtk_source_completion_cell_set_text (cell, self->detail);
            }
          else
            {
              g_autofree char *detail = g_strndup (self->detail, endptr - self->detail);
              gtk_source_completion_cell_set_text (cell, detail);
            }
        }
      break;

    case GTK_SOURCE_COMPLETION_COLUMN_DETAILS:
      /* TODO: If there is markdown, we *could* use a markedview here
       * and set_child() with the WebKit view.
       */
      gtk_source_completion_cell_set_text (cell, self->detail);
      break;

    default:
    case GTK_SOURCE_COMPLETION_COLUMN_AFTER:
    case GTK_SOURCE_COMPLETION_COLUMN_BEFORE:
      /* TODO: Can we get this info from LSP? */
      gtk_source_completion_cell_set_text (cell, NULL);
      break;
    }
}

/**
 * ide_lsp_completion_item_get_snippet:
 * @self: a #IdeLspCompletionItem
 *
 * Creates a new snippet for the completion item to be inserted into
 * the document.
 *
 * Returns: (transfer full): an #GtkSourceSnippet
 */
GtkSourceSnippet *
ide_lsp_completion_item_get_snippet (IdeLspCompletionItem *self)
{
  g_autoptr(GtkSourceSnippet) snippet = NULL;
  g_autoptr(GtkSourceSnippetChunk) plainchunk = NULL;
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

          if ((snippet = gtk_source_snippet_new_parsed (snippet_new_text, &error)))
            return g_steal_pointer (&snippet);

          g_warning ("Failed to parse snippet: %s: %s",
                     error->message, snippet_text);
        }
      else if (format == 2 && snippet_text != NULL)
        {
          g_autoptr(GError) error = NULL;

          if ((snippet = gtk_source_snippet_new_parsed (snippet_text, &error)))
            return g_steal_pointer (&snippet);

          g_warning ("Failed to parse snippet: %s: %s",
                     error->message, snippet_text);
        }
    }

  snippet = gtk_source_snippet_new (NULL, NULL);
  plainchunk = gtk_source_snippet_chunk_new ();
  gtk_source_snippet_chunk_set_text (plainchunk, text);
  gtk_source_snippet_chunk_set_text_set (plainchunk, TRUE);
  gtk_source_snippet_add_chunk (snippet, plainchunk);

  return g_steal_pointer (&snippet);
}


/**
 * ide_lsp_completion_item_get_additional_text_edits:
 * @self: a #IdeLspCompletionItem
 * @file: The file the completion is applied to
 *
 * Obtain an array of all additional text edits to be applied to the project.
 *
 * Returns: (transfer full) (element-type IdeTextEdit) (nullable): a #GPtrArray of #IdeTextEdit
 */
GPtrArray *
ide_lsp_completion_item_get_additional_text_edits (IdeLspCompletionItem *self,
                                                   GFile                *file)
{
  g_autoptr(GPtrArray) result = NULL;
  g_autoptr(GVariantIter) text_edit_iter = NULL;
  GVariant *text_edit;

  g_return_val_if_fail (IDE_IS_LSP_COMPLETION_ITEM (self), NULL);

  if (!JSONRPC_MESSAGE_PARSE (self->variant, "additionalTextEdits" , JSONRPC_MESSAGE_GET_ITER (&text_edit_iter)))
    return NULL;

  result = g_ptr_array_new_with_free_func (g_object_unref);

  while (g_variant_iter_loop (text_edit_iter, "v", &text_edit))
    {
      IdeTextEdit *edit = ide_lsp_decode_text_edit (text_edit, file);

      if (edit != NULL)
        g_ptr_array_add (result, edit);
#ifdef IDE_ENABLE_TRACE
      else
        {
          g_autofree char *msg = g_variant_print (text_edit, TRUE);
          IDE_TRACE_MSG ("Additional text edit could not be parsed: %s", msg);
        }
#endif
    }

  return IDE_PTR_ARRAY_STEAL_FULL (&result);
}
