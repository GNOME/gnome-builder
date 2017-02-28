/* tmpl-branch-node.c
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

#define G_LOG_DOMAIN "tmpl-branch-node"

#include "tmpl-branch-node.h"
#include "tmpl-condition-node.h"
#include "tmpl-debug.h"
#include "tmpl-error.h"
#include "tmpl-util-private.h"

struct _TmplBranchNode
{
  TmplNode   parent_instance;

  TmplNode  *if_branch;
  GPtrArray *children;
};

G_DEFINE_TYPE (TmplBranchNode, tmpl_branch_node, TMPL_TYPE_NODE)

static gboolean
tmpl_branch_node_accept (TmplNode      *node,
                         TmplLexer     *lexer,
                         GCancellable  *cancellable,
                         GError       **error)
{
  TmplBranchNode *self = (TmplBranchNode *)node;

  TMPL_ENTRY;

  g_assert (TMPL_IS_BRANCH_NODE (self));
  g_assert (self->if_branch != NULL);
  g_assert (lexer != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (!tmpl_node_accept (self->if_branch, lexer, cancellable, error))
    TMPL_RETURN (FALSE);

  /*
   * At this point, the if branch should have everything, so we are
   * looking for ELSE_IF, ELSE, or END. Everything else is a syntax
   * error.
   */

  while (TRUE)
    {
      TmplToken *token = NULL;

      if (!tmpl_lexer_next (lexer, &token, cancellable, error))
        TMPL_RETURN (FALSE);

      switch (tmpl_token_type (token))
        {
        case TMPL_TOKEN_EOF:
          g_set_error (error,
                       TMPL_ERROR,
                       TMPL_ERROR_SYNTAX_ERROR,
                       "Unexpected end-of-file reached");
          TMPL_RETURN (FALSE);

        case TMPL_TOKEN_END:
          tmpl_token_free (token);
          TMPL_RETURN (TRUE);

        case TMPL_TOKEN_ELSE:
        case TMPL_TOKEN_ELSE_IF:
          {
            TmplNode *child;
            TmplExpr *expr = NULL;

            if (tmpl_token_type (token) != TMPL_TOKEN_ELSE)
              {
                const gchar *exprstr;

                exprstr = tmpl_token_get_text (token);
                expr = tmpl_expr_from_string (exprstr, error);
              }
            else
              expr = tmpl_expr_new_boolean (TRUE);

            tmpl_token_free (token);

            if (expr == NULL)
              TMPL_RETURN (FALSE);

            child = tmpl_condition_node_new (expr);

            if (self->children == NULL)
              self->children = g_ptr_array_new_with_free_func (g_object_unref);
            g_ptr_array_add (self->children, child);

            if (!tmpl_node_accept (child, lexer, cancellable, error))
              TMPL_RETURN (FALSE);
          }
          break;

        case TMPL_TOKEN_TEXT:
        case TMPL_TOKEN_IF:
        case TMPL_TOKEN_EXPRESSION:
        case TMPL_TOKEN_FOR:
        case TMPL_TOKEN_INCLUDE:
        default:
          tmpl_token_free (token);
          g_set_error (error,
                       TMPL_ERROR,
                       TMPL_ERROR_SYNTAX_ERROR,
                       "Invalid token, expected else if, else, or end.");
          TMPL_RETURN (FALSE);
        }
    }
}

static void
tmpl_branch_node_visit_children (TmplNode        *node,
                                 TmplNodeVisitor  visitor,
                                 gpointer         user_data)
{
  TmplBranchNode *self = (TmplBranchNode *)node;

  g_assert (TMPL_IS_NODE (node));
  g_assert (visitor != NULL);

  if (self->if_branch)
    visitor (self->if_branch, user_data);

  if (self->children != NULL)
    {
      for (guint i = 0; i < self->children->len; i++)
        {
          TmplNode *child = g_ptr_array_index (self->children, i);

          visitor (child, user_data);
        }
    }
}

static void
tmpl_branch_node_finalize (GObject *object)
{
  TmplBranchNode *self = (TmplBranchNode *)object;

  g_clear_pointer (&self->children, g_ptr_array_unref);
  g_clear_object (&self->if_branch);

  G_OBJECT_CLASS (tmpl_branch_node_parent_class)->finalize (object);
}

static void
tmpl_branch_node_class_init (TmplBranchNodeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  TmplNodeClass *node_class = TMPL_NODE_CLASS (klass);

  object_class->finalize = tmpl_branch_node_finalize;

  node_class->accept = tmpl_branch_node_accept;
  node_class->visit_children = tmpl_branch_node_visit_children;
}

static void
tmpl_branch_node_init (TmplBranchNode *self)
{
}

/**
 * tmpl_branch_node_new:
 * @expr: (transfer full): A TmplExpr
 *
 * Creates a new branch (if, else if, else) using @expr for the (if)
 * branch.
 *
 * Returns: (transfer full): A #TmplBranchNode.
 */
TmplNode *
tmpl_branch_node_new (TmplExpr *condition)
{
  TmplBranchNode *self;

  self = g_object_new (TMPL_TYPE_BRANCH_NODE, NULL);
  self->if_branch = tmpl_condition_node_new (condition);

  return TMPL_NODE (self);
}

static gboolean
condition_matches (TmplNode   *condition,
                   TmplScope  *scope,
                   GError    **error)
{
  TmplExpr *expr;
  GValue value = G_VALUE_INIT;
  gboolean ret;

  g_assert (TMPL_IS_CONDITION_NODE (condition));

  if (!(expr = tmpl_condition_node_get_condition (TMPL_CONDITION_NODE (condition))))
    return FALSE;

  if (!tmpl_expr_eval (expr, scope, &value, error))
    return FALSE;

  ret = tmpl_value_as_boolean (&value);
  TMPL_CLEAR_VALUE (&value);
  return ret;
}

TmplNode *
tmpl_branch_node_branch (TmplBranchNode  *self,
                         TmplScope       *scope,
                         GError         **error)
{
  GError *local_error = NULL;

  TMPL_ENTRY;

  g_return_val_if_fail (TMPL_IS_BRANCH_NODE (self), NULL);
  g_return_val_if_fail (self->if_branch != NULL, NULL);

  if (condition_matches (self->if_branch, scope, &local_error))
    return self->if_branch;

  if (local_error != NULL)
    {
      g_propagate_error (error, local_error);
      TMPL_RETURN (NULL);
    }

  if (self->children != NULL)
    {
      for (guint i = 0; i < self->children->len; i++)
        {
          TmplNode *child = g_ptr_array_index (self->children, i);

          if (condition_matches (child, scope, &local_error))
            TMPL_RETURN (child);

          if (local_error != NULL)
            {
              g_propagate_error (error, local_error);
              TMPL_RETURN (NULL);
            }
        }
    }

  TMPL_RETURN (NULL);
}
