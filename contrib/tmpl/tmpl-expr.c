/* tmpl-expr.c
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

#include "tmpl-expr.h"
#include "tmpl-expr-private.h"
#include "tmpl-expr-parser-private.h"

static gpointer tmpl_expr_new     (TmplExprType  type);
static void     tmpl_expr_destroy (TmplExpr     *expr);

G_DEFINE_BOXED_TYPE (TmplExpr, tmpl_expr, tmpl_expr_ref, tmpl_expr_unref)

TmplExpr *
tmpl_expr_ref (TmplExpr *self)
{
  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (self->any.ref_count > 0, NULL);

  g_atomic_int_inc (&self->any.ref_count);

  return self;
}

void
tmpl_expr_unref (TmplExpr *self)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (self->any.ref_count > 0);

  if (g_atomic_int_dec_and_test (&self->any.ref_count))
    tmpl_expr_destroy (self);
}

static gpointer
tmpl_expr_new (TmplExprType type)
{
  TmplExpr *ret;

  ret = g_slice_new0 (TmplExpr);
  ret->any.type = type;
  ret->any.ref_count = 1;

  return ret;
}

static void
tmpl_expr_destroy (TmplExpr *self)
{
  g_assert (self != NULL);
  g_assert (self->any.ref_count == 0);

  /* Set fields to NULL to aid in debugging. */

  switch (self->any.type)
    {
    case TMPL_EXPR_ADD:
    case TMPL_EXPR_DIV:
    case TMPL_EXPR_EQ:
    case TMPL_EXPR_GT:
    case TMPL_EXPR_GTE:
    case TMPL_EXPR_LT:
    case TMPL_EXPR_LTE:
    case TMPL_EXPR_MUL:
    case TMPL_EXPR_NE:
    case TMPL_EXPR_STMT_LIST:
    case TMPL_EXPR_SUB:
    case TMPL_EXPR_UNARY_MINUS:
    case TMPL_EXPR_USER_FN_CALL:
    case TMPL_EXPR_AND:
    case TMPL_EXPR_OR:
    case TMPL_EXPR_INVERT_BOOLEAN:
      g_clear_pointer (&self->simple.left, tmpl_expr_unref);
      g_clear_pointer (&self->simple.right, tmpl_expr_unref);
      break;

    case TMPL_EXPR_GETATTR:
      g_clear_pointer (&self->getattr.attr, g_free);
      g_clear_pointer (&self->getattr.left, tmpl_expr_unref);
      break;

    case TMPL_EXPR_SETATTR:
      g_clear_pointer (&self->setattr.attr, g_free);
      g_clear_pointer (&self->setattr.left, tmpl_expr_unref);
      g_clear_pointer (&self->setattr.right, tmpl_expr_unref);
      break;

    case TMPL_EXPR_BOOLEAN:
    case TMPL_EXPR_NUMBER:
      break;

    case TMPL_EXPR_STRING:
      g_clear_pointer (&self->string.value, g_free);
      break;

    case TMPL_EXPR_IF:
    case TMPL_EXPR_WHILE:
      g_clear_pointer (&self->flow.condition, tmpl_expr_unref);
      g_clear_pointer (&self->flow.primary, tmpl_expr_unref);
      g_clear_pointer (&self->flow.secondary, tmpl_expr_unref);
      break;

    case TMPL_EXPR_SYMBOL_REF:
      g_clear_pointer (&self->sym_ref.symbol, g_free);
      break;

    case TMPL_EXPR_SYMBOL_ASSIGN:
      g_clear_pointer (&self->sym_assign.symbol, g_free);
      g_clear_pointer (&self->sym_assign.right, tmpl_expr_unref);
      break;

    case TMPL_EXPR_FN_CALL:
      g_clear_pointer (&self->fn_call.param, tmpl_expr_unref);
      break;

    case TMPL_EXPR_GI_CALL:
      g_clear_pointer (&self->gi_call.name, g_free);
      g_clear_pointer (&self->gi_call.object, tmpl_expr_unref);
      g_clear_pointer (&self->gi_call.params, tmpl_expr_unref);
      break;

    case TMPL_EXPR_REQUIRE:
      g_clear_pointer (&self->require.name, g_free);
      g_clear_pointer (&self->require.version, g_free);
      break;

    default:
      g_assert_not_reached ();
    }

  g_slice_free (TmplExpr, self);
}

TmplExpr *
tmpl_expr_new_boolean (gboolean value)
{
  TmplExpr *ret;

  ret = tmpl_expr_new (TMPL_EXPR_BOOLEAN);
  ((TmplExprBoolean *)ret)->value = !!value;

  return ret;
}

TmplExpr *
tmpl_expr_new_number (gdouble value)
{
  TmplExprNumber *ret;

  ret = tmpl_expr_new (TMPL_EXPR_NUMBER);
  ret->number = value;

  return (TmplExpr *)ret;
}

TmplExpr *
tmpl_expr_new_string (const gchar *str,
                      gssize       length)
{
  TmplExprString *ret;

  ret = tmpl_expr_new (TMPL_EXPR_STRING);

  if (length < 0)
    ret->value = g_strdup (str);
  else
    ret->value = g_strndup (str, length);

  return (TmplExpr *)ret;
}

TmplExpr *
tmpl_expr_new_require (const gchar *typelib,
                       const gchar *version)
{
  TmplExprRequire *ret;

  ret = tmpl_expr_new (TMPL_EXPR_REQUIRE);
  ret->name = g_strdup (typelib);
  ret->version = g_strdup (version);

  return (TmplExpr *)ret;
}

TmplExpr *
tmpl_expr_new_simple (TmplExprType  type,
                      TmplExpr     *left,
                      TmplExpr     *right)
{
  TmplExprSimple *ret;

  ret = tmpl_expr_new (type);
  ret->left = left;
  ret->right = right;

  return (TmplExpr *)ret;
}

TmplExpr *
tmpl_expr_new_invert_boolean (TmplExpr *left)
{
  TmplExprSimple *ret;

  ret = tmpl_expr_new (TMPL_EXPR_INVERT_BOOLEAN);
  ret->left = left;

  return (TmplExpr *)ret;
}

TmplExpr *
tmpl_expr_new_flow (TmplExprType  type,
                    TmplExpr     *condition,
                    TmplExpr     *primary,
                    TmplExpr     *secondary)
{
  TmplExprFlow *ret;

  ret = tmpl_expr_new (type);
  ret->condition = condition;
  ret->primary = primary;
  ret->secondary = secondary;

  return (TmplExpr *)ret;
}

TmplExpr *
tmpl_expr_new_getattr (TmplExpr    *left,
                       const gchar *attr)
{
  TmplExprGetattr *ret;

  ret = tmpl_expr_new (TMPL_EXPR_GETATTR);
  ret->left = left;
  ret->attr = g_strdup (attr);

  return (TmplExpr *)ret;
}

TmplExpr *
tmpl_expr_new_setattr (TmplExpr    *left,
                       const gchar *attr,
                       TmplExpr    *right)
{
  TmplExprSetattr *ret;

  ret = tmpl_expr_new (TMPL_EXPR_SETATTR);
  ret->left = left;
  ret->attr = g_strdup (attr);
  ret->right = right;

  return (TmplExpr *)ret;
}

TmplExpr *
tmpl_expr_new_symbol_ref (const gchar *symbol)
{
  TmplExprSymbolRef *ret;

  ret = tmpl_expr_new (TMPL_EXPR_SYMBOL_REF);
  ret->symbol = g_strdup (symbol);

  return (TmplExpr *)ret;
}

TmplExpr *
tmpl_expr_new_symbol_assign (const gchar *symbol,
                             TmplExpr    *right)
{
  TmplExprSymbolAssign *ret;

  ret = tmpl_expr_new (TMPL_EXPR_SYMBOL_ASSIGN);
  ret->symbol = g_strdup (symbol);
  ret->right = right;

  return (TmplExpr *)ret;
}

TmplExpr *
tmpl_expr_new_fn_call (TmplExprBuiltin  builtin,
                       TmplExpr        *param)
{
  TmplExprFnCall *ret;

  ret = tmpl_expr_new (TMPL_EXPR_FN_CALL);
  ret->builtin = builtin;
  ret->param = param;

  return (TmplExpr *)ret;
}

TmplExpr *
tmpl_expr_new_user_fn_call (const gchar *symbol,
                            TmplExpr    *params)
{
  TmplExprUserFnCall *ret;

  ret = tmpl_expr_new (TMPL_EXPR_USER_FN_CALL);
  ret->symbol = g_strdup (symbol);
  ret->params = params;

  return (TmplExpr *)ret;
}

TmplExpr *
tmpl_expr_new_gi_call (TmplExpr    *object,
                       const gchar *name,
                       TmplExpr    *params)
{
  TmplExprGiCall *ret;

  ret = tmpl_expr_new (TMPL_EXPR_GI_CALL);
  ret->object = object;
  ret->name = g_strdup (name);
  ret->params = params;

  return (TmplExpr *)ret;
}

TmplExpr *
tmpl_expr_from_string (const gchar  *str,
                       GError      **error)
{
  TmplExprParser parser = { 0 };
  TmplExpr *ret = NULL;

  g_return_val_if_fail (str != NULL, NULL);

  if (!tmpl_expr_parser_init (&parser, error))
      return NULL;

  if (tmpl_expr_parser_parse_string (&parser, str, error))
    ret = parser.ast, parser.ast = NULL;

  tmpl_expr_parser_destroy (&parser);

  return ret;
}
