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

#include <string.h>

#include "config.h"

#include "util/ide-glib.h"

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
