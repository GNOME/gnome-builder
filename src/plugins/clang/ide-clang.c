/* ide-clang.c
 *
 * Copyright 2018-2023 Christian Hergert <chergert@redhat.com>
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

/* Prologue {{{1 */

#define G_LOG_DOMAIN "ide-clang"

#include <libide-code.h>

#include "ide-clang.h"
#include "ide-clang-util.h"

#define IDE_CLANG_HIGHLIGHTER_TYPE          "c:type"
#define IDE_CLANG_HIGHLIGHTER_FUNCTION_NAME "def:function"
#define IDE_CLANG_HIGHLIGHTER_ENUM_NAME     "def:constant"
#define IDE_CLANG_HIGHLIGHTER_MACRO_NAME    "c:preprocessor"

#define PRIORITY_DIAGNOSE     (-200)
#define PRIORITY_COMPLETE     (-100)
#define PRIORITY_GET_LOCATION (-50)
#define PRIORITY_GET_SYMTREE  (50)
#define PRIORITY_FIND_SCOPE   (100)
#define PRIORITY_INDEX_FILE   (500)
#define PRIORITY_HIGHLIGHT    (300)

#if 0
# define PROBE G_STMT_START { g_printerr ("PROBE: %s\n", G_STRFUNC); } G_STMT_END
#else
# define PROBE G_STMT_START { } G_STMT_END
#endif

struct _IdeClang
{
  GObject     parent;
  GFile      *workdir;
  GHashTable *unsaved_files;
  CXIndex     index;
};

typedef struct
{
  struct CXUnsavedFile *files;
  GPtrArray            *bytes;
  GPtrArray            *paths;
  guint                 len;
} UnsavedFiles;

G_DEFINE_FINAL_TYPE (IdeClang, ide_clang, G_TYPE_OBJECT)

static GHashTable *unsupported_by_clang;
static GHashTable *auto_suffixes;

static void
unsaved_files_free (UnsavedFiles *uf)
{
  g_clear_pointer (&uf->files, g_free);
  g_clear_pointer (&uf->bytes, g_ptr_array_unref);
  g_clear_pointer (&uf->paths, g_ptr_array_unref);
  g_slice_free (UnsavedFiles, uf);
}

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

static gboolean
is_cplusplus_param (const gchar *param)
{
  /* Skip past -, -- */
  if (*param == '-')
    {
      param++;
      if (*param == '-')
        param++;
    }

  if (g_str_has_prefix (param, "std=") ||
      g_str_has_prefix (param, "x="))
    {
      /* Assume + means C++ of some sort */
      if (strchr (param, '+') != NULL)
        return TRUE;
    }

  return FALSE;
}

static gchar **
load_stdcpp_includes (void)
{
  GPtrArray *ar = g_ptr_array_new ();
  g_autofree gchar *stdout_buf = NULL;
  g_autoptr(GSubprocessLauncher) launcher = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autoptr(GError) error = NULL;
  g_auto(GStrv) lines = NULL;
  gboolean in_search_includes = FALSE;

  launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDIN_PIPE |
                                        G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                        G_SUBPROCESS_FLAGS_STDERR_MERGE);
  g_subprocess_launcher_set_cwd (launcher, g_get_home_dir ());
  subprocess = g_subprocess_launcher_spawn (launcher,
                                            &error,
                                            "clang++", "-v", "-x", "c++", "-E", "-",
                                            NULL);
  if (subprocess == NULL)
    goto skip_failure;

  if (!g_subprocess_communicate_utf8 (subprocess, "", NULL, &stdout_buf, NULL, &error))
    goto skip_failure;

  lines = g_strsplit (stdout_buf, "\n", 0);

  for (guint i = 0; lines[i] != NULL; i++)
    {
      if (g_str_equal (lines[i], "#include <...> search starts here:"))
        {
          in_search_includes = TRUE;
          continue;
        }

      if (!g_ascii_isspace (lines[i][0]))
        {
          in_search_includes = FALSE;
          continue;
        }

      if (in_search_includes)
        g_ptr_array_add (ar, g_strdup_printf ("-I%s", g_strstrip (lines[i])));
    }

skip_failure:
  g_ptr_array_add (ar, NULL);
  return (gchar **)g_ptr_array_free (ar, FALSE);
}

static const gchar * const *
get_stdcpp_includes (void)
{
  static gchar **stdcpp_includes;

  if (g_once_init_enter (&stdcpp_includes))
    g_once_init_leave (&stdcpp_includes, load_stdcpp_includes ());

  return (const gchar * const *)stdcpp_includes;
}

static gboolean
maybe_header (const char *path)
{
  const char *dot;

  if (path == NULL)
    return FALSE;

  if (!(dot = strrchr (path, '.')))
    return FALSE;

  return strcmp (dot, ".h") == 0 ||
         strcmp (dot, ".hh") == 0 ||
         strcmp (dot, ".hpp") == 0 ||
         strcmp (dot, ".h++") == 0 ||
         strcmp (dot, ".hxx") == 0;
}

static gchar **
ide_clang_cook_flags (const gchar         *path,
                      const gchar * const *flags)
{
  GPtrArray *cooked = g_ptr_array_new ();
  const gchar *llvm_flags = ide_clang_get_llvm_flags ();
  g_autofree gchar *include = NULL;
  gboolean is_cplusplus = FALSE;
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
          const char *lookup = flags[i];

          is_cplusplus |= is_cplusplus_param (flags[i]);

          if (g_str_has_prefix (flags[i], "-Werror="))
            lookup = flags[i] + strlen ("-Werror=");
          else if (g_str_has_prefix (flags[i], "-Wno-error="))
            lookup = flags[i] + strlen ("-Wno-error=");
          else if (g_str_has_prefix (flags[i], "-W"))
            lookup = flags[i] + strlen ("-W");
          else
            lookup = NULL;

          if (lookup && g_hash_table_contains (unsupported_by_clang, lookup))
            continue;

          g_ptr_array_add (cooked, g_strdup (flags[i]));

          if (g_strcmp0 (include, flags[i]) == 0)
            g_clear_pointer (&include, g_free);
        }
    }

  /* Make sure we always include -xc++ if we think this is a C++ file */
  if (!is_cplusplus && ide_path_is_cpp_like (path))
    g_ptr_array_insert (cooked, pos++, g_strdup ("-xc++"));

  /* Insert -Idirname as first include if we didn't find it in the list of
   * include paths from the request. That ensures we have something that is
   * very similar to what clang would do unless they specified the path
   * somewhere else.
   */
  if (include != NULL)
    g_ptr_array_insert (cooked, pos++, g_steal_pointer (&include));

  /* See if we need to add the C++ standard library */
  if (is_cplusplus || ide_path_is_cpp_like (path))
    {
      const gchar * const *stdcpp_includes = get_stdcpp_includes ();

      for (guint i = 0; stdcpp_includes[i] != NULL; i++)
        g_ptr_array_insert (cooked, pos++, g_strdup (stdcpp_includes[i]));
    }

  /* If this looks like a header, set -Wno-unused-function so that we
   * don't get warnings "static inline" not being used. Set it last so
   * that it applies after -Wall, etc.
   *
   * https://gitlab.gnome.org/GNOME/gnome-builder/-/issues/961
   */
  if (maybe_header (path))
    g_ptr_array_add (cooked, g_strdup ("-Wno-unused-function"));

  g_ptr_array_add (cooked, NULL);

  return (gchar **)g_ptr_array_free (cooked, FALSE);
}

static UnsavedFiles *
ide_clang_get_unsaved_files (IdeClang *self)
{
  UnsavedFiles *ret;
  GHashTableIter iter;
  const gchar *path;
  GArray *ufs;
  GBytes *bytes;

  g_assert (IDE_IS_CLANG (self));

  ret = g_slice_new0 (UnsavedFiles);
  ret->len = g_hash_table_size (self->unsaved_files);
  ret->bytes = g_ptr_array_new_full (ret->len, (GDestroyNotify)g_bytes_unref);
  ret->paths = g_ptr_array_new_full (ret->len, g_free);

  ufs = g_array_new (FALSE, FALSE, sizeof (struct CXUnsavedFile));

  g_hash_table_iter_init (&iter, self->unsaved_files);

  while (g_hash_table_iter_next (&iter, (gpointer *)&path, (gpointer *)&bytes))
    {
      struct CXUnsavedFile uf;
      const guint8 *data;
      gsize len;

      data = g_bytes_get_data (bytes, &len);

      uf.Filename = g_strdup (path);
      uf.Contents = (const gchar *)data;
      uf.Length = len;

      g_ptr_array_add (ret->bytes, g_bytes_ref (bytes));
      g_ptr_array_add (ret->paths, (gchar *)uf.Filename);
      g_array_append_val (ufs, uf);
    }

  ret->files = (struct CXUnsavedFile *)(gpointer)g_array_free (ufs, FALSE);

  return g_steal_pointer (&ret);
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
      kind = IDE_SYMBOL_KIND_STRUCT;
      break;

    case CXCursor_UnionDecl:
      kind = IDE_SYMBOL_KIND_UNION;
      break;

    case CXCursor_ClassDecl:
      kind = IDE_SYMBOL_KIND_CLASS;
      break;

    case CXCursor_FunctionDecl:
      kind = IDE_SYMBOL_KIND_FUNCTION;
      break;

    case CXCursor_EnumDecl:
      kind = IDE_SYMBOL_KIND_ENUM;
      break;

    case CXCursor_EnumConstantDecl:
      kind = IDE_SYMBOL_KIND_ENUM_VALUE;
      break;

    case CXCursor_FieldDecl:
      kind = IDE_SYMBOL_KIND_FIELD;
      break;

    case CXCursor_InclusionDirective:
      kind = IDE_SYMBOL_KIND_HEADER;
      break;

    case CXCursor_VarDecl:
      kind = IDE_SYMBOL_KIND_VARIABLE;
      break;

    case CXCursor_NamespaceAlias:
      kind = IDE_SYMBOL_KIND_NAMESPACE;
      break;

    case CXCursor_CXXMethod:
    case CXCursor_Destructor:
    case CXCursor_Constructor:
      kind = IDE_SYMBOL_KIND_METHOD;
      break;

    case CXCursor_MacroDefinition:
    case CXCursor_MacroExpansion:
      kind = IDE_SYMBOL_KIND_MACRO;
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
  g_autoptr(IdeLocation) srcloc = NULL;
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

  if (line) line--;
  if (column) column--;

  srcloc = ide_location_new (gfile, line, column);
  cxname = clang_getCursorSpelling (cursor);
  symkind = ide_clang_get_symbol_kind (cursor, &symflags);

  return ide_symbol_new (clang_getCString (cxname), symkind, symflags, srcloc, srcloc);
}

static void
ide_clang_finalize (GObject *object)
{
  IdeClang *self = (IdeClang *)object;

  g_clear_object (&self->workdir);
  g_clear_pointer (&self->unsaved_files, g_hash_table_unref);
  g_clear_pointer (&self->index, clang_disposeIndex);

  G_OBJECT_CLASS (ide_clang_parent_class)->finalize (object);
}

static void
ide_clang_class_init (IdeClangClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_clang_finalize;

  unsupported_by_clang = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_add (unsupported_by_clang, (char *)"strict-null-sentinel");
  g_hash_table_add (unsupported_by_clang, (char *)"logical-op");
  g_hash_table_add (unsupported_by_clang, (char *)"no-dangling-pointer");
  g_hash_table_add (unsupported_by_clang, (char *)"maybe-uninitialized");
  g_hash_table_add (unsupported_by_clang, (char *)"no-stringop-overflow");

  auto_suffixes = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_add (auto_suffixes, (char *)"_auto");
  g_hash_table_add (auto_suffixes, (char *)"_autolist");
  g_hash_table_add (auto_suffixes, (char *)"_autoptr");
  g_hash_table_add (auto_suffixes, (char *)"_autoqueue");
  g_hash_table_add (auto_suffixes, (char *)"_autoslist");
}

static void
ide_clang_init (IdeClang *self)
{
  self->index = clang_createIndex (0, 0);
  self->unsaved_files = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                               (GDestroyNotify)g_bytes_unref);
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
    case IDE_SYMBOL_KIND_FUNCTION:
      return "f\x1F";

    case IDE_SYMBOL_KIND_STRUCT:
      return "s\x1F";

    case IDE_SYMBOL_KIND_VARIABLE:
      return "v\x1F";

    case IDE_SYMBOL_KIND_UNION:
      return "u\x1F";

    case IDE_SYMBOL_KIND_ENUM:
      return "e\x1F";

    case IDE_SYMBOL_KIND_CLASS:
      return "c\x1F";

    case IDE_SYMBOL_KIND_ENUM_VALUE:
      return "a\x1F";

    case IDE_SYMBOL_KIND_MACRO:
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

  if (ide_str_equal0 (path, state->path))
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
  IdeSymbolKind kind = IDE_SYMBOL_KIND_NONE;
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
  if (ide_str_empty0 (cname))
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
                           g_ptr_array_unref);
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

  PROBE;

  g_return_if_fail (IDE_IS_CLANG (self));
  g_return_if_fail (path != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  /* We don't use unsaved files here, because we only want to index
   * the files on disk.
   */

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
  ide_task_set_priority (task, PRIORITY_INDEX_FILE);
  ide_task_run_in_thread (task, ide_clang_index_file_worker);
}

/**
 * ide_clang_index_file_finish:
 * @self: a #IdeClang
 *
 * Finishes a request to index a file.
 *
 * Returns: (transfer full): a #GPtrArray of #IdeCodeIndexEntry
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
  CXIndex       index;
  UnsavedFiles *ufs;
  GPtrArray    *diagnostics;
  GFile        *workdir;
  gchar        *path;
  gchar       **argv;
  guint         argc;
} Diagnose;

static void
diagnose_free (gpointer data)
{
  Diagnose *state = data;

  g_clear_pointer (&state->ufs, unsaved_files_free);
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

  if (g_path_is_absolute (path))
    return g_strdup (path);

  file = g_file_new_for_path (path);
  if (g_file_has_prefix (file, workdir))
    return g_strdup (path);

  child = g_file_get_child (workdir, path);

  return path_or_uri (child);
}

static IdeLocation *
create_location (GFile             *workdir,
                 CXSourceLocation   cxloc,
                 IdeLocation *alternate)
{
  g_autofree gchar *path = NULL;
  g_autoptr(GFile) file = NULL;
  g_auto(CXString) str = {0};
  CXFile cxfile = NULL;
  unsigned line;
  unsigned column;
  unsigned offset;

  g_assert (G_IS_FILE (workdir));

  clang_getFileLocation (cxloc, &cxfile, &line, &column, &offset);

  str = clang_getFileName (cxfile);

  if (line == 0 || clang_getCString (str) == NULL)
    return alternate ? g_object_ref (alternate) : NULL;

  if (line > 0)
    line--;

  if (column > 0)
    column--;

  path = get_path (workdir, clang_getCString (str));
  file = g_file_new_for_path (path);

  return ide_location_new (file, line, column);
}

static IdeRange *
create_range (GFile         *workdir,
              CXSourceRange  cxrange)
{
  IdeRange *range = NULL;
  CXSourceLocation cxbegin;
  CXSourceLocation cxend;
  g_autoptr(IdeLocation) begin = NULL;
  g_autoptr(IdeLocation) end = NULL;

  g_assert (G_IS_FILE (workdir));

  cxbegin = clang_getRangeStart (cxrange);
  cxend = clang_getRangeEnd (cxrange);

  /* Sometimes the end location does not have a file associated with it,
   * so we force it to have the GFile of the first location.
   */
  begin = create_location (workdir, cxbegin, NULL);
  end = create_location (workdir, cxend, begin);

  if ((begin != NULL) && (end != NULL))
    range = ide_range_new (begin, end);

  return range;
}

static IdeDiagnostic *
create_diagnostic (GFile        *workdir,
                   GFile        *target,
                   CXDiagnostic *cxdiag)
{
  g_autoptr(IdeLocation) loc = NULL;
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
      IdeRange *range;

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

  code = clang_parseTranslationUnit2 (state->index,
                                      state->path,
                                      (const char * const *)state->argv,
                                      state->argc,
                                      state->ufs->files,
                                      state->ufs->len,
                                      options,
                                      &unit);

  if (code != CXError_Success)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "Failed to diagnose file \"%s\", exited with code %d",
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
                           g_ptr_array_unref);
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

  PROBE;

  g_return_if_fail (IDE_IS_CLANG (self));
  g_return_if_fail (path != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  state = g_slice_new0 (Diagnose);
  state->index = self->index;
  state->ufs = ide_clang_get_unsaved_files (self);
  state->path = g_strdup (path);
  state->argv = ide_clang_cook_flags (path, argv);
  state->argc = state->argv ? g_strv_length (state->argv) : 0;
  state->diagnostics = g_ptr_array_new ();

  if (self->workdir != NULL)
    state->workdir = g_object_ref (self->workdir);
  else
    state->workdir = g_file_new_for_path ((parent = g_path_get_dirname (path)));

  IDE_PTR_ARRAY_SET_FREE_FUNC (state->diagnostics, ide_object_unref_and_destroy);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_clang_diagnose_async);
  ide_task_set_kind (task, IDE_TASK_KIND_COMPILER);
  ide_task_set_task_data (task, state, diagnose_free);
  ide_task_set_priority (task, PRIORITY_DIAGNOSE);
  ide_task_run_in_thread (task, ide_clang_diagnose_worker);
}

/**
 * ide_clang_diagnose_finish:
 *
 * Finishes a request to diagnose a file.
 *
 * Returns: (transfer full) (element-type Ide.Diagnostic):
 *   a #GPtrArray of #IdeDiagnostic
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

  return IDE_PTR_ARRAY_STEAL_FULL (&ret);
}

/* Completion {{{1 */

typedef struct
{
  CXIndex        index;
  UnsavedFiles  *ufs;
  gchar         *path;
  gchar        **argv;
  gint           argc;
  guint          line;
  guint          column;
} Complete;

static void
complete_free (gpointer data)
{
  Complete *state = data;

  g_clear_pointer (&state->ufs, unsaved_files_free);
  g_clear_pointer (&state->path, g_free);
  g_clear_pointer (&state->argv, g_strfreev);
  g_slice_free (Complete, state);
}

static void
ide_clang_build_completion (GVariantBuilder    *builder,
                            CXCompletionResult *result)
{
  GVariantBuilder chunks_builder;
  g_autofree gchar *typed_text = NULL;
  g_auto(CXString) comment = {0};
  const gchar *comment_cstr;
  guint n_chunks;

  g_assert (builder != NULL);
  g_assert (result != NULL);

  g_variant_builder_add_parsed (builder, "{%s,<%u>}", "kind", result->CursorKind);

  comment = clang_getCompletionBriefComment (result->CompletionString);
  comment_cstr = clang_getCString (comment);
  if (comment_cstr && *comment_cstr)
    g_variant_builder_add_parsed (builder, "{%s,<%s>}", "comment", comment_cstr);

  if (clang_getCompletionAvailability (result->CompletionString))
    g_variant_builder_add_parsed (builder, "{%s,<%u>}", "avail",
                                  clang_getCompletionAvailability (result->CompletionString));

  n_chunks = clang_getNumCompletionChunks (result->CompletionString);

  g_variant_builder_init (&chunks_builder, G_VARIANT_TYPE ("aa{sv}"));

  for (guint i = 0; i < n_chunks; i++)
    {
      g_auto(CXString) str = clang_getCompletionChunkText (result->CompletionString, i);
      const gchar *text = clang_getCString (str);
      guint kind = clang_getCompletionChunkKind (result->CompletionString, i);

      g_variant_builder_open (&chunks_builder, G_VARIANT_TYPE_VARDICT);

      if (kind == CXCompletionChunk_TypedText && typed_text == NULL)
        {
          const char *bar = strrchr (text, '_');

          typed_text = g_utf8_casefold (text, -1);

          /* Convert Foo_autoptr into g_autoptr(Foo) but don't touch
           * things like "g_autoptr (TypeName)" where we have "g_autotptr"
           * as the typed text.
           */
          if (!g_str_has_prefix (text, "g_auto") &&
              (bar = strrchr (text, '_')) &&
              g_hash_table_contains (auto_suffixes, bar))
            {
              GString *string = g_string_new ("g");
              g_string_append (string, bar);
              g_string_append_c (string, '(');
              g_string_append_len (string, text, strlen (text) - strlen (bar));
              g_string_append_c (string, ')');
              g_variant_builder_add_parsed (&chunks_builder, "{%s,<%s>}", "text", string->str);
              g_string_free (string, TRUE);
            }
          else
            {
              g_variant_builder_add_parsed (&chunks_builder, "{%s,<%s>}", "text", text);
            }
        }
      else
        {
          g_variant_builder_add_parsed (&chunks_builder, "{%s,<%s>}", "text", text);
        }

      g_variant_builder_add_parsed (&chunks_builder, "{%s,<%u>}", "kind", kind);
      g_variant_builder_close (&chunks_builder);
    }

  if (typed_text != NULL)
    g_variant_builder_add_parsed (builder, "{%s,<%s>}", "keyword", typed_text);

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
  GVariantBuilder builder;
  enum CXErrorCode code;
  unsigned options;

  g_assert (IDE_IS_TASK (task));
  g_assert (IDE_IS_CLANG (source_object));
  g_assert (state != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  options = clang_defaultEditingTranslationUnitOptions ();

  code = clang_parseTranslationUnit2 (state->index,
                                      state->path,
                                      (const char * const *)state->argv,
                                      state->argc,
                                      state->ufs->files,
                                      state->ufs->len,
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
                                  state->ufs->files,
                                  state->ufs->len,
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
                           g_variant_unref);
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
  Complete *state;

  PROBE;

  g_return_if_fail (IDE_IS_CLANG (self));
  g_return_if_fail (path != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  state = g_slice_new0 (Complete);
  state->index = self->index;
  state->ufs = ide_clang_get_unsaved_files (self);
  state->path = g_strdup (path);
  state->argv = ide_clang_cook_flags (path, argv);
  state->argc = state->argv ? g_strv_length (state->argv) : 0;
  state->line = line;
  state->column = column;

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_check_cancellable (task, FALSE);
  ide_task_set_source_tag (task, ide_clang_complete_async);
  ide_task_set_kind (task, IDE_TASK_KIND_COMPILER);
  ide_task_set_task_data (task, state, complete_free);
  ide_task_set_priority (task, PRIORITY_COMPLETE);
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
  CXIndex        index;
  UnsavedFiles  *ufs;
  gchar         *path;
  gchar        **argv;
  gint           argc;
  guint          line;
  guint          column;
} FindNearestScope;

static void
find_nearest_scope_free (gpointer data)
{
  FindNearestScope *state = data;

  g_clear_pointer (&state->ufs, unsaved_files_free);
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

  code = clang_parseTranslationUnit2 (state->index,
                                      state->path,
                                      (const char * const *)state->argv,
                                      state->argc,
                                      state->ufs->files,
                                      state->ufs->len,
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
                             g_object_unref);
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

  PROBE;

  g_return_if_fail (IDE_IS_CLANG (self));
  g_return_if_fail (path != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  state = g_slice_new0 (FindNearestScope);
  state->index = self->index;
  state->ufs = ide_clang_get_unsaved_files (self);
  state->path = g_strdup (path);
  state->argv = ide_clang_cook_flags (path, argv);
  state->argc = state->argv ? g_strv_length (state->argv) : 0;
  state->line = line;
  state->column = column;

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_clang_find_nearest_scope_async);
  ide_task_set_kind (task, IDE_TASK_KIND_COMPILER);
  ide_task_set_task_data (task, state, find_nearest_scope_free);
  ide_task_set_priority (task, PRIORITY_FIND_SCOPE);
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
  CXIndex        index;
  UnsavedFiles  *ufs;
  GFile         *workdir;
  gchar         *path;
  gchar        **argv;
  gint           argc;
  guint          line;
  guint          column;
} LocateSymbol;

static void
locate_symbol_free (gpointer data)
{
  LocateSymbol *state = data;

  g_clear_pointer (&state->ufs, unsaved_files_free);
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
  g_autoptr(IdeLocation) declaration = NULL;
  g_autoptr(IdeLocation) definition = NULL;
  g_autoptr(IdeSymbol) ret = NULL;
  g_auto(CXTranslationUnit) unit = NULL;
  g_auto(CXString) cxstr = {0};
  CXSourceLocation cxlocation;
  enum CXErrorCode code;
  IdeSymbolFlags symflags;
  IdeSymbolKind symkind;
  CXCursor cursor;
  CXCursor tmpcursor;
  CXFile cxfile;
  unsigned options;

  g_assert (IDE_IS_TASK (task));
  g_assert (IDE_IS_CLANG (source_object));
  g_assert (state != NULL);
  g_assert (state->path != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  options = clang_defaultEditingTranslationUnitOptions ()
          | CXTranslationUnit_DetailedPreprocessingRecord;

  code = clang_parseTranslationUnit2 (state->index,
                                      state->path,
                                      (const char * const *)state->argv,
                                      state->argc,
                                      state->ufs->files,
                                      state->ufs->len,
                                      options,
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

  if (symkind == IDE_SYMBOL_KIND_HEADER)
    {
      g_auto(CXString) included_file_name = {0};
      CXFile included_file;
      const gchar *path;

      included_file = clang_getIncludedFile (cursor);
      included_file_name = clang_getFileName (included_file);
      path = clang_getCString (included_file_name);

      if (path != NULL)
        {
          g_autoptr(GFile) file = g_file_new_for_path (path);

          g_clear_object (&definition);
          declaration = ide_location_new (file, -1, -1);
        }
    }

  cxstr = clang_getCursorDisplayName (cursor);
  ret = ide_symbol_new (clang_getCString (cxstr), symkind, symflags, declaration, definition);

  ide_task_return_pointer (task,
                           g_steal_pointer (&ret),
                           g_object_unref);
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

  PROBE;

  g_return_if_fail (IDE_IS_CLANG (self));
  g_return_if_fail (path != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  state = g_slice_new0 (LocateSymbol);
  state->index = self->index;
  state->ufs = ide_clang_get_unsaved_files (self);
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
  ide_task_set_priority (task, PRIORITY_GET_LOCATION);
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
  CXIndex          index;
  UnsavedFiles    *ufs;
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

  g_clear_pointer (&state->ufs, unsaved_files_free);
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
        ret = ide_str_equal0 (clang_getCString (filename), state->path);
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
  g_auto(CXTranslationUnit) unit = NULL;
  GVariantBuilder builder;
  enum CXErrorCode code;
  CXCursor cursor;

  g_assert (IDE_IS_TASK (task));
  g_assert (IDE_IS_CLANG (source_object));
  g_assert (state != NULL);
  g_assert (state->path != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  code = clang_parseTranslationUnit2 (state->index,
                                      state->path,
                                      (const char * const *)state->argv,
                                      state->argc,
                                      state->ufs->files,
                                      state->ufs->len,
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

  ide_task_return_pointer (task, g_steal_pointer (&ret), g_variant_unref);
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

  PROBE;

  g_return_if_fail (IDE_IS_CLANG (self));
  g_return_if_fail (path != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  state = g_slice_new0 (GetSymbolTree);
  state->index = self->index;
  state->ufs = ide_clang_get_unsaved_files (self);
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
  ide_task_set_priority (task, PRIORITY_GET_SYMTREE);
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
  CXIndex        index;
  UnsavedFiles  *ufs;
  GFile         *workdir;
  gchar         *path;
  gchar        **argv;
  gint           argc;
} GetHighlightIndex;

static void
get_highlight_index_free (gpointer data)
{
  GetHighlightIndex *state = data;

  g_clear_pointer (&state->ufs, unsaved_files_free);
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
  g_auto(CXTranslationUnit) unit = NULL;
  enum CXErrorCode code;
  unsigned options;
  CXCursor cursor;

  g_assert (IDE_IS_TASK (task));
  g_assert (IDE_IS_CLANG (source_object));
  g_assert (state != NULL);
  g_assert (state->path != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  options = clang_defaultEditingTranslationUnitOptions ()
#if CINDEX_VERSION >= CINDEX_VERSION_ENCODE(0, 35)
          | CXTranslationUnit_KeepGoing
#endif
          | CXTranslationUnit_DetailedPreprocessingRecord;

  code = clang_parseTranslationUnit2 (state->index,
                                      state->path,
                                      (const char * const *)state->argv,
                                      state->argc,
                                      state->ufs->files,
                                      state->ufs->len,
                                      options,
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
    ide_highlight_index_insert (highlight, common_defines[i], (gpointer)"c:common-defines");
  ide_highlight_index_insert (highlight, "TRUE", (gpointer)"c:boolean");
  ide_highlight_index_insert (highlight, "FALSE", (gpointer)"c:boolean");
  ide_highlight_index_insert (highlight, "g_autoptr", (gpointer)"c:storage-class");
  ide_highlight_index_insert (highlight, "g_autolist", (gpointer)"c:storage-class");
  ide_highlight_index_insert (highlight, "g_autoslist", (gpointer)"c:storage-class");
  ide_highlight_index_insert (highlight, "g_autoqueue", (gpointer)"c:storage-class");
  ide_highlight_index_insert (highlight, "g_auto", (gpointer)"c:storage-class");
  ide_highlight_index_insert (highlight, "g_autofree", (gpointer)"c:storage-class");

  cursor = clang_getTranslationUnitCursor (unit);
  clang_visitChildren (cursor, build_index_visitor, highlight);

  ide_task_return_pointer (task,
                           g_steal_pointer (&highlight),
                           ide_highlight_index_unref);
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

  PROBE;

  g_return_if_fail (IDE_IS_CLANG (self));
  g_return_if_fail (path != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  state = g_slice_new0 (GetHighlightIndex);
  state->index = self->index;
  state->ufs = ide_clang_get_unsaved_files (self);
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
  ide_task_set_priority (task, PRIORITY_HIGHLIGHT);
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
  CXIndex        index;
  UnsavedFiles  *ufs;
  gchar         *path;
  gchar        **argv;
  gint           argc;
  guint          line;
  guint          column;
} GetIndexKey;

static void
get_index_key_free (gpointer data)
{
  GetIndexKey *state = data;

  g_clear_pointer (&state->ufs, unsaved_files_free);
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

  code = clang_parseTranslationUnit2 (state->index,
                                      state->path,
                                      (const char * const *)state->argv,
                                      state->argc,
                                      state->ufs->files,
                                      state->ufs->len,
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
  GetIndexKey *state;

  PROBE;

  g_return_if_fail (IDE_IS_CLANG (self));
  g_return_if_fail (path != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  state = g_slice_new0 (GetIndexKey);
  state->index = self->index;
  state->ufs = ide_clang_get_unsaved_files (self);
  state->path = g_strdup (path);
  state->argv = ide_clang_cook_flags (path, argv);
  state->argc = state->argv ? g_strv_length (state->argv) : 0;
  state->line = line;
  state->column = column;

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_clang_get_index_key_async);
  ide_task_set_kind (task, IDE_TASK_KIND_INDEXER);
  ide_task_set_task_data (task, state, get_index_key_free);
  ide_task_set_priority (task, PRIORITY_GET_LOCATION);
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

/* Set Unsaved File {{{1 */

void
ide_clang_set_unsaved_file (IdeClang *self,
                            GFile    *file,
                            GBytes   *bytes)
{
  g_autofree gchar *path = NULL;

  g_return_if_fail (IDE_IS_CLANG (self));
  g_return_if_fail (G_IS_FILE (file));

  if (!g_file_is_native (file))
    return;

  path = g_file_get_path (file);

  if (bytes == NULL)
    g_hash_table_remove (self->unsaved_files, path);
  else
    g_hash_table_insert (self->unsaved_files, g_steal_pointer (&path), g_bytes_ref (bytes));
}

/* vim:set foldmethod=marker: */
