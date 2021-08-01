/* xml-reader.c
 *
 * Copyright 2009  Christian Hergert  <chris@dronelabs.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * Author:
 *   Christian Hergert  <chris@dronelabs.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <glib/gi18n.h>
#include <string.h>
#include <libxml/xmlreader.h>

#include "xml-reader-private.h"

#define XML_TO_CHAR(s)  ((char *) (s))
#define CHAR_TO_XML(s)  ((unsigned char *) (s))
#define RETURN_STRDUP_AND_XMLFREE(stmt) \
  G_STMT_START {                        \
    guchar *x;                          \
    gchar *y;                           \
    x = stmt;                           \
    y = g_strdup((char *)x);            \
    xmlFree(x);                         \
    return y;                           \
  } G_STMT_END

struct _XmlReader
{
  GObject           parent_instance;
  xmlTextReaderPtr  xml;
  GInputStream     *stream;
  gchar            *cur_name;
  gchar            *encoding;
  gchar            *uri;
};

enum {
  PROP_0,
  PROP_ENCODING,
  PROP_URI,
  LAST_PROP
};

enum {
  ERROR,
  LAST_SIGNAL
};

G_DEFINE_QUARK (xml_reader_error, xml_reader_error)
G_DEFINE_FINAL_TYPE (XmlReader, xml_reader, G_TYPE_OBJECT)

static GParamSpec *properties [LAST_PROP];
static guint signals [LAST_SIGNAL];

#define XML_NODE_TYPE_ELEMENT      1
#define XML_NODE_TYPE_END_ELEMENT 15
#define XML_NODE_TYPE_ATTRIBUTE    2

static void
xml_reader_set_encoding (XmlReader   *reader,
                         const gchar *encoding)
{
   g_return_if_fail (XML_IS_READER (reader));
   g_free (reader->encoding);
   reader->encoding = g_strdup (encoding);
}

static void
xml_reader_set_uri (XmlReader   *reader,
                    const gchar *uri)
{
   g_return_if_fail (XML_IS_READER (reader));
   g_free (reader->uri);
   reader->uri = g_strdup (uri);
}

static void
xml_reader_clear (XmlReader *reader)
{
   g_return_if_fail(XML_IS_READER(reader));

   g_free (reader->cur_name);
   reader->cur_name = NULL;

   if (reader->xml) {
      xmlTextReaderClose(reader->xml);
      xmlFreeTextReader(reader->xml);
      reader->xml = NULL;
   }

   if (reader->stream) {
      g_object_unref(reader->stream);
      reader->stream = NULL;
   }
}

static void
xml_reader_finalize (GObject *object)
{
   XmlReader *reader = (XmlReader *)object;

   xml_reader_clear (reader);

   g_free (reader->encoding);
   reader->encoding = NULL;

   g_free (reader->uri);
   reader->uri = NULL;

   G_OBJECT_CLASS (xml_reader_parent_class)->finalize (object);
}

static void
xml_reader_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
   XmlReader *reader = (XmlReader *)object;

   switch (prop_id)
     {
     case PROP_ENCODING:
        g_value_set_string (value, reader->encoding);
        break;

     case PROP_URI:
        g_value_set_string (value, reader->uri);
        break;

     default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
     }
}

static void
xml_reader_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
   XmlReader *reader = (XmlReader *)object;

   switch (prop_id)
     {
     case PROP_ENCODING:
        xml_reader_set_encoding (reader, g_value_get_string (value));
        break;

     case PROP_URI:
        xml_reader_set_uri (reader, g_value_get_string (value));
        break;

     default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
     }
}

static void
xml_reader_class_init (XmlReaderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xml_reader_finalize;
  object_class->get_property = xml_reader_get_property;
  object_class->set_property = xml_reader_set_property;

  properties [PROP_ENCODING] =
    g_param_spec_string ("encoding",
                         "Encoding",
                         "Encoding",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_URI] =
    g_param_spec_string ("uri",
                         "URI",
                         "URI",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  signals [ERROR] =
    g_signal_new ("error",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_STRING);
}

static void
xml_reader_init (XmlReader *reader)
{
}

XmlReader*
xml_reader_new (void)
{
  return g_object_new (XML_TYPE_READER, NULL);
}

static void
xml_reader_error_cb (void                    *arg,
                     const char              *msg,
                     xmlParserSeverities      severity,
                     xmlTextReaderLocatorPtr  locator)
{
  XmlReader *reader = arg;

  g_assert (XML_IS_READER (reader));

  g_signal_emit (reader, signals [ERROR], 0, msg);
}

gboolean
xml_reader_load_from_path (XmlReader   *reader,
                           const gchar *path)
{
  g_return_val_if_fail (XML_IS_READER (reader), FALSE);

  xml_reader_clear (reader);

  if ((reader->xml = xmlNewTextReaderFilename (path)))
    xmlTextReaderSetErrorHandler (reader->xml, xml_reader_error_cb, reader);

  return (reader->xml != NULL);
}

gboolean
xml_reader_load_from_file (XmlReader     *reader,
                           GFile         *file,
                           GCancellable  *cancellable,
                           GError       **error)
{
  GFileInputStream *stream;
  gboolean ret;

  g_return_val_if_fail (XML_IS_READER (reader), FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  if (!(stream = g_file_read (file, cancellable, error)))
    return FALSE;

  ret = xml_reader_load_from_stream (reader, G_INPUT_STREAM (stream), error);

  g_clear_object (&stream);

  return ret;
}

gboolean
xml_reader_load_from_data (XmlReader   *reader,
                           const gchar *data,
                           gssize       length,
                           const gchar *uri,
                           const gchar *encoding)
{
  g_return_val_if_fail (XML_IS_READER (reader), FALSE);

  xml_reader_clear (reader);

  if (length == -1)
    length = strlen (data);

  reader->xml = xmlReaderForMemory (data, length, uri, encoding, 0);
  xmlTextReaderSetErrorHandler (reader->xml, xml_reader_error_cb, reader);

  return (reader->xml != NULL);
}

static int
xml_reader_io_read_cb (void *context,
                       char *buffer,
                       int   len)
{
  GInputStream *stream = (GInputStream *)context;
  g_return_val_if_fail (G_IS_INPUT_STREAM(stream), -1);
  return g_input_stream_read (stream, buffer, len, NULL, NULL);
}

static int
xml_reader_io_close_cb (void *context)
{
  GInputStream *stream = (GInputStream *)context;

  g_return_val_if_fail (G_IS_INPUT_STREAM(stream), -1);

  return g_input_stream_close (stream, NULL, NULL) ? 0 : -1;
}

gboolean
xml_reader_load_from_stream (XmlReader     *reader,
                             GInputStream  *stream,
                             GError       **error)
{
  g_return_val_if_fail (XML_IS_READER(reader), FALSE);

  xml_reader_clear (reader);

  reader->xml = xmlReaderForIO (xml_reader_io_read_cb,
                                xml_reader_io_close_cb,
                                stream,
                                reader->uri,
                                reader->encoding,
                                XML_PARSE_RECOVER | XML_PARSE_NOBLANKS | XML_PARSE_COMPACT);

  if (!reader->xml)
    {
      g_set_error (error,
                   XML_READER_ERROR,
                   XML_READER_ERROR_INVALID,
                   _("Could not parse XML from stream"));
      return FALSE;
    }

   reader->stream = g_object_ref (stream);

   xmlTextReaderSetErrorHandler (reader->xml, xml_reader_error_cb, reader);

   return TRUE;
}

const gchar *
xml_reader_get_value (XmlReader *reader)
{
  g_return_val_if_fail (XML_IS_READER (reader), NULL);

  g_return_val_if_fail (reader->xml != NULL, NULL);

  return XML_TO_CHAR (xmlTextReaderConstValue (reader->xml));
}

const gchar *
xml_reader_get_name (XmlReader *reader)
{
  g_return_val_if_fail (XML_IS_READER (reader), NULL);
  g_return_val_if_fail (reader->xml != NULL, NULL);

  return XML_TO_CHAR (xmlTextReaderConstName (reader->xml));
}

gchar *
xml_reader_read_string (XmlReader *reader)
{
  g_return_val_if_fail (XML_IS_READER (reader), NULL);
  g_return_val_if_fail (reader->xml != NULL, NULL);

  RETURN_STRDUP_AND_XMLFREE (xmlTextReaderReadString (reader->xml));
}

gchar *
xml_reader_get_attribute (XmlReader   *reader,
                          const gchar *name)
{
  g_return_val_if_fail (XML_IS_READER (reader), NULL);
  g_return_val_if_fail (reader->xml != NULL, NULL);

  RETURN_STRDUP_AND_XMLFREE (xmlTextReaderGetAttribute (reader->xml, CHAR_TO_XML (name)));
}

static gboolean
read_to_type_and_name (XmlReader   *reader,
                       gint         type,
                       const gchar *name)
{
  gboolean success = FALSE;

  g_return_val_if_fail (XML_IS_READER (reader), FALSE);

  g_return_val_if_fail (reader->xml != NULL, FALSE);

  while (xmlTextReaderRead (reader->xml) == 1)
    {
      if (xmlTextReaderNodeType (reader->xml) == type)
        {
          if (g_strcmp0 (XML_TO_CHAR (xmlTextReaderConstName (reader->xml)), name) == 0)
            {
              success = TRUE;
              break;
            }
        }
    }

  return success;
}

gboolean
xml_reader_read_start_element (XmlReader   *reader,
                               const gchar *name)
{
  g_return_val_if_fail (XML_IS_READER (reader), FALSE);

  if (read_to_type_and_name (reader, XML_NODE_TYPE_ELEMENT, name))
    {
      g_free (reader->cur_name);
      reader->cur_name = g_strdup (name);
      return TRUE;
    }

  return FALSE;
}

gboolean
xml_reader_read_end_element (XmlReader *reader)
{
  gboolean success = FALSE;

  g_return_val_if_fail (XML_IS_READER (reader), FALSE);

  if (reader->cur_name)
    success = read_to_type_and_name (reader, XML_NODE_TYPE_END_ELEMENT, reader->cur_name);

  return success;
}

gchar *
xml_reader_read_inner_xml (XmlReader *reader)
{
  g_return_val_if_fail (XML_IS_READER (reader), FALSE);

  RETURN_STRDUP_AND_XMLFREE (xmlTextReaderReadInnerXml (reader->xml));
}

gchar*
xml_reader_read_outer_xml (XmlReader *reader)
{
  g_return_val_if_fail (XML_IS_READER (reader), FALSE);

  RETURN_STRDUP_AND_XMLFREE (xmlTextReaderReadOuterXml (reader->xml));
}

gboolean
xml_reader_read_to_next (XmlReader *reader)
{
  g_return_val_if_fail (XML_IS_READER (reader), FALSE);

  return (xmlTextReaderNext (reader->xml) == 1);
}

gboolean
xml_reader_read (XmlReader *reader)
{
  g_return_val_if_fail (XML_IS_READER (reader), FALSE);

  return (xmlTextReaderRead (reader->xml) == 1);
}

gboolean
xml_reader_read_to_next_sibling (XmlReader *reader)
{
  g_return_val_if_fail (XML_IS_READER (reader), FALSE);

  xmlTextReaderMoveToElement (reader->xml);

  return (xmlTextReaderNextSibling (reader->xml) == 1);
}

gint
xml_reader_get_depth (XmlReader *reader)
{
  g_return_val_if_fail (XML_IS_READER(reader), -1);

  return xmlTextReaderDepth (reader->xml);
}

void
xml_reader_move_up_to_depth (XmlReader *reader,
                             gint       depth)
{
  g_return_if_fail(XML_IS_READER(reader));

  while (xml_reader_get_depth(reader) > depth)
    xml_reader_read_end_element(reader);
}

xmlReaderTypes
xml_reader_get_node_type (XmlReader *reader)
{
  g_return_val_if_fail (XML_IS_READER (reader), 0);

  return xmlTextReaderNodeType (reader->xml);
}

gboolean
xml_reader_is_empty_element (XmlReader *reader)
{
  g_return_val_if_fail (XML_IS_READER (reader), FALSE);

  return xmlTextReaderIsEmptyElement (reader->xml);
}

gboolean
xml_reader_is_a (XmlReader   *reader,
                 const gchar *name)
{
   return (g_strcmp0 (xml_reader_get_name (reader), name) == 0);
}

gboolean
xml_reader_is_a_local (XmlReader   *reader,
                       const gchar *local_name)
{
   return (g_strcmp0 (xml_reader_get_local_name (reader), local_name) == 0);
}

gboolean
xml_reader_is_namespace (XmlReader   *reader,
                         const gchar *ns)
{
   g_return_val_if_fail (XML_IS_READER (reader), FALSE);

   return (g_strcmp0 (XML_TO_CHAR(xmlTextReaderConstNamespaceUri (reader->xml)), ns) == 0);
}

const gchar *
xml_reader_get_local_name (XmlReader *reader)
{
   g_return_val_if_fail(XML_IS_READER (reader), NULL);

   return XML_TO_CHAR (xmlTextReaderConstLocalName (reader->xml));
}

gboolean
xml_reader_move_to_element (XmlReader *reader)
{
  g_return_val_if_fail (XML_IS_READER (reader), FALSE);

  return (xmlTextReaderMoveToElement (reader->xml) == 1);
}

gboolean
xml_reader_move_to_attribute (XmlReader   *reader,
                              const gchar *name)
{
  g_return_val_if_fail (XML_IS_READER (reader), FALSE);

  return (xmlTextReaderMoveToAttribute (reader->xml, CHAR_TO_XML (name)) == 1);
}

gboolean
xml_reader_move_to_first_attribute (XmlReader *reader)
{
  g_return_val_if_fail (XML_IS_READER (reader), FALSE);

  return (xmlTextReaderMoveToFirstAttribute (reader->xml) == 1);
}

gboolean
xml_reader_move_to_next_attribute (XmlReader *reader)
{
  g_return_val_if_fail (XML_IS_READER (reader), FALSE);

  return (xmlTextReaderMoveToNextAttribute (reader->xml) == 1);
}

gboolean
xml_reader_move_to_nth_attribute (XmlReader *reader,
                                  gint       nth)
{
  g_return_val_if_fail (XML_IS_READER (reader), FALSE);

  return (xmlTextReaderMoveToAttributeNo (reader->xml, nth) == 1);
}

gint
xml_reader_count_attributes (XmlReader *reader)
{
  g_return_val_if_fail (XML_IS_READER (reader), FALSE);

  return xmlTextReaderAttributeCount (reader->xml);
}

gint
xml_reader_get_line_number (XmlReader *reader)
{
  g_return_val_if_fail(XML_IS_READER(reader), -1);

  if (reader->xml)
    return xmlTextReaderGetParserLineNumber(reader->xml);

  return -1;
}
