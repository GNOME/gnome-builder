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

#include <glib/gstdio.h>
#include <errno.h>

#include <libide-threading.h>

#include "ide-gfile.h"
#include "ide-gfile-private.h"

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
 * Adds a pattern that can be used to match ignored files. These are global
 * to the application, so they should only include well-known ignored files
 * such as those internal to a build system, or version control system, and
 * similar.
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
  if (!g_utf8_validate(name, len, NULL))
    return TRUE;

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

      if (g_pattern_spec_match (pattern_spec, len, name, reversed))
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
  if (!g_utf8_validate(name, len, NULL))
    return TRUE;

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

      if (g_pattern_spec_match (pattern_spec, len, name, reversed))
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
                           g_ptr_array_unref);
}

static void
get_children_free (gpointer data)
{
  GetChildren *gc = data;

  g_free (gc->attributes);
  g_slice_free (GetChildren, gc);
}

#ifdef DEVELOPMENT_BUILD
static gboolean
delayed_run (gpointer data)
{
  g_autoptr(IdeTask) subtask = data;
  ide_task_run_in_thread (subtask, ide_g_file_get_children_worker);
  return G_SOURCE_REMOVE;
}
#endif

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
  GPtrArray *specs; // Array of GPatternSpec
  guint      depth;
} Find;

static void
find_free (Find *f)
{
  g_clear_pointer (&f->specs, g_ptr_array_unref);
  g_slice_free (Find, f);
}

static void
populate_descendants_matching (GFile        *file,
                               GCancellable *cancellable,
                               GPtrArray    *results,
                               GPtrArray    *specs,
                               guint         depth)
{
  g_autoptr(GFileEnumerator) enumerator = NULL;
  g_autoptr(GPtrArray) children = NULL;

  g_assert (G_IS_FILE (file));
  g_assert (results != NULL);
  g_assert (specs != NULL);

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

      for (int i = 0; i < specs->len; i++) {
        if (g_pattern_spec_match_string (g_ptr_array_index (specs, i), name)) {
          g_ptr_array_add (results, g_file_enumerator_get_child (enumerator, info));
          break;
        }
      }

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
            populate_descendants_matching (child, cancellable, results, specs, depth - 1);
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
  g_assert (f->specs != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  ret = g_ptr_array_new_with_free_func (g_object_unref);
  populate_descendants_matching (file, cancellable, ret, f->specs, f->depth);
  ide_task_return_pointer (task, g_steal_pointer (&ret), g_ptr_array_unref);
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
 */
GPtrArray *
ide_g_file_find_with_depth (GFile        *file,
                            const gchar  *pattern,
                            guint         max_depth,
                            GCancellable *cancellable)
{
  GPatternSpec *spec;
  GPtrArray *ret, *specs;

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
  specs = g_ptr_array_new_full (1, (GDestroyNotify) g_pattern_spec_free);
  g_ptr_array_add (specs, spec);
  populate_descendants_matching (file, cancellable, ret, specs, max_depth);
  return IDE_PTR_ARRAY_STEAL_FULL (&ret);
}
/**
 * ide_g_file_find_multiple_with_depth_async:
 * @file: a #IdeGlib
 * @patterns: a NULL-terminated array of glob pattern to search for using GPatternSpec
 * @max_depth: maximum tree depth to search
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: a callback to execute upon completion
 * @user_data: closure data for @callback
 *
 * Searches descendants of @file for files matching any @patterns.
 *
 * Only up to @max_depth subdirectories will be searched. However, if
 * @max_depth is zero, then all directories will be searched.
 *
 * You may only match on the filename, not the directory.
 */
void
ide_g_file_find_multiple_with_depth_async (GFile               *file,
                                           const gchar *const  *patterns,
                                           guint                depth,
                                           GCancellable        *cancellable,
                                           GAsyncReadyCallback  callback,
                                           gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  Find *f;
  guint patterns_len;

  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (patterns != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (depth == 0)
    depth = G_MAXUINT;

  task = ide_task_new (file, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_g_file_find_async);
  ide_task_set_priority (task, G_PRIORITY_LOW + 100);

  patterns_len = g_strv_length ((gchar **)patterns);
  f = g_slice_new0 (Find);
  f->specs = g_ptr_array_new_full (patterns_len, (GDestroyNotify) g_pattern_spec_free);
  f->depth = depth;
  for (guint i = 0; i < patterns_len; i++) {
    GPatternSpec *spec = g_pattern_spec_new (patterns[i]);
    if (spec == NULL)
      {
        ide_task_return_new_error (task,
                                   G_IO_ERROR,
                                   G_IO_ERROR_INVAL,
                                   "Invalid pattern spec: %s",
                                   patterns[i]);
        return;
      }

    g_ptr_array_add (f->specs, g_steal_pointer (&spec));
  }

  ide_task_set_task_data (task, f, find_free);
  ide_task_run_in_thread (task, ide_g_file_find_worker);
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
 */
void
ide_g_file_find_with_depth_async (GFile               *file,
                                  const gchar         *pattern,
                                  guint                depth,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  const gchar *patterns[2] = { pattern, NULL };

  g_return_if_fail (pattern != NULL);

  ide_g_file_find_multiple_with_depth_async (file, patterns, depth, cancellable, callback, user_data);
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
 */
gboolean
ide_g_host_file_get_contents (const gchar  *path,
                              gchar       **contents,
                              gsize        *len,
                              GError      **error)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autofree gchar *tmpfile = NULL;
  gboolean ret = FALSE;
  int fd;

  g_return_val_if_fail (path != NULL, FALSE);

  if (contents != NULL)
    *contents = NULL;

  if (len != NULL)
    *len = 0;

  if (!ide_is_flatpak ())
    return g_file_get_contents (path, contents, len, error);

  tmpfile = g_build_filename (g_get_tmp_dir (), ".ide-host-file-XXXXXX", NULL);

  /* We open a FD locally that we can write to and then pass that as our
   * stdout across the boundary so we can avoid incrementally reading
   * and instead do it once at the end.
   */
  if (-1 == (fd = g_mkstemp (tmpfile)))
    {
      int errsv = errno;
      g_set_error_literal (error,
                           G_FILE_ERROR,
                           g_file_error_from_errno (errsv),
                           g_strerror (errsv));
      return FALSE;
    }

  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                          G_SUBPROCESS_FLAGS_STDERR_SILENCE);
  ide_subprocess_launcher_take_stdout_fd (launcher, fd);
  ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
  ide_subprocess_launcher_push_argv (launcher, "cat");
  ide_subprocess_launcher_push_argv (launcher, path);

  if (!(subprocess = ide_subprocess_launcher_spawn (launcher, NULL, error)))
    goto failure;

  if (!ide_subprocess_wait_check (subprocess, NULL, error))
    goto failure;

  ret = g_file_get_contents (tmpfile, contents, len, error);

failure:
  g_unlink (tmpfile);

  return ret;
}

/**
 * ide_g_file_walk_with_ignore:
 * @directory: a #GFile that is a directory
 * @attributes: attributes to include in #GFileInfo
 * @ignore_file: (nullable): the filename within @directory to indicate that
 *   the directory should be ignored
 * @cancellable: (nullable): an optional cancellable
 * @callback: (scope call): a callback for each directory starting from
 *   the @directory
 * @callback_data: closure data for @callback
 *
 * Calls @callback for every directory starting from @directory.
 *
 * All of the fileinfo for the directory will be provided to the callback for
 * each directory.
 *
 * If @ignore_file is set, this function will check to see if that file exists
 * within @directory and skip it (and all descendants) if discovered.
 */
void
ide_g_file_walk_with_ignore (GFile               *directory,
                             const gchar         *attributes,
                             const gchar         *ignore_file,
                             GCancellable        *cancellable,
                             IdeFileWalkCallback  callback,
                             gpointer             callback_data)
{
  g_autoptr(GFileEnumerator) enumerator = NULL;
  g_autoptr(GPtrArray) directories = NULL;
  g_autoptr(GPtrArray) file_infos = NULL;
  g_autoptr(GString) str = NULL;
  g_autoptr(GError) error = NULL;
  GFileType directory_type;
  gpointer infoptr;
  static const gchar *required[] = {
    G_FILE_ATTRIBUTE_STANDARD_NAME,
    G_FILE_ATTRIBUTE_STANDARD_TYPE,
    NULL
  };

  g_return_if_fail (G_IS_FILE (directory));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (g_cancellable_is_cancelled (cancellable))
    return;

  directory_type = g_file_query_file_type (directory,
                                           G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                           cancellable);

  if (directory_type != G_FILE_TYPE_DIRECTORY)
    return;

  if (ignore_file != NULL)
    {
      g_autoptr(GFile) ignore = g_file_get_child (directory, ignore_file);

      if (g_file_query_exists (ignore, cancellable))
        return;
    }

  str = g_string_new (attributes);

  for (guint i = 0; required[i]; i++)
    {
      if (!strstr (str->str, required[i]))
        g_string_append_printf (str, ",%s", required[i]);
    }

  directories = g_ptr_array_new_with_free_func (g_object_unref);
  file_infos = g_ptr_array_new_with_free_func (g_object_unref);

  enumerator = g_file_enumerate_children (directory,
                                          str->str,
                                          G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                          cancellable,
                                          NULL);

  if (enumerator == NULL)
    return;

  while ((infoptr = g_file_enumerator_next_file (enumerator, cancellable, &error)))
    {
      g_autoptr(GFileInfo) info = infoptr;
      g_autoptr(GFile) child = g_file_enumerator_get_child (enumerator, info);

      if (ide_g_file_is_ignored (child))
        continue;

      if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY)
        g_ptr_array_add (directories, g_steal_pointer (&child));

      g_ptr_array_add (file_infos, g_steal_pointer (&info));
    }

  callback (directory, file_infos, callback_data);

  for (guint i = 0; i < directories->len; i++)
    {
      GFile *child = g_ptr_array_index (directories, i);

      if (g_cancellable_is_cancelled (cancellable))
        break;

      ide_g_file_walk_with_ignore (child,
                                   attributes,
                                   ignore_file,
                                   cancellable,
                                   callback,
                                   callback_data);
    }
}

/**
 * ide_g_file_walk:
 * @directory: a #GFile that is a directory
 * @attributes: attributes to include in #GFileInfo
 * @cancellable: (nullable): an optional cancellable
 * @callback: (scope call): a callback for each directory starting from @directory
 * @callback_data: closure data for @callback
 *
 * Calls @callback for every directory starting from @directory.
 *
 * All of the fileinfo for the directory will be provided to the callback for
 * each directory.
 */
void
ide_g_file_walk (GFile               *directory,
                 const gchar         *attributes,
                 GCancellable        *cancellable,
                 IdeFileWalkCallback  callback,
                 gpointer             callback_data)
{
  ide_g_file_walk_with_ignore (directory, attributes, NULL, cancellable, callback, callback_data);
}

static gboolean
iter_parents (GFile **fileptr)
{
  g_autoptr(GFile) item = *fileptr;
  *fileptr = g_file_get_parent (item);
  return *fileptr != NULL;
}

static gboolean
is_symlink (GFile  *file,
            gchar **target)
{
  g_autoptr(GFileInfo) info = NULL;

  g_assert (G_IS_FILE (file));
  g_assert (target != NULL);

  *target = NULL;

  if (!g_file_is_native (file))
    return FALSE;

  info = g_file_query_info (file,
                            G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK","
                            G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET,
                            G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                            NULL, NULL);

  if (info == NULL)
    return FALSE;

  if (g_file_info_get_is_symlink (info))
    {
      *target = g_strdup (g_file_info_get_symlink_target (info));
      return TRUE;
    }

  return FALSE;
}

GFile *
_ide_g_file_readlink (GFile *file)
{
  g_autoptr(GFile) iter = NULL;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  if (!g_file_is_native (file))
    return g_object_ref (file);

  iter = g_file_dup (file);

  do
    {
      g_autofree char *target = NULL;

      if (is_symlink (iter, &target))
        {
          g_autofree char *relative = g_file_get_relative_path (iter, file);

          if (!g_path_is_absolute (target))
            {
              g_autoptr(GFile) parent = g_file_get_parent (iter);
              g_autoptr(GFile) base = g_file_get_child (parent, target);

              if (relative == NULL)
                return g_steal_pointer (&base);
              else
                return g_file_get_child (base, relative);
            }

          return g_file_new_build_filename (target, relative, NULL);
        }
    }
  while (iter_parents (&iter));

  return g_object_ref (file);
}

static void
find_in_ancestors_worker (IdeTask      *task,
                          gpointer      source_object,
                          gpointer      task_data,
                          GCancellable *cancellable)
{
  const gchar *name = task_data;
  GFile *directory = (GFile *)source_object;
  GFile *current = NULL;

  g_assert (IDE_IS_TASK (task));
  g_assert (G_IS_FILE (directory));
  g_assert (name != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  current = g_object_ref (directory);

  while (current != NULL)
    {
      g_autoptr(GFile) target = g_file_get_child (current, name);
      g_autoptr(GFile) tmp = NULL;

      if (g_file_query_exists (target, cancellable))
        {
          ide_task_return_pointer (task, g_steal_pointer (&target), g_object_unref);
          goto cleanup;
        }

      tmp = g_steal_pointer (&current);
      current = g_file_get_parent (tmp);
    }

  ide_task_return_new_error (task,
                             G_IO_ERROR,
                             G_IO_ERROR_NOT_FOUND,
                             "Failed to locate file \"%s\" in ancestry",
                             name);

cleanup:
  g_clear_object (&current);
}

void
ide_g_file_find_in_ancestors_async (GFile               *directory,
                                    const gchar         *name,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;

  g_return_if_fail (G_IS_FILE (directory));
  g_return_if_fail (name != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (directory, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_g_file_find_in_ancestors_async);
  ide_task_set_task_data (task, g_strdup (name), g_free);
  ide_task_run_in_thread (task, find_in_ancestors_worker);
}

/**
 * ide_g_file_find_in_ancestors_finish:
 *
 * Returns: (transfer full): a #GFile if successful; otherwise %NULL
 *   and @error is et.
 */
GFile *
ide_g_file_find_in_ancestors_finish (GAsyncResult  *result,
                                     GError       **error)
{
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

gboolean
_ide_g_file_query_exists_on_host (GFile        *file,
                                  GCancellable *cancellable)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;

  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  if (!g_file_is_native (file))
    return FALSE;

  if (!ide_is_flatpak ())
    return g_file_query_exists (file, cancellable);

  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_SILENCE | G_SUBPROCESS_FLAGS_STDERR_SILENCE);
  ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
  ide_subprocess_launcher_push_argv (launcher, "ls");
  ide_subprocess_launcher_push_argv (launcher, "-d");
  ide_subprocess_launcher_push_argv (launcher, g_file_peek_path (file));

  if (!(subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, NULL)))
    return FALSE;

  return ide_subprocess_wait_check (subprocess, cancellable, NULL);
}

gboolean
_ide_path_query_exists_on_host (const char *path)
{
  g_autofree char *locally = NULL;
  g_autoptr(GFile) file = NULL;

  g_return_val_if_fail (path != NULL, FALSE);

  if (!ide_is_flatpak ())
    return g_file_test (path, G_FILE_TEST_EXISTS);

  /* First try via /var/run/host */
  locally = g_build_filename ("/var/run/host", path, NULL);
  if (g_file_test (locally, G_FILE_TEST_EXISTS))
    return TRUE;

  /* Fallback to using GFile functionality */
  file = g_file_new_for_path (path);
  return _ide_g_file_query_exists_on_host (file, NULL);
}
