/* ide-path-cache.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-path-cache"

#include "config.h"

#include "ide-path-cache.h"

/**
 * SECTION:ide-path-cache
 * @title: IdePathCache
 * @Short_description: thread-safe cache for path lookups
 *
 * #IdePathCache can be used to cache path lookup entries as often
 * needed by runtimes.
 *
 * This object is thread-safe and may be accessed from multiple
 * threads simultaneously.
 */

struct _IdePathCache
{
  GObject     parent_instance;
  GMutex      mutex;
  GHashTable *cache;
};

G_DEFINE_FINAL_TYPE (IdePathCache, ide_path_cache, G_TYPE_OBJECT)

static void
ide_path_cache_dispose (GObject *object)
{
  IdePathCache *self = (IdePathCache *)object;

  g_hash_table_remove_all (self->cache);

  G_OBJECT_CLASS (ide_path_cache_parent_class)->dispose (object);
}

static void
ide_path_cache_finalize (GObject *object)
{
  IdePathCache *self = (IdePathCache *)object;

  g_clear_pointer (&self->cache, g_hash_table_unref);
  g_mutex_clear (&self->mutex);

  G_OBJECT_CLASS (ide_path_cache_parent_class)->finalize (object);
}

static void
ide_path_cache_class_init (IdePathCacheClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_path_cache_dispose;
  object_class->finalize = ide_path_cache_finalize;
}

static void
ide_path_cache_init (IdePathCache *self)
{
  g_mutex_init (&self->mutex);
  self->cache = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
}

IdePathCache *
ide_path_cache_new (void)
{
  return g_object_new (IDE_TYPE_PATH_CACHE, NULL);
}

/**
 * ide_path_cache_lookup:
 * @self: a #IdePathCache
 * @program_name: the program name to lookup
 * @program_path: (out) (transfer full) (nullable): a location to store
 *   the @program_path for @program_name.
 *
 * %TRUE is returned when an entry is found in the cache. That entry
 * may be %NULL indicating a negative cache entry.
 *
 * Returns: %TRUE if @program_name was found and @program_path is set;
 *   otherwise %FALSE is returned.
 */
gboolean
ide_path_cache_lookup (IdePathCache  *self,
                       const char    *program_name,
                       char         **program_path)
{
  const char *path = NULL;
  gboolean ret;

  g_return_val_if_fail (IDE_IS_PATH_CACHE (self), FALSE);
  g_return_val_if_fail (program_name, FALSE);

  g_mutex_lock (&self->mutex);
  ret = g_hash_table_lookup_extended (self->cache, program_name, NULL, (gpointer *)&path);
  if (program_path != NULL)
    *program_path = path ? g_strdup (path) : NULL;
  g_mutex_unlock (&self->mutex);

  return ret;
}

/**
 * ide_path_cache_contains:
 * @self: a #IdePathCache
 * @program_name: the name of the program to lookup
 * @had_program_path: (out) (nullable): a location to store if a path
 *   was found for @program_name.
 *
 * This function helps for detecting negative cache entries without
 * copying the program_path string.
 *
 * Returns: %TRUE if an entry was found and @had_program_path is set;
 *   otherwise %FALSE.
 */
gboolean
ide_path_cache_contains (IdePathCache *self,
                         const char   *program_name,
                         gboolean     *had_program_path)
{
  const char *path = NULL;
  gboolean ret;

  g_return_val_if_fail (IDE_IS_PATH_CACHE (self), FALSE);
  g_return_val_if_fail (program_name, FALSE);

  g_mutex_lock (&self->mutex);
  ret = g_hash_table_lookup_extended (self->cache, program_name, NULL, (gpointer *)&path);
  if (had_program_path)
    *had_program_path = path != NULL;
  g_mutex_unlock (&self->mutex);

  return ret;
}

/**
 * ide_path_cache_insert:
 * @self: a #IdePathCache
 * @program_name: the name of the program
 * @program_path: (nullable): the path for the program
 *
 * Inserts a cache entry for @program_name for @program_path.
 *
 * @program_path may be %NULL to register a negative cache entry. See
 * ide_path_cache_lookup() for handling negative cache entries.
 */
void
ide_path_cache_insert (IdePathCache *self,
                       const char   *program_name,
                       const char   *program_path)
{
  char *key;
  char *value;

  g_return_if_fail (IDE_IS_PATH_CACHE (self));
  g_return_if_fail (program_name != NULL);

  key = g_strdup (program_name);
  value = g_strdup (program_path);

  g_mutex_lock (&self->mutex);
  g_hash_table_insert (self->cache, key, value);
  g_mutex_unlock (&self->mutex);
}
