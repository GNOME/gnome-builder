/* ide-gfile.c
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-gfile"

#include "config.h"

#include <libide-threading.h>

#include "ide-gfile.h"

static GPtrArray *g_ignored;
G_LOCK_DEFINE_STATIC (ignored);

static GPtrArray *
get_ignored_locked (void)
{
  static const gchar *ignored_patterns[] = {
    /* Ignore Gio temporary files */
    ".goutputstream-*",
    /* Ignore minified JS */
    "*.min.js",
    "*.min.js.*",
  };

  if (g_ignored == NULL)
    {
      g_ignored = g_ptr_array_new ();
      for (guint i = 0; i < G_N_ELEMENTS (ignored_patterns); i++)
        g_ptr_array_add (g_ignored, g_pattern_spec_new (ignored_patterns[i]));
    }

  return g_ignored;
}

/**
 * ide_g_file_add_ignored_pattern:
 * @pattern: a #GPatternSpec style glob pattern
 *
 * Adds a pattern that can be used to match ingored files. These are global
 * to the application, so they should only include well-known ignored files
 * such as those internal to a build system, or version control system, and
 * similar.
 *
 * Since: 3.32
 */
void
ide_g_file_add_ignored_pattern (const gchar *pattern)
{
  G_LOCK (ignored);
  g_ptr_array_add (get_ignored_locked (), g_pattern_spec_new (pattern));
  G_UNLOCK (ignored);
}

/**
 * ide_path_is_ignored:
 * @path: the path to the file
 *
 * Checks if @path should be ignored using the global file
 * ignores registered with Builder.
 *
 * Returns: %TRUE if @path should be ignored, otherwise %FALSE
 *
 * Since: 3.32
 */
gboolean
ide_path_is_ignored (const gchar *path)
{
  g_autofree gchar *name = NULL;
  g_autofree gchar *reversed = NULL;
  GPtrArray *ignored;
  gsize len;
  gboolean ret = FALSE;

  name = g_path_get_basename (path);
  len = strlen (name);
  reversed = g_utf8_strreverse (name, len);

  /* Ignore empty files for whatever reason */
  if (ide_str_empty0 (name))
    return TRUE;

  /* Ignore builtin backup files by GIO */
  if (name[len - 1] == '~')
    return TRUE;

  G_LOCK (ignored);

  ignored = get_ignored_locked ();

  for (guint i = 0; i < ignored->len; i++)
    {
      GPatternSpec *pattern_spec = g_ptr_array_index (ignored, i);

      if (g_pattern_match (pattern_spec, len, name, reversed))
        {
          ret = TRUE;
          break;
        }
    }

  G_UNLOCK (ignored);

  return ret;
}

/**
 * ide_g_file_is_ignored:
 * @file: a #GFile
 *
 * Checks if @file should be ignored using the internal ignore rules.  If you
 * care about the version control system, see #IdeVcs and ide_vcs_is_ignored().
 *
 * Returns: %TRUE if @file should be ignored; otherwise %FALSE.
 *
 * Since: 3.32
 */
gboolean
ide_g_file_is_ignored (GFile *file)
{
  g_autofree gchar *name = NULL;
  g_autofree gchar *reversed = NULL;
  GPtrArray *ignored;
  gsize len;
  gboolean ret = FALSE;

  name = g_file_get_basename (file);
  len = strlen (name);
  reversed = g_utf8_strreverse (name, len);

  /* Ignore empty files for whatever reason */
  if (ide_str_empty0 (name))
    return TRUE;

  /* Ignore builtin backup files by GIO */
  if (name[len - 1] == '~')
    return TRUE;

  G_LOCK (ignored);

  ignored = get_ignored_locked ();

  for (guint i = 0; i < ignored->len; i++)
    {
      GPatternSpec *pattern_spec = g_ptr_array_index (ignored, i);

      if (g_pattern_match (pattern_spec, len, name, reversed))
        {
          ret = TRUE;
          break;
        }
    }

  G_UNLOCK (ignored);

  return ret;
}

/**
 * ide_g_file_get_uncanonical_relative_path:
 * @file: a #GFile
 * @other: a #GFile with a common ancestor to @file
 *
 * This function is similar to g_file_get_relative_path() except that
 * @file and @other only need to have a shared common ancestor.
 *
 * This is useful if you must use a relative path instead of the absolute,
 * canonical path.
 *
 * This is being implemented for use when communicating to GDB. When that
 * becomes unnecessary, this should no longer be used.
 *
 * Returns: (nullable): A relative path, or %NULL if no common ancestor was
 *   found for the relative path.
 *
 * Since: 3.32
 */
gchar *
ide_g_file_get_uncanonical_relative_path (GFile *file,
                                          GFile *other)
{
  g_autoptr(GFile) ancestor = NULL;
  g_autoptr(GString) relatives = NULL;
  g_autofree gchar *path = NULL;
  g_autofree gchar *suffix = NULL;

  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (G_IS_FILE (other), NULL);

  /* Nothing for matching files */
  if (file == other || g_file_equal (file, other))
    return NULL;

  /* Make sure we're working with files of the same type */
  if (G_OBJECT_TYPE (file) != G_OBJECT_TYPE (other))
    return NULL;

  /* Already descendant, just give the actual path */
  if (g_file_has_prefix (other, file))
    return g_file_get_path (other);

  relatives = g_string_new ("/");

  /* Find the common ancestor */
  ancestor = g_object_ref (file);
  while (ancestor != NULL &&
         !g_file_has_prefix (other, ancestor) &&
         !g_file_equal (other, ancestor))
    {
      g_autoptr(GFile) parent = g_file_get_parent (ancestor);

      /* We reached the root, nothing more to do */
      if (g_file_equal (parent, ancestor))
        return NULL;

      g_string_append_len (relatives, "../", strlen ("../"));

      g_clear_object (&ancestor);
      ancestor = g_steal_pointer (&parent);
    }

  g_assert (G_IS_FILE (ancestor));
  g_assert (g_file_has_prefix (other, ancestor));
  g_assert (g_file_has_prefix (file, ancestor));

  path = g_file_get_path (file);
  suffix = g_file_get_relative_path (ancestor, other);

  if (path == NULL)
    path = g_strdup ("/");

  if (suffix == NULL)
    suffix = g_strdup ("/");

  return g_build_filename (path, relatives->str, suffix, NULL);
}

typedef struct
{
  gchar *attributes;
  GFileQueryInfoFlags flags;
} GetChildren;

static void
ide_g_file_get_children_worker (IdeTask      *task,
                                gpointer      source_object,
                                gpointer      task_data,
                                GCancellable *cancellable)
{
  g_autoptr(GFileEnumerator) enumerator = NULL;
  g_autoptr(GPtrArray) children = NULL;
  g_autoptr(GError) error = NULL;
  GetChildren *gc = task_data;
  GFile *dir = source_object;

  g_assert (IDE_IS_TASK (task));
  g_assert (G_IS_FILE (dir));
  g_assert (gc != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  children = g_ptr_array_new_with_free_func (g_object_unref);

  enumerator = g_file_enumerate_children (dir,
                                          gc->attributes,
                                          gc->flags,
                                          cancellable,
                                          &error);

  if (enumerator == NULL)
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  for (;;)
    {
      g_autoptr(GFileInfo) file_info = NULL;

      file_info = g_file_enumerator_next_file (enumerator, cancellable, &error);

      if (error != NULL)
        {
          ide_task_return_error (task, g_steal_pointer (&error));
          return;
        }

      if (file_info == NULL)
        break;

      g_ptr_array_add (children, g_steal_pointer (&file_info));
    }

  g_file_enumerator_close (enumerator, NULL, NULL);

  ide_task_return_pointer (task,
                           g_steal_pointer (&children),
                           (GDestroyNotify) g_ptr_array_unref);
}

static void
get_children_free (gpointer data)
{
  GetChildren *gc = data;

  g_free (gc->attributes);
  g_slice_free (GetChildren, gc);
}

/**
 * ide_g_file_get_children_async:
 * @file: a #IdeGlib
 * @attributes: attributes to retrieve
 * @flags: flags for the query
 * @io_priority: the io priority
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: a callback to execute upon completion
 * @user_data: closure data for @callback
 *
 * This function is like g_file_enumerate_children_async() except that
 * it returns a #GPtrArray of #GFileInfo instead of an enumerator.
 *
 * This can be convenient when you know you need all of the #GFileInfo
 * accessable at once, or the size will be small.
 *
 * Since: 3.32
 */
void
ide_g_file_get_children_async (GFile               *file,
                               const gchar         *attributes,
                               GFileQueryInfoFlags  flags,
                               gint                 io_priority,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  GetChildren *gc;

  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (attributes != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  gc = g_slice_new0 (GetChildren);
  gc->attributes = g_strdup (attributes);
  gc->flags = flags;

  task = ide_task_new (file, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_g_file_get_children_async);
  ide_task_set_priority (task, io_priority);
  ide_task_set_task_data (task, gc, get_children_free);

#ifdef DEVELOPMENT_BUILD
  /* Useful for testing slow interactions on project-tree and such */
  if (g_getenv ("IDE_G_FILE_DELAY"))
    {
      gboolean
      delayed_run (gpointer data)
      {
        g_autoptr(IdeTask) subtask = data;
        ide_task_run_in_thread (subtask, ide_g_file_get_children_worker);
        return G_SOURCE_REMOVE;
      }
      g_timeout_add_seconds (1, delayed_run, g_object_ref (task));
      return;
    }
#endif

  ide_task_run_in_thread (task, ide_g_file_get_children_worker);
}

/**
 * ide_g_file_get_children_finish:
 * @file: a #GFile
 * @result: a #GAsyncResult provided to callback
 * @error: a location for a #GError, or %NULL
 *
 * Completes an asynchronous request to ide_g_file_get_children_async().
 *
 * Returns: (transfer full) (element-type Gio.FileInfo): A #GPtrArray
 *   of #GFileInfo if successful, otherwise %NULL.
 *
 * Since: 3.32
 */
GPtrArray *
ide_g_file_get_children_finish (GFile         *file,
                                GAsyncResult  *result,
                                GError       **error)
{
  GPtrArray *ret;

  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);
  g_return_val_if_fail (ide_task_is_valid (IDE_TASK (result), file), NULL);

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  return IDE_PTR_ARRAY_STEAL_FULL (&ret);
}

typedef struct
{
  GPatternSpec *spec;
  guint         depth;
} Find;

static void
find_free (Find *f)
{
  g_clear_pointer (&f->spec, g_pattern_spec_free);
  g_slice_free (Find, f);
}

static void
populate_descendants_matching (GFile        *file,
                               GCancellable *cancellable,
                               GPtrArray    *results,
                               GPatternSpec *spec,
                               guint         depth)
{
  g_autoptr(GFileEnumerator) enumerator = NULL;
  g_autoptr(GPtrArray) children = NULL;

  g_assert (G_IS_FILE (file));
  g_assert (results != NULL);
  g_assert (spec != NULL);

  if (depth == 0)
    return;

  enumerator = g_file_enumerate_children (file,
                                          G_FILE_ATTRIBUTE_STANDARD_NAME","
                                          G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK","
                                          G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                          G_FILE_QUERY_INFO_NONE,
                                          cancellable,
                                          NULL);

  if (enumerator == NULL)
    return;

  for (;;)
    {
      g_autoptr(GFileInfo) info = g_file_enumerator_next_file (enumerator, cancellable, NULL);
      const gchar *name;
      GFileType file_type;

      if (info == NULL)
        break;

      name = g_file_info_get_name (info);
      file_type = g_file_info_get_file_type (info);

      if (g_pattern_match_string (spec, name))
        g_ptr_array_add (results, g_file_enumerator_get_child (enumerator, info));

      if (!g_file_info_get_is_symlink (info) && file_type == G_FILE_TYPE_DIRECTORY)
        {
          if (children == NULL)
            children = g_ptr_array_new_with_free_func (g_object_unref);
          g_ptr_array_add (children, g_file_enumerator_get_child (enumerator, info));
        }
    }

  g_file_enumerator_close (enumerator, cancellable, NULL);

  if (children != NULL)
    {
      for (guint i = 0; i < children->len; i++)
        {
          GFile *child = g_ptr_array_index (children, i);

          /* Don't recurse into known bad directories */
          if (!ide_g_file_is_ignored (child))
            populate_descendants_matching (child, cancellable, results, spec, depth - 1);
        }
    }
}

static void
ide_g_file_find_worker (IdeTask      *task,
                        gpointer      source_object,
                        gpointer      task_data,
                        GCancellable *cancellable)
{
  GFile *file = source_object;
  Find *f = task_data;
  g_autoptr(GPtrArray) ret = NULL;

  g_assert (IDE_IS_TASK (task));
  g_assert (G_IS_FILE (file));
  g_assert (f != NULL);
  g_assert (f->spec != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  ret = g_ptr_array_new_with_free_func (g_object_unref);
  populate_descendants_matching (file, cancellable, ret, f->spec, f->depth);
  ide_task_return_pointer (task, g_steal_pointer (&ret), (GDestroyNotify)g_ptr_array_unref);
}

/**
 * ide_g_file_find_with_depth:
 * @file: a #GFile
 * @pattern: the glob pattern to search for using GPatternSpec
 * @max_depth: maximum tree depth to search
 * @cancellable: (nullable): a #GCancellable or %NULL
 *
 *
 * Returns: (transfer full) (element-type GFile): a #GPtrArray of #GFile.
 *
 * Since: 3.32
 */
GPtrArray *
ide_g_file_find_with_depth (GFile        *file,
                            const gchar  *pattern,
                            guint         max_depth,
                            GCancellable *cancellable)
{
  g_autoptr(GPatternSpec) spec = NULL;
  GPtrArray *ret;

  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (pattern != NULL, NULL);

  if (!(spec = g_pattern_spec_new (pattern)))
    {
      g_warning ("Failed to build pattern spec for \"%s\"", pattern);
      return NULL;
    }

  if (max_depth == 0)
    max_depth = G_MAXUINT;

  ret = g_ptr_array_new ();
  populate_descendants_matching (file, cancellable, ret, spec, max_depth);
  return IDE_PTR_ARRAY_STEAL_FULL (&ret);
}

/**
 * ide_g_file_find_with_depth_async:
 * @file: a #IdeGlib
 * @pattern: the glob pattern to search for using GPatternSpec
 * @max_depth: maximum tree depth to search
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: a callback to execute upon completion
 * @user_data: closure data for @callback
 *
 * Searches descendants of @file for files matching @pattern.
 *
 * Only up to @max_depth subdirectories will be searched. However, if
 * @max_depth is zero, then all directories will be searched.
 *
 * You may only match on the filename, not the directory.
 *
 * Since: 3.32
 */
void
ide_g_file_find_with_depth_async (GFile               *file,
                                  const gchar         *pattern,
                                  guint                depth,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  Find *f;

  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (pattern != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (depth == 0)
    depth = G_MAXUINT;

  task = ide_task_new (file, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_g_file_find_async);
  ide_task_set_priority (task, G_PRIORITY_LOW + 100);

  f = g_slice_new0 (Find);
  f->spec = g_pattern_spec_new (pattern);
  f->depth = depth;
  ide_task_set_task_data (task, f, find_free);

  if (f->spec == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVAL,
                                 "Invalid pattern spec: %s",
                                 pattern);
      return;
    }

  ide_task_run_in_thread (task, ide_g_file_find_worker);
}

/**
 * ide_g_file_find_async:
 * @file: a #IdeGlib
 * @pattern: the glob pattern to search for using GPatternSpec
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: a callback to execute upon completion
 * @user_data: closure data for @callback
 *
 * Searches descendants of @file for files matching @pattern.
 *
 * You may only match on the filename, not the directory.
 *
 * Since: 3.32
 */
void
ide_g_file_find_async (GFile               *file,
                       const gchar         *pattern,
                       GCancellable        *cancellable,
                       GAsyncReadyCallback  callback,
                       gpointer             user_data)
{
  ide_g_file_find_with_depth_async (file, pattern, G_MAXUINT, cancellable, callback, user_data);
}

/**
 * ide_g_file_find_finish:
 * @file: a #GFile
 * @result: a result provided to callback
 * @error: a location for a #GError or %NULL
 *
 * Gets the files that were found which matched the pattern.
 *
 * Returns: (transfer full) (element-type Gio.File): A #GPtrArray of #GFile
 *
 * Since: 3.32
 */
GPtrArray *
ide_g_file_find_finish (GFile         *file,
                        GAsyncResult  *result,
                        GError       **error)
{
  GPtrArray *ret;

  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  return IDE_PTR_ARRAY_STEAL_FULL (&ret);
}

/**
 * ide_g_host_file_get_contents:
 * @path: the path on the host
 * @contents: (out): a location for the contents
 * @len: (out): a location for the size, not including trailing \0
 * @error: location for a #GError, or %NULL
 *
 * This is similar to g_file_get_contents() but ensures that we get
 * the file from the host, rather than our mount namespace.
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 *
 * Since: 3.32
 */
gboolean
ide_g_host_file_get_contents (const gchar  *path,
                              gchar       **contents,
                              gsize        *len,
                              GError      **error)
{
  g_return_val_if_fail (path != NULL, FALSE);

  if (contents != NULL)
    *contents = NULL;

  if (len != NULL)
    *len = 0;

  if (!ide_is_flatpak ())
    return g_file_get_contents (path, contents, len, error);

  {
    g_autoptr(IdeSubprocessLauncher) launcher = NULL;
    g_autoptr(IdeSubprocess) subprocess = NULL;
    g_autoptr(GBytes) stdout_buf = NULL;

    launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                            G_SUBPROCESS_FLAGS_STDERR_SILENCE);
    ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
    ide_subprocess_launcher_push_argv (launcher, "cat");
    ide_subprocess_launcher_push_argv (launcher, path);

    if (!(subprocess = ide_subprocess_launcher_spawn (launcher, NULL, error)))
      return FALSE;

    if (!ide_subprocess_communicate (subprocess, NULL, NULL, &stdout_buf, NULL, error))
      return FALSE;

    if (len != NULL)
      *len = g_bytes_get_size (stdout_buf);

    if (contents != NULL)
      {
        const guint8 *data;
        gsize n;

        /* g_file_get_contents() gurantees a trailing null byte */
        data = g_bytes_get_data (stdout_buf, &n);
        *contents = g_malloc (n + 1);
        memcpy (*contents, data, n);
        (*contents)[n] = '\0';
      }
  }

  return TRUE;
}
