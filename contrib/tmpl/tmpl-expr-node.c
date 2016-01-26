/* tmpl-expr-node.c
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

#define G_LOG_DOMAIN "tmpl-expr-node"

#include "tmpl-debug.h"
#include "tmpl-expr-node.h"

struct _TmplExprNode
{
  TmplNode  parent_instance;
  TmplExpr *expr;
};

G_DEFINE_TYPE (TmplExprNode, tmpl_expr_node, TMPL_TYPE_NODE)

static gboolean
tmpl_expr_node_accept (TmplNode      *node,
                       TmplLexer     *lexer,
                       GCancellable  *cancellable,
                       GError       **error)
{
  TMPL_ENTRY;
  /* no children */
  TMPL_RETURN (TRUE);
}

static void
tmpl_expr_node_visit_children (TmplNode        *node,
                               TmplNodeVisitor  visitor,
                               gpointer         user_data)
{
  TMPL_ENTRY;
  /* no children */
  TMPL_EXIT;
}

static void
tmpl_expr_node_finalize (GObject *object)
{
  TmplExprNode *self = (TmplExprNode *)object;

  g_clear_pointer (&self->expr, tmpl_expr_unref);

  G_OBJECT_CLASS (tmpl_expr_node_parent_class)->finalize (object);
}

static void
tmpl_expr_node_class_init (TmplExprNodeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  TmplNodeClass *node_class = TMPL_NODE_CLASS (klass);

  object_class->finalize = tmpl_expr_node_finalize;

  node_class->accept = tmpl_expr_node_accept;
  node_class->visit_children = tmpl_expr_node_visit_children;
}

static void
tmpl_expr_node_init (TmplExprNode *self)
{
}

/**
 * tmpl_expr_node_new:
 * @expr: (transfer full): The expression
 *
 * Creates a new node, stealing the reference to @expr.
 *
 * Returns: (transfer full): A #TmplExprNode
 */
TmplNode *
tmpl_expr_node_new (TmplExpr *expr)
{
  TmplExprNode *self;

  self = g_object_new (TMPL_TYPE_EXPR_NODE, NULL);
  self->expr = expr;

  return TMPL_NODE (self);
}

/**
 * tmpl_expr_node_get_expr:
 *
 * Gets the expression.
 *
 * Returns: (transfer none): A #TmplExpr.
 */
TmplExpr *
tmpl_expr_node_get_expr (TmplExprNode *self)
{
  g_return_val_if_fail (TMPL_IS_EXPR_NODE (self), NULL);

  return self->expr;
}
