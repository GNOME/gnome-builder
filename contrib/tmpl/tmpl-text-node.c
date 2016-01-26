/* tmpl-text-node.c
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

#define G_LOG_DOMAIN "tmpl-text-node"

#include "tmpl-debug.h"
#include "tmpl-text-node.h"

struct _TmplTextNode
{
  TmplNode  parent_instance;
  gchar    *text;
};

G_DEFINE_TYPE (TmplTextNode, tmpl_text_node, TMPL_TYPE_NODE)

static gboolean
tmpl_text_node_accept (TmplNode      *node,
                       TmplLexer     *lexer,
                       GCancellable  *cancellable,
                       GError       **error)
{
  TMPL_ENTRY;
  /* no children */
  TMPL_RETURN (TRUE);
}

static void
tmpl_text_node_visit_children (TmplNode        *node,
                               TmplNodeVisitor  visitor,
                               gpointer         user_data)
{
  TMPL_ENTRY;
  /* no children */
  TMPL_EXIT;
}

static void
tmpl_text_node_finalize (GObject *object)
{
  TmplTextNode *self = (TmplTextNode *)object;

  g_clear_pointer (&self->text, g_free);

  G_OBJECT_CLASS (tmpl_text_node_parent_class)->finalize (object);
}

static void
tmpl_text_node_class_init (TmplTextNodeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  TmplNodeClass *node_class = TMPL_NODE_CLASS (klass);

  object_class->finalize = tmpl_text_node_finalize;

  node_class->accept = tmpl_text_node_accept;
  node_class->visit_children = tmpl_text_node_visit_children;
}

static void
tmpl_text_node_init (TmplTextNode *node)
{
}

/**
 * tmpl_text_node_new:
 * @text: (transfer full): the text for the node
 *
 * Creates a new text node.
 *
 * Returns: (transfer full): the new node.
 */
TmplNode *
tmpl_text_node_new (gchar *text)
{
  TmplTextNode *self;

  self = g_object_new (TMPL_TYPE_TEXT_NODE, NULL);
  self->text = text;

  return TMPL_NODE (self);
}

const gchar *
tmpl_text_node_get_text (TmplTextNode *self)
{
  g_return_val_if_fail (TMPL_IS_TEXT_NODE (self), NULL);

  return self->text;
}
