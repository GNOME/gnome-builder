/* ide-xml-utils.h
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
 */

#pragma once

#include <glib.h>

#include "../gi/ide-gi-objects.h"

G_BEGIN_DECLS

typedef gboolean (*IdeXmlUtilsWalkerFunc) (IdeGiBase   *object,
                                           const gchar *name,
                                           gpointer     data);

gsize    ide_xml_utils_get_text_limit       (const gchar            *text,
                                             gsize                   paragraphs,
                                             gsize                   lines,
                                             gboolean               *has_moore);
gboolean ide_xml_utils_gi_class_walker      (IdeGiBase              *object,
                                             const gchar            *name,
                                             IdeXmlUtilsWalkerFunc   func,
                                             gpointer                data);
gboolean ide_xml_utils_is_name_char         (gunichar                ch);
gboolean ide_xml_utils_skip_attribute_name  (const gchar           **cursor);
gboolean ide_xml_utils_skip_attribute_value (const gchar           **cursor,
                                             gchar                   term);
gboolean ide_xml_utils_skip_element_name    (const gchar           **cursor);
gboolean ide_xml_utils_parse_version        (const gchar            *version,
                                             guint16                *major,
                                             guint16                *minor,
                                             guint16                *micro);
gint     ide_xml_utils_version_compare      (guint16                 major_v1,
                                             guint16                 minor_v1,
                                             guint16                 micro_v1,
                                             guint16                 major_v2,
                                             guint16                 minor_v2,
                                             guint16                 micro_v2);

G_END_DECLS
