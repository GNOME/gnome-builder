/* tmpl-condition-node.c
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

#define G_LOG_DOMAIN "tmpl-condition-node"

#include "tmpl-condition-node.h"
#include "tmpl-debug.h"
#include "tmpl-error.h"

struct _TmplConditionNode
{
  TmplNode   parent_instance;

  GPtrArray *children;
  TmplExpr  *condition;
};

G_DEFINE_TYPE (TmplConditionNode, tmpl_condition_node, TMPL_TYPE_NODE)

static gboolean
tmpl_condition_node_accept (TmplNode      *node,
                            TmplLexer     *lexer,
                            GCancellable  *cancellable,
                            GError       **error)
{
  TmplConditionNode *self = (TmplConditionNode *)node;
  TmplToken *token = NULL;

  TMPL_ENTRY;

  g_assert (TMPL_IS_NODE (node));
  g_assert (lexer != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  /*
   * We are an if/else if/else condition, if we come across an
   * end/else if/else then we need to unget and allow the parent
   * branch to resolve.
   */

  while (TRUE)
    {
      TmplNode *child;

      if (!tmpl_lexer_next (lexer, &token, cancellable, error))
        TMPL_RETURN (FALSE);

      switch (tmpl_token_type (token))
        {
        case TMPL_TOKEN_EOF:
          g_set_error (error,
                       TMPL_ERROR,
                       TMPL_ERROR_SYNTAX_ERROR,
                       "Unexpected end-of-file reached.");
          TMPL_RETURN (FALSE);

        case TMPL_TOKEN_ELSE_IF:
        case TMPL_TOKEN_ELSE:
        case TMPL_TOKEN_END:
          tmpl_lexer_unget (lexer, token);
          TMPL_RETURN (TRUE);

        case TMPL_TOKEN_TEXT:
        case TMPL_TOKEN_IF:
        case TMPL_TOKEN_FOR:
        case TMPL_TOKEN_EXPRESSION:
          child = tmpl_node_new_for_token (token, error);
          tmpl_token_free (token);

          if (child == NULL)
            TMPL_RETURN (FALSE);

          if (self->children == NULL)
            self->children = g_ptr_array_new_with_free_func (g_object_unref);

          g_ptr_array_add (self->children, child);

          if (!tmpl_node_accept (child, lexer, cancellable, error))
            TMPL_RETURN (FALSE);

          break;

        case TMPL_TOKEN_INCLUDE:
        default:
          g_set_error (error,
                       TMPL_ERROR,
                       TMPL_ERROR_SYNTAX_ERROR,
                       "Invalid token type");
          TMPL_RETURN (FALSE);
        }
    }

  g_assert_not_reached ();
}

static void
tmpl_condition_node_visit_children (TmplNode        *node,
                                    TmplNodeVisitor  visitor,
                                    gpointer         user_data)
{
  TmplConditionNode *self = (TmplConditionNode *)node;

  g_assert (TMPL_IS_CONDITION_NODE (self));
  g_assert (visitor != NULL);

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
tmpl_condition_node_finalize (GObject *object)
{
  TmplConditionNode *self = (TmplConditionNode *)object;

  g_clear_pointer (&self->condition, tmpl_expr_unref);
  g_clear_pointer (&self->children, g_ptr_array_unref);

  G_OBJECT_CLASS (tmpl_condition_node_parent_class)->finalize (object);
}

static void
tmpl_condition_node_class_init (TmplConditionNodeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  TmplNodeClass *node_class = TMPL_NODE_CLASS (klass);

  object_class->finalize = tmpl_condition_node_finalize;

  node_class->accept = tmpl_condition_node_accept;
  node_class->visit_children = tmpl_condition_node_visit_children;
}

static void
tmpl_condition_node_init (TmplConditionNode *self)
{
}

TmplNode *
tmpl_condition_node_new (TmplExpr *condition)
{
  TmplConditionNode *self;

  self = g_object_new (TMPL_TYPE_CONDITION_NODE, NULL);
  self->condition = condition;

  return TMPL_NODE (self);
}

TmplExpr *
tmpl_condition_node_get_condition (TmplConditionNode *self)
{
  g_return_val_if_fail (TMPL_IS_CONDITION_NODE (self), NULL);

  return self->condition;
}
