/* tmpl-lexer.h
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

#ifndef TMPL_LEXER_H
#define TMPL_LEXER_H

#include <gio/gio.h>

#include "tmpl-token.h"
#include "tmpl-template-locator.h"

G_BEGIN_DECLS

typedef struct _TmplLexer TmplLexer;

GType      tmpl_lexer_get_type (void);
TmplLexer *tmpl_lexer_new      (GInputStream         *stream,
                                TmplTemplateLocator  *locator);
void       tmpl_lexer_free     (TmplLexer            *self);
void       tmpl_lexer_unget    (TmplLexer            *self,
                                TmplToken            *token);
gboolean   tmpl_lexer_next     (TmplLexer            *self,
                                TmplToken           **token,
                                GCancellable         *cancellable,
                                GError              **error);


G_END_DECLS

#endif /* TMPL_LEXER_H */
