/* ide-clang-completion-item.c
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-clang-completion"

#include <clang-c/Index.h>
#include <glib/gi18n.h>

#include "ide-clang-completion-item.h"
#include "ide-clang-completion-item-private.h"
#include "ide-clang-private.h"

static void completion_proposal_iface_init (GtkSourceCompletionProposalIface *);

G_DEFINE_TYPE_WITH_CODE (IdeClangCompletionItem, ide_clang_completion_item, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROPOSAL,
                                                completion_proposal_iface_init))

enum {
  PROP_0,
  PROP_INDEX,
  PROP_RESULTS,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static void
ide_clang_completion_item_lazy_init (IdeClangCompletionItem *self)
{
  CXCompletionResult *result;
  GString *markup = NULL;
  unsigned num_chunks;
  unsigned i;

  g_assert (IDE_IS_CLANG_COMPLETION_ITEM (self));

  if (G_LIKELY (self->initialized))
    return;

  result = ide_clang_completion_item_get_result (self);
  num_chunks = clang_getNumCompletionChunks (result->CompletionString);
  markup = g_string_new (NULL);

  g_assert (result);
  g_assert (num_chunks);
  g_assert (markup);

  switch ((int)result->CursorKind)
    {
    case CXCursor_CXXMethod:
    case CXCursor_Constructor:
    case CXCursor_Destructor:
    case CXCursor_MemberRef:
    case CXCursor_MemberRefExpr:
    case CXCursor_ObjCClassMethodDecl:
    case CXCursor_ObjCInstanceMethodDecl:
      self->icon_name = "lang-method-symbolic";
      break;

    case CXCursor_ConversionFunction:
    case CXCursor_FunctionDecl:
    case CXCursor_FunctionTemplate:
      self->icon_name = "lang-function-symbolic";
      break;

    case CXCursor_FieldDecl:
      self->icon_name = "struct-field-symbolic";
      break;

    case CXCursor_VarDecl:
      /* local? */
    case CXCursor_ParmDecl:
    case CXCursor_ObjCIvarDecl:
    case CXCursor_ObjCPropertyDecl:
    case CXCursor_ObjCSynthesizeDecl:
    case CXCursor_NonTypeTemplateParameter:
    case CXCursor_Namespace:
    case CXCursor_NamespaceAlias:
    case CXCursor_NamespaceRef:
      break;

    case CXCursor_StructDecl:
      self->icon_name = "lang-struct-symbolic";
      break;

    case CXCursor_UnionDecl:
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
      break;

    case CXCursor_EnumConstantDecl:
      self->icon_name = "lang-enum-value-symbolic";
      break;

    case CXCursor_EnumDecl:
      self->icon_name = "lang-enum-symbolic";
      break;

    case CXCursor_NotImplemented:
    default:
      break;
    }

  for (i = 0; i < num_chunks; i++)
    {
      enum CXCompletionChunkKind kind;
      const gchar *text;
      g_autofree gchar *escaped = NULL;
      g_auto(CXString) cxstr = {0};

      kind = clang_getCompletionChunkKind (result->CompletionString, i);
      cxstr = clang_getCompletionChunkText (result->CompletionString, i);
      text = clang_getCString (cxstr);

      if (text)
        escaped = g_markup_escape_text (text, -1);
      else
        escaped = g_strdup ("");

      switch (kind)
        {
        case CXCompletionChunk_TypedText:
          g_string_append_printf (markup, "<b>%s</b>", escaped);
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
          g_string_append (markup, escaped);
          break;

        case CXCompletionChunk_Informative:
          if (dzl_str_equal0 (text, "const "))
            g_string_append (markup, text);
          break;

        case CXCompletionChunk_ResultType:
          g_string_append (markup, escaped);
          g_string_append_c (markup, ' ');
          break;

        case CXCompletionChunk_Optional:
          g_string_append_printf (markup, "<i>%s</i>", escaped);
          break;

        default:
          break;
        }
    }

  self->markup = g_string_free (markup, FALSE);
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

static IdeSourceSnippet *
ide_clang_completion_item_create_snippet (IdeClangCompletionItem *self,
                                          IdeFileSettings        *file_settings)
{
  CXCompletionResult *result;
  IdeSourceSnippet *snippet;
  IdeSpacesStyle spaces = 0;
  unsigned num_chunks;
  guint tab_stop = 0;

  g_assert (IDE_IS_CLANG_COMPLETION_ITEM (self));
  g_assert (!file_settings || IDE_IS_FILE_SETTINGS (file_settings));

  result = ide_clang_completion_item_get_result (self);
  snippet = ide_source_snippet_new (NULL, NULL);
  num_chunks = clang_getNumCompletionChunks (result->CompletionString);

  if (file_settings != NULL)
    spaces = ide_file_settings_get_spaces_style (file_settings);

  for (unsigned i = 0; i < num_chunks; i++)
    {
      g_auto(CXString) cxstr = {0};
      enum CXCompletionChunkKind kind;
      IdeSourceSnippetChunk *chunk;
      const gchar *text;

      kind = clang_getCompletionChunkKind (result->CompletionString, i);
      cxstr = clang_getCompletionChunkText (result->CompletionString, i);
      text = clang_getCString (cxstr);

      switch (kind)
        {
        case CXCompletionChunk_TypedText:
          chunk = ide_source_snippet_chunk_new ();
          ide_source_snippet_chunk_set_text (chunk, text);
          ide_source_snippet_chunk_set_text_set (chunk, TRUE);
          ide_source_snippet_add_chunk (snippet, chunk);
          g_clear_object (&chunk);
          break;

        case CXCompletionChunk_Text:
          chunk = ide_source_snippet_chunk_new ();
          ide_source_snippet_chunk_set_text (chunk, text);
          ide_source_snippet_chunk_set_text_set (chunk, TRUE);
          ide_source_snippet_add_chunk (snippet, chunk);
          g_clear_object (&chunk);
          break;

        case CXCompletionChunk_Placeholder:
          chunk = ide_source_snippet_chunk_new ();
          ide_source_snippet_chunk_set_text (chunk, text);
          ide_source_snippet_chunk_set_text_set (chunk, TRUE);
          ide_source_snippet_chunk_set_tab_stop (chunk, ++tab_stop);
          ide_source_snippet_add_chunk (snippet, chunk);
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
              chunk = ide_source_snippet_chunk_new ();
              ide_source_snippet_chunk_set_text (chunk, " ");
              ide_source_snippet_chunk_set_text_set (chunk, TRUE);
              ide_source_snippet_add_chunk (snippet, chunk);
              g_clear_object (&chunk);
            }
          chunk = ide_source_snippet_chunk_new ();
          ide_source_snippet_chunk_set_text (chunk, text);
          ide_source_snippet_chunk_set_text_set (chunk, TRUE);
          ide_source_snippet_add_chunk (snippet, chunk);
          g_clear_object (&chunk);
          break;

        case CXCompletionChunk_VerticalSpace:
          /* insert the vertical space */
          chunk = ide_source_snippet_chunk_new ();
          ide_source_snippet_chunk_set_text (chunk, text);
          ide_source_snippet_chunk_set_text_set (chunk, TRUE);
          ide_source_snippet_add_chunk (snippet, chunk);
          g_clear_object (&chunk);
          /* now perform indentation */
          chunk = ide_source_snippet_chunk_new ();
          ide_source_snippet_chunk_set_text (chunk, "\t");
          ide_source_snippet_chunk_set_text_set (chunk, TRUE);
          ide_source_snippet_add_chunk (snippet, chunk);
          g_clear_object (&chunk);
          break;

        case CXCompletionChunk_Informative:
        case CXCompletionChunk_Optional:
        case CXCompletionChunk_ResultType:
        default:
          break;
        }
    }

  return snippet;
}

static gchar *
ide_clang_completion_item_get_markup (GtkSourceCompletionProposal *proposal)
{
  IdeClangCompletionItem *self = (IdeClangCompletionItem *)proposal;

  g_assert (IDE_IS_CLANG_COMPLETION_ITEM (self));

  ide_clang_completion_item_lazy_init (self);

  return g_strdup (self->markup);
}

static const gchar *
ide_clang_completion_item_get_icon_name (GtkSourceCompletionProposal *proposal)
{
  IdeClangCompletionItem *self = (IdeClangCompletionItem *)proposal;

  g_assert (IDE_IS_CLANG_COMPLETION_ITEM (self));

  ide_clang_completion_item_lazy_init (self);

  return self->icon_name;
}

static void
ide_clang_completion_item_finalize (GObject *object)
{
  IdeClangCompletionItem *self = (IdeClangCompletionItem *)object;

  g_clear_pointer (&self->brief_comment, g_free);
  g_clear_pointer (&self->typed_text, g_free);
  g_clear_pointer (&self->markup, g_free);
  g_clear_pointer (&self->results, ide_ref_ptr_unref);

  G_OBJECT_CLASS (ide_clang_completion_item_parent_class)->finalize (object);
}

static void
ide_clang_completion_item_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  IdeClangCompletionItem *self = IDE_CLANG_COMPLETION_ITEM (object);

  switch (prop_id)
    {
    case PROP_INDEX:
      g_value_set_uint (value, self->index);
      break;

    case PROP_RESULTS:
      g_value_set_boxed (value, self->results);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_clang_completion_item_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  IdeClangCompletionItem *self = IDE_CLANG_COMPLETION_ITEM (object);

  switch (prop_id)
    {
    case PROP_INDEX:
      self->index = g_value_get_uint (value);
      break;

    case PROP_RESULTS:
      g_clear_pointer (&self->results, ide_ref_ptr_unref);
      self->results = g_value_dup_boxed (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
completion_proposal_iface_init (GtkSourceCompletionProposalIface *iface)
{
  iface->get_icon_name = ide_clang_completion_item_get_icon_name;
  iface->get_markup = ide_clang_completion_item_get_markup;
}

static void
ide_clang_completion_item_class_init (IdeClangCompletionItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_clang_completion_item_finalize;
  object_class->get_property = ide_clang_completion_item_get_property;
  object_class->set_property = ide_clang_completion_item_set_property;

  properties [PROP_INDEX] =
    g_param_spec_uint ("index",
                       "Index",
                       "The index in the result set.",
                       0,
                       G_MAXUINT-1,
                       0,
                       (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_RESULTS] =
    g_param_spec_boxed ("results",
                        "Results",
                        "The Clang result set.",
                        IDE_TYPE_REF_PTR,
                        (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
ide_clang_completion_item_init (IdeClangCompletionItem *self)
{
  self->link.data = self;
  self->typed_text_index = -1;
}

/**
 * ide_clang_completion_item_get_snippet:
 * @self: an #IdeClangCompletionItem.
 * @file_settings: (nullable): an #IdeFileSettings or %NULL
 *
 * Gets the #IdeSourceSnippet to be inserted when expanding this completion item.
 *
 * Returns: (transfer full): An #IdeSourceSnippet.
 */
IdeSourceSnippet *
ide_clang_completion_item_get_snippet (IdeClangCompletionItem *self,
                                       IdeFileSettings        *file_settings)
{
  g_return_val_if_fail (IDE_IS_CLANG_COMPLETION_ITEM (self), NULL);
  g_return_val_if_fail (!file_settings || IDE_IS_FILE_SETTINGS (file_settings), NULL);

  return ide_clang_completion_item_create_snippet (self, file_settings);
}

/**
 * ide_clang_completion_item_get_typed_text:
 * @self: An #IdeClangCompletionItem.
 *
 * Gets the text that would be expected to be typed to insert this completion
 * item into the text editor.
 *
 * Returns: A string which should not be modified or freed.
 */
const gchar *
ide_clang_completion_item_get_typed_text (IdeClangCompletionItem *self)
{
  CXCompletionResult *result;
  g_auto(CXString) cxstr = {0};

  g_return_val_if_fail (IDE_IS_CLANG_COMPLETION_ITEM (self), NULL);

  if (self->typed_text)
    return self->typed_text;

  result = ide_clang_completion_item_get_result (self);

  /*
   * Determine the index of the typed text. Each completion result should have
   * exaction one of these.
   */
  if (G_UNLIKELY (self->typed_text_index == -1))
    {
      guint num_chunks = clang_getNumCompletionChunks (result->CompletionString);

      for (guint i = 0; i < num_chunks; i++)
        {
          enum CXCompletionChunkKind kind;

          kind = clang_getCompletionChunkKind (result->CompletionString, i);
          if (kind == CXCompletionChunk_TypedText)
            {
              self->typed_text_index = i;
              break;
            }
        }
    }

  if (self->typed_text_index == -1)
    {
      /*
       * FIXME:
       *
       * This seems like an implausible result, but we are definitely
       * hitting it occasionally.
       */
      return "";
    }

#ifdef IDE_ENABLE_TRACE
  {
    enum CXCompletionChunkKind kind;
    unsigned num_chunks;

    g_assert (self->typed_text_index >= 0);

    num_chunks = clang_getNumCompletionChunks (result->CompletionString);
    g_assert ((gint)num_chunks > self->typed_text_index);

    kind = clang_getCompletionChunkKind (result->CompletionString, self->typed_text_index);
    g_assert (kind == CXCompletionChunk_TypedText);
  }
#endif

  cxstr = clang_getCompletionChunkText (result->CompletionString, self->typed_text_index);
  self->typed_text = g_strdup (clang_getCString (cxstr));

  return self->typed_text;
}

/**
 * ide_clang_completion_item_get_brief_comment:
 * @self: An #IdeClangCompletionItem.
 *
 * Gets the brief comment that can be used to show extra information for the
 * result.
 *
 * Returns: A string which should not be modified or freed.
 */
const gchar *
ide_clang_completion_item_get_brief_comment (IdeClangCompletionItem *self)
{
  CXCompletionResult *result;

  g_return_val_if_fail (IDE_IS_CLANG_COMPLETION_ITEM (self), NULL);

  if (self->brief_comment == NULL)
    {
      g_auto(CXString) cxstr = {0};

      result = ide_clang_completion_item_get_result (self);
      cxstr = clang_getCompletionBriefComment (result->CompletionString);
      self->brief_comment = g_strdup (clang_getCString (cxstr));
    }

  return self->brief_comment;
}

IdeClangCompletionItem *
ide_clang_completion_item_new (IdeRefPtr *results,
                               guint      index)
{
  IdeClangCompletionItem *ret;

  ret = g_object_new (IDE_TYPE_CLANG_COMPLETION_ITEM, NULL);
  ret->results = ide_ref_ptr_ref (results);
  ret->index = index;

  return ret;
}
