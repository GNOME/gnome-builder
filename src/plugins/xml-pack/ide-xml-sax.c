/* ide-xml-sax.c
 *
 * Copyright 2017 Sebastien Lafargue <slafargue@gnome.org>
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

#include <glib/gi18n.h>
#include <string.h>

#include <libxml/parserInternals.h>

#include "ide-xml-sax.h"

struct _IdeXmlSax
{
  GObject        parent_instance;

  xmlSAXHandler  handler;
  xmlParserCtxt *context;

  guint          initialized : 1;
};

G_DEFINE_FINAL_TYPE (IdeXmlSax, ide_xml_sax, G_TYPE_OBJECT)

IdeXmlSax *
ide_xml_sax_new (void)
{
  return g_object_new (IDE_TYPE_XML_SAX, NULL);
}

static void
ide_xml_sax_finalize (GObject *object)
{
  G_OBJECT_CLASS (ide_xml_sax_parent_class)->finalize (object);
}

static void
ide_xml_sax_class_init (IdeXmlSaxClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_xml_sax_finalize;
}

static void
ide_xml_sax_init (IdeXmlSax *self)
{
}

void
ide_xml_sax_set_callback (IdeXmlSax             *self,
                          IdeXmlSaxCallbackType  callback_type,
                          gpointer               callback)
{
  xmlSAXHandler *handler;

  g_return_if_fail (IDE_IS_XML_SAX (self));
  g_return_if_fail (callback != NULL);

  self->initialized = TRUE;

  handler = &self->handler;
  switch (callback_type)
    {
    case IDE_XML_SAX_CALLBACK_TYPE_ATTRIBUTE:
      handler->attributeDecl = callback;
      break;

    case IDE_XML_SAX_CALLBACK_TYPE_CDATA:
      handler->cdataBlock = callback;
      break;

    case IDE_XML_SAX_CALLBACK_TYPE_CHAR:
      handler->characters = callback;
      break;

    case IDE_XML_SAX_CALLBACK_TYPE_COMMENT:
      handler->comment = callback;
      break;

    case IDE_XML_SAX_CALLBACK_TYPE_START_DOCUMENT:
      handler->startDocument = callback;
      break;

    case IDE_XML_SAX_CALLBACK_TYPE_START_ELEMENT:
      handler->startElement = callback;
      break;

    case IDE_XML_SAX_CALLBACK_TYPE_END_DOCUMENT:
      handler->endDocument = callback;
      break;

    case IDE_XML_SAX_CALLBACK_TYPE_END_ELEMENT:
      handler->endElement = callback;
      break;

    case IDE_XML_SAX_CALLBACK_TYPE_ENTITY:
      handler->entityDecl = callback;
      break;

    case IDE_XML_SAX_CALLBACK_TYPE_INTERNAL_SUBSET:
      handler->internalSubset = callback;
      break;

    case IDE_XML_SAX_CALLBACK_TYPE_EXTERNAL_SUBSET:
      handler->externalSubset = callback;
      break;

    case IDE_XML_SAX_CALLBACK_TYPE_PROCESSING_INSTRUCTION:
      handler->processingInstruction = callback;
      break;

    case IDE_XML_SAX_CALLBACK_TYPE_WARNING:
      handler->warning = callback;
      break;

    case IDE_XML_SAX_CALLBACK_TYPE_ERROR:
      handler->error = callback;
      break;

    case IDE_XML_SAX_CALLBACK_TYPE_FATAL_ERROR:
      handler->fatalError = callback;
      break;

    default:
      g_assert_not_reached ();
    }
}

void
ide_xml_sax_clear (IdeXmlSax *self)
{
  g_return_if_fail (IDE_IS_XML_SAX (self));

  memset ((void *)(&self->handler), 0, sizeof (xmlSAXHandler));
}

gboolean
ide_xml_sax_parse (IdeXmlSax   *self,
                   const gchar *data,
                   gsize        length,
                   const gchar *uri,
                   gpointer     user_data)
{
  gboolean wellformed;

  g_return_val_if_fail (IDE_IS_XML_SAX (self), FALSE);
  g_return_val_if_fail (data != NULL, FALSE);
  g_return_val_if_fail (length > 0, FALSE);

  g_return_val_if_fail (self->initialized == TRUE, FALSE);
  g_return_val_if_fail (self->context == NULL, FALSE);

  self->context = xmlCreateMemoryParserCtxt (data, length);
  self->context->userData = user_data;

  self->context->sax = &self->handler;
  self->handler.initialized = XML_SAX2_MAGIC;
  xmlCtxtUseOptions (self->context, XML_PARSE_RECOVER | XML_PARSE_NOENT);

  xmlParseDocument (self->context);
  wellformed = self->context->wellFormed;

  self->context->sax = NULL;
  g_clear_pointer (&self->context, xmlFreeParserCtxt);

  return wellformed;
}

static void
get_tag_location (IdeXmlSax    *self,
                  gint         *line,
                  gint         *line_offset,
                  gint         *end_line,
                  gint         *end_line_offset,
                  const gchar **content,
                  gsize        *size)
{
  xmlParserInput *input;
  const gchar *base;
  const gchar *current;
  const gchar *end_current;
  const gchar *line_start;
  const gchar *end_line_start;
  gint start_line_number;
  gint end_line_number;
  gint size_offset = 1;
  gunichar ch;
  gboolean end_line_found = FALSE;

  g_assert (IDE_IS_XML_SAX (self));
  g_assert (line != NULL);
  g_assert (line_offset != NULL);
  g_assert (end_line != NULL);
  g_assert (end_line_offset != NULL);
  g_assert (content != NULL);
  g_assert (size != NULL);

  /* TODO: handle other types of line break */

  input = self->context->input;
  base = (const gchar *)input->base;
  current = (const gchar *)input->cur;
  *end_line = end_line_number = start_line_number = xmlSAX2GetLineNumber (self->context);

    /* Adjust the element size, can be a start, a end or an auto-closed one */
  ch = g_utf8_get_char (current);
  if (ch != '>')
    {
      /* End element case */
      if (current > base && g_utf8_get_char (current - 1) == '>')
        {
          --current;
          size_offset = 0;
        }
      /* Auto-closed start element case */
      else if (ch == '/' && g_utf8_get_char (current + 1) == '>')
        {
          ++current;
          size_offset = 2;
        }
      /* Not properly closed tag */
      else
        {
          ch = g_utf8_get_char (--current);
          if (ch == '<')
            {
              /* Empty node */
              *line = *end_line = end_line_number;
              *line_offset = *end_line_offset = xmlSAX2GetColumnNumber (self->context) - 1;
              *size = 1;
              return;
            }
          else
            {
              while (current >= base)
                {
                  if (ch == '\n')
                    --end_line_number;

                  if (!g_unichar_isspace (ch) || current == base)
                    break;

                  current = g_utf8_prev_char (current);
                  ch = g_utf8_get_char (current);
                }

              end_current = current;
              *end_line = start_line_number = end_line_number;
              size_offset = 0;
              goto next;
            }
        }
    }

  end_current = current;
  if (g_utf8_get_char (current) != '>')
    {
      *line = start_line_number;
      *line_offset = *end_line_offset = xmlSAX2GetColumnNumber (self->context);
      *content = NULL;
      *size = 0;

      return;
    }

next:
  /* Search back the tag start and adjust the start and end line */
  while (current > base)
    {
      ch = g_utf8_get_char (current);
      if (ch == '<')
        break;

      if (ch == '\n')
        {
          --start_line_number;
          if (!end_line_found )
            {
              end_line_start = current + 1;
              end_line_found = TRUE;
            }
        }

      current = g_utf8_prev_char (current);
    }

  /* Search back the tag start offset */
  line_start = current;
  while (line_start > base)
    {
      ch = g_utf8_get_char (line_start);
      if (ch == '\n')
        {
          ++line_start;
          if (!end_line_found )
            {
              end_line_start = line_start;
              end_line_found = TRUE;
            }

          break;
        }

      line_start = g_utf8_prev_char (line_start);
    }

  if (!end_line_found)
    end_line_start = line_start;

  *line = start_line_number;
  *line_offset = (current - line_start) + 1;
  *end_line_offset = (end_current - end_line_start) + 1;
  *content = current;
  *size = (const gchar *)input->cur - current + size_offset;
}

gboolean
ide_xml_sax_get_location (IdeXmlSax    *self,
                          gint         *start_line,
                          gint         *start_line_offset,
                          gint         *end_line,
                          gint         *end_line_offset,
                          const gchar **content,
                          gsize        *size)
{
  gint tmp_line = 0;
  gint tmp_line_offset = 0;
  gint tmp_end_line = 0;
  gint tmp_end_line_offset = 0;
  const gchar *tmp_content = NULL;
  gsize tmp_size = 0;

  g_return_val_if_fail (IDE_IS_XML_SAX (self), FALSE);
  g_return_val_if_fail (self->context != NULL, FALSE);

  get_tag_location (self, &tmp_line, &tmp_line_offset, &tmp_end_line, &tmp_end_line_offset, &tmp_content, &tmp_size);

  if (start_line != NULL)
    *start_line = tmp_line;

  if (start_line_offset != NULL)
    *start_line_offset = tmp_line_offset;

  if (content != NULL)
    *content = tmp_content;

  if (size != NULL)
    *size = tmp_size;

  if (end_line != NULL)
    *end_line = tmp_end_line;

  if (end_line_offset != NULL)
    *end_line_offset = tmp_end_line_offset;

  return (tmp_end_line > 0 && tmp_end_line_offset > 0);
}

gint
ide_xml_sax_get_depth (IdeXmlSax *self)
{
  g_return_val_if_fail (IDE_IS_XML_SAX (self), FALSE);
  g_return_val_if_fail (self->context != NULL, FALSE);

  return self->context->nameNr;
}

xmlParserCtxt *
ide_xml_sax_get_context (IdeXmlSax *self)
{
  g_return_val_if_fail (IDE_IS_XML_SAX (self), NULL);

  return self->context;
}

