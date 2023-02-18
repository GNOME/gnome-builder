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
#include <libxml/tree.h>
#include <libxml/parser.h>

#include "ide-xml-formatter.h"

struct _IdeXmlFormatter
{
  IdeObject parent_instance;
};


static void
formatter_iface_init (IdeFormatterInterface *iface);

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

static void
ide_xml_formatter_format_async (IdeFormatter        *formatter,
                                IdeBuffer           *buffer,
                                IdeFormatterOptions *options,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  GtkTextIter start = {0};
  GtkTextIter end = {0};
  char *text = NULL;
  xmlDocPtr doc = NULL;
  xmlChar *mem = NULL;
  int doc_txt_len = 0;
  g_autoptr(IdeTask) task = NULL;
  // The only leak-free solution I could think of
  const char* indents[8] = {" ",
                            "  ",
                            "   ",
                            "    ",
                            "     ",
                            "      ",
                            "       ",
                            "        "};

  g_assert (IDE_IS_XML_FORMATTER (formatter));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (formatter, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_xml_formatter_format_async);
  gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &start, &end);
  text = gtk_text_buffer_get_text (GTK_TEXT_BUFFER (buffer), &start, &end, TRUE);
  doc = xmlParseDoc ((const xmlChar *)text);
  if (!doc)
    {
      // TODO: Right error code
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NONE,
                                 "Can't format invalid HTML/XML");
      return;
    }

  if (ide_formatter_options_get_insert_spaces (options))
    {
      int n = ide_formatter_options_get_tab_width (options);
      if (n != 0)
        xmlTreeIndentString = indents[n > 8 ? 7 : (n - 1)];
      else
        xmlTreeIndentString = "  ";
    }
  else
    {
      xmlTreeIndentString = "\t";
    }
  xmlDocDumpFormatMemoryEnc (doc, &mem, &doc_txt_len, "UTF-8", TRUE);
  gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (buffer));
  gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &start, &end);
  gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &start, &end);
  gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer),
                          &start,
                          (char *)mem,
                          -1);
  gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (buffer));
  free (text);
  xmlFreeDoc (doc);
  ide_task_return_boolean (task, TRUE);
}

static gboolean
ide_xml_formatter_format_finish (IdeFormatter  *self,
                                 GAsyncResult  *result,
                                 GError       **error)
{
  g_assert (IDE_IS_FORMATTER (self));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
formatter_iface_init (IdeFormatterInterface *iface)
{
  iface->format_async = ide_xml_formatter_format_async;
  iface->format_finish = ide_xml_formatter_format_finish;
}
