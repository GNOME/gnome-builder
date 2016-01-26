/* tmpl-branch-node.h
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

#ifndef TMPL_BRANCH_NODE_H
#define TMPL_BRANCH_NODE_H

#include "tmpl-expr.h"
#include "tmpl-node.h"
#include "tmpl-scope.h"

G_BEGIN_DECLS

#define TMPL_TYPE_BRANCH_NODE (tmpl_branch_node_get_type())

G_DECLARE_FINAL_TYPE (TmplBranchNode, tmpl_branch_node, TMPL, BRANCH_NODE, TmplNode)

TmplNode *tmpl_branch_node_new    (TmplExpr        *condition);
TmplNode *tmpl_branch_node_branch (TmplBranchNode  *self,
                                   TmplScope       *scope,
                                   GError         **error);

G_END_DECLS

#endif /* TMPL_BRANCH_NODE_H */
