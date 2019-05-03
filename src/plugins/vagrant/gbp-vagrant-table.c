/* gbp-vagrant-table.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-vagrant-table"

#include "config.h"

#include <libide-io.h>

#include "gbp-vagrant-table.h"

struct _GbpVagrantTable
{
  gchar *data;
};

GbpVagrantTable *
gbp_vagrant_table_new_take (gchar *data)
{
  GbpVagrantTable *self;

  self = g_slice_new0 (GbpVagrantTable);
  self->data = data;

  return g_steal_pointer (&self);
}

void
gbp_vagrant_table_free (GbpVagrantTable *self)
{
  g_clear_pointer (&self->data, g_free);
  g_slice_free (GbpVagrantTable, self);
}

void
gbp_vagrant_table_iter_init (GbpVagrantTableIter *iter,
                             GbpVagrantTable     *table)
{
  ide_line_reader_init (&iter->reader, table->data, -1);
}

gboolean
gbp_vagrant_table_iter_next (GbpVagrantTableIter *iter)
{
  gchar *ret;
  gsize len;

  g_return_val_if_fail (iter != NULL, FALSE);

  if ((ret = ide_line_reader_next (&iter->reader, &len)))
    {
      iter->cur = ret;
      ret [len] = 0;
      return TRUE;
    }

  iter->cur = NULL;
  return FALSE;
}

static gchar *
unescape (gchar *str)
{
  gchar *ptr;
  gchar *endptr;

#define COMMA "%!(VAGRANT_COMMA)"

  endptr = str + strlen (str);

  while ((ptr = strstr (str, COMMA)))
    {
      gchar *after = ptr + strlen (COMMA);

      *ptr = ',';
      memmove (ptr + 1, after, endptr - after);
      ptr++;
    }

#undef COMMA

  return str;
}

gchar *
gbp_vagrant_table_iter_get_column (GbpVagrantTableIter *iter,
                                   guint                column)
{
  gchar *line;

  g_return_val_if_fail (iter != NULL, NULL);
  g_return_val_if_fail (iter->cur != NULL, NULL);

  line = iter->cur;

  while (column > 0 && line != NULL)
    {
      column--;
      line = strchr (line, ',');
      if (line != NULL)
        line++;
    }

  if (column == 0 && line != NULL)
    {
      gchar *end = strchrnul (line, ',');
      g_autofree gchar *val = g_strndup (line, end - line);
      return unescape (g_steal_pointer (&val));
    }

  return NULL;
}
