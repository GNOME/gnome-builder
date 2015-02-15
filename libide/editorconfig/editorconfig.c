/*
 * Authors: Christian Hergert <christian@hergert.me>
 *
 * The author or authors of this code dedicate any and all copyright interest
 * in this code to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and successors. We
 * intend this dedication to be an overt act of relinquishment in perpetuity of
 * all present and future rights to this code under copyright law.
 */

#include <fnmatch.h>
#include <glib/gi18n.h>
#include <string.h>

#include "editorconfig.h"

static gboolean
glob_match (const gchar  *pattern,
            const gchar  *string,
            GError      **error)
{
  int flags;
  int ret;

  if (g_str_equal (pattern, "__global__"))
    return TRUE;

  flags = FNM_PATHNAME | FNM_PERIOD | FNM_CASEFOLD;

  ret = fnmatch (pattern, string, flags);

  switch (ret)
    {
    case 0:
      return TRUE;

    case FNM_NOMATCH:
      return FALSE;

    default:
      return FALSE;
    }
}

static void
vfree (gpointer data)
{
  GValue *value = data;

  if (value)
    {
      g_value_unset (value);
      g_free (value);
    }
}

static gboolean
parse_key (GKeyFile     *key_file,
           const gchar  *group,
           const gchar  *key,
           GHashTable   *hashtable,
           GError      **error)
{
  gchar *lower;
  GValue *value;
  gboolean ret = FALSE;

  g_assert (key_file);
  g_assert (group);
  g_assert (key);
  g_assert (hashtable);

  lower = g_utf8_strdown (key, -1);
  value = g_new0 (GValue, 1);

  if (g_str_equal (key, "root"))
    {
      if (g_key_file_get_boolean (key_file, group, key, NULL))
        g_hash_table_remove_all (hashtable);
      ret = TRUE;
      goto cleanup;
    }
  else if (g_str_equal (key, "tab_width"))
    {
      GError *local_error = NULL;
      gint v;

      v = g_key_file_get_integer (key_file, group, key, &local_error);

      if (local_error)
        {
          g_propagate_error (error, local_error);
          goto cleanup;
        }

      g_value_init (value, G_TYPE_UINT);
      g_value_set_uint (value, MAX (1, v));
    }
  else if (g_str_equal (key, "indent_size"))
    {
      GError *local_error = NULL;
      gint v;

      v = g_key_file_get_integer (key_file, group, key, &local_error);

      if (local_error)
        {
          g_propagate_error (error, local_error);
          goto cleanup;
        }

      g_value_init (value, G_TYPE_INT);
      g_value_set_int (value, MAX (-1, v));
    }
  else if (g_str_equal (key, "trim_trailing_whitespace") || g_str_equal (key, "insert_final_newline"))
    {
      GError *local_error = NULL;
      gboolean v;

      v = g_key_file_get_boolean (key_file, group, key, &local_error);

      if (local_error)
        {
          g_propagate_error (error, local_error);
          goto cleanup;
        }

      g_value_init (value, G_TYPE_BOOLEAN);
      g_value_set_boolean (value, v);
    }
  else
    {
      gchar *str;

      str = g_key_file_get_string (key_file, group, key, NULL);
      g_value_init (value, G_TYPE_STRING);
      g_value_take_string (value, str);
    }

  g_assert (G_VALUE_TYPE (value) != G_TYPE_NONE);
  g_hash_table_replace (hashtable, g_strdup (key), value);
  value = NULL;

  ret = TRUE;

cleanup:
  g_free (value);
  g_free (lower);

  return ret;
}

static gboolean
parse_group (GKeyFile     *key_file,
             const gchar  *group,
             GHashTable   *hashtable,
             const gchar  *relpath,
             GError      **error)
{
  gchar **keys = NULL;
  gsize i;
  gboolean ret = FALSE;

  g_assert (key_file);
  g_assert (hashtable);
  g_assert (relpath);

  if (!(keys = g_key_file_get_keys (key_file, group, NULL, error)))
    goto cleanup;

  for (i = 0; keys [i]; i++)
    {
      if (!parse_key (key_file, group, keys [i], hashtable, error))
        goto cleanup;
    }

  ret = TRUE;

cleanup:
  g_clear_pointer (&keys, g_strfreev);

  return ret;
}

static gboolean
parse_file (GFile         *doteditorconfig,
            GCancellable  *cancellable,
            GHashTable    *hashtable,
            GFile         *target,
            GError       **error)
{
  GKeyFile *key_file = NULL;
  GString *mutated = NULL;
  gchar *contents = NULL;
  GFile *parent = NULL;
  gchar *relpath = NULL;
  gchar **groups = NULL;
  gsize len;
  gsize i;
  gboolean ret = FALSE;

  g_assert (G_IS_FILE (doteditorconfig));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (hashtable);
  g_assert (G_IS_FILE (target));

  parent = g_file_get_parent (doteditorconfig);
  g_assert (G_IS_FILE (parent));

  relpath = g_file_get_relative_path (parent, target);
  g_assert (relpath);

  if (!g_file_load_contents (doteditorconfig, cancellable, &contents, &len, NULL, error))
    goto cleanup;

  if (!g_utf8_validate (contents, len, NULL))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   _(".editorconfig did not contain valid UTF-8"));
      goto cleanup;
    }

  /* .editorconfig can have settings before a keyfile group */
  mutated = g_string_new ("[__global__]\n");
  g_string_append (mutated, contents);

  key_file = g_key_file_new ();

  if (!g_key_file_load_from_data (key_file, mutated->str, mutated->len, 0, error))
    goto cleanup;

  groups = g_key_file_get_groups (key_file, NULL);

  for (i = 0; groups [i]; i++)
    {
      gboolean matches;
      GError *local_error = NULL;

      matches = glob_match (groups [i], relpath, &local_error);

      if (local_error)
        {
          g_propagate_error (error, local_error);
          goto cleanup;
        }

      if (matches)
        {
          if (!parse_group (key_file, groups [i], hashtable, relpath, error))
            goto cleanup;
        }
    }

  ret = TRUE;

cleanup:
  if (mutated)
    g_string_free (mutated, TRUE);
  g_clear_pointer (&key_file, g_key_file_free);
  g_clear_pointer (&relpath, g_free);
  g_clear_pointer (&contents, g_free);
  g_clear_pointer (&groups, g_strfreev);
  g_clear_object (&parent);

  return ret;
}

/**
 * editorconfig_read:
 * @file: A #GFile containing the file to apply the settings for.
 * @cancellable: (allow-none): A #GCancellable or %NULL.
 * @error: (out) (allow-none): A location for a #GError, or %NULL.
 *
 * This function will read the .editorconfig rules that match @file starting
 * from it's parent directory and working it's way up to the root of the of
 * the project tree.
 *
 * Returns: (transfer container) (element-type gchar* GValue*): A #GHashTable
 *   containing the key/value pairs that should be applied to @file.
 */
GHashTable *
editorconfig_read (GFile         *file,
                   GCancellable  *cancellable,
                   GError       **error)
{
  GHashTable *hashtable;
  GQueue *queue;
  GFile *iter;

  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), NULL);
  g_return_val_if_fail (!error || !*error, NULL);

  /*
   * The following is a simple algorithm for applying the editorconfig files
   * in reverse-order so that the closer to the target file .editorconfig is,
   * the higher it's precedence.
   *
   * We work our way down starting from the sibling .editorconfig for the file.
   * If the file exists, we push it onto the queue's head.
   *
   * Once we know about all of the potential files working our way to the root
   * of the filesystem, we can pop items off the queues head and apply them
   * to the hashtable in order.
   *
   * The result is a hashtable containing string keys and GValue* values.
   *
   * Note that if we discover a "root = true" key along the way, we can simply
   * clear the hashtable to get the same affect as if we were to read each
   * file as we dove down. The reason we don't do that is that we then have to
   * hold on to all of the files in memory at once, instead of potentially
   * doing a little extra I/O starting from the root. I think the tradeoff
   * results in a bit cleaner code, so I'm going with that.
   */

  queue = g_queue_new ();
  iter = g_object_ref (file);

  do
    {
      GFile *parent;
      GFile *doteditorconfig;

      parent = g_file_get_parent (iter);
      doteditorconfig = g_file_get_child (parent, ".editorconfig");

      if (g_file_query_exists (doteditorconfig, cancellable))
        g_queue_push_head (queue, g_object_ref (doteditorconfig));

      g_clear_object (&doteditorconfig);
      g_clear_object (&iter);

      iter = parent;
    }
  while (g_file_has_parent (iter, NULL));

  g_clear_object (&iter);

  if (!queue->length)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_FOUND,
                   _("No .editorconfig files could be found."));
      goto cleanup;
    }

  hashtable = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, vfree);

  while ((iter = g_queue_pop_head (queue)))
    {
      if (!parse_file (iter, cancellable, hashtable, file, error))
        {
          g_object_unref (iter);
          goto cleanup;
        }

      g_object_unref (iter);
    }

cleanup:
  while ((iter = g_queue_pop_head (queue)))
    g_object_unref (iter);
  g_queue_free (queue);

  return hashtable;
}
