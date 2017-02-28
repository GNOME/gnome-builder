/* tmpl-iter-node.c
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

#define G_LOG_DOMAIN "tmpl-iter-node"

#include "tmpl-debug.h"
#include "tmpl-error.h"
#include "tmpl-iter-node.h"

struct _TmplIterNode
{
  TmplNode   parent_instance;

  gchar     *identifier;
  TmplExpr  *expr;
  GPtrArray *children;
};

G_DEFINE_TYPE (TmplIterNode, tmpl_iter_node, TMPL_TYPE_NODE)

static gboolean
tmpl_iter_node_accept (TmplNode      *node,
                       TmplLexer     *lexer,
                       GCancellable  *cancellable,
                       GError       **error)
{
  TmplIterNode *self = (TmplIterNode *)node;

  TMPL_ENTRY;

  g_assert (TMPL_IS_ITER_NODE (self));
  g_assert (lexer != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  while (TRUE)
    {
      TmplToken *token;
      TmplNode *child;

      if (!tmpl_lexer_next (lexer, &token, cancellable, error))
        TMPL_RETURN (FALSE);

      switch (tmpl_token_type (token))
        {
        case TMPL_TOKEN_EOF:
          tmpl_token_free (token);
          g_set_error (error,
                       TMPL_ERROR,
                       TMPL_ERROR_SYNTAX_ERROR,
                       "Unexpectedly reached end of file");
          TMPL_RETURN (FALSE);

        case TMPL_TOKEN_END:
          tmpl_token_free (token);
          TMPL_RETURN (TRUE);

        case TMPL_TOKEN_TEXT:
        case TMPL_TOKEN_IF:
        case TMPL_TOKEN_ELSE:
        case TMPL_TOKEN_ELSE_IF:
        case TMPL_TOKEN_FOR:
        case TMPL_TOKEN_EXPRESSION:
        case TMPL_TOKEN_INCLUDE:
        default:
          if (!(child = tmpl_node_new_for_token (token, error)))
            {
              tmpl_token_free (token);
              TMPL_RETURN (FALSE);
            }

          g_ptr_array_add (self->children, child);
          tmpl_token_free (token);

          if (!tmpl_node_accept (child, lexer, cancellable, error))
            TMPL_RETURN (FALSE);

          break;
        }
    }

  g_assert_not_reached ();
}

static void
tmpl_iter_node_visit_children (TmplNode        *node,
                               TmplNodeVisitor  visitor,
                               gpointer         user_data)
{
  TmplIterNode *self = (TmplIterNode *)node;

  g_assert (TMPL_IS_ITER_NODE (self));
  g_assert (visitor != NULL);

  for (guint i = 0; i < self->children->len; i++)
    {
      TmplNode *child = g_ptr_array_index (self->children, i);

      visitor (child, user_data);
    }
}

static void
tmpl_iter_node_finalize (GObject *object)
{
  TmplIterNode *self = (TmplIterNode *)object;

  g_clear_pointer (&self->identifier, g_free);
  g_clear_pointer (&self->expr, tmpl_expr_unref);
  g_clear_pointer (&self->children, g_ptr_array_unref);

  G_OBJECT_CLASS (tmpl_iter_node_parent_class)->finalize (object);
}

static void
tmpl_iter_node_class_init (TmplIterNodeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  TmplNodeClass *node_class = TMPL_NODE_CLASS (klass);

  object_class->finalize = tmpl_iter_node_finalize;

  node_class->accept = tmpl_iter_node_accept;
  node_class->visit_children = tmpl_iter_node_visit_children;
}

static void
tmpl_iter_node_init (TmplIterNode *self)
{
  self->children = g_ptr_array_new_with_free_func (g_object_unref);
}

/**
 * tmpl_iter_node_new:
 * @identifier: the name of the variable inside the loop.
 * @expr: (transfer full): A #TmplExpr.
 *
 * Returns: (transfer full): A #TmplIterNode.
 */
TmplNode *
tmpl_iter_node_new (const gchar *identifier,
                    TmplExpr    *expr)
{
  TmplIterNode *self;

  g_return_val_if_fail (expr != NULL, NULL);

  self = g_object_new (TMPL_TYPE_ITER_NODE, NULL);
  self->identifier = g_strdup (identifier);
  self->expr = expr;

  return TMPL_NODE (self);
}

/**
 * tmpl_iter_node_get_expr:
 *
 * Returns: (transfer none): An #TmplExpr.
 */
TmplExpr *
tmpl_iter_node_get_expr (TmplIterNode *self)
{
  g_return_val_if_fail (TMPL_IS_ITER_NODE (self), NULL);

  return self->expr;
}

const gchar *
tmpl_iter_node_get_identifier (TmplIterNode *self)
{
  g_return_val_if_fail (TMPL_IS_ITER_NODE (self), NULL);

  return self->identifier;
}
