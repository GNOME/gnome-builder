/* jsonrpc-input-stream-private.h
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef JSONRPC_INPUT_STREAM_PRIVATE_H
#define JSONRPC_INPUT_STREAM_PRIVATE_H

#include "jsonrpc-input-stream.h"

G_BEGIN_DECLS

gboolean _jsonrpc_input_stream_get_has_seen_gvariant (JsonrpcInputStream *self);

G_END_DECLS

#endif /* JSONRPC_INPUT_STREAM_PRIVATE_H */
