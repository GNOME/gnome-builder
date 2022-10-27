/* ide-clang-completion-item.c
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

#define G_LOG_DOMAIN "ide-clang-completion"

#include <clang-c/Index.h>
#include <glib/gi18n.h>

#include <libide-foundry.h>

#include "ide-clang-completion-item.h"

#if G_GNUC_CHECK_VERSION(4,0)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeclaration-after-statement"
#endif
#include "proposals.c"
#if G_GNUC_CHECK_VERSION(4,0)
# pragma GCC diagnostic pop
#endif

static char *
ide_clang_completion_item_get_typed_text (GtkSourceCompletionProposal *proposal)
{
  return g_strdup (IDE_CLANG_COMPLETION_ITEM (proposal)->typed_text);
}

static void
proposal_iface_init (GtkSourceCompletionProposalInterface *iface)
{
  iface->get_typed_text = ide_clang_completion_item_get_typed_text;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeClangCompletionItem, ide_clang_completion_item, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROPOSAL, proposal_iface_init))

static void
ide_clang_completion_item_do_init (IdeClangCompletionItem *self)
{
  g_autoptr(GString) markup = NULL;
  enum CXCursorKind kind;
  ChunksRef chunks;
  VariantRef v;
  gsize n_chunks;

  g_assert (IDE_IS_CLANG_COMPLETION_ITEM (self));
  g_assert (self->params == NULL);

  if (proposal_lookup (self->ref, "kind", NULL, &v))
    kind = variant_get_uint32 (v);
  else
    kind = 0;

  switch ((int)kind)
    {
    case CXCursor_CXXMethod:
    case CXCursor_Constructor:
    case CXCursor_Destructor:
    case CXCursor_MemberRef:
    case CXCursor_MemberRefExpr:
    case CXCursor_ObjCClassMethodDecl:
    case CXCursor_ObjCInstanceMethodDecl:
      self->icon_name = "lang-method-symbolic";
      self->kind = IDE_SYMBOL_KIND_METHOD;
      break;

    case CXCursor_ConversionFunction:
    case CXCursor_FunctionDecl:
    case CXCursor_FunctionTemplate:
      self->icon_name = "lang-function-symbolic";
      self->kind = IDE_SYMBOL_KIND_FUNCTION;
      break;

    case CXCursor_FieldDecl:
      self->icon_name = "lang-struct-field-symbolic";
      self->kind = IDE_SYMBOL_KIND_FIELD;
      break;

    case CXCursor_VarDecl:
      self->icon_name = "lang-variable-symbolic";
      self->kind = IDE_SYMBOL_KIND_VARIABLE;
      /* local? */
      break;

    case CXCursor_Namespace:
    case CXCursor_NamespaceAlias:
    case CXCursor_NamespaceRef:
      self->icon_name = "lang-namespace-symbolic";
      self->kind = IDE_SYMBOL_KIND_NAMESPACE;
      break;

    case CXCursor_ParmDecl:
    case CXCursor_ObjCIvarDecl:
    case CXCursor_ObjCPropertyDecl:
    case CXCursor_ObjCSynthesizeDecl:
    case CXCursor_NonTypeTemplateParameter:
      break;

    case CXCursor_StructDecl:
      self->icon_name = "lang-struct-symbolic";
      self->kind = IDE_SYMBOL_KIND_STRUCT;
      break;

    case CXCursor_UnionDecl:
      self->icon_name  = "lang-union-symbolic";
      self->kind = IDE_SYMBOL_KIND_UNION;
      break;

    case CXCursor_ClassDecl:
    case CXCursor_TypeRef:
    case CXCursor_TemplateRef:
    case CXCursor_TypedefDecl:
    case CXCursor_ClassTemplate:
    case CXCursor_ClassTemplatePartialSpecialization:
    case CXCursor_ObjCClassRef:
    case CXCursor_ObjCInterfaceDecl:
    case CXCursor_ObjCImplementationDecl:
    case CXCursor_ObjCCategoryDecl:
    case CXCursor_ObjCCategoryImplDecl:
    case CXCursor_ObjCProtocolDecl:
    case CXCursor_ObjCProtocolRef:
    case CXCursor_TemplateTypeParameter:
    case CXCursor_TemplateTemplateParameter:
      self->icon_name  = "lang-class-symbolic";
      self->kind = IDE_SYMBOL_KIND_CLASS;
      break;

    case CXCursor_MacroDefinition:
    case CXCursor_MacroExpansion:
      self->icon_name = "lang-define-symbolic";
      self->kind = IDE_SYMBOL_KIND_MACRO;
      break;

    case CXCursor_EnumConstantDecl:
      self->icon_name = "lang-enum-value-symbolic";
      self->kind = IDE_SYMBOL_KIND_ENUM_VALUE;
      break;

    case CXCursor_EnumDecl:
      self->icon_name = "lang-enum-symbolic";
      self->kind = IDE_SYMBOL_KIND_ENUM;
      break;

    case CXCursor_NotImplemented:
    default:
      break;
    }

  if (!proposal_lookup (self->ref, "chunks", NULL, &v))
    return;

  chunks = chunks_from_variant (v);
  n_chunks = chunks_get_length (chunks);
  markup = g_string_new (NULL);

  for (gsize i = 0; i < n_chunks; i++)
    {
      ChunkRef chunk = chunks_get_at (chunks, i);
      const char *text;
      enum CXCompletionChunkKind ckind;

      if (chunk_lookup (chunk, "kind", NULL, &v))
        ckind = variant_get_uint32 (v);
      else
        ckind = 0;

      if (chunk_lookup (chunk, "text", NULL, &v))
        text = variant_get_string (v);
      else
        text = NULL;

      switch ((int)ckind)
        {
        case CXCompletionChunk_TypedText:
          self->typed_text = text;
          break;

        case CXCompletionChunk_Placeholder:
        case CXCompletionChunk_Text:
        case CXCompletionChunk_LeftParen:
        case CXCompletionChunk_RightParen:
        case CXCompletionChunk_LeftBracket:
        case CXCompletionChunk_RightBracket:
        case CXCompletionChunk_LeftBrace:
        case CXCompletionChunk_RightBrace:
        case CXCompletionChunk_LeftAngle:
        case CXCompletionChunk_RightAngle:
        case CXCompletionChunk_Comma:
        case CXCompletionChunk_Colon:
        case CXCompletionChunk_SemiColon:
        case CXCompletionChunk_Equal:
        case CXCompletionChunk_HorizontalSpace:
        case CXCompletionChunk_VerticalSpace:
        case CXCompletionChunk_CurrentParameter:
          g_string_append (markup, text);
          break;

        case CXCompletionChunk_Informative:
          if (ide_str_equal0 (text, "const "))
            g_string_append (markup, text);
          break;

        case CXCompletionChunk_ResultType:
          self->return_type = text;
          break;

        case CXCompletionChunk_Optional:
          if (!ide_str_empty0 (text))
            g_string_append (markup, text);
          break;

        default:
          break;
        }
    }

  /* If typed text already has () in it, then just ignore what
   * we generated for params as it could cause problems with
   * macro completion (like g_autoptr()).
   */
  if (self->typed_text && strchr (self->typed_text, '(') == NULL)
    self->params = g_string_free (g_steal_pointer (&markup), FALSE);
}

static IdeSpacesStyle
get_space_before_mask (enum CXCompletionChunkKind kind)
{
  switch (kind)
    {
    case CXCompletionChunk_LeftParen:
      return IDE_SPACES_STYLE_BEFORE_LEFT_PAREN;

    case CXCompletionChunk_LeftBracket:
      return IDE_SPACES_STYLE_BEFORE_LEFT_BRACKET;

    case CXCompletionChunk_LeftBrace:
      return IDE_SPACES_STYLE_BEFORE_LEFT_BRACE;

    case CXCompletionChunk_LeftAngle:
      return IDE_SPACES_STYLE_BEFORE_LEFT_ANGLE;

    case CXCompletionChunk_Colon:
      return IDE_SPACES_STYLE_BEFORE_COLON;

    case CXCompletionChunk_Comma:
      return IDE_SPACES_STYLE_BEFORE_COMMA;

    case CXCompletionChunk_SemiColon:
      return IDE_SPACES_STYLE_BEFORE_SEMICOLON;

    case CXCompletionChunk_CurrentParameter:
    case CXCompletionChunk_Equal:
    case CXCompletionChunk_HorizontalSpace:
    case CXCompletionChunk_Informative:
    case CXCompletionChunk_Optional:
    case CXCompletionChunk_Placeholder:
    case CXCompletionChunk_ResultType:
    case CXCompletionChunk_RightAngle:
    case CXCompletionChunk_RightBrace:
    case CXCompletionChunk_RightBracket:
    case CXCompletionChunk_RightParen:
    case CXCompletionChunk_Text:
    case CXCompletionChunk_TypedText:
    case CXCompletionChunk_VerticalSpace:
    default:
      return IDE_SPACES_STYLE_IGNORE;
    }
}

static GtkSourceSnippet *
ide_clang_completion_item_create_snippet (IdeClangCompletionItem *self,
                                          IdeFileSettings        *file_settings)
{
  g_autoptr(GtkSourceSnippet) snippet = NULL;
  g_autoptr(GSettings) settings = NULL;
  IdeSpacesStyle spaces = 0;
  guint tab_stop = 0;
  ChunksRef chunks;
  VariantRef v;
  gsize n_chunks;

  g_assert (IDE_IS_CLANG_COMPLETION_ITEM (self));
  g_assert (!file_settings || IDE_IS_FILE_SETTINGS (file_settings));

  settings = g_settings_new ("org.gnome.builder.clang");

  snippet = gtk_source_snippet_new (NULL, NULL);

  if (file_settings != NULL)
    spaces = ide_file_settings_get_spaces_style (file_settings);

  if (!proposal_lookup (self->ref, "chunks", NULL, &v))
    return NULL;

  chunks = chunks_from_variant (v);
  n_chunks = chunks_get_length (chunks);

  for (gsize i = 0; i < n_chunks; i++)
    {
      ChunkRef chunk_ref = chunks_get_at (chunks, i);
      enum CXCompletionChunkKind kind;
      GtkSourceSnippetChunk *chunk;
      const char *text;

      if (chunk_lookup (chunk_ref, "kind", NULL, &v))
        kind = variant_get_uint32 (v);
      else
        kind = 0;

      if (chunk_lookup (chunk_ref, "text", NULL, &v))
        text = variant_get_string (v);
      else
        text = NULL;

      if (!g_settings_get_boolean (settings, "complete-parens"))
        {
          if (kind != CXCompletionChunk_TypedText)
            continue;
        }

      if (!g_settings_get_boolean (settings, "complete-params"))
        {
          if (kind == CXCompletionChunk_Placeholder)
            continue;

          if (kind == CXCompletionChunk_RightParen)
            {
              /* Insert | cursor right before right paren if we aren't
               * adding params but parents is enabled.
               */
              chunk = gtk_source_snippet_chunk_new ();
              gtk_source_snippet_chunk_set_focus_position (chunk, 0);
              gtk_source_snippet_add_chunk (snippet, chunk);
              g_clear_object (&chunk);
            }
        }

      switch (kind)
        {
        case CXCompletionChunk_TypedText:
          chunk = gtk_source_snippet_chunk_new ();
          gtk_source_snippet_chunk_set_text (chunk, text);
          gtk_source_snippet_chunk_set_text_set (chunk, TRUE);
          gtk_source_snippet_add_chunk (snippet, chunk);
          g_clear_object (&chunk);
          break;

        case CXCompletionChunk_Text:
          chunk = gtk_source_snippet_chunk_new ();
          gtk_source_snippet_chunk_set_text (chunk, text);
          gtk_source_snippet_chunk_set_text_set (chunk, TRUE);
          gtk_source_snippet_add_chunk (snippet, chunk);
          g_clear_object (&chunk);
          break;

        case CXCompletionChunk_Placeholder:
          chunk = gtk_source_snippet_chunk_new ();
          gtk_source_snippet_chunk_set_text (chunk, text);
          gtk_source_snippet_chunk_set_text_set (chunk, TRUE);
          gtk_source_snippet_chunk_set_focus_position (chunk, ++tab_stop);
          gtk_source_snippet_add_chunk (snippet, chunk);
          g_clear_object (&chunk);
          break;

        case CXCompletionChunk_CurrentParameter:
          break;

        case CXCompletionChunk_LeftParen:
        case CXCompletionChunk_RightParen:
        case CXCompletionChunk_LeftBracket:
        case CXCompletionChunk_RightBracket:
        case CXCompletionChunk_LeftBrace:
        case CXCompletionChunk_RightBrace:
        case CXCompletionChunk_LeftAngle:
        case CXCompletionChunk_RightAngle:
        case CXCompletionChunk_Comma:
        case CXCompletionChunk_Colon:
        case CXCompletionChunk_SemiColon:
        case CXCompletionChunk_Equal:
        case CXCompletionChunk_HorizontalSpace:
          if (spaces & get_space_before_mask (kind))
            {
              chunk = gtk_source_snippet_chunk_new ();
              gtk_source_snippet_chunk_set_text (chunk, " ");
              gtk_source_snippet_chunk_set_text_set (chunk, TRUE);
              gtk_source_snippet_add_chunk (snippet, chunk);
              g_clear_object (&chunk);
            }
          chunk = gtk_source_snippet_chunk_new ();
          gtk_source_snippet_chunk_set_text (chunk, text);
          gtk_source_snippet_chunk_set_text_set (chunk, TRUE);
          gtk_source_snippet_add_chunk (snippet, chunk);
          g_clear_object (&chunk);
          break;

        case CXCompletionChunk_VerticalSpace:
          /* insert the vertical space */
          chunk = gtk_source_snippet_chunk_new ();
          gtk_source_snippet_chunk_set_text (chunk, text);
          gtk_source_snippet_chunk_set_text_set (chunk, TRUE);
          gtk_source_snippet_add_chunk (snippet, chunk);
          g_clear_object (&chunk);
          /* now perform indentation */
          chunk = gtk_source_snippet_chunk_new ();
          gtk_source_snippet_chunk_set_text (chunk, "\t");
          gtk_source_snippet_chunk_set_text_set (chunk, TRUE);
          gtk_source_snippet_add_chunk (snippet, chunk);
          g_clear_object (&chunk);
          break;

        case CXCompletionChunk_Optional:
          /* TODO: There are sub-chunks we can lose for these
           * using clang_getCompletionChunkCompletionString().
           */
          break;

        case CXCompletionChunk_Informative:
        case CXCompletionChunk_ResultType:
        default:
          break;
        }
    }

  return g_steal_pointer (&snippet);
}

static void
ide_clang_completion_item_finalize (GObject *object)
{
  IdeClangCompletionItem *self = (IdeClangCompletionItem *)object;

  self->typed_text = NULL;

  g_clear_pointer (&self->results, g_variant_unref);
  g_clear_pointer (&self->params, g_free);

  G_OBJECT_CLASS (ide_clang_completion_item_parent_class)->finalize (object);
}

static void
ide_clang_completion_item_class_init (IdeClangCompletionItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_clang_completion_item_finalize;
}

static void
ide_clang_completion_item_init (IdeClangCompletionItem *self)
{
}

/**
 * ide_clang_completion_item_get_snippet:
 * @self: an #IdeClangCompletionItem.
 * @file_settings: (nullable): an #IdeFileSettings or %NULL
 *
 * Gets the #GtkSourceSnippet to be inserted when expanding this completion item.
 *
 * Returns: (transfer full): An #GtkSourceSnippet.
 */
GtkSourceSnippet *
ide_clang_completion_item_get_snippet (IdeClangCompletionItem *self,
                                       IdeFileSettings        *file_settings)
{
  g_return_val_if_fail (IDE_IS_CLANG_COMPLETION_ITEM (self), NULL);
  g_return_val_if_fail (!file_settings || IDE_IS_FILE_SETTINGS (file_settings), NULL);

  return ide_clang_completion_item_create_snippet (self, file_settings);
}

IdeClangCompletionItem *
ide_clang_completion_item_new (GVariant    *results,
                               ProposalRef  ref)
{
  IdeClangCompletionItem *ret;

  ret = g_object_new (IDE_TYPE_CLANG_COMPLETION_ITEM, NULL);
  ret->results = g_variant_ref (results);
  ret->ref = ref;

  ide_clang_completion_item_do_init (ret);

  return ret;
}

void
ide_clang_completion_item_display (IdeClangCompletionItem  *self,
                                   GtkSourceCompletionCell *cell,
                                   const char              *typed_text)
{
  GtkSourceCompletionColumn column;

  g_return_if_fail (IDE_IS_CLANG_COMPLETION_ITEM (self));
  g_return_if_fail (GTK_SOURCE_IS_COMPLETION_CELL (cell));

  column = gtk_source_completion_cell_get_column (cell);

  switch (column)
    {
    case GTK_SOURCE_COMPLETION_COLUMN_ICON:
      if (self->icon_name)
        gtk_source_completion_cell_set_icon_name (cell, self->icon_name);
      else
        gtk_source_completion_cell_set_icon_name (cell, "text-x-csrc-symbolic");

      break;

    case GTK_SOURCE_COMPLETION_COLUMN_TYPED_TEXT:
      {
        g_autofree char *with_params = NULL;
        const char *text = self->typed_text;
        PangoAttrList *attrs;

        if (text != NULL && self->params != NULL)
          text = with_params = g_strdup_printf ("%s %s", text, self->params);

        attrs = gtk_source_completion_fuzzy_highlight (self->typed_text, typed_text);
        gtk_source_completion_cell_set_text_with_attributes (cell, text, attrs);
        pango_attr_list_unref (attrs);

        break;
      }

    case GTK_SOURCE_COMPLETION_COLUMN_COMMENT:
      {
        VariantRef v;

        if (proposal_lookup (self->ref, "comment", NULL, &v))
          gtk_source_completion_cell_set_text (cell, variant_get_string (v));
        else
          gtk_source_completion_cell_set_text (cell, NULL);

        break;
      }

    case GTK_SOURCE_COMPLETION_COLUMN_DETAILS:
      gtk_source_completion_cell_set_text (cell, NULL);
      break;

    case GTK_SOURCE_COMPLETION_COLUMN_BEFORE:
      gtk_source_completion_cell_set_text (cell, self->return_type);
      break;

    case GTK_SOURCE_COMPLETION_COLUMN_AFTER:
      gtk_source_completion_cell_set_text (cell, NULL);
      break;

    default:
      break;
    }
}
