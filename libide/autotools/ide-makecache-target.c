/* ide-makecache-target.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#include "ide-makecache-target.h"

struct _IdeMakecacheTarget
{
  gchar *subdir;
  gchar *target;
};

void
ide_makecache_target_free (IdeMakecacheTarget *self)
{
  g_free (self->subdir);
  g_free (self->target);
  g_free (self);
}

IdeMakecacheTarget *
ide_makecache_target_new (const gchar *subdir,
                          const gchar *target)
{
  IdeMakecacheTarget *self;

  g_assert (target);

  if (subdir != NULL && (subdir [0] == '.' || subdir [0] == '\0'))
    subdir = NULL;

  self = g_new0 (IdeMakecacheTarget, 1);
  self->subdir = g_strdup (subdir);
  self->target = g_strdup (target);

  return self;
}

const gchar *
ide_makecache_target_get_subdir (IdeMakecacheTarget *self)
{
  g_assert (self);
  return self->subdir;
}

const gchar *
ide_makecache_target_get_target (IdeMakecacheTarget *self)
{
  g_assert (self);
  return self->target;
}

guint
ide_makecache_target_hash (gconstpointer data)
{
  const IdeMakecacheTarget *self = data;

  return (g_str_hash (self->subdir ?: "") ^ g_str_hash (self->target));
}

gboolean
ide_makecache_target_equal (gconstpointer data1,
                            gconstpointer data2)
{
  const IdeMakecacheTarget *a = data1;
  const IdeMakecacheTarget *b = data2;

  return ((g_strcmp0 (a->subdir, b->subdir) == 0) &&
          (g_strcmp0 (a->target, b->target) == 0));
}
