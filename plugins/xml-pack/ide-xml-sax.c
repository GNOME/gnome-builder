/* ide-xml-sax.c
 *
 * Copyright (C) 2017 Sebastien Lafargue <slafargue@gnome.org>
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

#include <glib/gi18n.h>
#include <string.h>
#include <libxml/parser.h>
#include <libxml/parserInternals.h>

#include "ide-xml-sax.h"

struct _IdeXmlSax
{
  GObject        parent_instance;

  xmlSAXHandler  handler;
  xmlParserCtxt *context;

  guint          initialized : 1;
};

G_DEFINE_TYPE (IdeXmlSax, ide_xml_sax, G_TYPE_OBJECT)

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

gboolean
ide_xml_sax_get_position (IdeXmlSax *self,
                          gint      *line,
                          gint      *line_offset)
{
  g_return_val_if_fail (IDE_IS_XML_SAX (self), FALSE);
  g_return_val_if_fail (line != NULL, FALSE);
  g_return_val_if_fail (line_offset != NULL, FALSE);
  g_return_val_if_fail (self->context != NULL, FALSE);

  *line = xmlSAX2GetLineNumber (self->context);
  *line_offset = xmlSAX2GetColumnNumber (self->context);

  return (*line > 0 && *line_offset > 0);
}

gint
ide_xml_sax_get_depth (IdeXmlSax *self)
{
  g_return_val_if_fail (IDE_IS_XML_SAX (self), FALSE);
  g_return_val_if_fail (self->context != NULL, FALSE);

  return self->context->nameNr;
}

