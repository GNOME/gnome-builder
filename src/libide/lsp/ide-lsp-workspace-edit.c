/* ide-lsp-workspace-edit.c
 *
 * Copyright 2021 Georg Vienna <georg.vienna@himbarsoft.com>
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

#define G_LOG_DOMAIN "ide-lsp-workspace-edit"

#include "config.h"

#include <jsonrpc-glib.h>

#include "ide-lsp-workspace-edit.h"
#include "ide-lsp-util.h"

struct _IdeLspWorkspaceEdit
{
  GObject parent_instance;

  GVariant* variant;
};

G_DEFINE_FINAL_TYPE (IdeLspWorkspaceEdit, ide_lsp_workspace_edit, G_TYPE_OBJECT)

static void
ide_lsp_workspace_edit_finalize (GObject *object)
{
  IdeLspWorkspaceEdit *self = (IdeLspWorkspaceEdit *)object;

  g_clear_pointer (&self->variant, g_variant_unref);

  G_OBJECT_CLASS (ide_lsp_workspace_edit_parent_class)->finalize (object);
}

static void
ide_lsp_workspace_edit_class_init (IdeLspWorkspaceEditClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_lsp_workspace_edit_finalize;
}

static void
ide_lsp_workspace_edit_init (IdeLspWorkspaceEdit *self)
{
}

IdeLspWorkspaceEdit *
ide_lsp_workspace_edit_new (GVariant *variant)
{
  IdeLspWorkspaceEdit *self;

  g_return_val_if_fail (variant != NULL, NULL);

  self = g_object_new (IDE_TYPE_LSP_WORKSPACE_EDIT, NULL);
  self->variant = g_variant_ref (variant);

  return self;
}

/**
 * ide_lsp_workspace_edit_get_edits:
 *
 * Returns the list of text edits that this workspace edit contains.
 *
 * Returns: (element-type IdeTextEdit) (transfer full): a #GPtrArray of #IdeTextEdit.
 *
 */
GPtrArray *
ide_lsp_workspace_edit_get_edits(IdeLspWorkspaceEdit *self)
{
  g_autoptr(GVariant) changes = NULL;
  g_autoptr(GPtrArray) edits = NULL;

  IDE_ENTRY;
  g_return_val_if_fail(IDE_IS_LSP_WORKSPACE_EDIT(self), NULL);

  edits = g_ptr_array_new_with_free_func(g_object_unref);

  if (JSONRPC_MESSAGE_PARSE(self->variant, "documentChanges", JSONRPC_MESSAGE_GET_VARIANT(&changes)))
    {
      if (g_variant_is_of_type(changes, G_VARIANT_TYPE_ARRAY))
        {
          GVariantIter iter;
          g_autoptr(GVariant) text_document_edit = NULL;
          const gchar *uri = NULL;

          g_variant_iter_init(&iter, changes);
          while (g_variant_iter_loop(&iter, "v", &text_document_edit))
            {
              g_autoptr(GVariant) text_edits = NULL;

              if (JSONRPC_MESSAGE_PARSE(text_document_edit,
                                        "textDocument", "{",
                                          "uri", JSONRPC_MESSAGE_GET_STRING(&uri),
                                        "}",
                                        "edits", JSONRPC_MESSAGE_GET_VARIANT(&text_edits)
                                        ))
                {
                  g_autoptr(GFile) gfile = NULL;
                  GVariantIter text_edit_iter;
                  g_autoptr(GVariant) item = NULL;

                  gfile = g_file_new_for_uri(uri);
                  g_variant_iter_init(&text_edit_iter, text_edits);
                  while (g_variant_iter_loop(&text_edit_iter, "v", &item))
                    {
                      IdeTextEdit* edit = ide_lsp_decode_text_edit(item, gfile);
                      if (edit)
                        {
                          g_ptr_array_add(edits, edit);
                        }
                    }
                }
            }
        }
    }
  else if (JSONRPC_MESSAGE_PARSE(self->variant, "changes", JSONRPC_MESSAGE_GET_VARIANT(&changes)))
    {
      if (g_variant_is_of_type(changes, G_VARIANT_TYPE_VARDICT))
        {
          GVariantIter iter;
          g_autoptr(GVariant) value = NULL;
          gchar *uri;

          g_variant_iter_init(&iter, changes);
          while (g_variant_iter_loop(&iter, "{sv}", &uri, &value))
            {
              g_autoptr(GFile) gfile = NULL;
              GVariantIter edit_iter;
              g_autoptr(GVariant) item = NULL;

              gfile = g_file_new_for_uri(uri);
              g_variant_iter_init(&edit_iter, value);
              while (g_variant_iter_loop(&edit_iter, "v", &item))
                {
                  IdeTextEdit* edit = ide_lsp_decode_text_edit(item, gfile);
                  if (edit)
                    {
                      g_ptr_array_add(edits, edit);
                    }
                }
            }
        }
    }

  IDE_RETURN(IDE_PTR_ARRAY_STEAL_FULL(&edits));
}

