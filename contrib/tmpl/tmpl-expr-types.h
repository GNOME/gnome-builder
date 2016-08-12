/* tmpl-expr-types.h
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

#if !defined (TMPL_GLIB_INSIDE) && !defined (TMPL_GLIB_COMPILATION)
# error "Only <tmpl-glib.h> can be included directly."
#endif

#ifndef TMPL_EXPR_TYPES_H
#define TMPL_EXPR_TYPES_H

#include <glib-object.h>

#include "tmpl-enums.h"

G_BEGIN_DECLS

#define TMPL_TYPE_EXPR   (tmpl_expr_get_type())
#define TMPL_TYPE_SCOPE  (tmpl_scope_get_type())
#define TMPL_TYPE_SYMBOL (tmpl_symbol_get_type())

typedef union  _TmplExpr   TmplExpr;
typedef struct _TmplScope  TmplScope;
typedef struct _TmplSymbol TmplSymbol;

typedef enum
{
  TMPL_SYMBOL_EXPR,
  TMPL_SYMBOL_VALUE,
} TmplSymbolType;

typedef enum
{
  TMPL_EXPR_ADD = 1,
  TMPL_EXPR_SUB,
  TMPL_EXPR_MUL,
  TMPL_EXPR_DIV,
  TMPL_EXPR_BOOLEAN,
  TMPL_EXPR_NUMBER,
  TMPL_EXPR_STRING,
  TMPL_EXPR_GT,
  TMPL_EXPR_LT,
  TMPL_EXPR_NE,
  TMPL_EXPR_EQ,
  TMPL_EXPR_GTE,
  TMPL_EXPR_LTE,
  TMPL_EXPR_UNARY_MINUS,
  TMPL_EXPR_STMT_LIST,
  TMPL_EXPR_IF,
  TMPL_EXPR_WHILE,
  TMPL_EXPR_SYMBOL_REF,
  TMPL_EXPR_SYMBOL_ASSIGN,
  TMPL_EXPR_FN_CALL,
  TMPL_EXPR_USER_FN_CALL,
  TMPL_EXPR_GETATTR,
  TMPL_EXPR_SETATTR,
  TMPL_EXPR_GI_CALL,
  TMPL_EXPR_REQUIRE,
  TMPL_EXPR_AND,
  TMPL_EXPR_OR,
  TMPL_EXPR_INVERT_BOOLEAN,
} TmplExprType;

typedef enum
{
  TMPL_EXPR_BUILTIN_ABS,
  TMPL_EXPR_BUILTIN_CEIL,
  TMPL_EXPR_BUILTIN_FLOOR,
  TMPL_EXPR_BUILTIN_HEX,
  TMPL_EXPR_BUILTIN_LOG,
  TMPL_EXPR_BUILTIN_PRINT,
  TMPL_EXPR_BUILTIN_REPR,
  TMPL_EXPR_BUILTIN_SQRT,
} TmplExprBuiltin;

GType tmpl_expr_get_type   (void);
GType tmpl_scope_get_type  (void);
GType tmpl_symbol_get_type (void);

G_END_DECLS

#endif /* TMPL_EXPR_TYPES_H */
