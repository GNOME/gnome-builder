/* dspy-signature.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#define G_LOG_DOMAIN "dspy-signature"

#include <glib/gi18n.h>

#include "dspy-private.h"

static GHashTable *
get_common_signatures (void)
{
  static GHashTable *common;

  if (g_once_init_enter (&common))
    {
      GHashTable *ht = g_hash_table_new (g_str_hash, g_str_equal);

#define INSERT_COMMON(type,word) g_hash_table_insert(ht, (gchar *)type, (gchar *)word)
      INSERT_COMMON ("n",     "int16");
      INSERT_COMMON ("q",     "uint16");
      INSERT_COMMON ("i",     "int32");
      INSERT_COMMON ("u",     "uint32");
      INSERT_COMMON ("x",     "int64");
      INSERT_COMMON ("t",     "uint64");
      INSERT_COMMON ("s",     "string");
      INSERT_COMMON ("b",     "boolean");
      INSERT_COMMON ("y",     "byte");
      INSERT_COMMON ("o",     "Object Path");
      INSERT_COMMON ("g",     "Signature");
      INSERT_COMMON ("d",     "double");
      INSERT_COMMON ("v",     "Variant");
      INSERT_COMMON ("h",     "File Descriptor");
      INSERT_COMMON ("as",    "string[]");
      INSERT_COMMON ("a{sv}", "Vardict");
      INSERT_COMMON ("ay",    "Byte Array");
#undef INSERT_COMMON

      g_once_init_leave (&common, ht);
    }

  return common;
}

gchar *
_dspy_signature_humanize (const gchar *signature)
{
  GHashTable *common;
  const gchar *found;

  if (signature == NULL)
    return NULL;

  common = get_common_signatures ();

  if ((found = g_hash_table_lookup (common, signature)))
    return g_strdup (found);

  /* If this is a simple array of something else ... */
  if ((found = g_hash_table_lookup (common, signature + 1)))
    /* translators: %s is replaced with the simple D-Bus type string */
    return g_strdup_printf (_("Array of [%s]"), found);

  return g_strdup (signature);
}
