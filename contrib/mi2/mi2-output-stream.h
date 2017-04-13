/* mi2-output-stream.h
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

#ifndef MI2_OUTPUT_STREAM_H
#define MI2_OUTPUT_STREAM_H

#include <gio/gio.h>

#include "mi2-message.h"

G_BEGIN_DECLS

#define MI2_TYPE_OUTPUT_STREAM (mi2_output_stream_get_type())

G_DECLARE_DERIVABLE_TYPE (Mi2OutputStream, mi2_output_stream, MI2, OUTPUT_STREAM, GDataOutputStream)

struct _Mi2OutputStreamClass
{
  GDataOutputStreamClass parent_instance;

  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
  gpointer _reserved5;
  gpointer _reserved6;
  gpointer _reserved7;
  gpointer _reserved8;
};

Mi2OutputStream *mi2_output_stream_new                  (GOutputStream        *base_stream);
void             mi2_output_stream_write_message_async  (Mi2OutputStream      *stream,
                                                         Mi2Message           *message,
                                                         GCancellable         *cancellable,
                                                         GAsyncReadyCallback   callback,
                                                         gpointer              user_data);
gboolean         mi2_output_stream_write_message_finish (Mi2OutputStream      *self,
                                                         GAsyncResult         *result,
                                                         GError              **error);

G_END_DECLS

#endif /* MI2_OUTPUT_STREAM_H */
