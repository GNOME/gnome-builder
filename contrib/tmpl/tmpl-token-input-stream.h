/* tmpl-token-input-stream.h
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

#ifndef TMPL_TOKEN_INPUT_STREAM_H
#define TMPL_TOKEN_INPUT_STREAM_H

#include <gio/gio.h>

#include "tmpl-token.h"

G_BEGIN_DECLS

#define TMPL_TYPE_TOKEN_INPUT_STREAM (tmpl_token_input_stream_get_type())

G_DECLARE_FINAL_TYPE (TmplTokenInputStream, tmpl_token_input_stream, TMPL, TOKEN_INPUT_STREAM, GDataInputStream)

TmplTokenInputStream *tmpl_token_input_stream_new        (GInputStream          *base_stream);
TmplToken            *tmpl_token_input_stream_read_token (TmplTokenInputStream  *self,
                                                          GCancellable          *cancellable,
                                                          GError               **error);

G_END_DECLS

#endif /* TMPL_TOKEN_INPUT_STREAM_H */
