/* gbp-gjs-symbol-node.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-gjs-symbol-node"

#include "config.h"

#include "gbp-gjs-symbol-node.h"

#define GET_STRING(n)     (n ? json_node_get_string(n) : NULL)
#define GET_NODE(o,...)   (get_node_at_path (o, __VA_ARGS__, NULL))
#define GET_OBJECT(o,...) (get_object(get_node_at_path (o, __VA_ARGS__, NULL)))

struct _GbpGjsSymbolNode
{
  IdeSymbolNode parent_instance;

  GPtrArray *children;

  guint line;
  guint line_offset;
};

static void gbp_gjs_symbol_node_load_children (GbpGjsSymbolNode *self,
                                               JsonNode         *children);

G_DEFINE_FINAL_TYPE (GbpGjsSymbolNode, gbp_gjs_symbol_node, IDE_TYPE_SYMBOL_NODE)

static inline JsonObject *
get_object (JsonNode *node)
{
  if (node == NULL)
    return NULL;
  if (JSON_NODE_HOLDS_OBJECT (node))
    return json_node_get_object (node);
  return NULL;
}

static inline void
gbp_gjs_symbol_node_set_kind (GbpGjsSymbolNode *self,
                              IdeSymbolKind     kind)
{
  g_object_set (self, "kind", kind, NULL);
}

static inline void
gbp_gjs_symbol_node_set_name (GbpGjsSymbolNode *self,
                              const char       *name)
{
  g_object_set (self, "name", name, NULL);
}

static void
gbp_gjs_symbol_node_take_child (GbpGjsSymbolNode *self,
                                GbpGjsSymbolNode *child)
{
  g_assert (GBP_IS_GJS_SYMBOL_NODE (self));
  g_assert (GBP_IS_GJS_SYMBOL_NODE (child));

  if (self->children == NULL)
    self->children = g_ptr_array_new_with_free_func (g_object_unref);

  g_ptr_array_add (self->children, child);
}

static void
gbp_gjs_symbol_node_dispose (GObject *object)
{
  GbpGjsSymbolNode *self = (GbpGjsSymbolNode *)object;

  g_clear_pointer (&self->children, g_ptr_array_unref);

  G_OBJECT_CLASS (gbp_gjs_symbol_node_parent_class)->dispose (object);
}

static void
gbp_gjs_symbol_node_class_init (GbpGjsSymbolNodeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gbp_gjs_symbol_node_dispose;
}

static void
gbp_gjs_symbol_node_init (GbpGjsSymbolNode *self)
{
}

G_GNUC_NULL_TERMINATED
static JsonNode *
get_node_at_path (JsonObject *object,
                  const char *first_child,
                  ...)
{
  const char *child = first_child;
  JsonNode *ret = NULL;
  va_list args;

  va_start (args, first_child);
  while (child != NULL)
    {
      JsonNode *node = json_object_get_member (object, child);
      child = va_arg (args, const char *);

      if (node == NULL)
        break;

      if (child == NULL && node != NULL)
        {
          ret = node;
          break;
        }

      if (!JSON_NODE_HOLDS_OBJECT (node))
        break;

      object = json_node_get_object (node);
    }
  va_end (args);

  return ret;
}

static gboolean
get_line_and_column (JsonObject *object,
                     guint      *line,
                     guint      *column)
{
  JsonNode *node;

  g_assert (line != NULL);
  g_assert (column != NULL);

  *line = 0;
  *column = 0;

  if (object == NULL)
    return FALSE;

  if (!(node = get_node_at_path (object, "loc", "start", "line", NULL)))
    return FALSE;
  *line = json_node_get_int (node);

  if (!(node = get_node_at_path (object, "loc", "start", "column", NULL)))
    return FALSE;
  *column = MAX (0, json_node_get_int (node));

  return TRUE;
}

static gboolean
is_module_import (JsonObject *object)
{
  if (object == NULL)
    return FALSE;

  return ide_str_equal0 ("imports", GET_STRING (GET_NODE (object, "init", "object", "name"))) ||
         ide_str_equal0 ("imports", GET_STRING (GET_NODE (object, "init", "object", "object", "name"))) ||
         ide_str_equal0 ("require", GET_STRING (GET_NODE (object, "init", "callee", "name")));
}

static gboolean
is_module_exports (JsonObject *object)
{
  JsonObject *left;

  if (object == NULL ||
      !ide_str_equal0 ("AssignmentExpression", GET_STRING (GET_NODE (object, "expression", "type"))) ||
      !(left = GET_OBJECT (object, "expression", "left")) ||
      !ide_str_equal0 ("MemberExpression", GET_STRING (GET_NODE (left, "type"))) ||
      (!ide_str_equal0 ("Identifier", GET_STRING (GET_NODE (left, "object", "type"))) ||
       !ide_str_equal0 ("module", GET_STRING (GET_NODE (left, "object", "name")))) ||
      (!ide_str_equal0 ("Identifier", GET_STRING (GET_NODE (left, "property", "type"))) ||
       !ide_str_equal0 ("exports", GET_STRING (GET_NODE (left, "property", "name")))))
    return FALSE;

  return TRUE;
}

static gboolean
is_gobject_class (JsonObject *object)
{
  const char *name;
  const char *pname;

  if (object == NULL)
    return FALSE;

  if (!(name = GET_STRING (GET_NODE (object, "init", "callee", "object", "name"))) ||
      !(pname = GET_STRING (GET_NODE (object, "init", "callee", "property", "name"))))
    return FALSE;

  return strcasecmp (name, "gobject") == 0 && ide_str_equal0 (pname, "registerClass");
}

static gboolean
is_legacy_gobject_class (JsonObject *object)
{
  const char *name;
  const char *pname;

  if (object == NULL)
    return FALSE;

  if (!(name = GET_STRING (GET_NODE (object, "init", "callee", "object", "name"))) ||
      !(pname = GET_STRING (GET_NODE (object, "init", "callee", "property", "name"))))
    return FALSE;

  return (strcasecmp (name, "gobject") == 0 || strcasecmp (name, "lang") == 0) &&
         ide_str_equal0 (pname, "Class");
}



static gboolean
gbp_gjs_symbol_node_load_variable_decl (GbpGjsSymbolNode *parent,
                                        JsonObject       *object)
{
  g_autoptr(GPtrArray) children = NULL;
  JsonNode *decls;
  JsonArray *decls_ar;
  guint n_decls;

  g_assert (GBP_IS_GJS_SYMBOL_NODE (parent));

  if (object == NULL)
    return TRUE;

  if (!(decls = GET_NODE (object, "declarations")) ||
      !JSON_NODE_HOLDS_ARRAY (decls) ||
      !(decls_ar = json_node_get_array (decls)))
    return TRUE;

  children = g_ptr_array_new_with_free_func (g_object_unref);
  n_decls = json_array_get_length (decls_ar);

  for (guint i = 0; i < n_decls; i++)
    {
      g_autoptr(GbpGjsSymbolNode) new_child = NULL;
      g_autoptr(GPtrArray) grandchildren = NULL;
      JsonNode *decl = json_array_get_element (decls_ar, i);
      JsonObject *decl_obj;
      IdeSymbolKind kind;
      const char *name;
      guint line = 0;
      guint column = 0;

      if (decl == NULL ||
          !JSON_NODE_HOLDS_OBJECT (decl) ||
          !(decl_obj = json_node_get_object (decl)))
        continue;

      /* Destructured assignment, ignore */
      if (ide_str_equal0 ("Identifier", GET_STRING (GET_NODE (decl_obj, "id", "type"))))
        return TRUE;

      kind = IDE_SYMBOL_KIND_VARIABLE;
      name = GET_STRING (GET_NODE (decl_obj, "id", "name"));
      get_line_and_column (GET_OBJECT (decl_obj, "id"), &line, &column);

      if (is_module_import (decl_obj))
        continue;

      if (is_gobject_class (decl_obj))
        {
          JsonNode *args = GET_NODE (decl_obj, "init", "arguments");
          JsonArray *args_ar;
          guint n_args;

          if (args == NULL ||
              !JSON_NODE_HOLDS_ARRAY (args) ||
              !(args_ar = json_node_get_array (args)))
            continue;

          n_args = json_array_get_length (args_ar);

          for (guint j = 0; j < n_args; j++)
            {
              g_autoptr(GbpGjsSymbolNode) child = NULL;
              JsonNode *arg = json_array_get_element (args_ar, j);
              JsonNode *id;
              JsonObject *arg_obj;
              JsonObject *id_obj;

              if (arg == NULL ||
                  !JSON_NODE_HOLDS_OBJECT (arg) ||
                  !(arg_obj = json_node_get_object (arg)) ||
                  !ide_str_equal0 ("ClassExpression", GET_STRING (GET_NODE (arg_obj, "type"))) ||
                  !(id = GET_NODE (arg_obj, "id")) ||
                  !JSON_NODE_HOLDS_OBJECT (id) ||
                  !(id_obj = json_node_get_object (id)))
                continue;

              get_line_and_column (id_obj, &line, &column);
              kind = IDE_SYMBOL_KIND_CLASS;
              name = GET_STRING (GET_NODE (id_obj, "name"));

              child = g_object_new (GBP_TYPE_GJS_SYMBOL_NODE, NULL);
              gbp_gjs_symbol_node_load_children (child, GET_NODE (arg_obj, "body"));
              if (child->children != NULL)
                g_ptr_array_extend_and_steal (grandchildren, g_steal_pointer (&child->children));

              break;
            }
        }
      else if (is_legacy_gobject_class (decl_obj))
        {
          g_autoptr(GbpGjsSymbolNode) child = NULL;
          JsonNode *args;
          JsonNode *arg;
          JsonObject *arg_obj;
          JsonArray *args_ar;

          kind = IDE_SYMBOL_KIND_CLASS;

          if (!(args = GET_NODE (decl_obj, "init", "arguments")) ||
              !JSON_NODE_HOLDS_ARRAY (args) ||
              !(args_ar = json_node_get_array (args)) ||
              json_array_get_length (args_ar) < 1 ||
              (arg = json_array_get_element (args_ar, 0)) ||
              JSON_NODE_HOLDS_OBJECT (arg) ||
              !(arg_obj = json_node_get_object (arg)) ||
              !ide_str_equal0 ("ObjectExpression", GET_STRING (GET_NODE (arg_obj, "type"))))
            continue;

          kind = IDE_SYMBOL_KIND_CLASS;

          child = g_object_new (GBP_TYPE_GJS_SYMBOL_NODE, NULL);
          gbp_gjs_symbol_node_load_children (child, GET_NODE (arg_obj, "properties"));
          if (child->children != NULL)
            g_ptr_array_extend_and_steal (grandchildren, g_steal_pointer (&child->children));
        }

      new_child = g_object_new (GBP_TYPE_GJS_SYMBOL_NODE, NULL);
      new_child->line = line;
      new_child->line_offset = column;
      new_child->children = g_steal_pointer (&grandchildren);
      gbp_gjs_symbol_node_set_name (new_child, name);
      gbp_gjs_symbol_node_set_kind (new_child, kind);
      gbp_gjs_symbol_node_take_child (parent, g_steal_pointer (&new_child));
    }

  return TRUE;
}

static void
gbp_gjs_symbol_node_load_children (GbpGjsSymbolNode *self,
                                   JsonNode         *children)
{
  JsonArray *ar;
  guint n_children;

  g_assert (GBP_IS_GJS_SYMBOL_NODE (self));

  if (children == NULL || !JSON_NODE_HOLDS_ARRAY (children))
    return;

  ar = json_node_get_array (children);
  n_children = json_array_get_length (ar);

  for (guint i = 0; i < n_children; i++)
    {
      JsonNode *node = json_array_get_element (ar, i);
      GbpGjsSymbolNode *child;
      JsonObject *child_obj;

      if (node != NULL &&
          JSON_NODE_HOLDS_OBJECT (node) &&
          (child_obj = json_node_get_object (node)))
        {
          const char *type = GET_STRING (GET_NODE (child_obj, "type"));

          if (ide_str_equal0 (type, "VariableDeclaration"))
            gbp_gjs_symbol_node_load_variable_decl (self, child_obj);
          else if ((child = gbp_gjs_symbol_node_new (child_obj)))
            gbp_gjs_symbol_node_take_child (self, child);
        }
    }
}

static gboolean
gbp_gjs_symbol_node_load_program (GbpGjsSymbolNode *self,
                                  JsonObject       *object)
{
  g_assert (GBP_IS_GJS_SYMBOL_NODE (self));
  g_assert (object != NULL);

  gbp_gjs_symbol_node_set_kind (self, IDE_SYMBOL_KIND_PACKAGE);
  gbp_gjs_symbol_node_set_name (self, GET_STRING (GET_NODE (object, "loc", "source")));
  gbp_gjs_symbol_node_load_children (self, GET_NODE (object, "body"));

  return TRUE;
}

static gboolean
gbp_gjs_symbol_node_load_function_decl (GbpGjsSymbolNode *self,
                                        JsonObject       *object)
{
  g_assert (GBP_IS_GJS_SYMBOL_NODE (self));
  g_assert (object != NULL);

  gbp_gjs_symbol_node_set_kind (self, IDE_SYMBOL_KIND_FUNCTION);
  gbp_gjs_symbol_node_set_name (self, GET_STRING (GET_NODE (object, "id", "name")));

  return TRUE;
}

static gboolean
gbp_gjs_symbol_node_load_property (GbpGjsSymbolNode *self,
                                   JsonObject       *object)
{
  const char *name;
  const char *prop_kind;

  g_assert (GBP_IS_GJS_SYMBOL_NODE (self));
  g_assert (object != NULL);

  if (ide_str_equal0 ("FunctionExpression", GET_NODE (object, "value", "type")))
    return FALSE;

  name = GET_STRING (GET_NODE (object, "key", "name"));
  if (ide_str_equal0 (name, "_init"))
    return FALSE;

  prop_kind = GET_STRING (GET_NODE (object, "kind"));
  if (prop_kind != NULL && g_strv_contains (IDE_STRV_INIT ("get", "set"), prop_kind))
    return FALSE;

  gbp_gjs_symbol_node_set_kind (self, IDE_SYMBOL_KIND_METHOD);
  gbp_gjs_symbol_node_set_name (self, name);

  return TRUE;
}

static gboolean
gbp_gjs_symbol_node_load_class_stmt (GbpGjsSymbolNode *self,
                                     JsonObject       *object)
{
  g_assert (GBP_IS_GJS_SYMBOL_NODE (self));
  g_assert (object != NULL);

  gbp_gjs_symbol_node_set_kind (self, IDE_SYMBOL_KIND_CLASS);
  gbp_gjs_symbol_node_set_name (self, GET_STRING (GET_NODE (object, "id", "name")));
  gbp_gjs_symbol_node_load_children (self, GET_NODE (object, "body"));

  return TRUE;
}

static gboolean
gbp_gjs_symbol_node_load_class_method (GbpGjsSymbolNode *self,
                                       JsonObject       *object)
{
  const char *name;
  const char *kind;

  g_assert (GBP_IS_GJS_SYMBOL_NODE (self));
  g_assert (object != NULL);

  name = GET_STRING (GET_NODE (object, "name", "name"));
  if (name != NULL && g_strv_contains (IDE_STRV_INIT ("constructed", "_init"), name))
    return FALSE;

  kind = GET_STRING (GET_NODE (object, "kind"));
  if (kind != NULL && g_strv_contains (IDE_STRV_INIT ("get", "set"), kind))
    return FALSE;

  gbp_gjs_symbol_node_set_kind (self, IDE_SYMBOL_KIND_METHOD);
  gbp_gjs_symbol_node_set_name (self, name);

  return TRUE;
}

static gboolean
gbp_gjs_symbol_node_load_expr_stmt (GbpGjsSymbolNode *self,
                                    JsonObject       *object)
{
  JsonObject *klass;

  g_assert (GBP_IS_GJS_SYMBOL_NODE (self));
  g_assert (object != NULL);

  if (!(klass = GET_OBJECT (object, "expression", "right")) ||
      !ide_str_equal0 ("ClassExpression", GET_STRING (GET_NODE (klass, "type"))))
    return FALSE;

  gbp_gjs_symbol_node_set_kind (self, IDE_SYMBOL_KIND_CLASS);
  gbp_gjs_symbol_node_set_name (self, GET_STRING (GET_NODE (klass, "id", "name")));
  get_line_and_column (klass, &self->line, &self->line_offset);
  gbp_gjs_symbol_node_load_children (self, GET_NODE (klass, "body"));

  return TRUE;
}

static gboolean
gbp_gjs_symbol_node_load (GbpGjsSymbolNode *self,
                          JsonObject       *object)
{
  const char *type;

  g_assert (GBP_IS_GJS_SYMBOL_NODE (self));
  g_assert (object != NULL);

  if (!(type = json_object_get_string_member (object, "type")))
    return FALSE;

  get_line_and_column (object, &self->line, &self->line_offset);

  if (g_str_equal (type, "Program"))
    return gbp_gjs_symbol_node_load_program (self, object);
  else if (g_str_equal (type, "FunctionDeclaration"))
    return gbp_gjs_symbol_node_load_function_decl (self, object);
  else if (g_str_equal (type, "Property"))
    return gbp_gjs_symbol_node_load_property (self, object);
  else if (g_str_equal (type, "ClassStatement"))
    return gbp_gjs_symbol_node_load_class_stmt (self, object);
  else if (g_str_equal (type, "ClassMethod"))
    return gbp_gjs_symbol_node_load_class_method (self, object);
  else if (g_str_equal (type, "ExpressionStatement") && is_module_exports (object))
    return gbp_gjs_symbol_node_load_expr_stmt (self, object);
  else
    return FALSE;
}

GbpGjsSymbolNode *
gbp_gjs_symbol_node_new (JsonObject *object)
{
  g_autoptr(GbpGjsSymbolNode) self = NULL;

  g_return_val_if_fail (object != NULL, NULL);

  self = g_object_new (GBP_TYPE_GJS_SYMBOL_NODE, NULL);

  if (!gbp_gjs_symbol_node_load (self, object))
    return NULL;

  return g_steal_pointer (&self);
}

guint
gbp_gjs_symbol_node_get_n_children (GbpGjsSymbolNode *self)
{
  g_return_val_if_fail (GBP_IS_GJS_SYMBOL_NODE (self), 0);

  if (self->children == NULL)
    return 0;

  return self->children->len;
}

IdeSymbolNode *
gbp_gjs_symbol_node_get_nth_child (GbpGjsSymbolNode *self,
                                   guint             nth_child)
{
  g_return_val_if_fail (GBP_IS_GJS_SYMBOL_NODE (self), NULL);

  if (self->children == NULL || nth_child >= self->children->len)
    return NULL;

  return g_object_ref (g_ptr_array_index (self->children, nth_child));
}
