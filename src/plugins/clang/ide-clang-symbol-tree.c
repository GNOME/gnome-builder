/* ide-clang-symbol-tree.c
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

#define G_LOG_DOMAIN "ide-clang-symbol-tree"

#include <clang-c/Index.h>
#include <glib/gi18n.h>

#include "ide-clang-private.h"
#include "ide-clang-symbol-node.h"
#include "ide-clang-symbol-tree.h"

struct _IdeClangSymbolTree
{
  GObject    parent_instance;

  IdeRefPtr *native;
  GFile     *file;
  gchar     *path;
  GArray    *children;
};

typedef struct
{
  const gchar   *path;
  GArray        *children;
} TraversalState;

static void symbol_tree_iface_init (IdeSymbolTreeInterface *iface);

G_DEFINE_TYPE_WITH_CODE (IdeClangSymbolTree, ide_clang_symbol_tree, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_SYMBOL_TREE, symbol_tree_iface_init))

enum {
  PROP_0,
  PROP_FILE,
  PROP_NATIVE,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

/**
 * ide_clang_symbol_tree_get_file:
 * @self: an #IdeClangSymbolTree.
 *
 * Gets the #IdeClangSymbolTree:file property.
 *
 * Returns: (transfer none): a #GFile.
 */
GFile *
ide_clang_symbol_tree_get_file (IdeClangSymbolTree *self)
{
  g_return_val_if_fail (IDE_IS_CLANG_SYMBOL_TREE (self), NULL);

  return self->file;
}

static void
ide_clang_symbol_tree_set_file (IdeClangSymbolTree *self,
                                GFile              *file)
{
  g_return_if_fail (IDE_IS_CLANG_SYMBOL_TREE (self));
  g_return_if_fail (G_IS_FILE (file));

  self->file = g_object_ref (file);
  self->path = g_file_get_path (file);
}

static gboolean
cursor_is_recognized (TraversalState *state,
                      CXCursor        cursor)
{
  CXString filename;
  CXSourceLocation cxloc;
  CXFile file;
  enum CXCursorKind kind;
  gboolean ret = FALSE;

  kind = clang_getCursorKind (cursor);

  switch ((int)kind)
    {
    /*
     * TODO: Support way more CXCursorKind.
     */

    case CXCursor_ClassDecl:
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
      cxloc = clang_getCursorLocation (cursor);
      clang_getFileLocation (cxloc, &file, NULL, NULL, NULL);
      filename = clang_getFileName (file);
      ret = dzl_str_equal0 (clang_getCString (filename), state->path);
      clang_disposeString (filename);
      break;

    default:
      break;
    }

  return ret;
}

static enum CXChildVisitResult
count_recognizable_children (CXCursor     cursor,
                             CXCursor     parent,
                             CXClientData user_data)
{
  TraversalState *state = user_data;

  if (cursor_is_recognized (state, cursor))
    g_array_append_val (state->children, cursor);

  return CXChildVisit_Continue;
}

static guint
ide_clang_symbol_tree_get_n_children (IdeSymbolTree *symbol_tree,
                                      IdeSymbolNode *parent)
{
  IdeClangSymbolTree *self = (IdeClangSymbolTree *)symbol_tree;
  CXTranslationUnit tu;
  CXCursor cursor;
  TraversalState state = { 0 };
  GArray *children = NULL;
  guint count;

  g_return_val_if_fail (IDE_IS_CLANG_SYMBOL_TREE (self), 0);
  g_return_val_if_fail (!parent || IDE_IS_CLANG_SYMBOL_NODE (parent), 0);
  g_return_val_if_fail (self->native != NULL, 0);

  if (parent == NULL)
    children = self->children;
  else
    children = _ide_clang_symbol_node_get_children (IDE_CLANG_SYMBOL_NODE (parent));

  if (children != NULL)
    return children->len;

  if (parent == NULL)
    {
      tu = ide_ref_ptr_get (self->native);
      cursor = clang_getTranslationUnitCursor (tu);
    }
  else
    {
      cursor = _ide_clang_symbol_node_get_cursor (IDE_CLANG_SYMBOL_NODE (parent));
    }

  children = g_array_new (FALSE, FALSE, sizeof (CXCursor));

  state.path = self->path;
  state.children = children;

  clang_visitChildren (cursor,
                       count_recognizable_children,
                       &state);

  if (parent == NULL)
    self->children = g_array_ref (children);
  else
    _ide_clang_symbol_node_set_children (IDE_CLANG_SYMBOL_NODE (parent), children);

  count = children->len;

  g_array_unref (children);

  return count;
}

static IdeSymbolNode *
ide_clang_symbol_tree_get_nth_child (IdeSymbolTree *symbol_tree,
                                     IdeSymbolNode *parent,
                                     guint          nth)
{
  IdeClangSymbolTree *self = (IdeClangSymbolTree *)symbol_tree;
  IdeContext *context;
  GArray *children;

  g_return_val_if_fail (IDE_IS_CLANG_SYMBOL_TREE (self), NULL);
  g_return_val_if_fail (!parent || IDE_IS_SYMBOL_NODE (parent), NULL);

  context = ide_object_get_context (IDE_OBJECT (self));

  if (parent == NULL)
    children = self->children;
  else
    children = _ide_clang_symbol_node_get_children (IDE_CLANG_SYMBOL_NODE (parent));

  g_assert (children != NULL);

  if (nth < children->len)
    {
      CXCursor cursor;

      cursor = g_array_index (children, CXCursor, nth);
      return _ide_clang_symbol_node_new (context, cursor);
    }

  g_warning ("nth child %u is out of bounds", nth);

  return NULL;
}

static void
ide_clang_symbol_tree_finalize (GObject *object)
{
  IdeClangSymbolTree *self = (IdeClangSymbolTree *)object;

  g_clear_pointer (&self->native, ide_ref_ptr_unref);
  g_clear_pointer (&self->children, g_array_unref);
  g_clear_pointer (&self->path, g_free);

  G_OBJECT_CLASS (ide_clang_symbol_tree_parent_class)->finalize (object);
}

static void
ide_clang_symbol_tree_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  IdeClangSymbolTree *self = IDE_CLANG_SYMBOL_TREE (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, ide_clang_symbol_tree_get_file (self));
      break;

    case PROP_NATIVE:
      g_value_set_boxed (value, self->native);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_clang_symbol_tree_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  IdeClangSymbolTree *self = IDE_CLANG_SYMBOL_TREE (object);

  switch (prop_id)
    {
    case PROP_FILE:
      ide_clang_symbol_tree_set_file (self, g_value_get_object (value));
      break;

    case PROP_NATIVE:
      self->native = g_value_dup_boxed (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_clang_symbol_tree_class_init (IdeClangSymbolTreeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_clang_symbol_tree_finalize;
  object_class->get_property = ide_clang_symbol_tree_get_property;
  object_class->set_property = ide_clang_symbol_tree_set_property;

  properties [PROP_FILE] =
    g_param_spec_object ("file",
                         "File",
                         "File",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_NATIVE] =
    g_param_spec_boxed ("native",
                        "Native",
                        "Native",
                        IDE_TYPE_REF_PTR,
                        (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
ide_clang_symbol_tree_init (IdeClangSymbolTree *self)
{
}

static void
symbol_tree_iface_init (IdeSymbolTreeInterface *iface)
{
  iface->get_n_children = ide_clang_symbol_tree_get_n_children;
  iface->get_nth_child = ide_clang_symbol_tree_get_nth_child;
}
