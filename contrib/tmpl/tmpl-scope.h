/* tmpl-scope.h
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

#ifndef TMPL_SCOPE_H
#define TMPL_SCOPE_H

#include "tmpl-expr-types.h"

G_BEGIN_DECLS

typedef gboolean (*TmplScopeResolver) (TmplScope    *scope,
                                       const gchar  *name,
                                       TmplSymbol  **symbol,
                                       gpointer      user_data);

TmplScope  *tmpl_scope_new             (void);
TmplScope  *tmpl_scope_new_with_parent (TmplScope         *parent);
TmplScope  *tmpl_scope_ref             (TmplScope         *self);
void        tmpl_scope_unref           (TmplScope         *self);
TmplSymbol *tmpl_scope_peek            (TmplScope         *self,
                                        const gchar       *name);
TmplSymbol *tmpl_scope_get             (TmplScope         *self,
                                        const gchar       *name);
void        tmpl_scope_set             (TmplScope         *self,
                                        const gchar       *name,
                                        TmplSymbol        *symbol);
void        tmpl_scope_set_value       (TmplScope         *self,
                                        const gchar       *name,
                                        const GValue      *symbol);
void        tmpl_scope_set_boolean     (TmplScope         *self,
                                        const gchar       *name,
                                        gboolean          symbol);
void        tmpl_scope_set_double      (TmplScope         *self,
                                        const gchar       *name,
                                        gdouble           symbol);
void        tmpl_scope_set_string      (TmplScope         *self,
                                        const gchar       *name,
                                        const gchar       *symbol);
void        tmpl_scope_set_object      (TmplScope         *self,
                                        const gchar       *name,
                                        gpointer          symbol);
void        tmpl_scope_set_resolver    (TmplScope         *self,
                                        TmplScopeResolver  resolver,
                                        gpointer           user_data,
                                        GDestroyNotify     destroy);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (TmplScope, tmpl_scope_unref)

G_END_DECLS

#endif /* TMPL_SCOPE_H */
