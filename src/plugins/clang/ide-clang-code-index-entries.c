/* ide-clang-code-index-entries.c
 *
 * Copyright © 2017 Anoop Chandu <anoopchandu96@gmail.com>
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

#define G_LOG_DOMAIN "ide-clang-code-index-entries"

#include "ide-clang-code-index-entries.h"
#include "ide-clang-private.h"

 /*
  * This is an implementation of IdeCodeIndexEntries. This will have a TU and
  * it will use that to deliver entries.
  */
struct _IdeClangCodeIndexEntries
{
  GObject                 parent;

  CXTranslationUnit       tu;

  GQueue                  cursors;
  GQueue                  decl_cursors;

  gchar                  *main_file;
};

enum {
  PROP_0,
  PROP_MAIN_FILE,
  PROP_UNIT,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void index_entries_iface_init (IdeCodeIndexEntriesInterface *iface);

G_DEFINE_TYPE_EXTENDED (IdeClangCodeIndexEntries, ide_clang_code_index_entries, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_CODE_INDEX_ENTRIES, index_entries_iface_init))


static void
cx_cursor_free (CXCursor *cursor)
{
  g_slice_free (CXCursor, cursor);
}

/*
 * Visit all children of a node and push those into cursors queue.
 * push declaration cursor into decl_cursors queue only if its from the main file.
 */
static enum CXChildVisitResult
visitor (CXCursor     cursor,
         CXCursor     parent,
         CXClientData client_data)
{
  IdeClangCodeIndexEntries *self = client_data;
  enum CXCursorKind cursor_kind;
  CXSourceLocation location;
  CXFile file;
  g_auto(CXString) cx_file_name = {0};
  const char *file_name;
  CXCursor *child_cursor;

  g_assert (!clang_Cursor_isNull (cursor));

  child_cursor = g_slice_new (CXCursor);
  *child_cursor = cursor;
  g_queue_push_tail (&self->cursors, child_cursor);

  location = clang_getCursorLocation (cursor);

  clang_getSpellingLocation (location, &file, NULL, NULL, NULL);

  cx_file_name = clang_getFileName (file);
  file_name = clang_getCString (cx_file_name);

  cursor_kind = clang_getCursorKind (cursor);

  if (!g_strcmp0 (file_name, self->main_file))
    {
      if ((cursor_kind >= CXCursor_StructDecl && cursor_kind <= CXCursor_Namespace) ||
          (cursor_kind >= CXCursor_Constructor && cursor_kind <= CXCursor_NamespaceAlias) ||
          cursor_kind == CXCursor_TypeAliasDecl ||
          cursor_kind == CXCursor_MacroDefinition)
        {
          g_queue_push_tail (&self->decl_cursors, child_cursor);
        }
    }

  /* TODO: Record MACRO EXPANSION FOR G_DEFINE_TYPE, G_DECLARE_TYPE */

  return CXChildVisit_Continue;
}

/*
 * decl_cursors store declarations to be returned by this class. If decl_cursors
 * is not empty then this function returns a declaration popped from queue,
 * else this will do Breadth first traversal on AST till it finds a declaration.
 * On next request when decl_cursors is empty it will continue traversal
 * from where it has stopped in previously.
 */
static IdeCodeIndexEntry *
ide_clang_code_index_entries_real_get_next_entry (IdeClangCodeIndexEntries *self,
                                                  gboolean                 *finish)
{
  CXSourceLocation location;
  guint line = 0;
  guint column = 0;
  guint offset = 0;
  enum CXLinkageKind linkage;
  g_auto(CXString) cx_name = {0};
  const gchar *cname = NULL;
  gchar *prefix = NULL;
  g_autofree gchar *name = NULL;
  g_autofree gchar *key = NULL;
  IdeSymbolKind kind = IDE_SYMBOL_NONE;
  IdeSymbolFlags flags = IDE_SYMBOL_FLAGS_NONE;
  CXCursor *cursor = NULL;
  enum CXCursorKind cursor_kind;

  g_assert (IDE_IS_CLANG_CODE_INDEX_ENTRIES (self));
  g_assert (finish != NULL);

  *finish = FALSE;
  /* First declaration missing */
  /* Traverse AST till atleast one declaration is found */
  while (g_queue_is_empty (&self->decl_cursors))
    {
      if (g_queue_is_empty (&self->cursors))
        {
          clang_disposeTranslationUnit (self->tu);
          self->tu = NULL;
          *finish = TRUE;
          return NULL;
        }

      cursor = g_queue_pop_head (&self->cursors);

      /* Resume visiting children.*/
      clang_visitChildren (*cursor, visitor, self);
      g_slice_free (CXCursor, cursor);
    }

  cursor = g_queue_pop_head (&self->decl_cursors);

  location = clang_getCursorLocation (*cursor);
  clang_getSpellingLocation (location, NULL, &line, &column, &offset);

  cx_name = clang_getCursorSpelling (*cursor);
  cname = clang_getCString (cx_name);

  if ((cname == NULL) || (cname[0] == '\0'))
    return NULL;

  cursor_kind = clang_getCursorKind (*cursor);

  /*
   * If current cursor is a type alias then resolve actual type of this recursively
   * by resolving parent type.
   */
  if ((cursor_kind == CXCursor_TypedefDecl) ||
     (cursor_kind == CXCursor_NamespaceAlias) || (cursor_kind == CXCursor_TypeAliasDecl))
    {
      CXType type;
      CXCursor temp = *cursor;

      type = clang_getTypedefDeclUnderlyingType (temp);

      while (CXType_Invalid != type.kind)
        {
          temp = clang_getTypeDeclaration (type);
          type = clang_getTypedefDeclUnderlyingType (temp);
        }

      cursor_kind = clang_getCursorKind (temp);
    }

  /* Translate CXCursorKind to IdeSymbolKind */
  switch ((int)cursor_kind)
    {
    case CXCursor_StructDecl:
      kind = IDE_SYMBOL_STRUCT;
      break;
    case CXCursor_UnionDecl:
      kind = IDE_SYMBOL_UNION;
      break;
    case CXCursor_ClassDecl:
      kind = IDE_SYMBOL_CLASS;
      break;
    case CXCursor_EnumDecl:
      kind = IDE_SYMBOL_ENUM;
      break;
    case CXCursor_FieldDecl:
      kind = IDE_SYMBOL_FIELD;
      break;
    case CXCursor_EnumConstantDecl:
      kind = IDE_SYMBOL_ENUM_VALUE;
      break;
    case CXCursor_FunctionDecl:
      kind = IDE_SYMBOL_FUNCTION;
      break;
    case CXCursor_CXXMethod:
      kind = IDE_SYMBOL_METHOD;
      break;
    case CXCursor_VarDecl:
    case CXCursor_ParmDecl:
      kind = IDE_SYMBOL_VARIABLE;
      break;
    case CXCursor_TypedefDecl:
    case CXCursor_NamespaceAlias:
    case CXCursor_TypeAliasDecl:
      kind = IDE_SYMBOL_ALIAS;
      break;
    case CXCursor_Namespace:
      kind = IDE_SYMBOL_NAMESPACE;
      break;
    case CXCursor_FunctionTemplate:
    case CXCursor_ClassTemplate:
      kind = IDE_SYMBOL_TEMPLATE;
      break;
    case CXCursor_MacroDefinition:
      kind = IDE_SYMBOL_MACRO;
      break;
    default:
      kind = IDE_SYMBOL_NONE;
      break;
    }

  /* Add prefix to name so that filters can be applied */
  if (kind == IDE_SYMBOL_FUNCTION)
    prefix = "f\x1F";
  else if (kind == IDE_SYMBOL_STRUCT)
    prefix = "s\x1F";
  else if (kind == IDE_SYMBOL_VARIABLE)
    prefix = "v\x1F";
  else if (kind == IDE_SYMBOL_UNION)
    prefix = "u\x1F";
  else if (kind == IDE_SYMBOL_ENUM)
    prefix = "e\x1F";
  else if (kind == IDE_SYMBOL_CLASS)
    prefix = "c\x1F";
  else if (kind == IDE_SYMBOL_ENUM_VALUE)
    prefix = "a\x1F";
  else if (kind == IDE_SYMBOL_MACRO)
    prefix = "m\x1F";
  else
    prefix = "x\x1F";

  name = g_strconcat (prefix, cname, NULL);

  if (clang_isCursorDefinition (*cursor))
    flags |= IDE_SYMBOL_FLAGS_IS_DEFINITION;

  linkage = clang_getCursorLinkage (*cursor);

  if (linkage == CXLinkage_Internal)
    {
      flags |= IDE_SYMBOL_FLAGS_IS_STATIC;
    }
  else if (linkage == CXLinkage_NoLinkage)
    {
      flags |= IDE_SYMBOL_FLAGS_IS_MEMBER;
    }
  else
    {
      g_auto(CXString) usr = {0};

      usr = clang_getCursorUSR (*cursor);
      key = g_strdup (clang_getCString (usr));
    }

  return g_object_new (IDE_TYPE_CODE_INDEX_ENTRY,
                       "name", name,
                       "key", key,
                       "kind", kind,
                       "flags", flags,
                       "begin-line", line,
                       "begin-line-offset", column,
                       NULL);
}

/* Retrives next IdeCodeIndexEntry. */
static IdeCodeIndexEntry *
ide_clang_code_index_entries_get_next_entry (IdeCodeIndexEntries *entries)
{
  IdeClangCodeIndexEntries *self = (IdeClangCodeIndexEntries *)entries;
  g_autoptr(IdeCodeIndexEntry) entry = NULL;
  gboolean finish = FALSE;

  g_assert (IDE_IS_CLANG_CODE_INDEX_ENTRIES (self));

  entry = ide_clang_code_index_entries_real_get_next_entry (self, &finish);
  while (entry == NULL && !finish)
    entry = ide_clang_code_index_entries_real_get_next_entry (self, &finish);

  return g_steal_pointer (&entry);
}

static void
ide_clang_code_index_entries_finalize (GObject *object)
{
  IdeClangCodeIndexEntries *self = (IdeClangCodeIndexEntries *)object;

  g_queue_foreach (&self->decl_cursors, (GFunc)cx_cursor_free, NULL);
  g_queue_clear (&self->decl_cursors);

  g_queue_foreach (&self->cursors, (GFunc)cx_cursor_free, NULL);
  g_queue_clear (&self->cursors);

  if (self->tu != NULL)
    {
      clang_disposeTranslationUnit (self->tu);
      self->tu = NULL;
    }

  G_OBJECT_CLASS(ide_clang_code_index_entries_parent_class)->finalize (object);
}

static void
ide_clang_code_index_entries_constructed (GObject *object)
{
  IdeClangCodeIndexEntries *self = (IdeClangCodeIndexEntries *)object;
  CXCursor root_cursor;

  G_OBJECT_CLASS (ide_clang_code_index_entries_parent_class)->constructed (object);

  root_cursor = clang_getTranslationUnitCursor (self->tu);
  g_queue_push_head (&self->cursors, g_slice_dup (CXCursor, &root_cursor));
}

static void
ide_clang_code_index_entries_set_property (GObject       *object,
                                           guint          prop_id,
                                           const GValue  *value,
                                           GParamSpec    *pspec)
{
  IdeClangCodeIndexEntries *self = (IdeClangCodeIndexEntries *)object;

  switch (prop_id)
    {
    case PROP_MAIN_FILE:
      self->main_file = g_strdup ((gchar *)g_value_get_pointer (value));
      break;
    case PROP_UNIT:
      self->tu = g_value_get_pointer (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_clang_code_index_entries_get_property (GObject    *object,
                                           guint       prop_id,
                                           GValue     *value,
                                           GParamSpec *pspec)
{
  IdeClangCodeIndexEntries *self = (IdeClangCodeIndexEntries *)object;

  switch (prop_id)
    {
    case PROP_MAIN_FILE:
      g_value_set_pointer (value, self->main_file);
      break;
    case PROP_UNIT:
      g_value_set_pointer (value, &self->tu);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_clang_code_index_entries_class_init (IdeClangCodeIndexEntriesClass *klass)
{
  GObjectClass *object_class = (GObjectClass *)klass;

  object_class->finalize = ide_clang_code_index_entries_finalize;
  object_class->constructed = ide_clang_code_index_entries_constructed;
  object_class->set_property = ide_clang_code_index_entries_set_property;
  object_class->get_property = ide_clang_code_index_entries_get_property;

  properties [PROP_MAIN_FILE] =
    g_param_spec_pointer ("main-file",
                          "Main File",
                          "Name of file from which TU is parsed.",
                          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties [PROP_UNIT] =
    g_param_spec_pointer ("unit",
                          "Unit",
                          "Translation Unit from which index entries are to be generated",
                          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
index_entries_iface_init (IdeCodeIndexEntriesInterface *iface)
{
  iface->get_next_entry = ide_clang_code_index_entries_get_next_entry;
}

static void
ide_clang_code_index_entries_init (IdeClangCodeIndexEntries *self)
{
}

IdeClangCodeIndexEntries *
ide_clang_code_index_entries_new (CXTranslationUnit  unit,
                                  const gchar       *main_file)
{
  g_return_val_if_fail ((unit != NULL), NULL);
  g_return_val_if_fail ((main_file != NULL), NULL);

  return g_object_new (IDE_TYPE_CLANG_CODE_INDEX_ENTRIES,
                       "unit", unit,
                       "main-file", main_file,
                       NULL);
}
