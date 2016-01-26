/* tmpl-token.h
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


#ifndef TMPL_TOKEN_H
#define TMPL_TOKEN_H

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _TmplToken TmplToken;

typedef enum
{
  TMPL_TOKEN_EOF,
  TMPL_TOKEN_TEXT,
  TMPL_TOKEN_IF,
  TMPL_TOKEN_ELSE_IF,
  TMPL_TOKEN_ELSE,
  TMPL_TOKEN_END,
  TMPL_TOKEN_FOR,
  TMPL_TOKEN_EXPRESSION,
  TMPL_TOKEN_INCLUDE,
} TmplTokenType;

TmplToken     *tmpl_token_new_generic      (gchar     *str);
TmplToken     *tmpl_token_new_unichar      (gunichar   ch);
TmplToken     *tmpl_token_new_text         (gchar     *text);
TmplToken     *tmpl_token_new_eof          (void);
const gchar   *tmpl_token_get_text         (TmplToken *self);
TmplTokenType  tmpl_token_type             (TmplToken *self);
void           tmpl_token_free             (TmplToken *self);
gchar         *tmpl_token_include_get_path (TmplToken *self);

G_END_DECLS

#endif /* TMPL_TOKEN_H */
