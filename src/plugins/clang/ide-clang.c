/* ide-clang.c
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

/* Prologue {{{1 */

#define G_LOG_DOMAIN "ide-clang"

#include <ide.h>

#include "ide-clang.h"
#include "ide-clang-util.h"

struct _IdeClang
{
  GObject parent;
};

G_DEFINE_TYPE (IdeClang, ide_clang, G_TYPE_OBJECT)

static const gchar *
ide_clang_get_llvm_flags (void)
{
  static const gchar *llvm_flags = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autofree gchar *stdoutstr = NULL;
  g_autofree gchar *include = NULL;

  if G_LIKELY (llvm_flags != NULL)
    return llvm_flags;

  subprocess = g_subprocess_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE,
                                 NULL,
                                 "clang",
                                 "-print-file-name=include",
                                 NULL);

  if (!subprocess ||
      !g_subprocess_communicate_utf8 (subprocess, NULL, NULL, &stdoutstr, NULL, NULL))
    return NULL;

  g_strstrip (stdoutstr);

  if (g_str_equal (stdoutstr, "include"))
    return NULL;

  include = g_strdup_printf ("-I%s", stdoutstr);
  llvm_flags = g_intern_string (include);

  return llvm_flags;
}

static gchar **
ide_clang_cook_flags (const gchar * const *flags)
{
  GPtrArray *cooked = g_ptr_array_new ();
  const gchar *llvm_flags = ide_clang_get_llvm_flags ();

  if (llvm_flags != NULL)
    g_ptr_array_add (cooked, g_strdup (llvm_flags));

  if (flags != NULL)
    {
      for (guint i = 0; flags[i]; i++)
        g_ptr_array_add (cooked, g_strdup (flags[i]));
    }

  g_ptr_array_add (cooked, NULL);

  return (gchar **)g_ptr_array_free (cooked, FALSE);
}

static void
ide_clang_class_init (IdeClangClass *klass)
{
}

static void
ide_clang_init (IdeClang *self)
{
}

IdeClang *
ide_clang_new (void)
{
  return g_object_new (IDE_TYPE_CLANG, NULL);
}

/* Index File {{{1 */

typedef struct
{
  GPtrArray  *entries;
  gchar      *path;
  GQueue      decl_cursors;
  GQueue      cursors;
  gchar     **argv;
  guint       argc;
} IndexFile;

static void
index_file_free (gpointer data)
{
  IndexFile *state = data;

  g_clear_pointer (&state->path, g_free);
  g_clear_pointer (&state->argv, g_strfreev);
  g_clear_pointer (&state->entries, g_ptr_array_unref);

  g_queue_foreach (&state->decl_cursors, (GFunc)_ide_clang_dispose_cursor, NULL);
  g_queue_clear (&state->decl_cursors);

  g_queue_foreach (&state->cursors, (GFunc)_ide_clang_dispose_cursor, NULL);
  g_queue_clear (&state->cursors);

  g_slice_free (IndexFile, state);
}

static const gchar *
ide_clang_index_symbol_prefix (IdeSymbolKind kind)
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
ide_clang_index_file_visitor (CXCursor     cursor,
                              CXCursor     parent,
                              CXClientData client_data)
{
  IndexFile *state = client_data;
  g_auto(CXString) cxpath = {0};
  CXSourceLocation location;
  const char *path;
  CXFile file;

  g_assert (state != NULL);
  g_assert (!clang_Cursor_isNull (cursor));

  /*
   * Visit all children of a node and push those into cursors queue. Push
   * declaration cursor into decl_cursors queue only if its from the main
   * file.
   */

  g_queue_push_tail (&state->cursors, g_slice_dup (CXCursor, &cursor));

  location = clang_getCursorLocation (cursor);
  clang_getSpellingLocation (location, &file, NULL, NULL, NULL);

  cxpath = clang_getFileName (file);
  path = clang_getCString (cxpath);

  if (dzl_str_equal0 (path, state->path))
    {
      enum CXCursorKind cursor_kind = clang_getCursorKind (cursor);

      if ((cursor_kind >= CXCursor_StructDecl && cursor_kind <= CXCursor_Namespace) ||
          (cursor_kind >= CXCursor_Constructor && cursor_kind <= CXCursor_NamespaceAlias) ||
          cursor_kind == CXCursor_TypeAliasDecl ||
          cursor_kind == CXCursor_MacroDefinition)
        g_queue_push_tail (&state->decl_cursors, g_slice_dup (CXCursor, &cursor));
    }

  return CXChildVisit_Continue;
}

/**
 * ide_clang_index_file_next_entry:
 * @state: our state for indexing
 * @builder: a reusable builder to build entries
 * @finish: (out): if we've exhuasted the cursors
 *
 * decl_cursors store declarations to be returned by this class. If
 * decl_cursors is not empty then this function returns a declaration popped
 * from queue, else this will do Breadth first traversal on AST till it finds a
 * declaration.  On next request when decl_cursors is empty it will continue
 * traversal from where it has stopped in previously.
 */
static IdeCodeIndexEntry *
ide_clang_index_file_next_entry (IndexFile                *state,
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

  g_assert (state != NULL);
  g_assert (builder != NULL);
  g_assert (finish != NULL);

  *finish = FALSE;

  /* First declaration missing */
  /* Traverse AST until at least one declaration is found */

  while (g_queue_is_empty (&state->decl_cursors))
    {
      g_autoptr(CXCursor) decl_cursor = NULL;

      if (g_queue_is_empty (&state->cursors))
        {
          *finish = TRUE;
          return NULL;
        }

      decl_cursor = g_queue_pop_head (&state->cursors);
      g_assert (decl_cursor != NULL);

      clang_visitChildren (*decl_cursor, ide_clang_index_file_visitor, state);
    }

  g_assert (!g_queue_is_empty (&state->decl_cursors));

  cursor = g_queue_pop_head (&state->decl_cursors);
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

  kind = ide_clang_translate_kind (cursor_kind);
  prefix = ide_clang_index_symbol_prefix (kind);
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

static void
ide_clang_index_file_worker (IdeTask      *task,
                             gpointer      source_object,
                             gpointer      task_data,
                             GCancellable *cancellable)
{
  IndexFile *state = task_data;
  g_autoptr(IdeCodeIndexEntryBuilder) builder = NULL;
  g_auto(CXTranslationUnit) unit = NULL;
  g_auto(CXIndex) index = NULL;
  CXCursor root;
  enum CXErrorCode code;

  g_assert (IDE_IS_CLANG (source_object));
  g_assert (IDE_IS_TASK (task));
  g_assert (state != NULL);
  g_assert (state->path != NULL);
  g_assert (state->entries != NULL);

  index = clang_createIndex (0, 0);
  code = clang_parseTranslationUnit2 (index,
                                      state->path,
                                      (const char * const *)state->argv,
                                      state->argc,
                                      NULL,
                                      0,
                                      (CXTranslationUnit_DetailedPreprocessingRecord |
#if CINDEX_VERSION >= CINDEX_VERSION_ENCODE(0, 43)
                                       CXTranslationUnit_SingleFileParse |
#endif
#if CINDEX_VERSION >= CINDEX_VERSION_ENCODE(0, 35)
                                       CXTranslationUnit_KeepGoing |
#endif
                                       CXTranslationUnit_SkipFunctionBodies),
                                      &unit);

  if (code != CXError_Success)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "Failed to index file \"%s\", exited with code %d",
                                 state->path, code);
      return;
    }

  root = clang_getTranslationUnitCursor (unit);
  g_queue_push_head (&state->cursors, g_slice_dup (CXCursor, &root));

  builder = ide_code_index_entry_builder_new ();

  for (;;)
    {
      g_autoptr(IdeCodeIndexEntry) entry = NULL;
      gboolean finish = FALSE;

      if ((entry = ide_clang_index_file_next_entry (state, builder, &finish)))
        {
          g_ptr_array_add (state->entries, g_steal_pointer (&entry));
          continue;
        }

      if (!finish)
        continue;

      break;
    }
  
  ide_task_return_pointer (task,
                           g_steal_pointer (&state->entries),
                           (GDestroyNotify)g_ptr_array_unref);
}

/**
 * ide_clang_index_file_async:
 * @self: a #IdeClang
 * @path: the path to the C/C++/Obj-C file on local disk
 * @argv: the command line arguments for clang
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: a callback to execute up on completion
 * @user_data: closure data for @callback
 *
 * Asynchronously requests that indexable entries are extracted from the file
 * found at @path. The results (an array of #IdeCodeIndexEntry) can be accessed
 * via ide_clang_index_file_finish() using the result provided to @callback
 *
 * Since: 3.30
 */
void
ide_clang_index_file_async (IdeClang            *self,
                            const gchar         *path,
                            const gchar * const *argv,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  IndexFile *state;

  g_return_if_fail (IDE_IS_CLANG (self));
  g_return_if_fail (path != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  state = g_slice_new0 (IndexFile);
  state->path = g_strdup (path);
  state->argv = ide_clang_cook_flags (argv);
  state->argc = state->argv ? g_strv_length (state->argv) : 0;
  state->entries = g_ptr_array_new ();

  IDE_PTR_ARRAY_SET_FREE_FUNC (state->entries, ide_code_index_entry_free);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_clang_index_file_async);
  ide_task_set_kind (task, IDE_TASK_KIND_INDEXER);
  ide_task_set_task_data (task, state, index_file_free);
  ide_task_run_in_thread (task, ide_clang_index_file_worker);
}

/**
 * ide_clang_index_file_finish:
 * @self: a #IdeClang
 *
 * Finishes a request to index a file.
 *
 * Returns: (transfer full): a #GPtrArray of #IdeCodeIndexEntry
 *
 * Since: 3.30
 */
GPtrArray *
ide_clang_index_file_finish (IdeClang      *self,
                             GAsyncResult  *result,
                             GError       **error)
{
  GPtrArray *ret;

  g_return_val_if_fail (IDE_IS_CLANG (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  IDE_PTR_ARRAY_CLEAR_FREE_FUNC (ret);

  return ret;
}

/* Diagnose {{{1 */

typedef struct
{
  GPtrArray  *diagnostics;
  gchar      *path;
  gchar     **argv;
  guint       argc;
} Diagnose;

static void
diagnose_free (gpointer data)
{
  Diagnose *state = data;

  g_clear_pointer (&state->path, g_free);
  g_clear_pointer (&state->argv, g_strfreev);
  g_clear_pointer (&state->diagnostics, g_ptr_array_unref);

  g_slice_free (Diagnose, state);
}

static IdeDiagnostic *
create_diagnostic (CXDiagnostic diag)
{
  g_assert (diag != NULL);


  return NULL;
}

static void
ide_clang_diagnose_worker (IdeTask      *task,
                           gpointer      source_object,
                           gpointer      task_data,
                           GCancellable *cancellable)
{
  Diagnose *state = task_data;
  g_auto(CXTranslationUnit) unit = NULL;
  g_auto(CXIndex) index = NULL;
  enum CXErrorCode code;
  unsigned options;
  guint n_diags;

  g_assert (IDE_IS_CLANG (source_object));
  g_assert (IDE_IS_TASK (task));
  g_assert (state != NULL);
  g_assert (state->path != NULL);
  g_assert (state->diagnostics != NULL);

  options = (clang_defaultEditingTranslationUnitOptions () |
#if CINDEX_VERSION >= CINDEX_VERSION_ENCODE(0, 43)
             CXTranslationUnit_SingleFileParse |
#endif
#if CINDEX_VERSION >= CINDEX_VERSION_ENCODE(0, 35)
             CXTranslationUnit_KeepGoing |
#endif
             CXTranslationUnit_DetailedPreprocessingRecord |
             CXTranslationUnit_SkipFunctionBodies);

  index = clang_createIndex (0, 0);
  code = clang_parseTranslationUnit2 (index,
                                      state->path,
                                      (const char * const *)state->argv,
                                      state->argc,
                                      NULL,
                                      0,
                                      options,
                                      &unit);

  if (code != CXError_Success)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "Failed to index file \"%s\", exited with code %d",
                                 state->path, code);
      return;
    }

  n_diags = clang_getNumDiagnostics (unit);

  for (guint i = 0; i < n_diags; i++)
    {
      g_autoptr(CXDiagnostic) cxdiag = NULL;
      g_autoptr(IdeDiagnostic) diag = NULL;

      cxdiag = clang_getDiagnostic (unit, i);
      diag = create_diagnostic (cxdiag);

      if (diag != NULL)
        g_ptr_array_add (state->diagnostics, g_steal_pointer (&diag));
    }

  ide_task_return_pointer (task,
                           g_steal_pointer (&state->diagnostics),
                           (GDestroyNotify)g_ptr_array_unref);
}

/**
 * ide_clang_diagnose_async:
 * @self: a #IdeClang
 * @path: the path to the C/C++/Obj-C file on local disk
 * @argv: the command line arguments for clang
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: a callback to execute up on completion
 * @user_data: closure data for @callback
 *
 * Asynchronously requests that the file be diagnosed.
 *
 * This generates diagnostics related to the file after parsing it.
 *
 * Since: 3.30
 */
void
ide_clang_diagnose_async (IdeClang            *self,
                          const gchar         *path,
                          const gchar * const *argv,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  Diagnose *state;

  g_return_if_fail (IDE_IS_CLANG (self));
  g_return_if_fail (path != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  state = g_slice_new0 (Diagnose);
  state->path = g_strdup (path);
  state->argv = ide_clang_cook_flags (argv);
  state->argc = state->argv ? g_strv_length (state->argv) : 0;
  state->diagnostics = g_ptr_array_new ();

  IDE_PTR_ARRAY_SET_FREE_FUNC (state->diagnostics, ide_diagnostic_unref);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_clang_diagnose_async);
  ide_task_set_kind (task, IDE_TASK_KIND_COMPILER);
  ide_task_set_task_data (task, state, diagnose_free);
  ide_task_run_in_thread (task, ide_clang_diagnose_worker);
}

/**
 * ide_clang_diagnose_finish:
 *
 * Finishes a request to diagnose a file.
 *
 * Returns: (transfer full) (element-type Ide.Diagnostic):
 *   a #GPtrArray of #IdeDiagnostic
 *
 * Since: 3.30
 */
GPtrArray *
ide_clang_diagnose_finish (IdeClang      *self,
                           GAsyncResult  *result,
                           GError       **error)
{
  GPtrArray *ret;

  g_return_val_if_fail (IDE_IS_CLANG (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  IDE_PTR_ARRAY_CLEAR_FREE_FUNC (ret);

  return ret;
}

/* Get symbol at source location {{{1 */

/* vim:set foldmethod=marker: */
