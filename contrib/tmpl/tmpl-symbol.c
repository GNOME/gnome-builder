/* tmpl-symbol.c
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
#include "tmpl-symbol.h"

G_DEFINE_BOXED_TYPE (TmplSymbol, tmpl_symbol, tmpl_symbol_ref, tmpl_symbol_unref)

struct _TmplSymbol
{
  volatile gint  ref_count;
  TmplSymbolType type;
  union {
    GValue    value;
    struct {
      TmplExpr  *expr;
      GPtrArray *params;
    } expr;
  } u;
};

TmplSymbol *
tmpl_symbol_new (void)
{
  TmplSymbol *self;

  self = g_slice_new0 (TmplSymbol);
  self->ref_count = 1;
  self->type = TMPL_SYMBOL_VALUE;

  return self;
}

static inline void
tmpl_symbol_clear (TmplSymbol *self)
{
  if ((self->type == TMPL_SYMBOL_VALUE) &&
      (G_VALUE_TYPE (&self->u.value) != G_TYPE_INVALID))
    g_value_unset (&self->u.value);
  else if (self->type == TMPL_SYMBOL_EXPR)
    {
      g_clear_pointer (&self->u.expr.expr, tmpl_expr_unref);
      g_clear_pointer (&self->u.expr.params, g_ptr_array_unref);
    }
}

TmplSymbol *
tmpl_symbol_ref (TmplSymbol *self)
{
  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (self->ref_count > 0, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

void
tmpl_symbol_unref (TmplSymbol *self)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (self->ref_count > 0);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    {
      tmpl_symbol_clear (self);
      g_slice_free (TmplSymbol, self);
    }
}

void
tmpl_symbol_assign_value (TmplSymbol   *self,
                          const GValue *value)
{
  g_return_if_fail (self != NULL);

  tmpl_symbol_clear (self);

  self->type = TMPL_SYMBOL_VALUE;

  if ((value != NULL) && (G_VALUE_TYPE (value) != G_TYPE_INVALID))
    {
      g_value_init (&self->u.value, G_VALUE_TYPE (value));
      g_value_copy (value, &self->u.value);
    }
}

/**
 * tmpl_symbol_assign_expr: (skip)
 * @self: A #TmplSymbol.
 * @expr: (nullable): An expression to assign, or %NULL.
 * params: (element-type utf8): A #GPtrArray of strings.
 *
 * Sets the symbol as a %TMPL_SYMBOL_EXPR with the given ordered and
 * named parameters.
 */
void
tmpl_symbol_assign_expr (TmplSymbol *self,
                         TmplExpr   *expr,
                         GPtrArray  *params)
{
  g_return_if_fail (self != NULL);

  tmpl_symbol_clear (self);

  self->type = TMPL_SYMBOL_EXPR;

  if (expr != NULL)
    self->u.expr.expr = tmpl_expr_ref (expr);

  if (params != NULL)
    self->u.expr.params = g_ptr_array_ref (params);
}

TmplSymbolType
tmpl_symbol_get_symbol_type (TmplSymbol *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return self->type;
}

/**
 * tmpl_symbol_get_expr:
 * @self: A #TmplSymbol
 * @params: (out) (element-type utf8) (transfer none) (nullable): A list of parameters
 *
 * Returns: (transfer none): A #TmplExpr.
 */
TmplExpr *
tmpl_symbol_get_expr (TmplSymbol  *self,
                      GPtrArray  **params)
{
  g_return_val_if_fail (self != NULL, 0);

  if (self->type != TMPL_SYMBOL_EXPR)
    {
      g_warning ("Attempt to fetch TmplExpr from a value symbol");
      return NULL;
    }

  if (params != NULL)
    *params = self->u.expr.params;

  return self->u.expr.expr;
}

void
tmpl_symbol_get_value (TmplSymbol *self,
                       GValue     *value)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (value != NULL);

  if (self->type != TMPL_SYMBOL_VALUE)
    {
      g_warning ("Attempt to fetch value from a expr symbol");
      return;
    }

  if (G_VALUE_TYPE (&self->u.value) != G_TYPE_INVALID)
    {
      g_value_init (value, G_VALUE_TYPE (&self->u.value));
      g_value_copy (&self->u.value, value);
    }
}

void
tmpl_symbol_assign_boolean (TmplSymbol *self,
                            gboolean    v_bool)
{
  GValue value = G_VALUE_INIT;

  g_return_if_fail (self != NULL);

  g_value_init (&value, G_TYPE_BOOLEAN);
  g_value_set_boolean (&value, v_bool);
  tmpl_symbol_assign_value (self, &value);
  g_value_unset (&value);
}

void
tmpl_symbol_assign_double (TmplSymbol *self,
                           gdouble     v_double)
{
  GValue value = G_VALUE_INIT;

  g_return_if_fail (self != NULL);

  g_value_init (&value, G_TYPE_DOUBLE);
  g_value_set_double (&value, v_double);
  tmpl_symbol_assign_value (self, &value);
  g_value_unset (&value);
}

void
tmpl_symbol_assign_string (TmplSymbol  *self,
                           const gchar *v_string)
{
  GValue value = G_VALUE_INIT;

  g_return_if_fail (self != NULL);

  g_value_init (&value, G_TYPE_STRING);
  g_value_set_string (&value, v_string);
  tmpl_symbol_assign_value (self, &value);
  g_value_unset (&value);
}

void
tmpl_symbol_assign_object (TmplSymbol *self,
                           gpointer    v_object)
{
  GValue value = G_VALUE_INIT;

  g_return_if_fail (self != NULL);

  g_value_init (&value, G_TYPE_OBJECT);
  g_value_set_object (&value, v_object);
  tmpl_symbol_assign_value (self, &value);
  g_value_unset (&value);
}
