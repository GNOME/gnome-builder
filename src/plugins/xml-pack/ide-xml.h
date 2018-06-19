/* ide-xml.h
 *
 * Copyright 2015 Dimitris Zenios <dimitris.zenios@gmail.com>
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

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef enum {
  IDE_XML_ELEMENT_TAG_UNKNOWN    = 0,
  IDE_XML_ELEMENT_TAG_START      = 1,
  IDE_XML_ELEMENT_TAG_END        = 2,
  IDE_XML_ELEMENT_TAG_START_END  = 3,
} IdeXmlElementTagType;

gboolean             ide_xml_in_element            (const GtkTextIter *iter);
gboolean             ide_xml_find_next_element     (const GtkTextIter *iter,
                                                    GtkTextIter       *start,
                                                    GtkTextIter       *end);
gboolean             ide_xml_get_current_element   (const GtkTextIter *iter,
                                                    GtkTextIter       *start,
                                                    GtkTextIter       *end);
gboolean             ide_xml_find_previous_element (const GtkTextIter *iter,
                                                    GtkTextIter       *start,
                                                    GtkTextIter       *end);
gboolean             ide_xml_find_closing_element  (const GtkTextIter *start,
                                                    const GtkTextIter *end,
                                                    GtkTextIter       *found_element_start,
                                                    GtkTextIter       *found_element_end);
gboolean             ide_xml_find_opening_element  (const GtkTextIter *start,
                                                    const GtkTextIter *end,
                                                    GtkTextIter       *found_element_start,
                                                    GtkTextIter       *found_element_end);
gchar               *ide_xml_get_element_name      (const GtkTextIter *start,
                                                    const GtkTextIter *end);
IdeXmlElementTagType ide_xml_get_element_tag_type  (const GtkTextIter *start,
                                                    const GtkTextIter *end);

G_END_DECLS
