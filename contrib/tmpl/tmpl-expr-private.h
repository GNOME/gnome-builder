/* tmpl-expr-private.h
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

#ifndef TMPL_EXPR_PRIVATE_H
#define TMPL_EXPR_PRIVATE_H

#include "tmpl-expr.h"

G_BEGIN_DECLS

typedef struct
{
  TmplExprType   type;
  volatile gint  ref_count;
  TmplExpr      *left;
  TmplExpr      *right;
} TmplExprSimple;

typedef struct
{
  TmplExprType   type;
  volatile gint  ref_count;
  TmplExpr      *object;
  gchar         *name;
  TmplExpr      *params;
} TmplExprGiCall;

typedef struct
{
  TmplExprType     type;
  volatile gint    ref_count;
  TmplExprBuiltin  builtin;
  TmplExpr        *param;
} TmplExprFnCall;

typedef struct
{
  TmplExprType   type;
  volatile gint  ref_count;
  gchar         *symbol;
  TmplExpr      *params;
} TmplExprUserFnCall;

typedef struct
{
  TmplExprType   type;
  volatile gint  ref_count;
  TmplExpr      *condition;
  TmplExpr      *primary;
  TmplExpr      *secondary;
} TmplExprFlow;

typedef struct
{
  TmplExprType  type;
  volatile gint ref_count;
  gdouble       number;
} TmplExprNumber;

typedef struct
{
  TmplExprType  type;
  volatile gint ref_count;
  guint         value: 1;
} TmplExprBoolean;

typedef struct
{
  TmplExprType   type;
  volatile gint  ref_count;
  gchar         *value;
} TmplExprString;

typedef struct
{
  TmplExprType   type;
  volatile gint  ref_count;
  gchar         *symbol;
} TmplExprSymbolRef;

typedef struct
{
  TmplExprType   type;
  volatile gint  ref_count;
  gchar         *symbol;
  TmplExpr      *right;
} TmplExprSymbolAssign;

typedef struct
{
  TmplExprType   type;
  volatile gint  ref_count;
  gchar         *attr;
  TmplExpr      *left;
} TmplExprGetattr;

typedef struct
{
  TmplExprType   type;
  volatile gint  ref_count;
  gchar         *attr;
  TmplExpr      *left;
  TmplExpr      *right;
} TmplExprSetattr;

typedef struct
{
  TmplExprType   type;
  volatile gint  ref_count;
  gchar         *name;
  gchar         *version;
} TmplExprRequire;

typedef struct
{
  TmplExprType   type;
  volatile gint  ref_count;
} TmplExprAny;

union _TmplExpr
{
  TmplExprAny          any;
  TmplExprSimple       simple;
  TmplExprGiCall       gi_call;
  TmplExprFnCall       fn_call;
  TmplExprUserFnCall   user_fn_call;
  TmplExprFlow         flow;
  TmplExprNumber       number;
  TmplExprString       string;
  TmplExprSymbolRef    sym_ref;
  TmplExprSymbolAssign sym_assign;
  TmplExprGetattr      getattr;
  TmplExprSetattr      setattr;
  TmplExprRequire      require;
};

G_END_DECLS

#endif /* TMPL_EXPR_PRIVATE_H */
