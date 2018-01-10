/* ide-clang-symbol-node.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-clang-symbol-node"

#include <clang-c/Index.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include "ide-clang-symbol-node.h"

struct _IdeClangSymbolNode
{
  IdeSymbolNode parent_instance;

  CXCursor  cursor;
  GArray   *children;
};

G_DEFINE_TYPE (IdeClangSymbolNode, ide_clang_symbol_node, IDE_TYPE_SYMBOL_NODE)

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
get_symbol_kind (CXCursor        cursor,
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

    case CXCursor_VarDecl:
      kind = IDE_SYMBOL_VARIABLE;
      break;

    default:
      break;
    }

  *flags = local_flags;

  return kind;
}

IdeClangSymbolNode *
_ide_clang_symbol_node_new (IdeContext *context,
                            CXCursor    cursor)
{
  IdeClangSymbolNode *self;
  IdeSymbolFlags flags = 0;
  IdeSymbolKind kind;
  CXString cxname;
  const gchar *name;

  kind = get_symbol_kind (cursor, &flags);
  cxname = clang_getCursorSpelling (cursor);
  name = clang_getCString (cxname);

  self = g_object_new (IDE_TYPE_CLANG_SYMBOL_NODE,
                       "context", context,
                       "kind", kind,
                       "flags", flags,
                       "name", ide_str_empty0 (name) ? _("anonymous") : name,
                       NULL);

  self->cursor = cursor;

  clang_disposeString (cxname);

  return self;
}

CXCursor
_ide_clang_symbol_node_get_cursor (IdeClangSymbolNode *self)
{
  g_return_val_if_fail (IDE_IS_CLANG_SYMBOL_NODE (self), clang_getNullCursor ());;

  return self->cursor;
}

static void
ide_clang_symbol_node_get_location_async (IdeSymbolNode       *symbol_node,
                                          GCancellable        *cancellable,
                                          GAsyncReadyCallback  callback,
                                          gpointer             user_data)
{
  IdeClangSymbolNode *self = (IdeClangSymbolNode *)symbol_node;
  IdeSourceLocation *ret;
  IdeContext *context;
  const gchar *filename;
  CXString cxfilename;
  CXSourceLocation cxloc;
  CXFile file;
  GFile *gfile;
  IdeFile *ifile;
  guint line = 0;
  guint line_offset = 0;
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (IDE_IS_CLANG_SYMBOL_NODE (self));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_clang_symbol_node_get_location_async);

  cxloc = clang_getCursorLocation (self->cursor);
  clang_getFileLocation (cxloc, &file, &line, &line_offset, NULL);
  cxfilename = clang_getFileName (file);
  filename = clang_getCString (cxfilename);

  /*
   * TODO: Remove IdeFile from all this junk.
   */

  context = ide_object_get_context (IDE_OBJECT (self));
  gfile = g_file_new_for_path (filename);
  ifile = g_object_new (IDE_TYPE_FILE,
                        "file", gfile,
                        "context", context,
                        NULL);

  ret = ide_source_location_new (ifile, line-1, line_offset-1, 0);

  g_clear_object (&ifile);
  g_clear_object (&gfile);
  clang_disposeString (cxfilename);

  g_task_return_pointer (task, ret, (GDestroyNotify)ide_source_location_unref);
}

static IdeSourceLocation *
ide_clang_symbol_node_get_location_finish (IdeSymbolNode  *symbol_node,
                                           GAsyncResult   *result,
                                           GError        **error)
{
  g_return_val_if_fail (IDE_IS_CLANG_SYMBOL_NODE (symbol_node), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
ide_clang_symbol_node_finalize (GObject *object)
{
  IdeClangSymbolNode *self = (IdeClangSymbolNode *)object;

  g_clear_pointer (&self->children, g_array_unref);

  G_OBJECT_CLASS (ide_clang_symbol_node_parent_class)->finalize (object);
}

static void
ide_clang_symbol_node_class_init (IdeClangSymbolNodeClass *klass)
{
  IdeSymbolNodeClass *node_class = IDE_SYMBOL_NODE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_clang_symbol_node_finalize;

  node_class->get_location_async = ide_clang_symbol_node_get_location_async;
  node_class->get_location_finish = ide_clang_symbol_node_get_location_finish;
}

static void
ide_clang_symbol_node_init (IdeClangSymbolNode *self)
{
}

GArray *
_ide_clang_symbol_node_get_children (IdeClangSymbolNode *self)
{
  g_return_val_if_fail (IDE_IS_CLANG_SYMBOL_NODE (self), NULL);

  return self->children;
}

void
_ide_clang_symbol_node_set_children (IdeClangSymbolNode *self,
                                     GArray             *children)
{
  g_return_if_fail (IDE_IS_CLANG_SYMBOL_NODE (self));
  g_return_if_fail (self->children == NULL);
  g_return_if_fail (children != NULL);

  self->children = g_array_ref (children);
}
