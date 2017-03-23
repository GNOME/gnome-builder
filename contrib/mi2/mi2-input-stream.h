/* mi2-input-stream.h
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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

#ifndef MI2_INPUT_STREAM_H
#define MI2_INPUT_STREAM_H

#include <gio/gio.h>

#include "mi2-message.h"

G_BEGIN_DECLS

#define MI2_TYPE_INPUT_STREAM (mi2_input_stream_get_type())

G_DECLARE_DERIVABLE_TYPE (Mi2InputStream, mi2_input_stream, MI2, INPUT_STREAM, GDataInputStream)

struct _Mi2InputStreamClass
{
  GDataInputStreamClass parent_class;

  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
  gpointer _reserved5;
  gpointer _reserved6;
  gpointer _reserved7;
  gpointer _reserved8;
};

Mi2InputStream *mi2_input_stream_new                 (GInputStream         *base_stream);
void            mi2_input_stream_read_message_async  (Mi2InputStream       *self,
                                                      GCancellable         *cancellable,
                                                      GAsyncReadyCallback   callback,
                                                      gpointer              user_data);
Mi2Message     *mi2_input_stream_read_message_finish (Mi2InputStream       *self,
                                                      GAsyncResult         *result,
                                                      GError              **error);

G_END_DECLS

#endif /* MI2_INPUT_STREAM_H */
