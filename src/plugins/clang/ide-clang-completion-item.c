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

G_DEFINE_TYPE_WITH_CODE (IdeClangCompletionItem, ide_clang_completion_item, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_COMPLETION_PROPOSAL, NULL))

static void
ide_clang_completion_item_do_init (IdeClangCompletionItem *self)
{
  g_autoptr(GVariant) result = NULL;
  g_autoptr(GVariant) chunks = NULL;
  g_autoptr(GString) markup = NULL;
  enum CXCursorKind kind;
  GVariantIter iter;
  GVariant *chunk;

  g_assert (IDE_IS_CLANG_COMPLETION_ITEM (self));

  result = ide_clang_completion_item_get_result (self);

  if (!g_variant_lookup (result, "kind", "u", &kind))
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

  if (!(chunks = g_variant_lookup_value (result, "chunks", NULL)))
    return;

  markup = g_string_new (NULL);

  g_variant_iter_init (&iter, chunks);

  while ((chunk = g_variant_iter_next_value (&iter)))
    {
      const gchar *text;
      enum CXCompletionChunkKind ckind;

      if (!g_variant_lookup (chunk, "kind", "u", &ckind))
        ckind = 0;

      if (!g_variant_lookup (chunk, "text", "&s", &text))
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
          g_string_append_printf (markup, "<i>%s</i>", text);
          break;

        default:
          break;
        }

      g_variant_unref (chunk);
    }

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

static IdeSnippet *
ide_clang_completion_item_create_snippet (IdeClangCompletionItem *self,
                                          IdeFileSettings        *file_settings)
{
  g_autoptr(IdeSnippet) snippet = NULL;
  g_autoptr(GVariant) result = NULL;
  g_autoptr(GVariant) chunks = NULL;
  g_autoptr(GSettings) settings = NULL;
  GVariantIter iter;
  GVariant *vchunk;
  IdeSpacesStyle spaces = 0;
  guint tab_stop = 0;

  g_assert (IDE_IS_CLANG_COMPLETION_ITEM (self));
  g_assert (!file_settings || IDE_IS_FILE_SETTINGS (file_settings));

  settings = g_settings_new ("org.gnome.builder.clang");

  result = ide_clang_completion_item_get_result (self);
  snippet = ide_snippet_new (NULL, NULL);

  if (file_settings != NULL)
    spaces = ide_file_settings_get_spaces_style (file_settings);

  if (!(chunks = g_variant_lookup_value (result, "chunks", NULL)))
    return NULL;

  g_variant_iter_init (&iter, chunks);

  while ((vchunk = g_variant_iter_next_value (&iter)))
    {
      enum CXCompletionChunkKind kind;
      IdeSnippetChunk *chunk;
      const gchar *text;

      if (!g_variant_lookup (vchunk, "kind", "u", &kind))
        kind = 0;

      if (!g_variant_lookup (vchunk, "text", "&s", &text))
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
              chunk = ide_snippet_chunk_new ();
              ide_snippet_chunk_set_tab_stop (chunk, 0);
              ide_snippet_add_chunk (snippet, chunk);
              g_clear_object (&chunk);
            }
        }

      switch (kind)
        {
        case CXCompletionChunk_TypedText:
          chunk = ide_snippet_chunk_new ();
          ide_snippet_chunk_set_text (chunk, text);
          ide_snippet_chunk_set_text_set (chunk, TRUE);
          ide_snippet_add_chunk (snippet, chunk);
          g_clear_object (&chunk);
          break;

        case CXCompletionChunk_Text:
          chunk = ide_snippet_chunk_new ();
          ide_snippet_chunk_set_text (chunk, text);
          ide_snippet_chunk_set_text_set (chunk, TRUE);
          ide_snippet_add_chunk (snippet, chunk);
          g_clear_object (&chunk);
          break;

        case CXCompletionChunk_Placeholder:
          chunk = ide_snippet_chunk_new ();
          ide_snippet_chunk_set_text (chunk, text);
          ide_snippet_chunk_set_text_set (chunk, TRUE);
          ide_snippet_chunk_set_tab_stop (chunk, ++tab_stop);
          ide_snippet_add_chunk (snippet, chunk);
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
              chunk = ide_snippet_chunk_new ();
              ide_snippet_chunk_set_text (chunk, " ");
              ide_snippet_chunk_set_text_set (chunk, TRUE);
              ide_snippet_add_chunk (snippet, chunk);
              g_clear_object (&chunk);
            }
          chunk = ide_snippet_chunk_new ();
          ide_snippet_chunk_set_text (chunk, text);
          ide_snippet_chunk_set_text_set (chunk, TRUE);
          ide_snippet_add_chunk (snippet, chunk);
          g_clear_object (&chunk);
          break;

        case CXCompletionChunk_VerticalSpace:
          /* insert the vertical space */
          chunk = ide_snippet_chunk_new ();
          ide_snippet_chunk_set_text (chunk, text);
          ide_snippet_chunk_set_text_set (chunk, TRUE);
          ide_snippet_add_chunk (snippet, chunk);
          g_clear_object (&chunk);
          /* now perform indentation */
          chunk = ide_snippet_chunk_new ();
          ide_snippet_chunk_set_text (chunk, "\t");
          ide_snippet_chunk_set_text_set (chunk, TRUE);
          ide_snippet_add_chunk (snippet, chunk);
          g_clear_object (&chunk);
          break;

        case CXCompletionChunk_Informative:
        case CXCompletionChunk_Optional:
        case CXCompletionChunk_ResultType:
        default:
          break;
        }

      g_variant_unref (vchunk);
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
 * Gets the #IdeSnippet to be inserted when expanding this completion item.
 *
 * Returns: (transfer full): An #IdeSnippet.
 *
 * Since: 3.32
 */
IdeSnippet *
ide_clang_completion_item_get_snippet (IdeClangCompletionItem *self,
                                       IdeFileSettings        *file_settings)
{
  g_return_val_if_fail (IDE_IS_CLANG_COMPLETION_ITEM (self), NULL);
  g_return_val_if_fail (!file_settings || IDE_IS_FILE_SETTINGS (file_settings), NULL);

  return ide_clang_completion_item_create_snippet (self, file_settings);
}

/**
 * ide_clang_completion_item_new:
 * @variant: the toplevel variant of all results
 * @index: the index of the item
 * @keyword: pointer to folded form of the text
 *
 * The @keyword parameter is not copied, it is expected to be valid
 * string found within @variant (and therefore associated with its
 * life-cycle).
 *
 * Since: 3.32
 */
IdeClangCompletionItem *
ide_clang_completion_item_new (GVariant    *variant,
                               guint        index,
                               const gchar *keyword)
{
  IdeClangCompletionItem *ret;

  g_assert (variant != NULL);
  g_assert (keyword != NULL);

  ret = g_object_new (IDE_TYPE_CLANG_COMPLETION_ITEM, NULL);
  ret->results = g_variant_ref (variant);
  ret->index = index;
  ret->typed_text = keyword;

  ide_clang_completion_item_do_init (ret);

  return ret;
}
