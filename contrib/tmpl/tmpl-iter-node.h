/* tmpl-iter-node.h
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

#ifndef TMPL_ITER_NODE_H
#define TMPL_ITER_NODE_H

#include "tmpl-expr.h"
#include "tmpl-node.h"

G_BEGIN_DECLS

#define TMPL_TYPE_ITER_NODE (tmpl_iter_node_get_type())

G_DECLARE_FINAL_TYPE (TmplIterNode, tmpl_iter_node, TMPL, ITER_NODE, TmplNode)

TmplNode    *tmpl_iter_node_new            (const gchar  *identifier,
                                            TmplExpr     *expr);
TmplExpr    *tmpl_iter_node_get_expr       (TmplIterNode *self);
const gchar *tmpl_iter_node_get_identifier (TmplIterNode *self);

G_END_DECLS

#endif /* TMPL_ITER_NODE_H */
