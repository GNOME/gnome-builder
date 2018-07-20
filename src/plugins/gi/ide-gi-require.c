/* ide-gi-require.c
 *
 * Copyright (C) 2017 Sebastien Lafargue <slafargue@gnome.org>
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
#include <ide.h>

#include "ide-gi-require.h"

G_DEFINE_BOXED_TYPE (IdeGiRequire, ide_gi_require, ide_gi_require_ref, ide_gi_require_unref)

/* Keep in sync with corresponding enums in ide-gi-require.h */
static const gchar * IDE_GI_REQUIRE_COMP_NAMES [] =
{
  "=",
  "<",
  "<=",
  ">",
  ">=",
};

static void
require_entry_free (gpointer data)
{
  IdeGiRequireEntry *self = (IdeGiRequireEntry *)data;

  g_slice_free (IdeGiRequireEntry, self);
}

static inline IdeGiRequireEntry *
require_entry_copy (gpointer data)
{
  IdeGiRequireEntry *self = (IdeGiRequireEntry *)data;
  IdeGiRequireEntry *copy;

  copy = g_slice_new0 (IdeGiRequireEntry);
  copy->min = self->min;
  copy->max = self->max;
  copy->is_range = self->is_range;

  return copy;
}

static void
foreach_dump_func (gpointer key,
                   gpointer value,
                   gpointer user_data)
{
  const gchar *ns = (const gchar *)key;
  const IdeGiRequireEntry *entry = (const IdeGiRequireEntry *)value;
  const gchar *comp_str;

  comp_str = IDE_GI_REQUIRE_COMP_NAMES[entry->min.comp];
  g_print ("%s %s %d.%d", ns, comp_str, entry->min.major_version, entry->min.minor_version);
  if (entry->is_range)
    {
      comp_str = IDE_GI_REQUIRE_COMP_NAMES[entry->max.comp];
      g_print ("&& %s %s %d.%d", ns, comp_str, entry->max.major_version, entry->max.minor_version);
    }

  g_print ("\n");
}

void
ide_gi_require_dump (IdeGiRequire *self)
{
  g_return_if_fail (self != NULL);

  g_hash_table_foreach (self->entries, foreach_dump_func, NULL);
}

static void
require_copy_entry_func (gpointer key,
                         gpointer value,
                         gpointer user_data)
{
  const gchar *ns = (const gchar *)key;
  IdeGiRequireEntry *entry = (IdeGiRequireEntry *)value;
  IdeGiRequire *req = (IdeGiRequire *)user_data;
  IdeGiRequireEntry *new_entry = NULL;

  g_assert (entry != NULL);
  g_assert (req != NULL);

  new_entry = g_slice_new0 (IdeGiRequireEntry);
  new_entry->min = entry->min;
  new_entry->max = entry->max;
  new_entry->is_range = entry->is_range;

  g_hash_table_insert (req->entries, g_strdup (ns), new_entry);
}

void
ide_gi_require_foreach (IdeGiRequire *self,
                        GHFunc        func,
                        gpointer      user_data)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (func != NULL);

  g_hash_table_foreach (self->entries, func, user_data);
}

IdeGiRequire *
ide_gi_require_copy (IdeGiRequire *self)
{
  IdeGiRequire *req;

  g_return_val_if_fail (self != NULL, NULL);

  req = ide_gi_require_new ();
  ide_gi_require_foreach (self, require_copy_entry_func, req);

  return req;
}

/**
 * ide_gi_require_lookup:
 *
 * Get the #IdeGiRequireEntry for the namespace or %NULL if there's no entry.
 *
 * Returns: (nullable) (transfer none)
 */
IdeGiRequireEntry *
ide_gi_require_lookup (IdeGiRequire *self,
                       const gchar  *ns)
{
  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (ns != NULL, NULL);

  return (IdeGiRequireEntry *)g_hash_table_lookup (self->entries, ns);
}

gboolean
ide_gi_require_contains (IdeGiRequire       *self,
                         const gchar        *ns,
                         guint16             major_version,
                         IdeGiRequireEntry **existing_entry)
{
  IdeGiRequireEntry *entry;

  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (!dzl_str_empty0 (ns), FALSE);

  if ((entry = (IdeGiRequireEntry *)g_hash_table_lookup (self->entries, ns)) &&
      entry->min.major_version == major_version)
    {
      if (existing_entry != NULL)
        *existing_entry = entry;

      return TRUE;
    }

  return FALSE;
}

static gboolean
is_minor_version_match (IdeGiRequireBound *bound,
                        guint16            minor_version)
{
  g_assert (bound != NULL);

  switch (bound->comp)
    {
    case IDE_GI_REQUIRE_COMP_GREATER_OR_EQUAL:
      if (minor_version >= bound->minor_version)
        return TRUE;

      break;

    case IDE_GI_REQUIRE_COMP_GREATER:
      if (minor_version > bound->minor_version)
        return TRUE;

      break;

    case IDE_GI_REQUIRE_COMP_EQUAL:
      if (minor_version == bound->minor_version)
        return TRUE;

      break;

    case IDE_GI_REQUIRE_COMP_LESS_OR_EQUAL:
      if (minor_version <= bound->minor_version)
        return TRUE;

      break;

    case IDE_GI_REQUIRE_COMP_LESS:
      if (minor_version < bound->minor_version)
        return TRUE;

      break;

    default:
      g_assert_not_reached ();
    }

  return FALSE;
}

static gboolean
is_version_match (IdeGiRequireEntry *entry,
                  guint16            major_version,
                  guint16            minor_version)
{
  g_assert (entry != NULL);

  if (major_version != entry->min.major_version)
    return FALSE;

  if (!is_minor_version_match (&entry->min, minor_version))
    return FALSE;

  if (entry->is_range && !is_minor_version_match (&entry->max, minor_version))
    return FALSE;

  return TRUE;
}

gboolean
ide_gi_require_match (IdeGiRequire *self,
                      const gchar  *ns,
                      guint16       major_version,
                      guint16       minor_version)
{
  IdeGiRequireEntry *entry;

  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (!dzl_str_empty0 (ns), FALSE);

  if (NULL == (entry = g_hash_table_lookup (self->entries, ns)))
    return FALSE;

  return is_version_match (entry, major_version, minor_version);
}

gboolean
ide_gi_require_match_namespace (IdeGiRequire   *self,
                                IdeGiNamespace *ns)
{
  IdeGiRequireEntry *entry;
  const gchar *name;

  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (ns != NULL, FALSE);

  name = ide_gi_namespace_get_name (ns);
  if (NULL == (entry = g_hash_table_lookup (self->entries, name)))
    return FALSE;

  return is_version_match (entry,
                           ide_gi_namespace_get_major_version (ns),
                           ide_gi_namespace_get_minor_version (ns));
}

static inline gint
sign_of (gint value)
{
  return (value > 0) - (value < 0);
}

/* Return 0 if equal, -1 if min < max or 1 if min > max */
static gint
compare_bounds (IdeGiRequireBound *min,
                IdeGiRequireBound *max)
{
  if (min->major_version == max->major_version)
    {
      if (min->minor_version == max->minor_version)
        return 0;

      return sign_of (max->minor_version - min->minor_version);
    }
  else
    return sign_of (max->major_version - min->major_version);
}

static inline gboolean
check_bound (IdeGiRequireBound *bound)
{
  return !(bound->major_version == 0 &&
           bound->minor_version == 0 &&
           (bound->comp == IDE_GI_REQUIRE_COMP_LESS || bound->comp == IDE_GI_REQUIRE_COMP_LESS_OR_EQUAL));
}

/* Check the validity of bounds:
 * - no < or <= to 0.0
 * - min bound always <= to max bound
 * - min and max bound major_version should be the same.
 */
static gboolean
check_bounds (IdeGiRequireBound *min,
              IdeGiRequireBound *max,
              gboolean           is_range)
{
  if (!check_bound (min))
    {
      g_warning ("The min bound is not valid");
      return FALSE;
    }

  if (!is_range)
    return TRUE;

  if (!check_bound (max))
    {
      g_warning ("The max bound is not valid");
      return FALSE;
    }

  if (min->major_version != max->major_version)
    {
      g_warning ("The min and max bounds major version are different");
      return FALSE;
    }

  if (compare_bounds (min, max) > 0)
    {
      g_warning ("The min bound is superior to the max bound");
      return FALSE;
    }

  return TRUE;
}

/**
 * ide_gi_require_add:
 *
 * Add requirements for a namespace using a single bound.
 * Redefining an already added namespace replace it.
 *
 * Returns: %FALSE if the validity checks failed
 */
gboolean
ide_gi_require_add (IdeGiRequire      *self,
                    const gchar       *ns,
                    IdeGiRequireBound  bound)
{
  IdeGiRequireEntry *entry;

  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (!dzl_str_empty0 (ns), FALSE);

  if (!check_bounds (&bound, NULL, FALSE))
    return FALSE;

  if ((entry = (IdeGiRequireEntry *)g_hash_table_lookup (self->entries, ns)))
    {
      entry->min = bound;
      entry->max = (IdeGiRequireBound){0};
      entry->is_range = FALSE;
    }
  else
    {
      entry = g_slice_new0 (IdeGiRequireEntry);
      entry->min = bound;
      entry->is_range = FALSE;

      g_hash_table_insert (self->entries, g_strdup (ns), entry);
    }

  return TRUE;
}

/**
 * ide_gi_require_add_range:
 *
 * Add requirements for a namespace using a range.
 * Redefining an already added namespace, using the same major version, replace it.
 *
 * Returns: %FALSE if the validity checks failed
 */
gboolean
ide_gi_require_add_range (IdeGiRequire      *self,
                          const gchar       *ns,
                          IdeGiRequireBound  min_bound,
                          IdeGiRequireBound  max_bound)
{
  IdeGiRequireEntry *entry;

  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (!dzl_str_empty0 (ns), FALSE);

  if (!check_bounds (&min_bound, &max_bound, TRUE))
    return FALSE;

  if ((entry = (IdeGiRequireEntry *)g_hash_table_lookup (self->entries, ns)))
    {
      entry->min = min_bound;
      entry->max = max_bound;
      entry->is_range = TRUE;
    }
  else
    {
      entry = g_slice_new0 (IdeGiRequireEntry);
      entry->min = min_bound;
      entry->max = max_bound;
      entry->is_range = TRUE;

      g_hash_table_insert (self->entries, g_strdup (ns), entry);
    }

  return TRUE;
}

gboolean
ide_gi_require_remove (IdeGiRequire *self,
                       const gchar  *ns)
{
  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (!dzl_str_empty0 (ns), FALSE);

  return g_hash_table_remove (self->entries, ns);
}

static inline gboolean
is_greater_or_equal_than (IdeGiRequireEntry *src_entry,
                          IdeGiRequireEntry *dst_entry)
{
  IdeGiRequireBound src_ref;
  IdeGiRequireBound dst_ref;

  src_ref = (src_entry->is_range) ? src_entry->max : src_entry->min;
  dst_ref = (dst_entry->is_range) ? dst_entry->max : dst_entry->min;

  return (compare_bounds (&src_ref, &dst_ref) != -1);
}

static inline gboolean
is_greater_or_equal_than_bound (IdeGiRequireEntry *src_entry,
                                IdeGiRequireBound *bound)
{
  IdeGiRequireBound src_ref;

  src_ref = (src_entry->is_range) ? src_entry->max : src_entry->min;

  return (compare_bounds (&src_ref, bound) != -1);
}

/**
 * ide_gi_require_merge:
 * @self: #IdeGiRequire
 * @added: a #IdeGiRequire to merge with
 * @strategy: a #IdeGiRequireStrategy merging strategy
 *
 * Merge the @added #IdeGiRequire using the @strategy
 *
 * Returns: none
 */
void
ide_gi_require_merge (IdeGiRequire              *self,
                      IdeGiRequire              *added,
                      IdeGiRequireMergeStrategy  strategy)
{
  GHashTableIter iter;
  gpointer key, value;

  g_return_if_fail (self != NULL);
  g_return_if_fail (added != NULL);

  g_hash_table_iter_init (&iter, added->entries);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      IdeGiRequireEntry *dst_entry = (IdeGiRequireEntry *)value;
      IdeGiRequireEntry *src_entry;

      if ((src_entry = (IdeGiRequireEntry *)g_hash_table_lookup (self->entries, key)))
        {
          if (strategy == IDE_GI_REQUIRE_MERGE_STRATEGY_KEEP_GREATEST)
            {
              if (!is_greater_or_equal_than (src_entry, dst_entry))
                {
                  src_entry->min = dst_entry->min;
                  src_entry->max = dst_entry->max;
                  src_entry->is_range = dst_entry->is_range;
                }
            }
          else
            g_assert (strategy == IDE_GI_REQUIRE_MERGE_STRATEGY_KEEP_SOURCE);
        }
      else
        g_hash_table_insert (self->entries,
                             g_strdup (key),
                             require_entry_copy (dst_entry));
    }
}

/**
 * ide_gi_require_merge_namespace:
 * @self: #IdeGiRequire
 * @strategy: a #IdeGiRequireStrategy merging strategy
 * @ns: nnamespace name
 * @major_version; namespace major version
 * @minor_version; namespace minor version
 *
 * @strategy: a #IdeGiRequireStrategy merging strategy
 *
 * Merge the @added #IdeGiRequire using the @strategy
 *
 * Returns: none
 */
void
ide_gi_require_merge_namespace (IdeGiRequire              *self,
                                IdeGiRequireMergeStrategy  strategy,
                                const gchar               *ns,
                                guint16                    major_version,
                                guint16                    minor_version)
{
  IdeGiRequireEntry *entry;
  IdeGiRequireBound bound = (IdeGiRequireBound){.comp = IDE_GI_REQUIRE_COMP_EQUAL,
                                                .major_version = major_version,
                                                .minor_version = minor_version};

  g_return_if_fail (self != NULL);
  g_return_if_fail (!dzl_str_empty0 (ns));

  if ((entry = (IdeGiRequireEntry *)g_hash_table_lookup (self->entries, ns)))
    {
      if (strategy == IDE_GI_REQUIRE_MERGE_STRATEGY_KEEP_GREATEST)
        {
          if (!is_greater_or_equal_than_bound (entry, &bound))
            {
              entry->min = bound;
              entry->max = (IdeGiRequireBound){0};
              entry->is_range = FALSE;
            }
        }
      else
        g_assert (strategy == IDE_GI_REQUIRE_MERGE_STRATEGY_KEEP_SOURCE);
    }
  else
    {
      entry = g_slice_new0 (IdeGiRequireEntry);
      entry->min = bound;
      entry->is_range = FALSE;

      g_hash_table_insert (self->entries,
                           g_strdup (ns),
                           entry);
    }
}

IdeGiRequire *
ide_gi_require_new (void)
{
  IdeGiRequire *self;

  self = g_slice_new0 (IdeGiRequire);
  self->ref_count = 1;

  self->entries = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, require_entry_free);

  return self;
}

static void
ide_gi_require_free (IdeGiRequire *self)
{
  g_assert (self);
  g_assert_cmpint (self->ref_count, ==, 0);

  dzl_clear_pointer (&self->entries, g_hash_table_unref);
  g_slice_free (IdeGiRequire, self);
}

IdeGiRequire *
ide_gi_require_ref (IdeGiRequire *self)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

void
ide_gi_require_unref (IdeGiRequire *self)
{
  g_return_if_fail (self);
  g_return_if_fail (self->ref_count);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    ide_gi_require_free (self);
}
