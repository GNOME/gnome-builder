/* ide-xml-utils.c
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

#include <dazzle.h>

#include <stdlib.h>
#include <string.h>

#include "ide-xml-utils.h"

static inline gboolean
is_name_start_char (gunichar ch)
{
  return ((ch >= 'A' && ch <= 'Z') ||
          (ch >= 'a' && ch <= 'z') ||
          ch == ':' ||
          ch == '_' ||
          (ch >= 0xC0 && ch <= 0xD6) ||
          (ch >= 0xD8 && ch <= 0xF6) ||
          (ch >= 0xF8 && ch <= 0x2FF) ||
          (ch >= 0x370 && ch <= 0x37D) ||
          (ch >= 0x37F && ch <= 0x1FFF) ||
          (ch >= 0x200C && ch <= 0x200D) ||
          (ch >= 0x2070 && ch <= 0x218F) ||
          (ch >= 0x2C00 && ch <= 0x2FEF) ||
          (ch >= 0x3001 && ch <= 0xD7FF) ||
          (ch >= 0xF900 && ch <= 0xFDCF) ||
          (ch >= 0xFDF0 && ch <= 0xFFFD) ||
          (ch >= 0x10000 && ch <= 0xEFFFF));
}

gboolean
ide_xml_utils_is_name_char (gunichar ch)
{
  return (is_name_start_char (ch) ||
          ch == '-' ||
          ch == '.' ||
          (ch >= '0' && ch <= '9') ||
          ch == 0xB7 ||
          (ch >= 0x300 && ch <= 0x36F) ||
          (ch >= 0x203F && ch <= 0x2040));
}

/* Return %TRUE if we found spaces, cursor is updated to the new position */
static inline gboolean
skip_whitespaces (const gchar **cursor)
{
  const gchar *p = *cursor;
  gunichar ch;

  g_assert (cursor != NULL && *cursor != NULL);

  while ((ch = g_utf8_get_char (*cursor)) && g_unichar_isspace (ch))
    *cursor = g_utf8_next_char (*cursor);

  return (p != *cursor);
}

static void
jump_to_next_attribute (const gchar **cursor)
{
  const gchar *p = *cursor;
  gunichar ch;
  gchar term;
  gboolean has_spaces = FALSE;

  while ((ch = g_utf8_get_char (p)))
    {
      if (g_unichar_isspace (ch))
        {
          skip_whitespaces (&p);
          break;
        }

      if (ch == '=')
        break;

      p = g_utf8_next_char (p);
    }

  if (ch != '=')
    {
      *cursor = p;
      return;
    }

  p++;
  if (skip_whitespaces (&p))
    has_spaces = TRUE;

  ch = g_utf8_get_char (p);

  if (ch == '"' || ch == '\'')
    {
      term = ch;
      while ((ch = g_utf8_get_char (p)) && ch != term)
        p = g_utf8_next_char (p);

      if (ch == term)
        {
          p++;
          skip_whitespaces (&p);
        }
    }
  else if (!has_spaces)
    {
      while ((ch = g_utf8_get_char (p)) && !g_unichar_isspace (ch))
        p = g_utf8_next_char (p);
    }

  *cursor = p;
}

/* Return %FALSE if not valid, cursor is updated to the new position */
gboolean
ide_xml_utils_skip_element_name (const gchar **cursor)
{
  const gchar *p = *cursor;
  gunichar ch;

  g_return_val_if_fail (cursor != NULL && *cursor != NULL, FALSE);

  if (!(ch = g_utf8_get_char (p)))
    return TRUE;

  if (!is_name_start_char (ch))
    return (g_unichar_isspace (ch));

  p = g_utf8_next_char (p);
  while ((ch = g_utf8_get_char (p)))
    {
      if (!ide_xml_utils_is_name_char (ch))
        {
          *cursor = p;
          return g_unichar_isspace (ch);
        }

      p = g_utf8_next_char (p);
    }

  *cursor = p;
  return TRUE;
}

/* Return %FALSE at the end of the string, cursor is updated to the new position */
gboolean
ide_xml_utils_skip_attribute_value (const gchar **cursor,
                                    gchar         term)
{
  const gchar *p = *cursor;
  gunichar ch;

  g_return_val_if_fail (cursor != NULL && *cursor != NULL, FALSE);

  while ((ch = g_utf8_get_char (p)) && ch != term)
    p = g_utf8_next_char (p);

  if (ch == term)
    p++;

  *cursor = p;
  return (ch != 0);
}

/* Return %FALSE if not valid, cursor is updated to the new position */
gboolean
ide_xml_utils_skip_attribute_name (const gchar **cursor)
{
  const gchar *p = *cursor;
  gunichar ch;

  g_return_val_if_fail (cursor != NULL && *cursor != NULL, FALSE);

  if (!(ch = g_utf8_get_char (p)))
    return TRUE;

  if (!is_name_start_char (ch))
    {
      if (g_unichar_isspace (ch))
        return TRUE;

      *cursor = g_utf8_next_char (*cursor);
      return FALSE;
    }

  p = g_utf8_next_char (p);
  while ((ch = g_utf8_get_char (p)))
    {
      if (!ide_xml_utils_is_name_char (ch))
        {
          *cursor = p;
          if (g_unichar_isspace (ch) || ch == '=')
            return TRUE;
          else
            {
              jump_to_next_attribute (cursor);
              return FALSE;
            }
        }

      p = g_utf8_next_char (p);
    }

  *cursor = p;
  return TRUE;
}

gboolean
ide_xml_utils_parse_version (const gchar *version,
                             guint16     *major,
                             guint16     *minor,
                             guint16     *micro)
{
  gchar *end;
  guint64 tmp_major = 0;
  guint64 tmp_minor = 0;
  guint64 tmp_micro = 0;

  g_assert (version != NULL);

  tmp_major = g_ascii_strtoull (version, &end, 10);
  if (tmp_major >= 0x100 || end == version)
    return FALSE;

  if (*end == '\0')
    goto next;

  if (*end != '.')
    return FALSE;

  version = end + 1;
  tmp_minor = g_ascii_strtoull (version, &end, 10);
  if (tmp_minor >= 0x100 || end == version)
    return FALSE;

  if (*end == '\0')
    goto next;

  if (*end != '.')
    return FALSE;

  version = end + 1;
  tmp_micro = g_ascii_strtoull (version, &end, 10);
  if (tmp_micro >= 0x100 || end == version)
    return FALSE;

next:
  if (major != NULL)
    *major = tmp_major;

  if (minor != NULL)
    *minor = tmp_minor;

  if (micro != NULL)
    *micro = tmp_micro;

  return TRUE;
}

/* Return 1 if v1 > v2, 0 if v1 == v2 or -1 if v1 < v2 */
gint
ide_xml_utils_version_compare (guint16 major_v1,
                               guint16 minor_v1,
                               guint16 micro_v1,
                               guint16 major_v2,
                               guint16 minor_v2,
                               guint16 micro_v2)
{
  if (major_v1 > major_v2)
    return 1;
  else if (major_v1 == major_v2)
    {
      if (minor_v1 > minor_v2)
        return 1;
      else if (minor_v1 == minor_v2)
        {
          if (micro_v1 > micro_v2)
            return 1;
          else if (micro_v1 == micro_v2)
            return 0;
        }
    }

  return -1;
}

#define LIMIT_MAX_CHARS = 1000;

/* Get the size of the text after limiting it to
 * n paragraphs or n lines.
 *
 * A value of 0 for lines or paragraphs means a not used limit.
 */
gsize
ide_xml_utils_get_text_limit (const gchar *text,
                              gsize        paragraphs,
                              gsize        lines,
                              gboolean    *has_more)
{
  const gchar *cursor, *limit, *end;
  gboolean para_limit = (paragraphs > 0);
  gboolean lines_limit = (lines > 0);
  gsize len;
  gsize count;

  if (dzl_str_empty0 (text))
    return 0;

  len = strlen (text);
  end = text + len;
  cursor = limit = text;
  while (cursor < end)
    {
      if (!(cursor = strchr (cursor, '\n')))
        break;

      limit = cursor;
      if (lines_limit && --lines == 0)
        break;

      cursor++;
      if (*cursor != '\n')
        continue;

      if (para_limit && --paragraphs == 0)
        break;

      cursor++;
    }

  if (cursor == NULL)
    {
      *has_more = FALSE;
      limit = text + len;
    }
  else
   *has_more = (cursor < end && *(++cursor) != '\0');

  count = limit - text;
  return count;
}

static gboolean
gi_class_walker (IdeGiBase             *object,
                 const gchar           *name,
                 IdeXmlUtilsWalkerFunc  func,
                 GHashTable            *visited,
                 gpointer               data)
{
  g_autofree gchar *qname = NULL;
  IdeGiBlobType type;
  guint16 n_interfaces;

  qname = ide_gi_base_get_qualified_name (object);
  if (g_hash_table_contains (visited, qname))
    return FALSE;

  if (func (object, name, data))
    return TRUE;

  g_hash_table_add (visited, g_steal_pointer (&qname));

  type = ide_gi_base_get_object_type (object);
  if (type == IDE_GI_BLOB_TYPE_CLASS)
    {
      g_autoptr(IdeGiClass) parent_class = ide_gi_class_get_parent ((IdeGiClass *)object);

      if (parent_class != NULL && gi_class_walker ((IdeGiBase *)parent_class, name, func, visited, data))
        return TRUE;

      n_interfaces = ide_gi_class_get_n_interfaces ((IdeGiClass *)object);
      for (guint i = 0; i < n_interfaces; i++)
        {
          g_autoptr(IdeGiInterface) interface = ide_gi_class_get_interface ((IdeGiClass *)object, i);

          if (interface != NULL && gi_class_walker ((IdeGiBase *)interface, name, func, visited, data))
            return TRUE;
        }
    }
  else if (type == IDE_GI_BLOB_TYPE_INTERFACE)
    {
      n_interfaces = ide_gi_interface_get_n_prerequisites ((IdeGiInterface *)object);
      for (guint i = 0; i < n_interfaces; i++)
        {
          g_autoptr(IdeGiBase) base = ide_gi_interface_get_prerequisite ((IdeGiInterface *)object, i);

          if (base != NULL && gi_class_walker (base, name, func, visited, data))
            return TRUE;
        }
    }

  return FALSE;
}

gboolean
ide_xml_utils_gi_class_walker (IdeGiBase             *object,
                               const gchar           *name,
                               IdeXmlUtilsWalkerFunc  func,
                               gpointer               data)
{
  g_autoptr(GHashTable) visited = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  g_assert (ide_gi_base_get_object_type (object) == IDE_GI_BLOB_TYPE_CLASS);
  g_assert (func != NULL);

  return gi_class_walker (object, name, func, visited, data);
}

/* Return a copy of str, limited to 'limit' chars,
 * with possibly stripped whitespaces and added
 * ellispsis at te end.
 */
gchar *
ide_xml_utils_limit_str (const gchar *str,
                         gsize        limit,
                         gboolean     strip,
                         gboolean     add_ellipsis)
{
  const gchar *begin = str;
  const gchar *end;
  gsize count = 0;
  gunichar ch;

  g_return_val_if_fail (!dzl_str_empty0 (str), NULL);
  g_return_val_if_fail (limit > 0, NULL);

  if (strip)
    for (begin = (gchar*) str; *begin && g_ascii_isspace (*begin); begin++)
     ;

  end = begin;
  while ((ch = g_utf8_get_char (end)))
    {
      if (++count > limit)
        break;

      end = g_utf8_next_char (end);
    }

  if (end > begin)
    {
      if (strip)
        {
          do
            {
              --end;
              if (!g_ascii_isspace (*end))
                {
                  end++;
                  break;
                }

              count--;
            } while (end > begin);
        }
    }

  if (add_ellipsis && count > limit)
    {
      GString *new_str = g_string_sized_new (end - begin + 4);

      g_string_append_len (new_str, begin, end - begin);
      g_string_append (new_str, " …");

      return g_string_free (new_str, FALSE);
    }
  else
    return g_strndup (begin, end - begin);
}
