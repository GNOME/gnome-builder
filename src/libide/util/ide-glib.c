/* ide-glib.c
 *
 * Copyright © 2016 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-glib"

#include <dazzle.h>
#include <string.h>

#include "config.h"

#include "util/ide-flatpak.h"
#include "util/ide-glib.h"
#include "subprocess/ide-subprocess.h"
#include "subprocess/ide-subprocess-launcher.h"
#include "vcs/ide-vcs.h"

typedef struct
{
  GType type;
  GTask *task;
  union {
    gboolean v_bool;
    gint v_int;
    GError *v_error;
    struct {
      gpointer pointer;
      GDestroyNotify destroy;
    } v_ptr;
  } u;
} TaskState;

static gboolean
do_return (gpointer user_data)
{
  TaskState *state = user_data;

  switch (state->type)
    {
    case G_TYPE_INT:
      g_task_return_int (state->task, state->u.v_int);
      break;

    case G_TYPE_BOOLEAN:
      g_task_return_boolean (state->task, state->u.v_bool);
      break;

    case G_TYPE_POINTER:
      g_task_return_pointer (state->task, state->u.v_ptr.pointer, state->u.v_ptr.destroy);
      state->u.v_ptr.pointer = NULL;
      state->u.v_ptr.destroy = NULL;
      break;

    default:
      if (state->type == G_TYPE_ERROR)
        {
          g_task_return_error (state->task, g_steal_pointer (&state->u.v_error));
          break;
        }

      g_assert_not_reached ();
    }

  g_clear_object (&state->task);
  g_slice_free (TaskState, state);

  return G_SOURCE_REMOVE;
}

static void
task_state_attach (TaskState *state)
{
  GMainContext *main_context;
  GSource *source;

  g_assert (state != NULL);
  g_assert (G_IS_TASK (state->task));

  main_context = g_task_get_context (state->task);

  source = g_timeout_source_new (0);
  g_source_set_callback (source, do_return, state, NULL);
  g_source_set_name (source, "[ide] ide_g_task_return_from_main");
  g_source_attach (source, main_context);
  g_source_unref (source);
}

/**
 * ide_g_task_return_boolean_from_main:
 *
 * This is just like g_task_return_boolean() except that it enforces
 * that the current stack return to the main context before dispatching
 * the callback.
 */
void
ide_g_task_return_boolean_from_main (GTask    *task,
                                     gboolean  value)
{
  TaskState *state;

  g_return_if_fail (G_IS_TASK (task));

  state = g_slice_new0 (TaskState);
  state->type = G_TYPE_BOOLEAN;
  state->task = g_object_ref (task);
  state->u.v_bool = !!value;

  task_state_attach (state);
}

void
ide_g_task_return_int_from_main (GTask *task,
                                 gint   value)
{
  TaskState *state;

  g_return_if_fail (G_IS_TASK (task));

  state = g_slice_new0 (TaskState);
  state->type = G_TYPE_INT;
  state->task = g_object_ref (task);
  state->u.v_int = value;

  task_state_attach (state);
}

void
ide_g_task_return_pointer_from_main (GTask          *task,
                                     gpointer        value,
                                     GDestroyNotify  notify)
{
  TaskState *state;

  g_return_if_fail (G_IS_TASK (task));

  state = g_slice_new0 (TaskState);
  state->type = G_TYPE_POINTER;
  state->task = g_object_ref (task);
  state->u.v_ptr.pointer = value;
  state->u.v_ptr.destroy = notify;

  task_state_attach (state);
}

/**
 * ide_g_task_return_error_from_main:
 * @task: a #GTask
 * @error: (transfer full): a #GError.
 *
 * Like g_task_return_error() but ensures we return to the main loop before
 * dispatching the result.
 */
void
ide_g_task_return_error_from_main (GTask  *task,
                                   GError *error)
{
  TaskState *state;

  g_return_if_fail (G_IS_TASK (task));

  state = g_slice_new0 (TaskState);
  state->type = G_TYPE_ERROR;
  state->task = g_object_ref (task);
  state->u.v_error = error;

  task_state_attach (state);
}

const gchar *
ide_gettext (const gchar *message)
{
  if (message != NULL)
    return g_dgettext (GETTEXT_PACKAGE, message);
  return NULL;
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
 * Since: 3.28
 */
gchar *
ide_g_file_get_uncanonical_relative_path (GFile *file,
                                          GFile *other)
{
  g_autoptr(GFile) ancestor = NULL;
  g_autoptr(GString) relatives = NULL;
  g_autofree gchar *scheme = NULL;
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
ide_g_file_get_children_worker (GTask        *task,
                                gpointer      source_object,
                                gpointer      task_data,
                                GCancellable *cancellable)
{
  g_autoptr(GFileEnumerator) enumerator = NULL;
  g_autoptr(GPtrArray) children = NULL;
  g_autoptr(GError) error = NULL;
  GetChildren *gc = task_data;
  GFile *dir = source_object;

  g_assert (G_IS_TASK (task));
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
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  for (;;)
    {
      g_autoptr(GFileInfo) file_info = NULL;

      file_info = g_file_enumerator_next_file (enumerator, cancellable, &error);

      if (error != NULL)
        {
          g_task_return_error (task, g_steal_pointer (&error));
          return;
        }

      if (file_info == NULL)
        break;

      g_ptr_array_add (children, g_steal_pointer (&file_info));
    }

  g_task_return_pointer (task,
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
 * Since: 3.28
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
  g_autoptr(GTask) task = NULL;
  GetChildren *gc;

  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (attributes != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  gc = g_slice_new0 (GetChildren);
  gc->attributes = g_strdup (attributes);
  gc->flags = flags;

  task = g_task_new (file, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_g_file_get_children_async);
  g_task_set_priority (task, io_priority);
  g_task_set_task_data (task, gc, get_children_free);
  g_task_run_in_thread (task, ide_g_file_get_children_worker);
}

/**
 * ide_g_file_get_children_finish:
 * @file: a #GFile
 * @result: a #GAsyncResult provided to callback
 * @error: a location for a #GError, or %NULL
 *
 * Completes an asynchronous request to ide_g_file_get_children_async().
 *
 * Returns: (transfer full) (element-type Gio.File): A #GPtrArray
 *   of #GFileInfo if successful, otherwise %NULL.
 *
 * Since: 3.28
 */
GPtrArray *
ide_g_file_get_children_finish (GFile         *file,
                                GAsyncResult  *result,
                                GError       **error)
{
  GPtrArray *ret;

  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);
  g_return_val_if_fail (g_task_is_valid (G_TASK (result), file), NULL);

  ret = g_task_propagate_pointer (G_TASK (result), error);

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
  dzl_clear_pointer (&f->spec, g_pattern_spec_free);
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
          if (!ide_vcs_is_ignored (NULL, child, NULL))
            populate_descendants_matching (child, cancellable, results, spec, depth - 1);
        }
    }
}

static void
ide_g_file_find_worker (GTask        *task,
                        gpointer      source_object,
                        gpointer      task_data,
                        GCancellable *cancellable)
{
  GFile *file = source_object;
  Find *f = task_data;
  g_autoptr(GPtrArray) ret = NULL;

  g_assert (G_IS_TASK (task));
  g_assert (G_IS_FILE (file));
  g_assert (f != NULL);
  g_assert (f->spec != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  ret = g_ptr_array_new_with_free_func (g_object_unref);
  populate_descendants_matching (file, cancellable, ret, f->spec, f->depth);
  g_task_return_pointer (task, g_steal_pointer (&ret), (GDestroyNotify)g_ptr_array_unref);
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
 * Since: 3.30
 */
void
ide_g_file_find_with_depth_async (GFile               *file,
                                  const gchar         *pattern,
                                  guint                depth,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  Find *f;

  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (pattern != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (depth == 0)
    depth = G_MAXUINT;

  task = g_task_new (file, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_g_file_find_async);
  g_task_set_priority (task, G_PRIORITY_LOW + 100);

  f = g_slice_new0 (Find);
  f->spec = g_pattern_spec_new (pattern);
  f->depth = depth;
  g_task_set_task_data (task, f, (GDestroyNotify)find_free);

  if (f->spec == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_INVAL,
                               "Invalid pattern spec: %s",
                               pattern);
      return;
    }

  g_task_run_in_thread (task, ide_g_file_find_worker);
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
 * Since: 3.28
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
  g_return_val_if_fail (G_IS_TASK (result), NULL);

  ret = g_task_propagate_pointer (G_TASK (result), error);

  return IDE_PTR_ARRAY_STEAL_FULL (&ret);
}

/**
 * ide_g_host_file_get_contents:
 * @path: the path on the host
 * @contents: (out): a location for the contents
 * @len: (out): a location for the size, not including trailing \0
 * @error: location for a #GError, or %NULL
 *
 * This is similar to g_get_file_contents() but ensures that we get
 * the file from the host, rather than our mount namespace.
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 *
 * Since: 3.28
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

    launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE);
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

gboolean
ide_environ_parse (const gchar  *pair,
                   gchar       **key,
                   gchar       **value)
{
  const gchar *eq;

  g_return_val_if_fail (pair != NULL, FALSE);

  if (key != NULL)
    *key = NULL;

  if (value != NULL)
    *value = NULL;

  if ((eq = strchr (pair, '=')))
    {
      if (key != NULL)
        *key = g_strndup (pair, eq - pair);

      if (value != NULL)
        *value = g_strdup (eq + 1);

      return TRUE;
    }

  return FALSE;
}
