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

#define IDE_CLANG_HIGHLIGHTER_TYPE          "c:type"
#define IDE_CLANG_HIGHLIGHTER_FUNCTION_NAME "def:function"
#define IDE_CLANG_HIGHLIGHTER_ENUM_NAME     "def:constant"
#define IDE_CLANG_HIGHLIGHTER_MACRO_NAME    "c:macro-name"

struct _IdeClang
{
  GObject  parent;
  GFile   *workdir;
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
ide_clang_cook_flags (const gchar         *path,
                      const gchar * const *flags)
{
  GPtrArray *cooked = g_ptr_array_new ();
  const gchar *llvm_flags = ide_clang_get_llvm_flags ();
  g_autofree gchar *include = NULL;
  guint pos;

  if (llvm_flags != NULL)
    g_ptr_array_add (cooked, g_strdup (llvm_flags));

  pos = cooked->len;

  if (path != NULL)
    {
      g_autofree gchar *current = g_path_get_dirname (path);
      include = g_strdup_printf ("-I%s", current);
    }

  if (flags != NULL)
    {
      for (guint i = 0; flags[i]; i++)
        {
          g_ptr_array_add (cooked, g_strdup (flags[i]));

          if (g_strcmp0 (include, flags[i]) == 0)
            g_clear_pointer (&include, g_free);
        }
    }

  /* Insert -Idirname as first include if we didn't find it in the list of
   * include paths from the request. That ensures we have something that is
   * very similar to what clang would do unless they specified the path
   * somewhere else.
   */
  if (include != NULL)
    g_ptr_array_insert (cooked, pos, g_steal_pointer (&include));

  g_ptr_array_add (cooked, NULL);

  return (gchar **)g_ptr_array_free (cooked, FALSE);
}

static gboolean
is_ignored_kind (enum CXCursorKind kind)
{
  switch ((int)kind)
    {
    case CXCursor_CXXMethod:
    case CXCursor_ClassDecl:
    case CXCursor_ClassTemplate:
    case CXCursor_Constructor:
    case CXCursor_Destructor:
    case CXCursor_EnumConstantDecl:
    case CXCursor_EnumDecl:
    case CXCursor_FunctionDecl:
    case CXCursor_FunctionTemplate:
    case CXCursor_Namespace:
    case CXCursor_NamespaceAlias:
    case CXCursor_StructDecl:
    case CXCursor_TranslationUnit:
    case CXCursor_TypeAliasDecl:
    case CXCursor_TypedefDecl:
    case CXCursor_UnionDecl:
      return FALSE;

    default:
      return TRUE;
    }
}

static CXCursor
move_to_previous_sibling (CXTranslationUnit unit,
                          CXCursor          cursor)
{
  CXSourceRange range = clang_getCursorExtent (cursor);
  CXSourceLocation begin = clang_getRangeStart (range);
  CXSourceLocation loc;
  CXFile file;
  unsigned line;
  unsigned column;

  clang_getFileLocation (begin, &file, &line, &column, NULL);
  loc = clang_getLocation (unit, file, line, column - 1);

  return clang_getCursor (unit, loc);
}

static enum CXChildVisitResult
find_child_type (CXCursor     cursor,
                 CXCursor     parent,
                 CXClientData user_data)
{
  enum CXCursorKind *child_kind = user_data;
  enum CXCursorKind kind = clang_getCursorKind (cursor);

  switch ((int)kind)
    {
    case CXCursor_StructDecl:
    case CXCursor_UnionDecl:
    case CXCursor_EnumDecl:
      *child_kind = kind;
      return CXChildVisit_Break;

    case CXCursor_TypeRef:
      cursor = clang_getCursorReferenced (cursor);
      *child_kind = clang_getCursorKind (cursor);
      return CXChildVisit_Break;

    default:
      break;
    }

  return CXChildVisit_Continue;
}

static IdeSymbolKind
ide_clang_get_symbol_kind (CXCursor        cursor,
                           IdeSymbolFlags *flags)
{
  enum CXAvailabilityKind availability;
  enum CXCursorKind cxkind;
  IdeSymbolFlags local_flags = 0;
  IdeSymbolKind kind = 0;

  availability = clang_getCursorAvailability (cursor);
  if (availability == CXAvailability_Deprecated)
    local_flags |= IDE_SYMBOL_FLAGS_IS_DEPRECATED;

  cxkind = clang_getCursorKind (cursor);

  if (cxkind == CXCursor_TypedefDecl)
    {
      enum CXCursorKind child_kind = 0;

      clang_visitChildren (cursor, find_child_type, &child_kind);
      cxkind = child_kind;
    }

  switch ((int)cxkind)
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

    case CXCursor_FunctionDecl:
      kind = IDE_SYMBOL_FUNCTION;
      break;

    case CXCursor_EnumDecl:
      kind = IDE_SYMBOL_ENUM;
      break;

    case CXCursor_EnumConstantDecl:
      kind = IDE_SYMBOL_ENUM_VALUE;
      break;

    case CXCursor_FieldDecl:
      kind = IDE_SYMBOL_FIELD;
      break;

    case CXCursor_InclusionDirective:
      kind = IDE_SYMBOL_HEADER;
      break;

    default:
      break;
    }

  *flags = local_flags;

  return kind;
}

static IdeSymbol *
create_symbol (const gchar  *path,
               CXCursor      cursor,
               GError      **error)
{
  g_auto(CXString) cxname = {0};
  g_autoptr(GFile) gfile = NULL;
  g_autoptr(IdeFile) ifile = NULL;
  g_autoptr(IdeSourceLocation) srcloc = NULL;
  IdeSymbolKind symkind;
  IdeSymbolFlags symflags = 0;
  CXSourceLocation loc;
  guint line;
  guint column;

  if (clang_Cursor_isNull (cursor))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_FOUND,
                   "Failed to locate position in translation unit");
      return NULL;
    }

  loc = clang_getCursorLocation (cursor);
  clang_getExpansionLocation (loc, NULL, &line, &column, NULL);
  gfile = g_file_new_for_path (path);
  ifile = ide_file_new (NULL, gfile);

  if (line) line--;
  if (column) column--;

  srcloc = ide_source_location_new (ifile, line, column, 0);
  cxname = clang_getCursorSpelling (cursor);
  symkind = ide_clang_get_symbol_kind (cursor, &symflags);

  return ide_symbol_new (clang_getCString (cxname),
                         symkind,
                         symflags,
                         NULL,
                         NULL,
                         srcloc);
}

static void
ide_clang_finalize (GObject *object)
{
  IdeClang *self = (IdeClang *)object;

  g_clear_object (&self->workdir);

  G_OBJECT_CLASS (ide_clang_parent_class)->finalize (object);
}

static void
ide_clang_class_init (IdeClangClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_clang_finalize;
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

void
ide_clang_set_workdir (IdeClang *self,
                       GFile    *workdir)
{
  g_return_if_fail (IDE_IS_CLANG (self));
  g_return_if_fail (!workdir || G_IS_FILE (workdir));

  g_set_object (&self->workdir, workdir);
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
  unsigned options;
  enum CXErrorCode code;

  g_assert (IDE_IS_CLANG (source_object));
  g_assert (IDE_IS_TASK (task));
  g_assert (state != NULL);
  g_assert (state->path != NULL);
  g_assert (state->entries != NULL);

  options = CXTranslationUnit_DetailedPreprocessingRecord
#if CINDEX_VERSION >= CINDEX_VERSION_ENCODE(0, 43)
          | CXTranslationUnit_SingleFileParse
#endif
#if CINDEX_VERSION >= CINDEX_VERSION_ENCODE(0, 35)
          | CXTranslationUnit_KeepGoing
#endif
          | CXTranslationUnit_SkipFunctionBodies;

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
  state->argv = ide_clang_cook_flags (path, argv);
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
  GFile      *workdir;
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
  g_clear_object (&state->workdir);

  g_slice_free (Diagnose, state);
}

static gboolean
cxfile_equal (CXFile  cxfile,
              GFile  *file)
{
  g_auto(CXString) cxstr = clang_getFileName (cxfile);
  g_autofree gchar *path = g_file_get_path (file);
  const gchar *cstr = clang_getCString (cxstr);

  return g_strcmp0 (cstr, path) == 0;
}

static gchar *
path_or_uri (GFile *file)
{
  return g_file_is_native (file) ?
         g_file_get_path (file) :
         g_file_get_uri (file);
}

static gchar *
get_path (GFile       *workdir,
          const gchar *path)
{
  g_autoptr(GFile) file = NULL;
  g_autoptr(GFile) child = NULL;

  if (path == NULL)
    return path_or_uri (workdir);

  file = g_file_new_for_path (path);
  if (g_file_has_prefix (file, workdir))
    return g_strdup (path);

  child = g_file_get_child (workdir, path);

  return path_or_uri (child);
}

static IdeSourceLocation *
create_location (GFile             *workdir,
                 CXSourceLocation   cxloc,
                 IdeSourceLocation *alternate)
{
  g_autofree gchar *path = NULL;
  g_autoptr(IdeFile) file = NULL;
  g_autoptr(GFile) gfile = NULL;
  g_auto(CXString) str = {0};
  CXFile cxfile = NULL;
  unsigned line;
  unsigned column;
  unsigned offset;

  g_assert (G_IS_FILE (workdir));

  clang_getFileLocation (cxloc, &cxfile, &line, &column, &offset);

  str = clang_getFileName (cxfile);

  if (line == 0 || clang_getCString (str) == NULL)
    return alternate ? ide_source_location_ref (alternate) : NULL;

  if (line > 0)
    line--;

  if (column > 0)
    column--;

  /* TODO: Remove IdeFile from IdeSourceLocation */

  path = get_path (workdir, clang_getCString (str));
  gfile = g_file_new_for_path (path);
  file = ide_file_new (NULL, gfile);

  return ide_source_location_new (file, line, column, offset);
}

static IdeSourceRange *
create_range (GFile         *workdir,
              CXSourceRange  cxrange)
{
  IdeSourceRange *range = NULL;
  CXSourceLocation cxbegin;
  CXSourceLocation cxend;
  g_autoptr(IdeSourceLocation) begin = NULL;
  g_autoptr(IdeSourceLocation) end = NULL;

  g_assert (G_IS_FILE (workdir));

  cxbegin = clang_getRangeStart (cxrange);
  cxend = clang_getRangeEnd (cxrange);

  /* Sometimes the end location does not have a file associated with it,
   * so we force it to have the IdeFile of the first location.
   */
  begin = create_location (workdir, cxbegin, NULL);
  end = create_location (workdir, cxend, begin);

  if ((begin != NULL) && (end != NULL))
    range = ide_source_range_new (begin, end);

  return range;
}

static IdeDiagnostic *
create_diagnostic (GFile        *workdir,
                   GFile        *target,
                   CXDiagnostic *cxdiag)
{
  g_autoptr(IdeSourceLocation) loc = NULL;
  enum CXDiagnosticSeverity cxseverity;
  IdeDiagnosticSeverity severity;
  IdeDiagnostic *diag;
  const gchar *spelling;
  g_auto(CXString) cxstr = {0};
  CXSourceLocation cxloc;
  CXFile cxfile = NULL;
  guint num_ranges;

  g_assert (!workdir || G_IS_FILE (workdir));
  g_assert (cxdiag != NULL);

  cxloc = clang_getDiagnosticLocation (cxdiag);
  clang_getExpansionLocation (cxloc, &cxfile, NULL, NULL, NULL);

  if (cxfile && !cxfile_equal (cxfile, target))
    return NULL;

  cxseverity = clang_getDiagnosticSeverity (cxdiag);
  severity = ide_clang_translate_severity (cxseverity);

  cxstr = clang_getDiagnosticSpelling (cxdiag);
  spelling = clang_getCString (cxstr);

  /*
   * I thought we could use an approach like the following to get deprecation
   * status. However, it has so far proven ineffective.
   *
   *   cursor = clang_getCursor (self->tu, cxloc);
   *   avail = clang_getCursorAvailability (cursor);
   */
  if ((severity == IDE_DIAGNOSTIC_WARNING) &&
      (spelling != NULL) &&
      (strstr (spelling, "deprecated") != NULL))
    severity = IDE_DIAGNOSTIC_DEPRECATED;

  loc = create_location (workdir, cxloc, NULL);

  diag = ide_diagnostic_new (severity, spelling, loc);
  num_ranges = clang_getDiagnosticNumRanges (cxdiag);

  for (guint i = 0; i < num_ranges; i++)
    {
      CXSourceRange cxrange;
      IdeSourceRange *range;

      cxrange = clang_getDiagnosticRange (cxdiag, i);
      range = create_range (workdir, cxrange);

      if (range != NULL)
        ide_diagnostic_take_range (diag, range);
    }

  return diag;
}

static void
ide_clang_diagnose_worker (IdeTask      *task,
                           gpointer      source_object,
                           gpointer      task_data,
                           GCancellable *cancellable)
{
  Diagnose *state = task_data;
  g_autoptr(GFile) file = NULL;
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

  options = clang_defaultEditingTranslationUnitOptions ()
#if CINDEX_VERSION >= CINDEX_VERSION_ENCODE(0, 35)
          | CXTranslationUnit_KeepGoing
#endif
          | CXTranslationUnit_DetailedPreprocessingRecord;

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
  file = g_file_new_for_path (state->path);

  for (guint i = 0; i < n_diags; i++)
    {
      g_autoptr(CXDiagnostic) cxdiag = NULL;
      g_autoptr(IdeDiagnostic) diag = NULL;

      cxdiag = clang_getDiagnostic (unit, i);
      diag = create_diagnostic (state->workdir, file, cxdiag);

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
  g_autofree gchar *parent = NULL;
  Diagnose *state;

  g_return_if_fail (IDE_IS_CLANG (self));
  g_return_if_fail (path != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  state = g_slice_new0 (Diagnose);
  state->path = g_strdup (path);
  state->argv = ide_clang_cook_flags (path, argv);
  state->argc = state->argv ? g_strv_length (state->argv) : 0;
  state->diagnostics = g_ptr_array_new ();

  if (self->workdir != NULL)
    state->workdir = g_object_ref (self->workdir);
  else
    state->workdir = g_file_new_for_path ((parent = g_path_get_dirname (path)));

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

/* Completion {{{1 */

typedef struct
{
  gchar  *path;
  gchar **argv;
  gint    argc;
  guint   line;
  guint   column;
} Complete;

static void
complete_free (gpointer data)
{
  Complete *state = data;

  g_clear_pointer (&state->path, g_free);
  g_clear_pointer (&state->argv, g_strfreev);
  g_slice_free (Complete, state);
}

static guint
translate_completion_kind (enum CXCursorKind kind)
{
  switch ((int)kind)
    {
    case CXCursor_StructDecl:
      return IDE_LSP_COMPLETION_STRUCT;

    case CXCursor_ClassDecl:
      return IDE_LSP_COMPLETION_CLASS;

    case CXCursor_Constructor:
      return IDE_LSP_COMPLETION_CONSTRUCTOR;

    case CXCursor_Destructor:
    case CXCursor_CXXMethod:
      return IDE_LSP_COMPLETION_METHOD;

    case CXCursor_FunctionDecl:
      return IDE_LSP_COMPLETION_FUNCTION;

    case CXCursor_EnumConstantDecl:
      return IDE_LSP_COMPLETION_ENUM_MEMBER;

    case CXCursor_EnumDecl:
      return IDE_LSP_COMPLETION_ENUM;

    case CXCursor_InclusionDirective:
      return IDE_LSP_COMPLETION_FILE;

    case CXCursor_PreprocessingDirective:
    case CXCursor_MacroDefinition:
    case CXCursor_MacroExpansion:
      return IDE_LSP_COMPLETION_TEXT;

    case CXCursor_TypeRef:
    case CXCursor_TypeAliasDecl:
    case CXCursor_TypeAliasTemplateDecl:
    case CXCursor_TypedefDecl:
      return IDE_LSP_COMPLETION_CLASS;

    default:
      return IDE_LSP_COMPLETION_TEXT;
    }
}

static void
ide_clang_build_completion (GVariantBuilder    *builder,
                            CXCompletionResult *result)
{
  GVariantBuilder chunks_builder;
  guint n_chunks;

  g_assert (builder != NULL);
  g_assert (result != NULL);

  g_variant_builder_add_parsed (builder, "{%s,<%i>}", "kind",
                                translate_completion_kind (result->CursorKind));

  if (clang_getCompletionAvailability (result->CompletionString))
    g_variant_builder_add_parsed (builder, "{%s,<%i>}", "availability",
                                  clang_getCompletionAvailability (result->CompletionString));

  n_chunks = clang_getNumCompletionChunks (result->CompletionString);

  g_variant_builder_init (&chunks_builder, G_VARIANT_TYPE ("aa{sv}"));

  for (guint i = 0; i < n_chunks; i++)
    {
      g_auto(CXString) str = clang_getCompletionChunkText (result->CompletionString, i);
      guint kind = clang_getCompletionChunkKind (result->CompletionString, i);

      g_variant_builder_open (&chunks_builder, G_VARIANT_TYPE_VARDICT);
      g_variant_builder_add_parsed (&chunks_builder, "{%s,<%s>}", "text", clang_getCString (str));
      g_variant_builder_add_parsed (&chunks_builder, "{%s,<%i>}", "kind", kind);
      g_variant_builder_close (&chunks_builder);
    }

  g_variant_builder_add (builder, "{sv}", "chunks", g_variant_builder_end (&chunks_builder));
}

static void
ide_clang_complete_worker (IdeTask      *task,
                           gpointer      source_object,
                           gpointer      task_data,
                           GCancellable *cancellable)
{
  Complete *state = task_data;
  g_autoptr(CXCodeCompleteResults) results = NULL;
  g_auto(CXTranslationUnit) unit = NULL;
  g_auto(CXIndex) index = NULL;
  GVariantBuilder builder;
  enum CXErrorCode code;
  unsigned options;

  g_assert (IDE_IS_TASK (task));
  g_assert (IDE_IS_CLANG (source_object));
  g_assert (state != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  /* TODO: We need file sync for unsaved buffers */

  options = clang_defaultEditingTranslationUnitOptions ();

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
                                 "Failed to complete \"%s\", exited with code %d",
                                 state->path, code);
      return;
    }

  results = clang_codeCompleteAt (unit,
                                  state->path,
                                  state->line,
                                  state->column,
                                  NULL,
                                  0,
                                  clang_defaultCodeCompleteOptions ());

  if (results == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "Failed to complete \"%s\", no results",
                                 state->path);
      return;
    }

#if 0
  clang_sortCodeCompletionResults (results->Results, results->NumResults);
#endif

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("aa{sv}"));

  for (guint i = 0; i < results->NumResults; i++)
    {
      g_variant_builder_open (&builder, G_VARIANT_TYPE_VARDICT);
      ide_clang_build_completion (&builder, &results->Results[i]);
      g_variant_builder_close (&builder);
    }

  ide_task_return_pointer (task,
                           g_variant_ref_sink (g_variant_builder_end (&builder)),
                           (GDestroyNotify)g_variant_unref);
}

void
ide_clang_complete_async (IdeClang            *self,
                          const gchar         *path,
                          guint                line,
                          guint                column,
                          const gchar * const *argv,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autofree gchar *parent = NULL;
  Complete *state;

  g_return_if_fail (IDE_IS_CLANG (self));
  g_return_if_fail (path != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  state = g_slice_new0 (Complete);
  state->path = g_strdup (path);
  state->argv = ide_clang_cook_flags (path, argv);
  state->argc = state->argv ? g_strv_length (state->argv) : 0;
  state->line = line;
  state->column = column;

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_clang_complete_async);
  ide_task_set_kind (task, IDE_TASK_KIND_COMPILER);
  ide_task_set_task_data (task, state, complete_free);
  ide_task_run_in_thread (task, ide_clang_complete_worker);
}

GVariant *
ide_clang_complete_finish (IdeClang      *self,
                           GAsyncResult  *result,
                           GError       **error)
{
  g_return_val_if_fail (IDE_IS_CLANG (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

/* Find Nearest Scope {{{1 */

typedef struct
{
  gchar  *path;
  gchar **argv;
  gint    argc;
  guint   line;
  guint   column;
} FindNearestScope;

static void
find_nearest_scope_free (gpointer data)
{
  FindNearestScope *state = data;

  g_clear_pointer (&state->path, g_free);
  g_clear_pointer (&state->argv, g_strfreev);
  g_slice_free (FindNearestScope, state);
}

static void
ide_clang_find_nearest_scope_worker (IdeTask      *task,
                                     gpointer      source_object,
                                     gpointer      task_data,
                                     GCancellable *cancellable)
{
  FindNearestScope *state = task_data;
  g_autoptr(IdeSymbol) ret = NULL;
  g_auto(CXTranslationUnit) unit = NULL;
  g_auto(CXIndex) index = NULL;
  g_autoptr(GError) error = NULL;
  enum CXCursorKind kind;
  enum CXErrorCode code;
  CXSourceLocation loc;
  CXCursor cursor;
  CXFile file;

  g_assert (IDE_IS_TASK (task));
  g_assert (IDE_IS_CLANG (source_object));
  g_assert (state != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  /* TODO: We need file sync for unsaved buffers */

  index = clang_createIndex (0, 0);
  code = clang_parseTranslationUnit2 (index,
                                      state->path,
                                      (const char * const *)state->argv,
                                      state->argc,
                                      NULL,
                                      0,
                                      clang_defaultEditingTranslationUnitOptions (),
                                      &unit);

  if (code != CXError_Success)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "Failed to find_nearest_scope \"%s\", exited with code %d",
                                 state->path, code);
      return;
    }

  file = clang_getFile (unit, state->path);
  loc = clang_getLocation (unit, file, state->line, state->column);
  cursor = clang_getCursor (unit, loc);

  /*
   * Macros sort of mess us up and result in us thinking
   * we are in some sort of InvalidFile condition.
   */
  kind = clang_getCursorKind (cursor);
  if (kind == CXCursor_MacroExpansion)
    cursor = move_to_previous_sibling (unit, cursor);

  /*
   * The semantic parent may still be uninteresting to us,
   * so possibly keep walking up the AST until we get to
   * something better.
   */
  do
    {
      cursor = clang_getCursorSemanticParent (cursor);
      kind = clang_getCursorKind (cursor);
    }
  while (!clang_Cursor_isNull (cursor) && is_ignored_kind (kind));

  if (kind == CXCursor_TranslationUnit)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_FOUND,
                                 "The location does not have a semantic parent");
      return;
    }

  if (!(ret = create_symbol (state->path, cursor, &error)))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_pointer (task,
                             g_steal_pointer (&ret),
                             (GDestroyNotify)ide_symbol_unref);
}

void
ide_clang_find_nearest_scope_async (IdeClang            *self,
                                    const gchar         *path,
                                    const gchar * const *argv,
                                    guint                line,
                                    guint                column,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  FindNearestScope *state;

  g_return_if_fail (IDE_IS_CLANG (self));
  g_return_if_fail (path != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  state = g_slice_new0 (FindNearestScope);
  state->path = g_strdup (path);
  state->argv = ide_clang_cook_flags (path, argv);
  state->argc = state->argv ? g_strv_length (state->argv) : 0;
  state->line = line;
  state->column = column;

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_clang_find_nearest_scope_async);
  ide_task_set_kind (task, IDE_TASK_KIND_COMPILER);
  ide_task_set_task_data (task, state, find_nearest_scope_free);
  ide_task_run_in_thread (task, ide_clang_find_nearest_scope_worker);
}

IdeSymbol *
ide_clang_find_nearest_scope_finish (IdeClang      *self,
                                     GAsyncResult  *result,
                                     GError       **error)
{
  g_return_val_if_fail (IDE_IS_CLANG (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

/* Get symbol at source location {{{1 */

typedef struct
{
  GFile  *workdir;
  gchar  *path;
  gchar **argv;
  gint    argc;
  guint   line;
  guint   column;
} LocateSymbol;

static void
locate_symbol_free (gpointer data)
{
  LocateSymbol *state = data;

  g_clear_object (&state->workdir);
  g_clear_pointer (&state->path, g_free);
  g_clear_pointer (&state->argv, g_strfreev);
  g_slice_free (LocateSymbol, state);
}

static void
ide_clang_locate_symbol_worker (IdeTask      *task,
                                gpointer      source_object,
                                gpointer      task_data,
                                GCancellable *cancellable)
{
  LocateSymbol *state = task_data;
  g_autoptr(IdeSourceLocation) declaration = NULL;
  g_autoptr(IdeSourceLocation) definition = NULL;
  g_autoptr(IdeSourceLocation) canonical = NULL;
  g_autoptr(IdeSymbol) ret = NULL;
  g_auto(CXTranslationUnit) unit = NULL;
  g_auto(CXString) cxstr = {0};
  g_auto(CXIndex) index = NULL;
  CXSourceLocation cxlocation;
  enum CXErrorCode code;
  IdeSymbolFlags symflags;
  IdeSymbolKind symkind;
  CXCursor cursor;
  CXCursor tmpcursor;
  CXFile cxfile;

  g_assert (IDE_IS_TASK (task));
  g_assert (IDE_IS_CLANG (source_object));
  g_assert (state != NULL);
  g_assert (state->path != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  index = clang_createIndex (0, 0);
  code = clang_parseTranslationUnit2 (index,
                                      state->path,
                                      (const char * const *)state->argv,
                                      state->argc,
                                      NULL,
                                      0,
                                      clang_defaultEditingTranslationUnitOptions (),
                                      &unit);

  if (code != CXError_Success)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "Failed to find_nearest_scope \"%s\", exited with code %d",
                                 state->path, code);
      return;
    }

  cxfile = clang_getFile (unit, state->path);
  cxlocation = clang_getLocation (unit, cxfile, state->line, state->column);
  cursor = clang_getCursor (unit, cxlocation);

  if (clang_Cursor_isNull (cursor))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_FOUND,
                                 "Failed to locate cursor at position");
      return;
    }

  tmpcursor = clang_getCursorDefinition (cursor);

  if (clang_Cursor_isNull (tmpcursor))
    tmpcursor = clang_getCursorReferenced (cursor);

  if (!clang_Cursor_isNull (tmpcursor))
    {
      CXSourceLocation tmploc;
      CXSourceRange cxrange;

      cxrange = clang_getCursorExtent (tmpcursor);
      tmploc = clang_getRangeStart (cxrange);

      if (clang_isCursorDefinition (tmpcursor))
        definition = create_location (state->workdir, tmploc, NULL);
      else
        declaration = create_location (state->workdir, tmploc, NULL);

      cursor = tmpcursor;
    }

  symkind = ide_clang_get_symbol_kind (cursor, &symflags);

  if (symkind == IDE_SYMBOL_HEADER)
    {
      g_auto(CXString) included_file_name = {0};
      CXFile included_file;
      const gchar *path;

      included_file = clang_getIncludedFile (cursor);
      included_file_name = clang_getFileName (included_file);
      path = clang_getCString (included_file_name);

      if (path != NULL)
        {
          g_autoptr(IdeFile) file = NULL;
          g_autoptr(GFile) gfile = NULL;

          gfile = g_file_new_for_path (path);
          file = ide_file_new (NULL, gfile);

          g_clear_pointer (&definition, ide_symbol_unref);
          declaration = ide_source_location_new (file, 0, 0, 0);
        }
    }

  cxstr = clang_getCursorDisplayName (cursor);
  ret = ide_symbol_new (clang_getCString (cxstr), symkind, symflags,
                        declaration, definition, canonical);

  ide_task_return_pointer (task,
                           g_steal_pointer (&ret),
                           (GDestroyNotify)ide_symbol_unref);
}

void
ide_clang_locate_symbol_async (IdeClang            *self,
                               const gchar         *path,
                               const gchar * const *argv,
                               guint                line,
                               guint                column,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autofree gchar *parent = NULL;
  LocateSymbol *state;

  g_return_if_fail (IDE_IS_CLANG (self));
  g_return_if_fail (path != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  state = g_slice_new0 (LocateSymbol);
  state->path = g_strdup (path);
  state->argv = ide_clang_cook_flags (path, argv);
  state->argc = state->argv ? g_strv_length (state->argv) : 0;
  state->line = line;
  state->column = column;

  if (self->workdir != NULL)
    state->workdir = g_object_ref (self->workdir);
  else
    state->workdir = g_file_new_for_path ((parent = g_path_get_dirname (path)));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_clang_locate_symbol_async);
  ide_task_set_kind (task, IDE_TASK_KIND_COMPILER);
  ide_task_set_task_data (task, state, locate_symbol_free);
  ide_task_run_in_thread (task, ide_clang_locate_symbol_worker);
}

IdeSymbol *
ide_clang_locate_symbol_finish (IdeClang      *self,
                                GAsyncResult  *result,
                                GError       **error)
{
  g_return_val_if_fail (IDE_IS_CLANG (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

/* Get Symbol Tree {{{1 */

typedef struct
{
  GFile           *workdir;
  gchar           *path;
  gchar          **argv;
  gint             argc;
  GVariantBuilder *current;
} GetSymbolTree;

static void
get_symbol_tree_free (gpointer data)
{
  GetSymbolTree *state = data;

  g_clear_object (&state->workdir);
  g_clear_pointer (&state->path, g_free);
  g_clear_pointer (&state->argv, g_strfreev);
  g_slice_free (GetSymbolTree, state);
}

static gboolean
cursor_is_recognized (GetSymbolTree *state,
                      CXCursor       cursor)
{
  enum CXCursorKind kind;
  gboolean ret = FALSE;

  kind = clang_getCursorKind (cursor);

  switch ((int)kind)
    {
    /* TODO: Support way more CXCursorKind. */
    case CXCursor_ClassDecl:
    case CXCursor_Constructor:
    case CXCursor_Destructor:
    case CXCursor_CXXMethod:
    case CXCursor_EnumConstantDecl:
    case CXCursor_EnumDecl:
    case CXCursor_FieldDecl:
    case CXCursor_FunctionDecl:
    case CXCursor_Namespace:
    case CXCursor_StructDecl:
    case CXCursor_TypedefDecl:
    case CXCursor_UnionDecl:
    case CXCursor_VarDecl:
      {
        g_auto(CXString) filename = {0};
        CXSourceLocation cxloc;
        CXFile file;

        cxloc = clang_getCursorLocation (cursor);
        clang_getFileLocation (cxloc, &file, NULL, NULL, NULL);
        filename = clang_getFileName (file);
        ret = dzl_str_equal0 (clang_getCString (filename), state->path);
      }
      break;

    default:
      break;
    }

  return ret;
}

static enum CXChildVisitResult
traverse_cursors (CXCursor     cursor,
                  CXCursor     parent,
                  CXClientData user_data)
{
  GetSymbolTree *state = user_data;

  if (cursor_is_recognized (state, cursor))
    {
      g_autoptr(IdeSymbol) symbol = NULL;

      if ((symbol = create_symbol (state->path, cursor, NULL)))
        {
          g_autoptr(GVariant) var = ide_symbol_to_variant (symbol);
          GVariantBuilder *cur = state->current;
          GVariantBuilder builder;
          GVariant *children = NULL;
          GVariantDict dict;

          state->current = &builder;

          g_variant_dict_init (&dict, var);
          g_variant_builder_init (&builder, G_VARIANT_TYPE ("aa{sv}"));

          clang_visitChildren (cursor, traverse_cursors, state);

          children = g_variant_new_variant (g_variant_builder_end (&builder));
          g_variant_dict_insert_value (&dict, "children", children);
          g_variant_builder_add_value (cur, g_variant_dict_end (&dict));

          state->current = cur;
        }
    }

  return CXChildVisit_Continue;
}

static void
ide_clang_get_symbol_tree_worker (IdeTask      *task,
                                  gpointer      source_object,
                                  gpointer      task_data,
                                  GCancellable *cancellable)
{
  GetSymbolTree *state = task_data;
  g_autoptr(GVariant) ret = NULL;
  g_auto(CXIndex) index = NULL;
  g_auto(CXTranslationUnit) unit = NULL;
  GVariantBuilder builder;
  enum CXErrorCode code;
  CXCursor cursor;

  g_assert (IDE_IS_TASK (task));
  g_assert (IDE_IS_CLANG (source_object));
  g_assert (state != NULL);
  g_assert (state->path != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  index = clang_createIndex (0, 0);
  code = clang_parseTranslationUnit2 (index,
                                      state->path,
                                      (const char * const *)state->argv,
                                      state->argc,
                                      NULL,
                                      0,
                                      clang_defaultEditingTranslationUnitOptions (),
                                      &unit);

  if (code != CXError_Success)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "Failed to locate symbol at position");
      return;
    }

  state->current = &builder;

  cursor = clang_getTranslationUnitCursor (unit);

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("aa{sv}"));
  clang_visitChildren (cursor, traverse_cursors, state);
  ret = g_variant_ref_sink (g_variant_builder_end (&builder));

  ide_task_return_pointer (task, g_steal_pointer (&ret), (GDestroyNotify)g_variant_unref);
}

void
ide_clang_get_symbol_tree_async (IdeClang            *self,
                                 const gchar         *path,
                                 const gchar * const *argv,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autofree gchar *parent = NULL;
  GetSymbolTree *state;

  g_return_if_fail (IDE_IS_CLANG (self));
  g_return_if_fail (path != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  state = g_slice_new0 (GetSymbolTree);
  state->path = g_strdup (path);
  state->argv = ide_clang_cook_flags (path, argv);
  state->argc = state->argv ? g_strv_length (state->argv) : 0;

  if (self->workdir != NULL)
    state->workdir = g_object_ref (self->workdir);
  else
    state->workdir = g_file_new_for_path ((parent = g_path_get_dirname (path)));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_clang_get_symbol_tree_async);
  ide_task_set_kind (task, IDE_TASK_KIND_COMPILER);
  ide_task_set_task_data (task, state, get_symbol_tree_free);
  ide_task_run_in_thread (task, ide_clang_get_symbol_tree_worker);
}

GVariant *
ide_clang_get_symbol_tree_finish (IdeClang      *self,
                                  GAsyncResult  *result,
                                  GError       **error)
{
  g_return_val_if_fail (IDE_IS_CLANG (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

/* Get Highlight Index {{{1 */

typedef struct
{
  GFile  *workdir;
  gchar  *path;
  gchar **argv;
  gint    argc;
} GetHighlightIndex;

static void
get_highlight_index_free (gpointer data)
{
  GetHighlightIndex *state = data;

  g_clear_object (&state->workdir);
  g_clear_pointer (&state->path, g_free);
  g_clear_pointer (&state->argv, g_strfreev);
  g_slice_free (GetHighlightIndex, state);
}

static enum CXChildVisitResult
build_index_visitor (CXCursor cursor,
                     CXCursor     parent,
                     CXClientData user_data)
{
  IdeHighlightIndex *highlight = user_data;
  enum CXCursorKind kind;
  const gchar *style_name = NULL;

  g_assert (highlight != NULL);

  kind = clang_getCursorKind (cursor);

  switch ((int)kind)
    {
    case CXCursor_TypedefDecl:
    case CXCursor_TypeAliasDecl:
    case CXCursor_StructDecl:
    case CXCursor_ClassDecl:
      style_name = IDE_CLANG_HIGHLIGHTER_TYPE;
      break;

    case CXCursor_FunctionDecl:
      style_name = IDE_CLANG_HIGHLIGHTER_FUNCTION_NAME;
      break;

    case CXCursor_EnumDecl:
      style_name = IDE_CLANG_HIGHLIGHTER_ENUM_NAME;
      clang_visitChildren (cursor, build_index_visitor, user_data);
      break;

    case CXCursor_EnumConstantDecl:
      style_name = IDE_CLANG_HIGHLIGHTER_ENUM_NAME;
      break;

    case CXCursor_MacroDefinition:
      style_name = IDE_CLANG_HIGHLIGHTER_MACRO_NAME;
      break;

    default:
      break;
    }

  if (style_name != NULL)
    {
      g_auto(CXString) cxstr = {0};
      const gchar *word;

      cxstr = clang_getCursorSpelling (cursor);
      word = clang_getCString (cxstr);
      ide_highlight_index_insert (highlight, word, (gpointer)style_name);
    }

  return CXChildVisit_Continue;
}

static void
ide_clang_get_highlight_index_worker (IdeTask      *task,
                                      gpointer      source_object,
                                      gpointer      task_data,
                                      GCancellable *cancellable)
{
  static const gchar *common_defines[] = { "NULL", "MIN", "MAX", "__LINE__", "__FILE__" };
  GetHighlightIndex *state = task_data;
  g_autoptr(IdeHighlightIndex) highlight = NULL;
  g_auto(CXIndex) index = NULL;
  g_auto(CXTranslationUnit) unit = NULL;
  enum CXErrorCode code;
  CXCursor cursor;

  g_assert (IDE_IS_TASK (task));
  g_assert (IDE_IS_CLANG (source_object));
  g_assert (state != NULL);
  g_assert (state->path != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  index = clang_createIndex (0, 0);
  code = clang_parseTranslationUnit2 (index,
                                      state->path,
                                      (const char * const *)state->argv,
                                      state->argc,
                                      NULL,
                                      0,
                                      clang_defaultEditingTranslationUnitOptions (),
                                      &unit);

  if (code != CXError_Success)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "Failed to locate symbol at position");
      return;
    }

  highlight = ide_highlight_index_new ();

  for (guint i = 0; i < G_N_ELEMENTS (common_defines); i++)
    ide_highlight_index_insert (highlight, common_defines[i], "c:common-defines");
  ide_highlight_index_insert (highlight, "TRUE", "c:boolean");
  ide_highlight_index_insert (highlight, "FALSE", "c:boolean");
  ide_highlight_index_insert (highlight, "g_autoptr", "c:storage-class");
  ide_highlight_index_insert (highlight, "g_auto", "c:storage-class");
  ide_highlight_index_insert (highlight, "g_autofree", "c:storage-class");

  cursor = clang_getTranslationUnitCursor (unit);
  clang_visitChildren (cursor, build_index_visitor, highlight);

  ide_task_return_pointer (task,
                           g_steal_pointer (&highlight),
                           (GDestroyNotify)ide_highlight_index_unref);
}

void
ide_clang_get_highlight_index_async (IdeClang            *self,
                                     const gchar         *path,
                                     const gchar * const *argv,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autofree gchar *parent = NULL;
  GetHighlightIndex *state;

  g_return_if_fail (IDE_IS_CLANG (self));
  g_return_if_fail (path != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  state = g_slice_new0 (GetHighlightIndex);
  state->path = g_strdup (path);
  state->argv = ide_clang_cook_flags (path, argv);
  state->argc = state->argv ? g_strv_length (state->argv) : 0;

  if (self->workdir != NULL)
    state->workdir = g_object_ref (self->workdir);
  else
    state->workdir = g_file_new_for_path ((parent = g_path_get_dirname (path)));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_clang_get_highlight_index_async);
  ide_task_set_kind (task, IDE_TASK_KIND_COMPILER);
  ide_task_set_task_data (task, state, get_highlight_index_free);
  ide_task_run_in_thread (task, ide_clang_get_highlight_index_worker);
}

IdeHighlightIndex *
ide_clang_get_highlight_index_finish (IdeClang      *self,
                                      GAsyncResult  *result,
                                      GError       **error)
{
  g_return_val_if_fail (IDE_IS_CLANG (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

/* Get Index Key {{{1 */

typedef struct
{
  gchar  *path;
  gchar **argv;
  gint    argc;
  guint   line;
  guint   column;
} GetIndexKey;

static void
get_index_key_free (gpointer data)
{
  GetIndexKey *state = data;

  g_clear_pointer (&state->path, g_free);
  g_clear_pointer (&state->argv, g_strfreev);
  g_slice_free (GetIndexKey, state);
}

static void
ide_clang_get_index_key_worker (IdeTask      *task,
                                gpointer      source_object,
                                gpointer      task_data,
                                GCancellable *cancellable)
{
  GetIndexKey *state = task_data;
  g_auto(CXIndex) index = NULL;
  g_auto(CXTranslationUnit) unit = NULL;
  g_auto(CXString) cxusr = {0};
  const gchar *usr = NULL;
  enum CXErrorCode code;
  enum CXLinkageKind linkage;
  CXSourceLocation loc;
  CXCursor declaration;
  CXCursor cursor;
  CXFile file;

  g_assert (IDE_IS_TASK (task));
  g_assert (IDE_IS_CLANG (source_object));
  g_assert (state != NULL);
  g_assert (state->path != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  index = clang_createIndex (0, 0);
  code = clang_parseTranslationUnit2 (index,
                                      state->path,
                                      (const char * const *)state->argv,
                                      state->argc,
                                      NULL,
                                      0,
                                      clang_defaultEditingTranslationUnitOptions (),
                                      &unit);

  if (code != CXError_Success)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "Failed to locate symbol at position");
      return;
    }

  file = clang_getFile (unit, state->path);
  loc = clang_getLocation (unit, file, state->line, state->column);
  cursor = clang_getCursor (unit, loc);
  declaration = clang_getCursorReferenced (cursor);
  cxusr = clang_getCursorUSR (declaration);
  usr = clang_getCString (cxusr);
  linkage = clang_getCursorLinkage (declaration);

  if (linkage == CXLinkage_Internal || linkage == CXLinkage_NoLinkage || usr == NULL)
    ide_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "Failed to locate referenced cursor");
  else
    ide_task_return_pointer (task, g_strdup (usr), g_free);
}

void
ide_clang_get_index_key_async (IdeClang            *self,
                               const gchar         *path,
                               const gchar * const *argv,
                               guint                line,
                               guint                column,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autofree gchar *parent = NULL;
  GetIndexKey *state;

  g_return_if_fail (IDE_IS_CLANG (self));
  g_return_if_fail (path != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  state = g_slice_new0 (GetIndexKey);
  state->path = g_strdup (path);
  state->argv = ide_clang_cook_flags (path, argv);
  state->argc = state->argv ? g_strv_length (state->argv) : 0;
  state->line = line;
  state->column = column;

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_clang_get_index_key_async);
  ide_task_set_kind (task, IDE_TASK_KIND_INDEXER);
  ide_task_set_task_data (task, state, get_index_key_free);
  ide_task_run_in_thread (task, ide_clang_get_index_key_worker);
}

/**
 * ide_clang_get_index_key_finish:
 * @self: a #IdeClang
 *
 * Completes an async request to get the key for the symbol located
 * at a given source location.
 *
 * Returns: (transfer full): the key, or %NULL and @error is set
 */
gchar *
ide_clang_get_index_key_finish (IdeClang      *self,
                                GAsyncResult  *result,
                                GError       **error)
{
  g_return_val_if_fail (IDE_IS_CLANG (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

/* vim:set foldmethod=marker: */
