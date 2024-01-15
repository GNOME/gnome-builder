/*
 * ide-xml-formatter.c
 *
 * Copyright 2023 JCWasmx86 <JCWasmx86@t-online.de>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#include <glib/gi18n.h>

#include <libdex.h>

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xmlsave.h>

#include "ide-xml-formatter.h"

struct _IdeXmlFormatter
{
  IdeObject parent_instance;
};

G_LOCK_DEFINE_STATIC (accessing_libxml2_global);
static const char * const indents[] = {
  "\t",
  " ",
  "  ",
  "   ",
  "    ",
  "     ",
  "      ",
  "       ",
  "        ",
};

typedef struct
{
  GBytes *content;
  guint tab_width : 7;
  guint use_spaces : 1;
} FormatRequest;

static FormatRequest *
format_request_new (GBytes              *content,
                    IdeFormatterOptions *options)
{
  FormatRequest *req = g_new0 (FormatRequest, 1);

  req->content = g_bytes_ref (content);
  req->use_spaces = ide_formatter_options_get_insert_spaces (options);
  req->tab_width = ide_formatter_options_get_tab_width (options);

  return req;
}

static void
format_request_free (FormatRequest *req)
{
  g_clear_pointer (&req->content, g_bytes_unref);
  g_free (req);
}

static DexFuture *
apply_contents_to_buffer (DexFuture *completed,
                          gpointer   user_data)
{
  IdeBuffer *buffer = user_data;
  g_autoptr(GBytes) bytes = NULL;
  GtkTextMark *insert;
  GtkTextIter begin, end, iter;
  guint line, line_offset;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (DEX_IS_FUTURE (completed));
  g_assert (IDE_IS_BUFFER (buffer));

  bytes = dex_await_boxed (dex_ref (completed), NULL);

  g_assert (bytes != NULL);

  insert = gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (buffer));
  gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer), &iter, insert);
  line = gtk_text_iter_get_line (&iter);
  line_offset = gtk_text_iter_get_line_offset (&iter);

  gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &begin, &end);
  gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (buffer));
  gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &begin, &end);
  gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer),
                          &begin,
                          g_bytes_get_data (bytes, NULL),
                          -1);
  gtk_text_buffer_get_iter_at_line_offset (GTK_TEXT_BUFFER (buffer),
                                           &iter, line, line_offset);
  gtk_text_buffer_select_range (GTK_TEXT_BUFFER (buffer), &iter, &iter);
  gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (buffer));

  IDE_RETURN (dex_future_new_for_boolean (TRUE));
}

static DexFuture *
ide_xml_formatter_format_fiber (gpointer user_data)
{
  FormatRequest *req = user_data;
  g_autoptr(DexFuture) result = NULL;
  const char *original;
  const char *text;
  xmlDocPtr doc = NULL;
  xmlChar *formatted = NULL;
  int formatted_len = 0;

  IDE_ENTRY;

  g_assert (!IDE_IS_MAIN_THREAD ());
  g_assert (req != NULL);
  g_assert (req->content != NULL);

  text = (const char *)g_bytes_get_data (req->content, NULL);

  g_assert (text != NULL);
  g_assert (text[g_bytes_get_size (req->content)] == 0);

  if (!(doc = xmlParseDoc ((const xmlChar *)text)))
    IDE_RETURN (dex_future_new_reject (G_IO_ERROR,
                                       G_IO_ERROR_FAILED,
                                       _("Failed to parse XML document")));

  G_LOCK (accessing_libxml2_global);
  original = xmlTreeIndentString;

  if (req->use_spaces)
    xmlTreeIndentString = indents[MIN (req->tab_width, 8)];
  else
    xmlTreeIndentString = indents[0];

  xmlDocDumpFormatMemoryEnc (doc, &formatted, &formatted_len, "UTF-8", TRUE);
  result = dex_future_new_take_boxed (G_TYPE_BYTES,
                                      g_bytes_new_with_free_func (formatted,
                                                                  formatted_len,
                                                                  (GDestroyNotify)xmlFree,
                                                                  formatted));
  formatted = NULL;

  xmlTreeIndentString = original;
  G_UNLOCK (accessing_libxml2_global);

  IDE_RETURN (g_steal_pointer (&result));
}

static void
ide_xml_formatter_format_async (IdeFormatter        *formatter,
                                IdeBuffer           *buffer,
                                IdeFormatterOptions *options,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_autoptr(DexAsyncResult) result = NULL;
  g_autoptr(DexFuture) future = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GBytes) content = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_XML_FORMATTER (formatter));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (!options || IDE_IS_FORMATTER_OPTIONS (options));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  result = dex_async_result_new (formatter, cancellable, callback, user_data);
  content = ide_buffer_dup_content (buffer);

  future = dex_scheduler_spawn (dex_thread_pool_scheduler_get_default (),
                                0,
                                ide_xml_formatter_format_fiber,
                                format_request_new (content, options),
                                (GDestroyNotify)format_request_free);
  future = dex_future_then (future,
                            apply_contents_to_buffer,
                            g_object_ref (buffer),
                            g_object_unref);
  dex_async_result_await (result, g_steal_pointer (&future));

  IDE_EXIT;
}

static gboolean
ide_xml_formatter_format_finish (IdeFormatter  *self,
                                 GAsyncResult  *result,
                                 GError       **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_FORMATTER (self));
  g_assert (DEX_IS_ASYNC_RESULT (result));

  ret = dex_async_result_propagate_boolean (DEX_ASYNC_RESULT (result), error);

  IDE_RETURN (ret);
}

static void
formatter_iface_init (IdeFormatterInterface *iface)
{
  iface->format_async = ide_xml_formatter_format_async;
  iface->format_finish = ide_xml_formatter_format_finish;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeXmlFormatter, ide_xml_formatter, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_FORMATTER, formatter_iface_init))

static void
ide_xml_formatter_class_init (IdeXmlFormatterClass *klass)
{
}

static void
ide_xml_formatter_init (IdeXmlFormatter *self)
{
}
