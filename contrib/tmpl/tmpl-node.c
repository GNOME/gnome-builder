/* tmpl-node.c
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

#define G_LOG_DOMAIN "tmpl-node"

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>

#include "tmpl-branch-node.h"
#include "tmpl-debug.h"
#include "tmpl-error.h"
#include "tmpl-expr-node.h"
#include "tmpl-iter-node.h"
#include "tmpl-node.h"
#include "tmpl-parser.h"
#include "tmpl-text-node.h"

typedef struct
{
  GPtrArray *children;
} TmplNodePrivate;

typedef struct
{
  GString *str;
  gint     depth;
} TmplNodePrintf;

static void tmpl_node_printf_string (TmplNode *self,
                                     GString  *str,
                                     gint      depth);

G_DEFINE_TYPE_WITH_PRIVATE (TmplNode, tmpl_node, G_TYPE_OBJECT)

static gboolean
tmpl_node_real_accept (TmplNode      *self,
                       TmplLexer     *lexer,
                       GCancellable  *cancellable,
                       GError       **error)
{
  TmplNodePrivate *priv = tmpl_node_get_instance_private (self);
  TmplToken *token = NULL;
  TmplNode *child;

  TMPL_ENTRY;

  g_assert (TMPL_IS_NODE (self));
  g_assert (lexer != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  while (TRUE)
    {
      if (!tmpl_lexer_next (lexer, &token, cancellable, error))
        TMPL_RETURN (FALSE);

      switch (tmpl_token_type (token))
        {
        case TMPL_TOKEN_TEXT:
        case TMPL_TOKEN_EXPRESSION:
        case TMPL_TOKEN_IF:
        case TMPL_TOKEN_FOR:
          if (!(child = tmpl_node_new_for_token (token, error)))
            {
              tmpl_token_free (token);
              TMPL_RETURN (FALSE);
            }

          tmpl_token_free (token);

          if (priv->children == NULL)
            priv->children = g_ptr_array_new_with_free_func (g_object_unref);
          g_ptr_array_add (priv->children, child);

          if (!tmpl_node_accept (child, lexer, cancellable, error))
            TMPL_RETURN (FALSE);

          break;

        case TMPL_TOKEN_EOF:
          TMPL_RETURN (TRUE);

        case TMPL_TOKEN_ELSE_IF:
        case TMPL_TOKEN_ELSE:
        case TMPL_TOKEN_END:
        case TMPL_TOKEN_INCLUDE:
        default:
          tmpl_token_free (token);
          g_set_error (error,
                       TMPL_ERROR,
                       TMPL_ERROR_SYNTAX_ERROR,
                       "Received invalid token from lexer");
          TMPL_RETURN (FALSE);
        }
    }

  g_assert_not_reached ();
}

static void
tmpl_node_real_visit_children (TmplNode        *self,
                               TmplNodeVisitor  visitor,
                               gpointer         user_data)
{
  TmplNodePrivate *priv = tmpl_node_get_instance_private (self);
  gint i;

  TMPL_ENTRY;

  g_assert (TMPL_IS_NODE (self));
  g_assert (visitor != NULL);

  if (priv->children != NULL)
    {
      for (i = 0; i < priv->children->len; i++)
        {
          TmplNode *child = g_ptr_array_index (priv->children, i);

          visitor (child, user_data);
        }
    }

  TMPL_EXIT;
}

static void
tmpl_node_finalize (GObject *object)
{
  TmplNode *self = (TmplNode *)object;
  TmplNodePrivate *priv = tmpl_node_get_instance_private (self);

  g_clear_pointer (&priv->children, g_ptr_array_unref);

  G_OBJECT_CLASS (tmpl_node_parent_class)->finalize (object);
}

static void
tmpl_node_class_init (TmplNodeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = tmpl_node_finalize;

  klass->accept = tmpl_node_real_accept;
  klass->visit_children = tmpl_node_real_visit_children;
}

static void
tmpl_node_init (TmplNode *self)
{
}

TmplNode *
tmpl_node_new (void)
{
  return g_object_new (TMPL_TYPE_NODE, NULL);
}

gboolean
tmpl_node_accept (TmplNode      *self,
                  TmplLexer     *lexer,
                  GCancellable  *cancellable,
                  GError       **error)
{
  g_return_val_if_fail (TMPL_IS_NODE (self), FALSE);
  g_return_val_if_fail (lexer != NULL, FALSE);

  return TMPL_NODE_GET_CLASS (self)->accept (self, lexer, cancellable, error);
}

void
tmpl_node_visit_children (TmplNode        *self,
                          TmplNodeVisitor  visitor,
                          gpointer         user_data)
{
  g_return_if_fail (TMPL_IS_NODE (self));
  g_return_if_fail (visitor != NULL);

  return TMPL_NODE_GET_CLASS (self)->visit_children (self, visitor, user_data);
}

TmplNode *
tmpl_node_new_for_token (TmplToken  *token,
                         GError    **error)
{
  TmplNode *ret;

  TMPL_ENTRY;

  g_return_val_if_fail (token != NULL, NULL);

  switch (tmpl_token_type (token))
    {
    case TMPL_TOKEN_TEXT:
      ret = tmpl_text_node_new (g_strdup (tmpl_token_get_text (token)));
      TMPL_RETURN (ret);

    case TMPL_TOKEN_IF:
      {
        TmplExpr *expr;
        const gchar *exprstr;

        exprstr = tmpl_token_get_text (token);

        if (!(expr = tmpl_expr_from_string (exprstr, error)))
          TMPL_RETURN (NULL);

        ret = tmpl_branch_node_new (expr);
        TMPL_RETURN (ret);
      }

    case TMPL_TOKEN_FOR:
      {
        const gchar *item_in_expr;
        TmplExpr *expr;
        TmplNode *node = NULL;
        char *item = NULL;
        char *exprstr = NULL;

        if (!(item_in_expr = tmpl_token_get_text (token)))
          {
            g_set_error (error,
                         TMPL_ERROR,
                         TMPL_ERROR_SYNTAX_ERROR,
                         "Invalid for expression");
            TMPL_RETURN (NULL);
          }

        if (2 != sscanf (item_in_expr, "%ms in %ms", &item, &exprstr))
          {
            g_set_error (error,
                         TMPL_ERROR,
                         TMPL_ERROR_SYNTAX_ERROR,
                         "Invalid for expression: %s", item_in_expr);
            goto for_cleanup;
          }

        if (!(expr = tmpl_expr_from_string (exprstr, error)))
          goto for_cleanup;

        node = tmpl_iter_node_new (item, expr);

      for_cleanup:
        free (item);
        free (exprstr);

        return node;
      }

    case TMPL_TOKEN_EXPRESSION:
      {
        TmplExpr *expr;
        const gchar *exprstr;

        exprstr = tmpl_token_get_text (token);

        if (!(expr = tmpl_expr_from_string (exprstr, error)))
          TMPL_RETURN (NULL);

        ret = tmpl_expr_node_new (expr);
        TMPL_RETURN (ret);
      }

    case TMPL_TOKEN_ELSE_IF:
    case TMPL_TOKEN_ELSE:
    case TMPL_TOKEN_END:
    case TMPL_TOKEN_INCLUDE:
    case TMPL_TOKEN_EOF:
    default:
      g_assert_not_reached ();
      TMPL_RETURN (NULL);
    }
}

static void
tmpl_node_printf_visitor (TmplNode *node,
                          gpointer  user_data)
{
  TmplNodePrintf *state = user_data;

  g_assert (TMPL_IS_NODE (node));
  g_assert (state != NULL);
  g_assert (state->str != NULL);
  g_assert (state->depth > 0);

  tmpl_node_printf_string (node, state->str, state->depth);
}

static void
tmpl_node_printf_string (TmplNode *self,
                         GString  *str,
                         gint      depth)
{
  TmplNodePrintf state = { str, depth + 1 };
  gint i;

  g_assert (TMPL_IS_NODE (self));
  g_assert (str != NULL);

  for (i = 0; i < depth; i++)
    g_string_append (str, "  ");
  g_string_append (str, G_OBJECT_TYPE_NAME (self));
  g_string_append_c (str, '\n');

  tmpl_node_visit_children (self,
                            tmpl_node_printf_visitor,
                            &state);
}

gchar *
tmpl_node_printf (TmplNode *self)
{
  GString *str;

  g_return_val_if_fail (TMPL_IS_NODE (self), NULL);

  str = g_string_new (NULL);
  tmpl_node_printf_string (self, str, 0);

  return g_string_free (str, FALSE);
}
