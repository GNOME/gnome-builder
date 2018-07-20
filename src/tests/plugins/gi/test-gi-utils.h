/* test-gi-utils.h
 *
 * Copyright 2018 Sebastien Lafargue <slafargue@gnome.org>
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

#pragma once

#include <glib.h>

#include "../../../plugins/gi/ide-gi-utils.h"

#include <libxml/xmlreader.h>

G_BEGIN_DECLS

static inline void G_GNUC_UNUSED
_autofree_cleanup_xmlFree (void *p)
{
  void **pp = (void**)p;
  xmlFree (*pp);
}

#define g_autoxmlfree __attribute__((cleanup(_autofree_cleanup_xmlFree)))

// assert_message (const gchar *name);
#define assert_message(...)                                                                          \
  G_STMT_START {                                                                                     \
    g_autofree gchar *str = g_strdup_printf (__VA_ARGS__);                                           \
    g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, str);                          \
  } G_STMT_END

// assert_attr_str (xmlTextReaderPtr  reader,
//                  const gchar      *name,
//                  const gchar      *value);
#define assert_attr_str(reader, name, value)                                                         \
  G_STMT_START {                                                                                     \
    g_autoxmlfree guchar *attr_value = NULL;                                                         \
    attr_value = (guchar *)xmlTextReaderGetAttribute (reader, (guchar *)name);                       \
    if (!dzl_str_empty0 (attr_value) && !dzl_str_equal0 (attr_value, value))                         \
      {                                                                                              \
        gchar *str = g_strdup_printf ("%s != %s", attr_value, value);                                \
        g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, str);                      \
      }                                                                                              \
  } G_STMT_END

// assert_attr_bool (xmlTextReaderPtr  reader,
//                   const gchar      *name,
//                   const gchar      *default,
//                   gboolean          value);
#define assert_attr_bool(reader, name, default, value)                                               \
  G_STMT_START {                                                                                     \
    g_autoxmlfree guchar *attr_value = NULL;                                                         \
    g_autofree gchar *str = NULL;                                                                    \
    attr_value = xmlTextReaderGetAttribute (reader, (guchar *)name);                                 \
    str = (dzl_str_empty0 (attr_value)) ? g_strdup (default) : g_strdup ((gchar *)attr_value);       \
    if ((dzl_str_equal0 (str, "1") && !value) || (dzl_str_equal0 (str, "0") && value))               \
      {                                                                                              \
        gchar *err = g_strdup_printf ("%s != %s", default, value ? "1" : "0");                       \
        g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, err);                      \
      }                                                                                              \
  } G_STMT_END

// assert_attr_int (xmlTextReaderPtr  reader,
//                  const gchar      *name,
//                  const gchar      *default,
//                  gint              value);
#define assert_attr_int(reader, name, default, value)                                                \
  G_STMT_START {                                                                                     \
    g_autoxmlfree guchar *attr_value = NULL;                                                         \
    g_autofree gchar *str = NULL;                                                                    \
    attr_value = (guchar *)xmlTextReaderGetAttribute (reader, (guchar *)name);                       \
    str = (dzl_str_empty0 (attr_value)) ? g_strdup (default) : g_strdup ((gchar *)attr_value);       \
    if (atoi ((gchar *)str) != value)                                                                \
      {                                                                                              \
        gchar *err = g_strdup_printf ("%s != %s", str, value ? "1" : "0");                           \
        g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, err);                      \
      }                                                                                              \
  } G_STMT_END

// assert_attr_direction (xmlTextReaderPtr  reader,
//                        const gchar      *name,
//                        const gchar      *default,
//                        IdeGiDirection    direction);
#define assert_attr_direction(reader, name, default, direction)                                      \
  G_STMT_START {                                                                                     \
    g_autoxmlfree guchar *attr_value = NULL;                                                         \
    g_autofree gchar *str = NULL;                                                                    \
    attr_value = (guchar *)xmlTextReaderGetAttribute (reader, (guchar *)name);                       \
    str = (dzl_str_empty0 (attr_value)) ? g_strdup (default) : g_strdup ((gchar *)attr_value);       \
    for (guint i = 0; i < G_N_ELEMENTS (IDE_GI_DIRECTION_NAMES); i++)                                \
      if (dzl_str_equal0 (str, IDE_GI_DIRECTION_NAMES[i]) && (IdeGiDirection)i != direction)         \
        {                                                                                            \
          gchar *err = g_strdup_printf ("%s != %s", str, IDE_GI_DIRECTION_NAMES[direction]);         \
          g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, err);                    \
        }                                                                                            \
  } G_STMT_END

// assert_attr_stability (xmlTextReaderPtr  reader,
//                        const gchar      *name,
//                        const gchar      *default,
//                        IdeGiStability    stability);
#define assert_attr_stability(reader, name, default, stability)                                      \
  G_STMT_START {                                                                                     \
    g_autoxmlfree guchar *attr_value = NULL;                                                         \
    g_autofree gchar *str = NULL;                                                                    \
    attr_value = (guchar *)xmlTextReaderGetAttribute (reader, (guchar *)name);                       \
    str = (dzl_str_empty0 (attr_value)) ? g_strdup (default) : g_strdup ((gchar *)attr_value);       \
    for (guint i = 0; i < G_N_ELEMENTS (IDE_GI_STABILITY_NAMES); i++)                                \
      if (dzl_str_equal0 (str, IDE_GI_STABILITY_NAMES[i]) && (IdeGiStability)i != stability)         \
        {                                                                                            \
          gchar *err = g_strdup_printf ("%s != %s", str, IDE_GI_STABILITY_NAMES[stability]);         \
          g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, err);                    \
        }                                                                                            \
  } G_STMT_END

// assert_attr_scope (xmlTextReaderPtr  reader,
//                    const gchar      *name,
//                    const gchar      *default,
//                    IdeGiScope        scope);
#define assert_attr_scope(reader, name, default, scope)                                              \
  G_STMT_START {                                                                                     \
    g_autoxmlfree guchar *attr_value = NULL;                                                         \
    g_autofree gchar *str = NULL;                                                                    \
    attr_value = (guchar *)xmlTextReaderGetAttribute (reader, (guchar *)name);                       \
    str = (dzl_str_empty0 (attr_value)) ? g_strdup (default) : g_strdup ((gchar *)attr_value);       \
    for (guint i = 0; i < G_N_ELEMENTS (IDE_GI_SCOPE_NAMES); i++)                                    \
      if (dzl_str_equal0 (str, IDE_GI_SCOPE_NAMES[i]) && (IdeGiScope)i != scope)                     \
        {                                                                                            \
          gchar *err = g_strdup_printf ("%s != %s", str, IDE_GI_SCOPE_NAMES[scope]);                 \
          g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, err);                    \
        }                                                                                            \
  } G_STMT_END

// assert_attr_transfer (xmlTextReaderPtr        reader,
//                       const gchar            *name,
//                       const gchar      *default,
//                       IdeGiTransferOwnership  transfer);
#define assert_attr_transfer(reader, name, default, transfer)                                                 \
  G_STMT_START {                                                                                              \
    g_autoxmlfree guchar *attr_value = NULL;                                                                  \
    g_autofree gchar *str = NULL;                                                                             \
    attr_value = (guchar *)xmlTextReaderGetAttribute (reader, (guchar *)name);                                \
    str = (dzl_str_empty0 (attr_value)) ? g_strdup (default) : g_strdup ((gchar *)attr_value);                \
    for (guint i = 0; i < G_N_ELEMENTS (IDE_GI_TRANSFER_OWNERSHIP_NAMES); i++)                                \
      if (dzl_str_equal0 (str, IDE_GI_TRANSFER_OWNERSHIP_NAMES[i]) && (IdeGiTransferOwnership)i != transfer)  \
        {                                                                                                     \
          gchar *err = g_strdup_printf ("%s != %s", str, IDE_GI_TRANSFER_OWNERSHIP_NAMES[transfer]);          \
          g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, err);                             \
        }                                                                                                     \
  } G_STMT_END

// assert_attr_when (xmlTextReaderPtr  reader,
//                   const gchar      *name,
//                   const gchar      *default,
//                   IdeGiSignalWhen   when);
#define assert_attr_when(reader, name, default, when)                                                \
  G_STMT_START {                                                                                     \
    g_autoxmlfree guchar *attr_value = NULL;                                                         \
    g_autofree gchar *str = NULL;                                                                    \
    attr_value = (guchar *)xmlTextReaderGetAttribute (reader, (guchar *)name);                       \
    str = (dzl_str_empty0 (attr_value)) ? g_strdup (default) : g_strdup ((gchar *)attr_value);       \
    for (guint i = 0; i < G_N_ELEMENTS (IDE_GI_SIGNAL_WHEN_NAMES); i++)                              \
      if (dzl_str_equal0 (str, IDE_GI_SIGNAL_WHEN_NAMES[i]) && (IdeGiSignalWhen)i != when)           \
        {                                                                                            \
          gchar *err = g_strdup_printf ("%s != %s", str, IDE_GI_SIGNAL_WHEN_NAMES[when]);            \
          g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, err);                    \
        }                                                                                            \
  } G_STMT_END

G_END_DECLS
