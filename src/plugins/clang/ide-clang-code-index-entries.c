/* ide-clang-code-index-entries.c
 *
 * Copyright 2017 Anoop Chandu <anoopchandu96@gmail.com>
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
  GObject parent;

  /*
   * This is the index that was used to parse the translation unit. We are
   * responsible for disposing the index when finalizing.
   */
  CXIndex index;

  /*
   * The unit that was parsed by the indexer. We traverse this to locate
   * items within the file that are indexable. We also own the reference
   * and must clang_disposeIndex() when finalizing.
   */
  CXTranslationUnit unit;

  /*
   * Queue of cursors as we iterate through the translation unit. These are
   * GSlice allocated structures holding the raw CXCursor from the
   * translation unit. Since we own the unit/index, these are safe for the
   * lifetime of the object.
   *
   * These are manipulated from a worker thread when generating the entries.
   */
  GQueue cursors;
  GQueue decl_cursors;

  /* Path to the file that has been parsed. */
  gchar *path;

  /* If we've already run once, (so return empty result). */
  guint has_run : 1;
};

static void
cx_cursor_free (CXCursor *cursor)
{
  g_slice_free (CXCursor, cursor);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (CXCursor, cx_cursor_free)

static void
ide_clang_code_index_entries_drop_state (IdeClangCodeIndexEntries *self)
{
  g_queue_foreach (&self->decl_cursors, (GFunc)cx_cursor_free, NULL);
  g_queue_clear (&self->decl_cursors);

  g_queue_foreach (&self->cursors, (GFunc)cx_cursor_free, NULL);
  g_queue_clear (&self->cursors);

  g_clear_pointer (&self->unit, clang_disposeTranslationUnit);
  g_clear_pointer (&self->index, clang_disposeIndex);
}

static IdeSymbolKind
translate_kind (enum CXCursorKind cursor_kind)
{
  switch ((int)cursor_kind)
    {
    case CXCursor_StructDecl:
      return IDE_SYMBOL_STRUCT;

    case CXCursor_UnionDecl:
      return IDE_SYMBOL_UNION;

    case CXCursor_ClassDecl:
      return IDE_SYMBOL_CLASS;

    case CXCursor_EnumDecl:
      return IDE_SYMBOL_ENUM;

    case CXCursor_FieldDecl:
      return IDE_SYMBOL_FIELD;

    case CXCursor_EnumConstantDecl:
      return IDE_SYMBOL_ENUM_VALUE;

    case CXCursor_FunctionDecl:
      return IDE_SYMBOL_FUNCTION;

    case CXCursor_CXXMethod:
      return IDE_SYMBOL_METHOD;

    case CXCursor_VarDecl:
    case CXCursor_ParmDecl:
      return IDE_SYMBOL_VARIABLE;

    case CXCursor_TypedefDecl:
    case CXCursor_NamespaceAlias:
    case CXCursor_TypeAliasDecl:
      return IDE_SYMBOL_ALIAS;

    case CXCursor_Namespace:
      return IDE_SYMBOL_NAMESPACE;

    case CXCursor_FunctionTemplate:
    case CXCursor_ClassTemplate:
      return IDE_SYMBOL_TEMPLATE;

    case CXCursor_MacroDefinition:
      return IDE_SYMBOL_MACRO;

    default:
      return IDE_SYMBOL_NONE;
    }
}

static const gchar *
get_symbol_prefix (IdeSymbolKind kind)
{
  switch ((int)kind)
    {
    case IDE_SYMBOL_FUNCTION:
      return "f\x1F";

    case IDE_SYMBOL_STRUCT:
      return "s\x1F";

    case IDE_SYMBOL_VARIABLE:
      return "v\x1F";

    case IDE_SYMBOL_UNION:
      return "u\x1F";

    case IDE_SYMBOL_ENUM:
      return "e\x1F";

    case IDE_SYMBOL_CLASS:
      return "c\x1F";

    case IDE_SYMBOL_ENUM_VALUE:
      return "a\x1F";

    case IDE_SYMBOL_MACRO:
      return "m\x1F";

    default:
      return "x\x1F";
    }
}

static enum CXChildVisitResult
visitor (CXCursor     cursor,
         CXCursor     parent,
         CXClientData client_data)
{
  IdeClangCodeIndexEntries *self = client_data;
  g_auto(CXString) cxpath = {0};
  CXSourceLocation location;
  const char *path;
  CXFile file;

  g_assert (IDE_IS_CLANG_CODE_INDEX_ENTRIES (self));
  g_assert (!clang_Cursor_isNull (cursor));

  /*
   * Visit all children of a node and push those into cursors queue. Push
   * declaration cursor into decl_cursors queue only if its from the main
   * file.
   */

  g_queue_push_tail (&self->cursors, g_slice_dup (CXCursor, &cursor));

  location = clang_getCursorLocation (cursor);
  clang_getSpellingLocation (location, &file, NULL, NULL, NULL);

  cxpath = clang_getFileName (file);
  path = clang_getCString (cxpath);

  if (dzl_str_equal0 (path, self->path))
    {
      enum CXCursorKind cursor_kind = clang_getCursorKind (cursor);

      if ((cursor_kind >= CXCursor_StructDecl && cursor_kind <= CXCursor_Namespace) ||
          (cursor_kind >= CXCursor_Constructor && cursor_kind <= CXCursor_NamespaceAlias) ||
          cursor_kind == CXCursor_TypeAliasDecl ||
          cursor_kind == CXCursor_MacroDefinition)
        g_queue_push_tail (&self->decl_cursors, g_slice_dup (CXCursor, &cursor));
    }

  /* TODO: Record MACRO EXPANSION FOR G_DEFINE_TYPE, G_DECLARE_TYPE */

  return CXChildVisit_Continue;
}

/*
 * decl_cursors store declarations to be returned by this class. If
 * decl_cursors is not empty then this function returns a declaration popped
 * from queue, else this will do Breadth first traversal on AST till it
 * finds a declaration.  On next request when decl_cursors is empty it will
 * continue traversal from where it has stopped in previously.
 */
static IdeCodeIndexEntry *
ide_clang_code_index_entries_real_get_next_entry (IdeClangCodeIndexEntries *self,
                                                  IdeCodeIndexEntryBuilder *builder,
                                                  gboolean                 *finish)
{
  g_autoptr(CXCursor) cursor = NULL;
  g_autofree gchar *name = NULL;
  g_auto(CXString) cxname = {0};
  g_auto(CXString) usr = {0};
  CXSourceLocation location;
  IdeSymbolFlags flags = IDE_SYMBOL_FLAGS_NONE;
  IdeSymbolKind kind = IDE_SYMBOL_NONE;
  enum CXLinkageKind linkage;
  enum CXCursorKind cursor_kind;
  const gchar *cname = NULL;
  const gchar *prefix = NULL;
  const gchar *key = NULL;
  guint line = 0;
  guint column = 0;
  guint offset = 0;

  g_assert (!IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CLANG_CODE_INDEX_ENTRIES (self));
  g_assert (builder != NULL);
  g_assert (finish != NULL);

  *finish = FALSE;

  /* First declaration missing */
  /* Traverse AST till atleast one declaration is found */

  while (g_queue_is_empty (&self->decl_cursors))
    {
      g_autoptr(CXCursor) decl_cursor = NULL;

      if (g_queue_is_empty (&self->cursors))
        {
          *finish = TRUE;
          return NULL;
        }

      decl_cursor = g_queue_pop_head (&self->cursors);
      g_assert (decl_cursor != NULL);

      clang_visitChildren (*decl_cursor, visitor, self);
    }

  g_assert (!g_queue_is_empty (&self->decl_cursors));

  cursor = g_queue_pop_head (&self->decl_cursors);
  location = clang_getCursorLocation (*cursor);
  clang_getSpellingLocation (location, NULL, &line, &column, &offset);

  /* Skip this item if its NULL, we'll get called again to fetch
   * the next item. One possible chance for improvement here is
   * to jump to the next item instead of returning here.
   */
  cxname = clang_getCursorSpelling (*cursor);
  cname = clang_getCString (cxname);
  if (dzl_str_empty0 (cname))
    return NULL;

  /*
   * If current cursor is a type alias then resolve actual type of this
   * recursively by resolving parent type.
   */
  cursor_kind = clang_getCursorKind (*cursor);
  if ((cursor_kind == CXCursor_TypedefDecl) ||
     (cursor_kind == CXCursor_NamespaceAlias) ||
     (cursor_kind == CXCursor_TypeAliasDecl))
    {
      CXCursor temp = *cursor;
      CXType type = clang_getTypedefDeclUnderlyingType (temp);

      while (CXType_Invalid != type.kind)
        {
          temp = clang_getTypeDeclaration (type);
          type = clang_getTypedefDeclUnderlyingType (temp);
        }

      cursor_kind = clang_getCursorKind (temp);
    }

  kind = translate_kind (cursor_kind);
  prefix = get_symbol_prefix (kind);
  name = g_strconcat (prefix, cname, NULL);

  if (clang_isCursorDefinition (*cursor))
    flags |= IDE_SYMBOL_FLAGS_IS_DEFINITION;

  linkage = clang_getCursorLinkage (*cursor);
  if (linkage == CXLinkage_Internal)
    flags |= IDE_SYMBOL_FLAGS_IS_STATIC;
  else if (linkage == CXLinkage_NoLinkage)
    flags |= IDE_SYMBOL_FLAGS_IS_MEMBER;
  else
    {
      usr = clang_getCursorUSR (*cursor);
      key = clang_getCString (usr);
    }

  ide_code_index_entry_builder_set_name (builder, name);
  ide_code_index_entry_builder_set_key (builder, key);
  ide_code_index_entry_builder_set_kind (builder, kind);
  ide_code_index_entry_builder_set_flags (builder, flags);
  ide_code_index_entry_builder_set_range (builder, line, column, 0, 0);

  return ide_code_index_entry_builder_build (builder);
}

static GFile *
ide_clang_code_index_entries_get_file (IdeCodeIndexEntries *entries)
{
  IdeClangCodeIndexEntries *self = (IdeClangCodeIndexEntries *)entries;

  g_return_val_if_fail (IDE_IS_CLANG_CODE_INDEX_ENTRIES (self), NULL);

  return g_file_new_for_path (self->path);
}

static void
ide_clang_code_index_entries_worker (IdeTask      *task,
                                     gpointer      source_object,
                                     gpointer      task_data,
                                     GCancellable *cancellable)
{
  IdeClangCodeIndexEntries *self = source_object;
  g_autoptr(IdeCodeIndexEntryBuilder) builder = NULL;
  g_autoptr(GPtrArray) ret = NULL;

  g_assert (IDE_IS_TASK (task));
  g_assert (IDE_IS_CLANG_CODE_INDEX_ENTRIES (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  ret = g_ptr_array_new_with_free_func ((GDestroyNotify)ide_code_index_entry_free);
  builder = ide_code_index_entry_builder_new ();

  for (;;)
    {
      g_autoptr(IdeCodeIndexEntry) entry = NULL;
      gboolean finish = FALSE;

      if ((entry = ide_clang_code_index_entries_real_get_next_entry (self, builder, &finish)))
        {
          g_ptr_array_add (ret, g_steal_pointer (&entry));
          continue;
        }

      if (!finish)
        continue;

      break;
    }

  ide_task_return_pointer (task, g_steal_pointer (&ret), (GDestroyNotify)g_ptr_array_unref);
}

static void
ide_clang_code_index_entries_next_entries_async (IdeCodeIndexEntries *entries,
                                                 GCancellable        *cancellable,
                                                 GAsyncReadyCallback  callback,
                                                 gpointer             user_data)
{
  IdeClangCodeIndexEntries *self = (IdeClangCodeIndexEntries *)entries;
  g_autoptr(IdeTask) task = NULL;

  g_assert (IDE_IS_CLANG_CODE_INDEX_ENTRIES (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_clang_code_index_entries_next_entries_async);
  ide_task_set_priority (task, G_PRIORITY_LOW + 1000);
  ide_task_set_kind (task, IDE_TASK_KIND_INDEXER);

  if (self->has_run)
    ide_task_return_pointer (task,
                             g_ptr_array_new_with_free_func ((GDestroyNotify)ide_code_index_entry_free),
                             (GDestroyNotify)g_ptr_array_unref);
  else
    ide_task_run_in_thread (task, ide_clang_code_index_entries_worker);

  self->has_run = TRUE;
}

static GPtrArray *
ide_clang_code_index_entries_next_entries_finish (IdeCodeIndexEntries  *entries,
                                                  GAsyncResult         *result,
                                                  GError              **error)
{
  GPtrArray *ret;

  g_assert (IDE_IS_CLANG_CODE_INDEX_ENTRIES (entries));
  g_assert (IDE_IS_TASK (result));
  g_assert (ide_task_is_valid (result, entries));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  /* Drop state as early as possible */
  ide_clang_code_index_entries_drop_state (IDE_CLANG_CODE_INDEX_ENTRIES (entries));

  return IDE_PTR_ARRAY_STEAL_FULL (&ret);
}

static void
index_entries_iface_init (IdeCodeIndexEntriesInterface *iface)
{
  /*
   * We only implement the Async API, not the sync API so that we can generate
   * the results inside of a thread.
   */

  iface->get_file = ide_clang_code_index_entries_get_file;
  iface->next_entries_async = ide_clang_code_index_entries_next_entries_async;
  iface->next_entries_finish = ide_clang_code_index_entries_next_entries_finish;
}

G_DEFINE_TYPE_WITH_CODE (IdeClangCodeIndexEntries, ide_clang_code_index_entries, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_CODE_INDEX_ENTRIES, index_entries_iface_init))

static void
ide_clang_code_index_entries_finalize (GObject *object)
{
  IdeClangCodeIndexEntries *self = (IdeClangCodeIndexEntries *)object;

  ide_clang_code_index_entries_drop_state (self);

  G_OBJECT_CLASS(ide_clang_code_index_entries_parent_class)->finalize (object);
}

static void
ide_clang_code_index_entries_class_init (IdeClangCodeIndexEntriesClass *klass)
{
  GObjectClass *object_class = (GObjectClass *)klass;

  object_class->finalize = ide_clang_code_index_entries_finalize;
}

static void
ide_clang_code_index_entries_init (IdeClangCodeIndexEntries *self)
{
}

/**
 * ide_clang_code_index_entries_new:
 * @index: (transfer full): a #CXIndex to take ownership of
 * @unit: (transfer full): a #CXTranslationUnit to take ownership of
 * @path: the path of the file that was indexed
 *
 * Creates a new #IdeClangCodeIndexEntries that can be used to iterate
 * the translation unit for interesting data.
 *
 * Returns: (transfer full): a new #IdeClangCodeIndexEntries
 *
 * Thread safety: this object may be created from any thread, but the
 *   ide_clang_code_index_entries_get_next_entry() may only be called
 *   from the main thread, as required by the base interface.
 */
IdeClangCodeIndexEntries *
ide_clang_code_index_entries_new (CXIndex            index,
                                  CXTranslationUnit  unit,
                                  const gchar       *path)
{
  IdeClangCodeIndexEntries *self;
  CXCursor root;

  g_return_val_if_fail (index != NULL, NULL);
  g_return_val_if_fail (unit != NULL, NULL);
  g_return_val_if_fail (path != NULL, NULL);

  self = g_object_new (IDE_TYPE_CLANG_CODE_INDEX_ENTRIES, NULL);
  self->index = index;
  self->unit = unit;
  self->path = g_strdup (path);

  root = clang_getTranslationUnitCursor (unit);
  g_queue_push_head (&self->cursors, g_slice_dup (CXCursor, &root));

  return self;
}
