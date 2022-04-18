/* ide-makecache-target.c
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-makecache-target"

#include "config.h"

#include <libide-io.h>

#include "ide-makecache-target.h"

G_DEFINE_BOXED_TYPE (IdeMakecacheTarget, ide_makecache_target,
                     ide_makecache_target_ref, ide_makecache_target_unref)

struct _IdeMakecacheTarget
{
  volatile gint  ref_count;

  gchar         *subdir;
  gchar         *target;
};

void
ide_makecache_target_unref (IdeMakecacheTarget *self)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (self->ref_count > 0);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    {
      g_free (self->subdir);
      g_free (self->target);
      g_slice_free (IdeMakecacheTarget, self);
    }
}

IdeMakecacheTarget *
ide_makecache_target_ref (IdeMakecacheTarget *self)
{
  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (self->ref_count > 0, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

IdeMakecacheTarget *
ide_makecache_target_new (const gchar *subdir,
                          const gchar *target)
{
  IdeMakecacheTarget *self;

  g_assert (target);

  if (subdir != NULL && (subdir [0] == '.' || subdir [0] == '\0'))
    subdir = NULL;

  self = g_slice_new0 (IdeMakecacheTarget);
  self->ref_count = 1;
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

void
ide_makecache_target_set_target (IdeMakecacheTarget *self,
                                 const gchar        *target)
{
  g_assert (self);

  g_free (self->target);
  self->target = g_strdup (target);
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
