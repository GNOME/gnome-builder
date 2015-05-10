/* xml-reader.h
 *
 * Copyright (C) 2009 Christian Hergert  <chris@dronelabs.com>
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
 *   Christian Hergert <chris@dronelabs.com>
 *
 * Based upon work by:
 *   Emmanuele Bassi
 */

#ifndef XML_READER_H
#define XML_READER_H

#include <gio/gio.h>
#include <libxml/xmlreader.h>

G_BEGIN_DECLS

#define XML_TYPE_READER (xml_reader_get_type ())

#define XML_READER_ERROR (xml_reader_error_quark())

G_DECLARE_FINAL_TYPE (XmlReader, xml_reader, XML, READER, GObject)

typedef enum
{
   XML_READER_ERROR_INVALID,
} XmlReaderError;

GQuark                xml_reader_error_quark             (void);
XmlReader            *xml_reader_new                     (void);
gboolean              xml_reader_load_from_path          (XmlReader     *reader,
                                                          const gchar   *path);
gboolean              xml_reader_load_from_file          (XmlReader     *reader,
                                                          GFile         *file,
                                                          GCancellable  *cancellable,
                                                          GError       **error);
gboolean              xml_reader_load_from_data          (XmlReader     *reader,
                                                          const gchar   *data,
                                                          gsize          length,
                                                          const gchar   *uri,
                                                          const gchar   *encoding);
gboolean              xml_reader_load_from_stream        (XmlReader     *reader,
                                                          GInputStream  *stream,
                                                          GError       **error);

gint                  xml_reader_get_depth               (XmlReader   *reader);
xmlReaderTypes        xml_reader_get_node_type           (XmlReader   *reader);
const gchar          *xml_reader_get_value               (XmlReader   *reader);
const gchar          *xml_reader_get_name                (XmlReader   *reader);
const gchar          *xml_reader_get_local_name          (XmlReader   *reader);
gchar                *xml_reader_read_string             (XmlReader   *reader);
gchar                *xml_reader_get_attribute           (XmlReader   *reader,
                                                          const gchar *name);
gboolean              xml_reader_is_a                    (XmlReader   *reader,
                                                          const gchar *name);
gboolean              xml_reader_is_a_local              (XmlReader   *reader,
                                                          const gchar *local_name);
gboolean              xml_reader_is_namespace            (XmlReader   *reader,
                                                          const gchar *ns);
gboolean              xml_reader_is_empty_element        (XmlReader   *reader);

gboolean              xml_reader_read_start_element      (XmlReader   *reader,
                                                          const gchar *name);
gboolean              xml_reader_read_end_element        (XmlReader   *reader);

gchar                *xml_reader_read_inner_xml          (XmlReader   *reader);
gchar                *xml_reader_read_outer_xml          (XmlReader   *reader);

gboolean              xml_reader_read                    (XmlReader   *reader);
gboolean              xml_reader_read_to_next            (XmlReader   *reader);
gboolean              xml_reader_read_to_next_sibling    (XmlReader   *reader);

gboolean              xml_reader_move_to_element         (XmlReader   *reader);
gboolean              xml_reader_move_to_attribute       (XmlReader   *reader,
                                                          const gchar *name);
void                  xml_reader_move_up_to_depth        (XmlReader   *reader,
                                                          gint         depth);

gboolean              xml_reader_move_to_first_attribute (XmlReader   *reader);
gboolean              xml_reader_move_to_next_attribute  (XmlReader   *reader);
gint                  xml_reader_count_attributes        (XmlReader   *reader);
gboolean              xml_reader_move_to_nth_attribute   (XmlReader   *reader,
                                                          gint         nth);
gint                  xml_reader_get_line_number         (XmlReader   *reader);

G_END_DECLS

#endif /* XML_READER_H */
