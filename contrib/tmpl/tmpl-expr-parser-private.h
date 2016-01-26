/* tmpl-expr-parser-private.h
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

#ifndef TMPL_EXPR_PARSER_PRIVATE_H
#define TMPL_EXPR_PARSER_PRIVATE_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct
{
  TmplExpr  *ast;
  TmplScope *scope;
  gpointer   scanner;
  gchar     *error_str;
  gint       error_line;
  guint      reached_eof : 1;
} TmplExprParser;

void     tmpl_expr_parser_destroy      (TmplExprParser  *parser);
void     tmpl_expr_parser_flush        (TmplExprParser  *parser);
void     tmpl_expr_parser_error        (TmplExprParser  *parser,
                                        const char      *message);
gboolean tmpl_expr_parser_parse_string (TmplExprParser  *parser,
                                        const gchar     *input,
                                        GError         **error);
gboolean tmpl_expr_parser_init         (TmplExprParser  *parser,
                                        GError         **error);

G_END_DECLS

#endif /* TMPL_EXPR_PARSER_PRIVATE_H */
