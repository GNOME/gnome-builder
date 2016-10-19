/* tmpl-expr-eval.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <girepository.h>
#include <math.h>
#include <string.h>

#include "tmpl-error.h"
#include "tmpl-expr.h"
#include "tmpl-expr-private.h"
#include "tmpl-gi-private.h"
#include "tmpl-scope.h"
#include "tmpl-symbol.h"
#include "tmpl-util-private.h"

typedef gboolean (*BuiltinFunc)  (const GValue  *value,
                                  GValue        *return_value,
                                  GError       **error);
typedef gboolean (*FastDispatch) (const GValue  *left,
                                  const GValue  *right,
                                  GValue        *return_value,
                                  GError       **error);

static gboolean tmpl_expr_eval_internal  (TmplExpr  *node,
                                              TmplScope      *scope,
                                              GValue        *return_value,
                                              GError       **error);
static gboolean throw_type_mismatch          (GError       **error,
                                              const GValue  *left,
                                              const GValue  *right,
                                              const gchar   *message);
static gboolean builtin_abs                  (const GValue  *value,
                                              GValue        *return_value,
                                              GError       **error);
static gboolean builtin_ceil                 (const GValue  *value,
                                              GValue        *return_value,
                                              GError       **error);
static gboolean builtin_floor                (const GValue  *value,
                                              GValue        *return_value,
                                              GError       **error);
static gboolean builtin_hex                  (const GValue  *value,
                                              GValue        *return_value,
                                              GError       **error);
static gboolean builtin_log                  (const GValue  *value,
                                              GValue        *return_value,
                                              GError       **error);
static gboolean builtin_print                (const GValue  *value,
                                              GValue        *return_value,
                                              GError       **error);
static gboolean builtin_repr                 (const GValue  *value,
                                              GValue        *return_value,
                                              GError       **error);
static gboolean builtin_sqrt                 (const GValue  *value,
                                              GValue        *return_value,
                                              GError       **error);
static gboolean eq_enum_string               (const GValue  *left,
                                              const GValue  *right,
                                              GValue        *return_value,
                                              GError       **error);
static gboolean ne_enum_string               (const GValue  *left,
                                              const GValue  *right,
                                              GValue        *return_value,
                                              GError       **error);

static GHashTable *fast_dispatch;
static BuiltinFunc builtin_funcs [] = {
  builtin_abs,
  builtin_ceil,
  builtin_floor,
  builtin_hex,
  builtin_log,
  builtin_print,
  builtin_repr,
  builtin_sqrt,
};

static inline guint
build_hash (TmplExprType type,
            GType        left,
            GType        right)
{
  if (left && !G_TYPE_IS_FUNDAMENTAL (left))
    return 0;

  if (right && !G_TYPE_IS_FUNDAMENTAL (right))
    return 0;

  return type | (left << 16) | (right << 24);
}


static gboolean
throw_type_mismatch (GError       **error,
                     const GValue  *left,
                     const GValue  *right,
                     const gchar   *message)
{
  if (right != NULL)
    g_set_error (error,
                 TMPL_ERROR,
                 TMPL_ERROR_TYPE_MISMATCH,
                 "%s: %s and %s",
                 message,
                 G_VALUE_TYPE_NAME (left),
                 G_VALUE_TYPE_NAME (right));
  else
    g_set_error (error,
                 TMPL_ERROR,
                 TMPL_ERROR_TYPE_MISMATCH,
                 "%s: %s", message, G_VALUE_TYPE_NAME (left));

  return TRUE;
}

#define SIMPLE_NUMBER_OP(op, left, right, return_value, error) \
  G_STMT_START { \
    if (G_VALUE_HOLDS (left, G_VALUE_TYPE (right))) \
      { \
        if (G_VALUE_HOLDS (left, G_TYPE_DOUBLE)) \
          { \
            g_value_init (return_value, G_TYPE_DOUBLE); \
            g_value_set_double (return_value, \
                                g_value_get_double (left) \
                                op \
                                g_value_get_double (right)); \
            return TRUE; \
          } \
      } \
    return throw_type_mismatch (error, left, right, "invalid add"); \
  } G_STMT_END

static FastDispatch
find_dispatch_slow (TmplExprSimple *node,
                    const GValue   *left,
                    const GValue   *right)
{
  if (node->type == TMPL_EXPR_EQ)
    {
      if ((G_VALUE_HOLDS_STRING (left) && G_VALUE_HOLDS_ENUM (right)) ||
          (G_VALUE_HOLDS_STRING (right) && G_VALUE_HOLDS_ENUM (left)))
        return eq_enum_string;
    }

  if (node->type == TMPL_EXPR_NE)
    {
      if ((G_VALUE_HOLDS_STRING (left) && G_VALUE_HOLDS_ENUM (right)) ||
          (G_VALUE_HOLDS_STRING (right) && G_VALUE_HOLDS_ENUM (left)))
        return ne_enum_string;
    }

  return NULL;
}

static gboolean
tmpl_expr_simple_eval (TmplExprSimple  *node,
                       TmplScope       *scope,
                       GValue          *return_value,
                       GError         **error)
{
  GValue left = G_VALUE_INIT;
  GValue right = G_VALUE_INIT;
  gboolean ret = FALSE;

  g_assert (node != NULL);
  g_assert (scope != NULL);
  g_assert (return_value != NULL);

  if (tmpl_expr_eval_internal (node->left, scope, &left, error) &&
      ((node->right == NULL) ||
       tmpl_expr_eval_internal (node->right, scope, &right, error)))
    {
      FastDispatch dispatch = NULL;
      guint hash;

      hash = build_hash (node->type, G_VALUE_TYPE (&left), G_VALUE_TYPE (&right));

      if (hash != 0)
        dispatch = g_hash_table_lookup (fast_dispatch, GINT_TO_POINTER (hash));

      if G_UNLIKELY (dispatch == NULL)
        {
          dispatch = find_dispatch_slow (node, &left, &right);

          if (dispatch == NULL)
            {
              throw_type_mismatch (error, &left, &right, "type mismatch");
              goto cleanup;
            }
        }

      ret = dispatch (&left, &right, return_value, error);
    }

cleanup:
  TMPL_CLEAR_VALUE (&left);
  TMPL_CLEAR_VALUE (&right);

  return ret;
}

static gboolean
tmpl_expr_simple_eval_logical (TmplExprSimple  *node,
                               TmplScope       *scope,
                               GValue          *return_value,
                               GError         **error)
{
  GValue left = G_VALUE_INIT;
  GValue right = G_VALUE_INIT;
  gboolean ret = FALSE;

  g_assert (node != NULL);
  g_assert (scope != NULL);
  g_assert (return_value != NULL);

  g_value_init (return_value, G_TYPE_BOOLEAN);

  if (tmpl_expr_eval_internal (node->left, scope, &left, error) &&
      ((node->right == NULL) ||
       tmpl_expr_eval_internal (node->right, scope, &right, error)))
    {
      switch ((int)node->type)
        {
        case TMPL_EXPR_AND:
          g_value_set_boolean (return_value,
                               (tmpl_value_as_boolean (&left) && tmpl_value_as_boolean (&right)));
          ret = TRUE;
          break;

        case TMPL_EXPR_OR:
          g_value_set_boolean (return_value,
                               (tmpl_value_as_boolean (&left) || tmpl_value_as_boolean (&right)));
          ret = TRUE;
          break;

        default:
          g_set_error (error,
                       TMPL_ERROR,
                       TMPL_ERROR_RUNTIME_ERROR,
                       "Unknown logical operator type: %d", node->type);
          break;
        }
    }

  TMPL_CLEAR_VALUE (&left);
  TMPL_CLEAR_VALUE (&right);

  return ret;
}

static gboolean
tmpl_expr_fn_call_eval (TmplExprFnCall  *node,
                       TmplScope       *scope,
                       GValue         *return_value,
                       GError        **error)
{
  GValue left = G_VALUE_INIT;
  gboolean ret = FALSE;

  g_assert (node != NULL);
  g_assert (scope != NULL);
  g_assert (return_value != NULL);

  if (tmpl_expr_eval_internal (node->param, scope, &left, error))
    ret = builtin_funcs [node->builtin] (&left, return_value, error);

  TMPL_CLEAR_VALUE (&left);

  return ret;
}

static gboolean
tmpl_expr_flow_eval (TmplExprFlow  *node,
                    TmplScope     *scope,
                    GValue       *return_value,
                    GError      **error)
{
  GValue cond = G_VALUE_INIT;
  gboolean ret = FALSE;

  g_assert (node != NULL);
  g_assert (scope != NULL);
  g_assert (return_value != NULL);

  if (!tmpl_expr_eval_internal (node->condition, scope, &cond, error))
    goto cleanup;

  if (node->type == TMPL_EXPR_IF)
    {
      if (tmpl_value_as_boolean (&cond))
        {
          if (node->primary != NULL)
            {
              ret = tmpl_expr_eval_internal (node->primary, scope, return_value, error);
              goto cleanup;
            }
        }
      else
        {
          if (node->secondary != NULL)
            {
              ret = tmpl_expr_eval_internal (node->secondary, scope, return_value, error);
              goto cleanup;
            }
        }
    }
  else if (node->type == TMPL_EXPR_WHILE)
    {
      if (node->primary != NULL)
        {
          while (tmpl_value_as_boolean (&cond))
            {
              /* last iteration is result value */
              g_value_unset (return_value);
              if (!tmpl_expr_eval_internal (node->primary, scope, return_value, error))
                goto cleanup;

              g_value_unset (&cond);
              if (!tmpl_expr_eval_internal (node->condition, scope, &cond, error))
                goto cleanup;
            }
        }
    }

  g_set_error (error,
               TMPL_ERROR,
               TMPL_ERROR_INVALID_STATE,
               "Invalid AST");

cleanup:
  TMPL_CLEAR_VALUE (&cond);

  return ret;
}

static gboolean
tmpl_expr_stmt_list_eval (TmplExprSimple  *node,
                         TmplScope       *scope,
                         GValue         *return_value,
                         GError        **error)
{
  GValue left = G_VALUE_INIT;
  gboolean ret = FALSE;

  if (!tmpl_expr_eval_internal (node->left, scope, &left, error))
    goto cleanup;

  if (!tmpl_expr_eval_internal (node->left, scope, return_value, error))
    goto cleanup;

  ret = TRUE;

cleanup:
  TMPL_CLEAR_VALUE (&left);

  return ret;
}

static gboolean
tmpl_expr_symbol_ref_eval (TmplExprSymbolRef  *node,
                           TmplScope          *scope,
                           GValue             *return_value,
                           GError            **error)
{
  TmplSymbol *symbol;

  g_assert (node != NULL);
  g_assert (scope != NULL);

  symbol = tmpl_scope_peek (scope, node->symbol);

  if (symbol == NULL)
    {
      g_set_error (error,
                   TMPL_ERROR,
                   TMPL_ERROR_MISSING_SYMBOL,
                   "No such symbol \"%s\" in scope",
                   node->symbol);
      return FALSE;
    }

  if (tmpl_symbol_get_symbol_type (symbol) == TMPL_SYMBOL_VALUE)
    {
      tmpl_symbol_get_value (symbol, return_value);
      return TRUE;
    }

  g_set_error (error,
               TMPL_ERROR,
               TMPL_ERROR_NOT_A_VALUE,
               "The symbol \"%s\" is not a value",
               node->symbol);

  return FALSE;
}

static gboolean
tmpl_expr_symbol_assign_eval (TmplExprSymbolAssign  *node,
                              TmplScope             *scope,
                              GValue                *return_value,
                              GError               **error)
{
  TmplSymbol *symbol;

  g_assert (node != NULL);
  g_assert (scope != NULL);
  g_assert (return_value != NULL);

  if (!tmpl_expr_eval_internal (node->right, scope, return_value, error))
    return FALSE;

  symbol = tmpl_scope_get (scope, node->symbol);
  tmpl_symbol_assign_value (symbol, return_value);

  return TRUE;
}

static gboolean
tmpl_expr_getattr_eval (TmplExprGetattr  *node,
                        TmplScope        *scope,
                        GValue           *return_value,
                        GError          **error)
{
  GValue left = G_VALUE_INIT;
  GParamSpec *pspec;
  GObject *object;
  gboolean ret = FALSE;

  g_assert (node != NULL);
  g_assert (scope != NULL);
  g_assert (return_value != NULL);

  if (!tmpl_expr_eval_internal (node->left, scope, &left, error))
    goto cleanup;

  if (!G_VALUE_HOLDS_OBJECT (&left))
    {
      g_set_error (error,
                   TMPL_ERROR,
                   TMPL_ERROR_NOT_AN_OBJECT,
                   "Cannot access property \"%s\" of non-object \"%s\"",
                   node->attr, G_VALUE_TYPE_NAME (&left));
      goto cleanup;
    }

  object = g_value_get_object (&left);

  if (object == NULL)
    {
      g_set_error (error,
                   TMPL_ERROR,
                   TMPL_ERROR_NULL_POINTER,
                   "Cannot access property of null object");
      goto cleanup;
    }

  if (!(pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (object), node->attr)))
    {
      g_set_error (error,
                   TMPL_ERROR,
                   TMPL_ERROR_NO_SUCH_PROPERTY,
                   "No such property \"%s\" on object \"%s\"",
                   node->attr, G_OBJECT_TYPE_NAME (object));
      goto cleanup;
    }

  g_value_init (return_value, pspec->value_type);
  g_object_get_property (object, node->attr, return_value);

  ret = TRUE;

cleanup:
  TMPL_CLEAR_VALUE (&left);

  return ret;
}

static gboolean
tmpl_expr_setattr_eval (TmplExprSetattr  *node,
                        TmplScope        *scope,
                        GValue           *return_value,
                        GError          **error)
{
  GValue left = G_VALUE_INIT;
  GValue right = G_VALUE_INIT;
  GObject *object;
  gboolean ret = FALSE;

  g_assert (node != NULL);
  g_assert (scope != NULL);
  g_assert (return_value != NULL);

  if (!tmpl_expr_eval_internal (node->left, scope, &left, error))
    goto cleanup;

  if (!G_VALUE_HOLDS_OBJECT (&left))
    {
      g_set_error (error,
                   TMPL_ERROR,
                   TMPL_ERROR_NOT_AN_OBJECT,
                   "Cannot access property \"%s\" of non-object \"%s\"",
                   node->attr, G_VALUE_TYPE_NAME (&left));
      goto cleanup;
    }

  object = g_value_get_object (&left);

  if (object == NULL)
    {
      g_set_error (error,
                   TMPL_ERROR,
                   TMPL_ERROR_NULL_POINTER,
                   "Cannot access property of null object");
      goto cleanup;
    }

  if (!g_object_class_find_property (G_OBJECT_GET_CLASS (object), node->attr))
    {
      g_set_error (error,
                   TMPL_ERROR,
                   TMPL_ERROR_NO_SUCH_PROPERTY,
                   "No such property \"%s\" on object \"%s\"",
                   node->attr, G_OBJECT_TYPE_NAME (object));
      goto cleanup;
    }

  if (!tmpl_expr_eval_internal (node->right, scope, &right, error))
    goto cleanup;

  g_object_set_property (object, node->attr, &right);

  g_value_init (return_value, G_VALUE_TYPE (&right));
  g_value_copy (&right, return_value);

  ret = TRUE;

cleanup:
  TMPL_CLEAR_VALUE (&left);
  TMPL_CLEAR_VALUE (&right);

  g_assert (ret == TRUE || (error == NULL || *error != NULL));

  return ret;
}

static gchar *
make_title (const gchar *str)
{
  g_auto(GStrv) parts = NULL;
  GString *ret;

  g_assert (str != NULL);

  ret = g_string_new (NULL);

  for (; *str; str = g_utf8_next_char (str))
    {
      gunichar ch = g_utf8_get_char (str);

      if (!g_unichar_isalnum (ch))
        {
          if (ret->len && ret->str[ret->len - 1] != ' ')
            g_string_append_c (ret, ' ');
          continue;
        }

      if (ret->len && ret->str[ret->len - 1] != ' ')
        g_string_append_unichar (ret, ch);
      else
        g_string_append_unichar (ret, g_unichar_toupper (ch));
    }

  return g_string_free (ret, FALSE);
}

static gboolean
tmpl_expr_gi_call_eval (TmplExprGiCall  *node,
                        TmplScope       *scope,
                        GValue          *return_value,
                        GError         **error)
{
  GValue left = G_VALUE_INIT;
  GValue right = G_VALUE_INIT;
  GIRepository *repository;
  GIBaseInfo *base_info;
  GIFunctionInfo *function = NULL;
  GIArgument return_value_arg = { 0 };
  GITypeInfo return_value_type;
  TmplExpr *args;
  GObject *object;
  gboolean ret = FALSE;
  GArray *in_args = NULL;
  GArray *values = NULL;
  GType type;
  guint n_args;
  guint i;

  g_assert (node != NULL);
  g_assert (scope != NULL);
  g_assert (return_value != NULL);

  if (!tmpl_expr_eval_internal (node->object, scope, &left, error))
    goto cleanup;

  if (G_VALUE_HOLDS_STRING (&left))
    {
      const gchar *str = g_value_get_string (&left) ?: "";

      /*
       * TODO: This should be abstracted somewhere else rather than our G-I call.
       *       Basically we are adding useful string functions like:
       *
       *       "foo".upper()
       *       "foo".lower()
       *       "foo".casefold()
       *       "foo".reverse()
       *       "foo".len()
       *       "foo".title()
       */
      if (FALSE) {}
      else if (g_str_equal (node->name, "upper"))
        {
          g_value_init (return_value, G_TYPE_STRING);
          g_value_take_string (return_value, g_utf8_strup (str, -1));
          ret = TRUE;
        }
      else if (g_str_equal (node->name, "lower"))
        {
          g_value_init (return_value, G_TYPE_STRING);
          g_value_take_string (return_value, g_utf8_strdown (str, -1));
          ret = TRUE;
        }
      else if (g_str_equal (node->name, "casefold"))
        {
          g_value_init (return_value, G_TYPE_STRING);
          g_value_take_string (return_value, g_utf8_casefold (str, -1));
          ret = TRUE;
        }
      else if (g_str_equal (node->name, "reverse"))
        {
          g_value_init (return_value, G_TYPE_STRING);
          g_value_take_string (return_value, g_utf8_strreverse (str, -1));
          ret = TRUE;
        }
      else if (g_str_equal (node->name, "len"))
        {
          g_value_init (return_value, G_TYPE_UINT);
          g_value_set_uint (return_value, strlen (str));
          ret = TRUE;
        }
      else if (g_str_equal (node->name, "space"))
        {
          gchar *space;
          guint len = strlen (str);

          g_value_init (return_value, G_TYPE_STRING);
          space = g_malloc (len + 1);
          memset (space, ' ', len);
          space[len] = '\0';
          g_value_take_string (return_value, space);
          ret = TRUE;
        }
      else if (g_str_equal (node->name, "title"))
        {
          g_value_init (return_value, G_TYPE_STRING);
          g_value_take_string (return_value, make_title (str));
          ret = TRUE;
        }
      else
        {
          g_set_error (error,
                       TMPL_ERROR,
                       TMPL_ERROR_GI_FAILURE,
                       "No such method %s for string",
                       node->name);
        }

      goto cleanup;
    }

  if (G_VALUE_HOLDS_ENUM (&left))
    {
      if (FALSE) {}
      else if (g_str_equal (node->name, "nick"))
        {
          GEnumClass *enum_class = g_type_class_peek (G_VALUE_TYPE (&left));
          GEnumValue *enum_value = g_enum_get_value (enum_class, g_value_get_enum (&left));

          g_value_init (return_value, G_TYPE_STRING);

          if (enum_value != NULL)
            g_value_set_static_string (return_value, enum_value->value_nick);

          ret = TRUE;
        }
      else
        {
          g_set_error (error,
                       TMPL_ERROR,
                       TMPL_ERROR_GI_FAILURE,
                       "No such method %s for enum",
                       node->name);
        }

      goto cleanup;
    }

  if (!G_VALUE_HOLDS_OBJECT (&left))
    {
      g_set_error (error,
                   TMPL_ERROR,
                   TMPL_ERROR_NOT_AN_OBJECT,
                   "Cannot access function \"%s\" of non-object \"%s\"",
                   node->name, G_VALUE_TYPE_NAME (&left));
      goto cleanup;
    }

  object = g_value_get_object (&left);

  if (object == NULL)
    {
      g_set_error (error,
                   TMPL_ERROR,
                   TMPL_ERROR_NULL_POINTER,
                   "Cannot access function of null object");
      goto cleanup;
    }

  repository = g_irepository_get_default ();
  type = G_OBJECT_TYPE (object);

  while (g_type_is_a (type, G_TYPE_OBJECT))
    {
      guint n_ifaces;

      base_info = g_irepository_find_by_gtype (repository, type);

      if (base_info == NULL)
        {
          g_set_error (error,
                       TMPL_ERROR,
                       TMPL_ERROR_GI_FAILURE,
                       "Failed to locate GObject Introspection data. "
                       "Consider importing required module.");
          goto cleanup;
        }

      function = g_object_info_find_method ((GIObjectInfo *)base_info, node->name);
      if (function != NULL)
        break;

      /* Maybe the function is found in an interface */
      n_ifaces = g_object_info_get_n_interfaces ((GIObjectInfo *)base_info);
      for (i = 0; function == NULL && i < n_ifaces; i++)
        {
          GIInterfaceInfo *iface_info;

          iface_info = g_object_info_get_interface ((GIObjectInfo *)base_info, i);

          function = g_interface_info_find_method (iface_info, node->name);
        }
      if (function != NULL)
        break;

      type = g_type_parent (type);
    }

  if (function == NULL)
    {
      g_set_error (error,
                   TMPL_ERROR,
                   TMPL_ERROR_GI_FAILURE,
                   "No such method \"%s\" on object \"%s\"",
                   node->name, G_OBJECT_TYPE_NAME (object));
      goto cleanup;
    }

  n_args = g_callable_info_get_n_args ((GICallableInfo *)function);

  values = g_array_new (FALSE, TRUE, sizeof (GValue));
  g_array_set_size (values, n_args);

  in_args = g_array_new (FALSE, TRUE, sizeof (GIArgument));
  g_array_set_size (in_args, n_args + 1);

  g_array_index (in_args, GIArgument, 0).v_pointer = object;

  args = node->params;

  for (i = 0; i < n_args; i++)
    {
      GIArgInfo *arg_info = g_callable_info_get_arg ((GICallableInfo *)function, i);
      GIArgument *arg = &g_array_index (in_args, GIArgument, i + 1);
      GValue *value = &g_array_index (values, GValue, i);
      GITypeInfo type_info = { 0 };

      if (g_arg_info_get_direction (arg_info) != GI_DIRECTION_IN)
        {
          g_set_error (error,
                       TMPL_ERROR,
                       TMPL_ERROR_RUNTIME_ERROR,
                       "Only \"in\" parameters are supported");
          goto cleanup;
        }

      if (args == NULL)
        {
          g_set_error (error,
                       TMPL_ERROR,
                       TMPL_ERROR_SYNTAX_ERROR,
                       "Too few arguments to function \"%s\"",
                       node->name);
          goto cleanup;
        }

      if (args->any.type == TMPL_EXPR_STMT_LIST)
        {
          if (!tmpl_expr_eval_internal (((TmplExprSimple *)node)->left, scope, value, error))
            goto cleanup;

          args = ((TmplExprSimple *)args)->right;
        }
      else
        {
          if (!tmpl_expr_eval_internal (args, scope, value, error))
            goto cleanup;

          args = NULL;
        }

      g_arg_info_load_type (arg_info, &type_info);

      if (!tmpl_gi_argument_from_g_value (value, &type_info, arg, error))
        goto cleanup;
    }

  if ((args != NULL) && (n_args > 0))
    {
      g_set_error (error,
                   TMPL_ERROR,
                   TMPL_ERROR_SYNTAX_ERROR,
                   "Too many arguments to function \"%s\"",
                   node->name);
      goto cleanup;
    }

  if (!g_function_info_invoke (function,
                               (GIArgument *)(void *)in_args->data,
                               in_args->len,
                               NULL,
                               0,
                               &return_value_arg,
                               error))
    goto cleanup;

  g_callable_info_load_return_type ((GICallableInfo *)function, &return_value_type);

  if (!tmpl_gi_argument_to_g_value (return_value, &return_value_type, &return_value_arg, error))
    goto cleanup;

  ret = TRUE;

cleanup:
  g_clear_pointer (&in_args, g_array_unref);

  if (values != NULL)
    {
      for (i = 0; i < values->len; i++)
        {
          GValue *value = &g_array_index (values, GValue, i);

          if (G_VALUE_TYPE (value) != G_TYPE_INVALID)
            g_value_unset (value);
        }

      g_clear_pointer (&values, g_array_unref);
    }

  TMPL_CLEAR_VALUE (&left);
  TMPL_CLEAR_VALUE (&right);

  return ret;
}

static gboolean
tmpl_expr_user_fn_call_eval (TmplExprUserFnCall  *node,
                             TmplScope           *scope,
                             GValue              *return_value,
                             GError             **error)
{
  GPtrArray *args = NULL;
  TmplExpr *expr = NULL;
  TmplExpr *params = NULL;
  TmplScope *local_scope = NULL;
  TmplSymbol *symbol;
  gboolean ret = FALSE;
  gint n_args = 0;
  gint i;

  g_assert (node != NULL);
  g_assert (scope != NULL);
  g_assert (return_value != NULL);

  symbol = tmpl_scope_peek (scope, node->symbol);

  if (symbol == NULL)
    {
      g_set_error (error,
                   TMPL_ERROR,
                   TMPL_ERROR_MISSING_SYMBOL,
                   "No such function \"%s\"",
                   node->symbol);
      return FALSE;
    }

  if (tmpl_symbol_get_symbol_type (symbol) != TMPL_SYMBOL_EXPR)
    {
      g_set_error (error,
                   TMPL_ERROR,
                   TMPL_ERROR_NOT_A_FUNCTION,
                   "\"%s\" is not a function",
                   node->symbol);
      return FALSE;
    }

  expr = tmpl_symbol_get_expr (symbol, &args);
  n_args = args != NULL ? args->len : 0;

  local_scope = tmpl_scope_new_with_parent (scope);

  params = node->params;

  for (i = 0; i < n_args; i++)
    {
      const gchar *arg = g_ptr_array_index (args, i);
      GValue value = G_VALUE_INIT;

      if (params == NULL)
        {
          g_set_error (error,
                       TMPL_ERROR,
                       TMPL_ERROR_SYNTAX_ERROR,
                       "\"%s\" takes %d arguments, not %d",
                       node->symbol, n_args, i);
          return FALSE;
        }

      if (params->any.type == TMPL_EXPR_STMT_LIST)
        {
          TmplExprSimple *simple = (TmplExprSimple *)params;

          if (!tmpl_expr_eval_internal (simple->left, local_scope, &value, error))
            goto cleanup;

          params = simple->right;
        }
      else
        {
          if (!tmpl_expr_eval_internal (params, local_scope, &value, error))
            goto cleanup;

          params = NULL;
        }

      symbol = tmpl_scope_get (local_scope, arg);
      tmpl_symbol_assign_value (symbol, &value);

      TMPL_CLEAR_VALUE (&value);
    }

  if (params != NULL)
    {
      g_set_error (error,
                   TMPL_ERROR,
                   TMPL_ERROR_SYNTAX_ERROR,
                   "\"%s\" takes %d params",
                   node->symbol, n_args);
      goto cleanup;
    }

  if (!tmpl_expr_eval_internal (expr, local_scope, return_value, error))
    goto cleanup;

  ret = TRUE;

cleanup:
  g_clear_pointer (&local_scope, tmpl_scope_unref);

  return ret;
}

static gboolean
tmpl_expr_require_eval (TmplExprRequire  *node,
                        TmplScope        *scope,
                        GValue           *return_value,
                        GError          **error)
{
  GITypelib *typelib;
  TmplSymbol *symbol;

  g_assert (node != NULL);
  g_assert (scope != NULL);
  g_assert (return_value != NULL);

  typelib = g_irepository_require (g_irepository_get_default (),
                                   node->name,
                                   node->version,
                                   G_IREPOSITORY_LOAD_FLAG_LAZY,
                                   error);

  g_assert (typelib != NULL || (error == NULL || *error != NULL));

  if (typelib == NULL)
    return FALSE;

  g_value_init (return_value, TMPL_TYPE_TYPELIB);
  g_value_set_pointer (return_value, typelib);

  symbol = tmpl_scope_get (scope, node->name);
  tmpl_symbol_assign_value (symbol, return_value);

  return TRUE;
}

static gboolean
tmpl_expr_eval_internal (TmplExpr   *node,
                         TmplScope  *scope,
                         GValue     *return_value,
                         GError    **error)
{
  g_assert (node != NULL);
  g_assert (scope != NULL);
  g_assert (return_value != NULL);

  switch (node->any.type)
    {
    case TMPL_EXPR_ADD:
    case TMPL_EXPR_SUB:
    case TMPL_EXPR_MUL:
    case TMPL_EXPR_DIV:
    case TMPL_EXPR_UNARY_MINUS:
    case TMPL_EXPR_GT:
    case TMPL_EXPR_LT:
    case TMPL_EXPR_NE:
    case TMPL_EXPR_EQ:
    case TMPL_EXPR_GTE:
    case TMPL_EXPR_LTE:
      return tmpl_expr_simple_eval ((TmplExprSimple *)node, scope, return_value, error);

    case TMPL_EXPR_AND:
    case TMPL_EXPR_OR:
      return tmpl_expr_simple_eval_logical ((TmplExprSimple *)node, scope, return_value, error);

    case TMPL_EXPR_NUMBER:
      g_value_init (return_value, G_TYPE_DOUBLE);
      g_value_set_double (return_value, ((TmplExprNumber *)node)->number);
      return TRUE;

    case TMPL_EXPR_BOOLEAN:
      g_value_init (return_value, G_TYPE_BOOLEAN);
      g_value_set_boolean (return_value, ((TmplExprBoolean *)node)->value);
      return TRUE;

    case TMPL_EXPR_STRING:
      g_value_init (return_value, G_TYPE_STRING);
      g_value_set_string (return_value, ((TmplExprString *)node)->value);
      return TRUE;

    case TMPL_EXPR_STMT_LIST:
      return tmpl_expr_stmt_list_eval ((TmplExprSimple *)node, scope, return_value, error);

    case TMPL_EXPR_IF:
    case TMPL_EXPR_WHILE:
      return tmpl_expr_flow_eval ((TmplExprFlow *)node, scope, return_value, error);

    case TMPL_EXPR_SYMBOL_REF:
      return tmpl_expr_symbol_ref_eval ((TmplExprSymbolRef *)node, scope, return_value, error);

    case TMPL_EXPR_SYMBOL_ASSIGN:
      return tmpl_expr_symbol_assign_eval ((TmplExprSymbolAssign *)node, scope, return_value, error);

    case TMPL_EXPR_FN_CALL:
      return tmpl_expr_fn_call_eval ((TmplExprFnCall *)node, scope, return_value, error);

    case TMPL_EXPR_USER_FN_CALL:
      return tmpl_expr_user_fn_call_eval ((TmplExprUserFnCall *)node, scope, return_value, error);

    case TMPL_EXPR_GI_CALL:
      return tmpl_expr_gi_call_eval ((TmplExprGiCall *)node, scope, return_value, error);

    case TMPL_EXPR_GETATTR:
      return tmpl_expr_getattr_eval ((TmplExprGetattr *)node, scope, return_value, error);

    case TMPL_EXPR_SETATTR:
      return tmpl_expr_setattr_eval ((TmplExprSetattr *)node, scope, return_value, error);

    case TMPL_EXPR_REQUIRE:
      return tmpl_expr_require_eval ((TmplExprRequire *)node, scope, return_value, error);

    case TMPL_EXPR_INVERT_BOOLEAN:
      {
        GValue tmp = G_VALUE_INIT;
        gboolean ret;

        ret = tmpl_expr_eval_internal (((TmplExprSimple *)node)->left, scope, &tmp, error);

        if (ret)
          {
            g_value_init (return_value, G_TYPE_BOOLEAN);
            g_value_set_boolean (return_value, !tmpl_value_as_boolean (&tmp));
          }

        TMPL_CLEAR_VALUE (&tmp);

        g_assert (ret == TRUE || (error == NULL || *error != NULL));

        return ret;
      }

    default:
      break;
    }

  g_set_error (error,
               TMPL_ERROR,
               TMPL_ERROR_INVALID_OP_CODE,
               "invalid opcode: %04x", node->any.type);

  return FALSE;
}

static gboolean
div_double_double (const GValue  *left,
                   const GValue  *right,
                   GValue        *return_value,
                   GError       **error)
{
  gdouble denom = g_value_get_double (right);

  if (denom == 0.0)
    {
      g_set_error (error,
                   TMPL_ERROR,
                   TMPL_ERROR_DIVIDE_BY_ZERO,
                   "divide by zero");
      return FALSE;
    }

  g_value_init (return_value, G_TYPE_DOUBLE);
  g_value_set_double (return_value, g_value_get_double (left) / denom);

  return TRUE;
}

static gboolean
unary_minus_double (const GValue  *left,
                    const GValue  *right,
                    GValue        *return_value,
                    GError       **error)
{
  g_value_init (return_value, G_TYPE_DOUBLE);
  g_value_set_double (return_value, -g_value_get_double (left));
  return TRUE;
}

static gboolean
mul_double_string (const GValue  *left,
                   const GValue  *right,
                   GValue        *return_value,
                   GError       **error)
{
  GString *str;
  gint v;
  gint i;

  str = g_string_new (NULL);
  v = g_value_get_double (left);

  for (i = 0; i < v; i++)
    g_string_append (str, g_value_get_string (right));

  g_value_init (return_value, G_TYPE_STRING);
  g_value_take_string (return_value, g_string_free (str, FALSE));

  return TRUE;
}

static gboolean
mul_string_double (const GValue  *left,
                   const GValue  *right,
                   GValue        *return_value,
                   GError       **error)
{
  return mul_double_string (right, left, return_value, error);
}

static gboolean
add_string_string (const GValue  *left,
                   const GValue  *right,
                   GValue        *return_value,
                   GError       **error)
{
  g_value_init (return_value, G_TYPE_STRING);
  g_value_take_string (return_value,
                       g_strdup_printf ("%s%s",
                                        g_value_get_string (left),
                                        g_value_get_string (right)));
  return TRUE;
}

static gboolean
eq_string_string (const GValue  *left,
                  const GValue  *right,
                  GValue        *return_value,
                  GError       **error)
{
  const gchar *left_str = g_value_get_string (left);
  const gchar *right_str = g_value_get_string (right);

  g_value_init (return_value, G_TYPE_BOOLEAN);
  g_value_set_boolean (return_value, 0 == g_strcmp0 (left_str, right_str));

  return TRUE;
}

static gboolean
ne_string_string (const GValue  *left,
                  const GValue  *right,
                  GValue        *return_value,
                  GError       **error)
{
  const gchar *left_str = g_value_get_string (left);
  const gchar *right_str = g_value_get_string (right);

  g_value_init (return_value, G_TYPE_BOOLEAN);
  g_value_set_boolean (return_value, 0 != g_strcmp0 (left_str, right_str));

  return TRUE;
}

static gboolean
eq_enum_string (const GValue  *left,
                const GValue  *right,
                GValue        *return_value,
                GError       **error)
{
  const gchar *str;
  GEnumClass *klass;
  const GEnumValue *val;
  GType type;
  gint eval;

  if (G_VALUE_HOLDS_STRING (left))
    {
      str = g_value_get_string (left);
      eval = g_value_get_enum (right);
      type = G_VALUE_TYPE (right);
    }
  else
    {
      str = g_value_get_string (right);
      eval = g_value_get_enum (left);
      type = G_VALUE_TYPE (left);
    }

  klass = g_type_class_peek (type);
  val = g_enum_get_value ((GEnumClass *)klass, eval);

  g_value_init (return_value, G_TYPE_BOOLEAN);
  g_value_set_boolean (return_value, 0 == g_strcmp0 (str, val->value_nick));

  return TRUE;
}

static gboolean
ne_enum_string (const GValue  *left,
                const GValue  *right,
                GValue        *return_value,
                GError       **error)
{
  if (eq_enum_string (left, right, return_value, error))
    {
      g_value_set_boolean (return_value, !g_value_get_boolean (return_value));
      return TRUE;
    }

  return FALSE;
}

#define SIMPLE_OP_FUNC(func_name, ret_type, set_func, get_left, op, get_right)  \
static gboolean                                                                 \
func_name (const GValue  *left,                                                 \
           const GValue  *right,                                                \
           GValue        *return_value,                                         \
           GError       **error)                                                \
{                                                                               \
  g_value_init (return_value, ret_type);                                        \
  g_value_##set_func (return_value,                                             \
                      g_value_##get_left (left)                                 \
                      op                                                        \
                      g_value_##get_right (right));                             \
  return TRUE;                                                                  \
}

SIMPLE_OP_FUNC (add_double_double, G_TYPE_DOUBLE,  set_double,  get_double, +,  get_double)
SIMPLE_OP_FUNC (sub_double_double, G_TYPE_DOUBLE,  set_double,  get_double, -,  get_double)
SIMPLE_OP_FUNC (mul_double_double, G_TYPE_DOUBLE,  set_double,  get_double, *,  get_double)
SIMPLE_OP_FUNC (lt_double_double,  G_TYPE_BOOLEAN, set_boolean, get_double, <,  get_double)
SIMPLE_OP_FUNC (lte_double_double, G_TYPE_BOOLEAN, set_boolean, get_double, <=, get_double)
SIMPLE_OP_FUNC (gt_double_double,  G_TYPE_BOOLEAN, set_boolean, get_double, >,  get_double)
SIMPLE_OP_FUNC (eq_double_double,  G_TYPE_BOOLEAN, set_boolean, get_double, ==, get_double)
SIMPLE_OP_FUNC (ne_double_double,  G_TYPE_BOOLEAN, set_boolean, get_double, !=, get_double)
SIMPLE_OP_FUNC (gte_double_double, G_TYPE_BOOLEAN, set_boolean, get_double, >=, get_double)
SIMPLE_OP_FUNC (eq_uint_double,    G_TYPE_BOOLEAN, set_boolean, get_uint,   ==, get_double)
SIMPLE_OP_FUNC (eq_double_uint,    G_TYPE_BOOLEAN, set_boolean, get_double, ==, get_uint)
SIMPLE_OP_FUNC (ne_uint_double,    G_TYPE_BOOLEAN, set_boolean, get_uint,   !=, get_double)
SIMPLE_OP_FUNC (ne_double_uint,    G_TYPE_BOOLEAN, set_boolean, get_double, !=, get_uint)

#undef SIMPLE_OP_FUNC

static GHashTable *
build_dispatch_table (void)
{
  GHashTable *table;

  table = g_hash_table_new (NULL, NULL);

#define ADD_DISPATCH_FUNC(type, left, right, func) \
  g_hash_table_insert(table, \
                      GINT_TO_POINTER(build_hash(type, left, right)),\
                      func)

  ADD_DISPATCH_FUNC (TMPL_EXPR_ADD,         G_TYPE_DOUBLE, G_TYPE_DOUBLE, add_double_double);
  ADD_DISPATCH_FUNC (TMPL_EXPR_ADD,         G_TYPE_STRING, G_TYPE_STRING, add_string_string);
  ADD_DISPATCH_FUNC (TMPL_EXPR_SUB,         G_TYPE_DOUBLE, G_TYPE_DOUBLE, sub_double_double);
  ADD_DISPATCH_FUNC (TMPL_EXPR_MUL,         G_TYPE_DOUBLE, G_TYPE_DOUBLE, mul_double_double);
  ADD_DISPATCH_FUNC (TMPL_EXPR_DIV,         G_TYPE_DOUBLE, G_TYPE_DOUBLE, div_double_double);
  ADD_DISPATCH_FUNC (TMPL_EXPR_UNARY_MINUS, G_TYPE_DOUBLE, 0,             unary_minus_double);
  ADD_DISPATCH_FUNC (TMPL_EXPR_LT,          G_TYPE_DOUBLE, G_TYPE_DOUBLE, lt_double_double);
  ADD_DISPATCH_FUNC (TMPL_EXPR_GT,          G_TYPE_DOUBLE, G_TYPE_DOUBLE, gt_double_double);
  ADD_DISPATCH_FUNC (TMPL_EXPR_NE,          G_TYPE_DOUBLE, G_TYPE_DOUBLE, ne_double_double);
  ADD_DISPATCH_FUNC (TMPL_EXPR_LTE,         G_TYPE_DOUBLE, G_TYPE_DOUBLE, lte_double_double);
  ADD_DISPATCH_FUNC (TMPL_EXPR_GTE,         G_TYPE_DOUBLE, G_TYPE_DOUBLE, gte_double_double);
  ADD_DISPATCH_FUNC (TMPL_EXPR_EQ,          G_TYPE_DOUBLE, G_TYPE_DOUBLE, eq_double_double);
  ADD_DISPATCH_FUNC (TMPL_EXPR_EQ,          G_TYPE_UINT,   G_TYPE_DOUBLE, eq_uint_double);
  ADD_DISPATCH_FUNC (TMPL_EXPR_EQ,          G_TYPE_DOUBLE, G_TYPE_UINT,   eq_double_uint);
  ADD_DISPATCH_FUNC (TMPL_EXPR_NE,          G_TYPE_UINT,   G_TYPE_DOUBLE, ne_uint_double);
  ADD_DISPATCH_FUNC (TMPL_EXPR_NE,          G_TYPE_DOUBLE, G_TYPE_UINT,   ne_double_uint);
  ADD_DISPATCH_FUNC (TMPL_EXPR_MUL,         G_TYPE_STRING, G_TYPE_DOUBLE, mul_string_double);
  ADD_DISPATCH_FUNC (TMPL_EXPR_MUL,         G_TYPE_DOUBLE, G_TYPE_STRING, mul_double_string);
  ADD_DISPATCH_FUNC (TMPL_EXPR_EQ,          G_TYPE_STRING, G_TYPE_STRING, eq_string_string);
  ADD_DISPATCH_FUNC (TMPL_EXPR_NE,          G_TYPE_STRING, G_TYPE_STRING, ne_string_string);

#undef ADD_DISPATCH_FUNC

  return table;
}

gboolean
tmpl_expr_eval (TmplExpr   *node,
                TmplScope  *scope,
                GValue     *return_value,
                GError    **error)
{
  gboolean ret;

  g_return_val_if_fail (node != NULL, FALSE);
  g_return_val_if_fail (scope != NULL, FALSE);
  g_return_val_if_fail (return_value != NULL, FALSE);
  g_return_val_if_fail (G_VALUE_TYPE (return_value) == G_TYPE_INVALID, FALSE);

  if (g_once_init_enter (&fast_dispatch))
    g_once_init_leave (&fast_dispatch, build_dispatch_table ());

  ret = tmpl_expr_eval_internal (node, scope, return_value, error);

  g_assert (ret == TRUE || (error == NULL || *error != NULL));

  return ret;
}

static gboolean
builtin_abs (const GValue  *value,
             GValue        *return_value,
             GError       **error)
{
  throw_type_mismatch (error, value, NULL, "not implemented");
  return FALSE;
}

static gboolean
builtin_ceil (const GValue  *value,
              GValue        *return_value,
              GError       **error)
{
  if (G_VALUE_HOLDS_DOUBLE (value))
    {
      g_value_init (return_value, G_TYPE_DOUBLE);
      g_value_set_double (return_value, ceil (g_value_get_double (value)));
      return TRUE;
    }

  throw_type_mismatch (error, value, NULL, "requires double parameter");

  return FALSE;
}

static gboolean
builtin_floor (const GValue  *value,
               GValue        *return_value,
               GError       **error)
{
  if (G_VALUE_HOLDS_DOUBLE (value))
    {
      g_value_init (return_value, G_TYPE_DOUBLE);
      g_value_set_double (return_value, floor (g_value_get_double (value)));
      return TRUE;
    }

  throw_type_mismatch (error, value, NULL, "requires double parameter");

  return FALSE;
}

static gboolean
builtin_log (const GValue  *value,
             GValue        *return_value,
             GError       **error)
{
  if (G_VALUE_HOLDS_DOUBLE (value))
    {
      g_value_init (return_value, G_TYPE_DOUBLE);
      g_value_set_double (return_value, log (g_value_get_double (value)));
      return TRUE;
    }

  throw_type_mismatch (error, value, NULL, "requires double parameter");

  return FALSE;
}

static gboolean
builtin_print (const GValue  *value,
               GValue        *return_value,
               GError       **error)
{
  gchar *repr;

  repr = tmpl_value_repr (value);
  g_print ("%s\n", repr);
  g_free (repr);

  g_value_init (return_value, G_TYPE_BOOLEAN);
  g_value_set_boolean (return_value, TRUE);

  return TRUE;
}

static gboolean
builtin_sqrt (const GValue  *value,
              GValue        *return_value,
              GError       **error)
{
  if (G_VALUE_HOLDS_DOUBLE (value))
    {
      g_value_init (return_value, G_TYPE_DOUBLE);
      g_value_set_double (return_value, sqrt (g_value_get_double (value)));
      return TRUE;
    }

  throw_type_mismatch (error, value, NULL, "requires double parameter");

  return FALSE;
}

static gboolean
builtin_hex (const GValue  *value,
             GValue        *return_value,
             GError       **error)
{
  if (G_VALUE_HOLDS_DOUBLE (value))
    {
      gchar *str = g_strdup_printf ("0x%" G_GINT64_MODIFIER "x",
                                    (gint64)g_value_get_double (value));
      g_value_init (return_value, G_TYPE_STRING);
      g_value_take_string (return_value, str);
      return TRUE;
    }

  throw_type_mismatch (error, value, NULL, "requires number parameter");

  return FALSE;
}

static gboolean
builtin_repr (const GValue  *value,
              GValue        *return_value,
              GError       **error)
{
  g_value_init (return_value, G_TYPE_STRING);
  g_value_take_string (return_value, tmpl_value_repr (value));
  return TRUE;
}
