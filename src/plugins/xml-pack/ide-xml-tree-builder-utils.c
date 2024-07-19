/* ide-xml-tree-builder-utils.c
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

#include <string.h>

#include "ide-xml-tree-builder-utils-private.h"

#define HREF_LEN 6

const gchar *
list_get_attribute (const guchar **attributes,
                    const gchar  *name)
{
  const guchar **l = attributes;

  g_return_val_if_fail (!ide_str_empty0 (name), NULL);

  if (attributes == NULL)
    return NULL;

  while (l [0] != NULL)
    {
      if (ide_str_equal0 (name, l [0]))
        return (const gchar *)l [1];

      l += 2;
    }

  return NULL;
}

gchar *
get_schema_url (const gchar *data)
{
  gchar *begin;
  gchar *end;

  if (!data)
    return NULL;

  if (NULL != (begin = strstr (data, "href=\"")))
    {
      end = begin += HREF_LEN;
      while (end != NULL)
        {
          if (NULL != (end = strchr (begin, '"')))
            {
              if (*(end - 1) != '\\')
                return g_strndup (begin, end - begin);
            }
        }
    }

  return NULL;
}

const gchar *
get_schema_kind_string (IdeXmlSchemaKind kind)
{
  if (kind == SCHEMA_KIND_NONE)
    return "No schema";
  else if (kind == SCHEMA_KIND_DTD)
    return "DTD schema (.dtd or internal)";
  else if (kind == SCHEMA_KIND_RNG)
    return "RNG schema (.rng)";
  else if (kind == SCHEMA_KIND_XML_SCHEMA)
    return "XML schema (.xsd)";

  g_return_val_if_reached (NULL);
}


