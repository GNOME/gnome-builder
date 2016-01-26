/* tmpl-node.h
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

#ifndef TMPL_NODE_H
#define TMPL_NODE_H

#include <gio/gio.h>

#include "tmpl-lexer.h"

G_BEGIN_DECLS

#define TMPL_TYPE_NODE (tmpl_node_get_type())

G_DECLARE_DERIVABLE_TYPE (TmplNode, tmpl_node, TMPL, NODE, GObject)

typedef void (*TmplNodeVisitor) (TmplNode *self,
                                 gpointer  user_data);

struct _TmplNodeClass
{
  GObjectClass parent_class;

  gboolean (*accept)         (TmplNode        *self,
                              TmplLexer       *lexer,
                              GCancellable    *cancellable,
                              GError         **error);
  void     (*visit_children) (TmplNode        *self,
                              TmplNodeVisitor  visitor,
                              gpointer         user_data);
};

TmplNode *tmpl_node_new            (void);
TmplNode *tmpl_node_new_for_token  (TmplToken        *token,
                                    GError          **error);
gboolean  tmpl_node_accept         (TmplNode         *self,
                                    TmplLexer        *lexer,
                                    GCancellable     *cancellable,
                                    GError          **error);
gchar    *tmpl_node_printf         (TmplNode         *self);
void      tmpl_node_visit_children (TmplNode         *self,
                                    TmplNodeVisitor   visitor,
                                    gpointer          user_data);

G_END_DECLS

#endif /* TMPL_NODE_H */
