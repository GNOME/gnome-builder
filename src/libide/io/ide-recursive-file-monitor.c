/* ide-recursive-file-monitor.c
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-recursive-file-monitor"

#include "config.h"

#include <limits.h>
#include <stdlib.h>

#include "ide-recursive-file-monitor.h"

#define MONITOR_FLAGS 0

/**
 * SECTION:ide-recursive-file-monitor
 * @title: IdeRecursiveFileMonitor
 * @short_description: a recursive directory monitor
 *
 * This works by creating a #GFileMonitor for each directory underneath a root
 * directory (and recursively beyond that).
 *
 * This is only designed for use on Linux, where we are using a single inotify
 * FD. You can still hit the max watch limit, but it is much higher than the FD
 * limit.
 *
 * Since: 3.28
 */

struct _IdeRecursiveFileMonitor
{
  GObject                 parent_instance;

  GFile                  *root;
  GCancellable           *cancellable;

  GHashTable             *monitors_by_file;
  GHashTable             *files_by_monitor;

  IdeRecursiveIgnoreFunc  ignore_func;
  gpointer                ignore_func_data;
  GDestroyNotify          ignore_func_data_destroy;
};

enum {
  PROP_0,
  PROP_ROOT,
  N_PROPS
};

enum {
  CHANGED,
  N_SIGNALS
};

G_DEFINE_TYPE (IdeRecursiveFileMonitor, ide_recursive_file_monitor, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

static void
ide_recursive_file_monitor_track (IdeRecursiveFileMonitor *self,
                                  GFile                   *dir,
                                  GFileMonitor            *monitor);

static void
ide_recursive_file_monitor_unwatch (IdeRecursiveFileMonitor *self,
                                    GFile                   *file)
{
  GFileMonitor *monitor;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_RECURSIVE_FILE_MONITOR (self));
  g_assert (G_IS_FILE (file));

  monitor = g_hash_table_lookup (self->monitors_by_file, file);

  if (monitor != NULL)
    {
      g_object_ref (monitor);
      g_file_monitor_cancel (monitor);
      g_hash_table_remove (self->monitors_by_file, file);
      g_hash_table_remove (self->files_by_monitor, monitor);
      g_object_unref (monitor);
    }
}

static void
ide_recursive_file_monitor_collect_recursive (GPtrArray    *dirs,
                                              GFile        *parent,
                                              GCancellable *cancellable)
{
  g_autoptr(GFileEnumerator) enumerator = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (dirs != NULL);
  g_assert (G_IS_FILE (parent));
  g_assert (G_IS_CANCELLABLE (cancellable));

  enumerator = g_file_enumerate_children (parent,
                                          G_FILE_ATTRIBUTE_STANDARD_NAME","
                                          G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                          G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                          cancellable, &error);

  if (error != NULL)
    {
      g_warning ("Failed to iterate children: %s", error->message);
      g_clear_error (&error);
    }

  if (enumerator != NULL)
    {
      gpointer infoptr;

      while (NULL != (infoptr = g_file_enumerator_next_file (enumerator, cancellable, NULL)))
        {
          g_autoptr(GFileInfo) info = infoptr;

          if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY)
            {
              const gchar *name = g_file_info_get_name (info);
              g_autoptr(GFile) child = g_file_get_child (parent, name);

              /*
               * We add the child, and then recurse into the child immediately
               * so that we can keep the invariant that all descendants
               * immediately follow their ancestor. This allows us to simplify
               * our ignored-directory checks when we get back to the main
               * thread.
               */

              g_ptr_array_add (dirs, g_object_ref (child));
              ide_recursive_file_monitor_collect_recursive (dirs, child, cancellable);
            }
        }

      g_file_enumerator_close (enumerator, cancellable, NULL);
      g_clear_object (&enumerator);
    }
}

static GFile *
resolve_file (GFile *file)
{
  g_autofree gchar *orig_path = NULL;
  g_autoptr(GFile) new_file = NULL;
  char *real_path;

  g_assert (G_IS_FILE (file));

  /*
   * The goal here is to work our way up to the root and resolve any
   * symlinks in the path. If the file is not native, we don't care
   * about symlinks.
   */
  if (!g_file_is_native (file))
    return g_object_ref (file);

  orig_path = g_file_get_path (file);
#ifdef G_OS_UNIX
  real_path = realpath (orig_path, NULL);
#else
  real_path = _fullpath (orig_path, NULL, _MAX_PATH);
#endif

  /* unlikely, but PATH_MAX exceeded */
  if (real_path == NULL)
    return g_object_ref (file);

  new_file = g_file_new_for_path (real_path);
  free (real_path);

  return g_steal_pointer (&new_file);
}

static void
ide_recursive_file_monitor_collect_worker (GTask        *task,
                                           gpointer      source_object,
                                           gpointer      task_data,
                                           GCancellable *cancellable)
{
  g_autoptr(GPtrArray) dirs = NULL;
  g_autoptr(GFile) resolved = NULL;
  GFile *root = task_data;

  g_assert (G_IS_TASK (task));
  g_assert (G_IS_FILE (root));

  /* The first thing we want to do is resolve any symlinks out of
   * the path so that we are consistently working with the real
   * system path. This improves interaction with other APIs that
   * might not have given the callee back the symlink'd path and
   * instead the real path.
   */
  resolved = resolve_file (root);

  dirs = g_ptr_array_new_with_free_func (g_object_unref);
  g_ptr_array_add (dirs, g_object_ref (resolved));
  ide_recursive_file_monitor_collect_recursive (dirs, resolved, cancellable);

  g_task_return_pointer (task,
                         g_steal_pointer (&dirs),
                         (GDestroyNotify)g_ptr_array_unref);
}

static void
ide_recursive_file_monitor_collect (IdeRecursiveFileMonitor *self,
                                    GFile                   *root,
                                    GCancellable            *cancellable,
                                    GAsyncReadyCallback      callback,
                                    gpointer                 user_data)
{
  g_autoptr(GTask) task = NULL;

  g_assert (IDE_IS_RECURSIVE_FILE_MONITOR (self));
  g_assert (G_IS_FILE (root));
  g_assert (G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_recursive_file_monitor_collect);
  g_task_set_priority (task, G_PRIORITY_LOW);
  g_task_set_task_data (task, g_object_ref (root), g_object_unref);
  g_task_run_in_thread (task, ide_recursive_file_monitor_collect_worker);
}

static GPtrArray *
ide_recursive_file_monitor_collect_finish (IdeRecursiveFileMonitor  *self,
                                           GAsyncResult             *result,
                                           GError                  **error)
{
  g_assert (IDE_IS_RECURSIVE_FILE_MONITOR (self));
  g_assert (G_IS_TASK (result));
  g_assert (g_task_is_valid (G_TASK (result), self));

  return g_task_propagate_pointer (G_TASK (result), error);
}

static gboolean
ide_recursive_file_monitor_ignored (IdeRecursiveFileMonitor *self,
                                    GFile                   *file)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_RECURSIVE_FILE_MONITOR (self));
  g_assert (G_IS_FILE (file));

  if (self->ignore_func != NULL)
    return self->ignore_func (file, self->ignore_func_data);

  return FALSE;
}

static void
ide_recursive_file_monitor_changed (IdeRecursiveFileMonitor *self,
                                    GFile                   *file,
                                    GFile                   *other_file,
                                    GFileMonitorEvent        event,
                                    GFileMonitor            *monitor)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_RECURSIVE_FILE_MONITOR (self));
  g_assert (G_IS_FILE (file));
  g_assert (!other_file || G_IS_FILE (file));
  g_assert (G_IS_FILE_MONITOR (monitor));

  if (g_cancellable_is_cancelled (self->cancellable))
    return;

  if (ide_recursive_file_monitor_ignored (self, file))
    return;

  if (event == G_FILE_MONITOR_EVENT_DELETED)
    {
      if (g_hash_table_contains (self->monitors_by_file, file))
        ide_recursive_file_monitor_unwatch (self, file);
    }
  else if (event == G_FILE_MONITOR_EVENT_CREATED)
    {
      if (g_file_query_file_type (file, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL) == G_FILE_TYPE_DIRECTORY)
        {
          g_autoptr(GPtrArray) dirs = NULL;

          dirs = g_ptr_array_new_with_free_func (g_object_unref);
          g_ptr_array_add (dirs, g_object_ref (file));

          ide_recursive_file_monitor_collect_recursive (dirs, file, self->cancellable);

          for (guint i = 0; i < dirs->len; i++)
            {
              g_autoptr(GFileMonitor) dir_monitor = NULL;
              GFile *dir = g_ptr_array_index (dirs, i);

              if (!!(dir_monitor = g_file_monitor_directory (dir, MONITOR_FLAGS, self->cancellable, NULL)))
                ide_recursive_file_monitor_track (self, dir, dir_monitor);
            }
        }
    }

  g_signal_emit (self, signals [CHANGED], 0, file, other_file, event);
}


static void
ide_recursive_file_monitor_track (IdeRecursiveFileMonitor *self,
                                  GFile                   *dir,
                                  GFileMonitor            *monitor)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_RECURSIVE_FILE_MONITOR (self));
  g_assert (G_IS_FILE (dir));
  g_assert (G_IS_FILE_MONITOR (monitor));

  g_hash_table_insert (self->monitors_by_file,
                       g_object_ref (dir),
                       g_object_ref (monitor));

  g_hash_table_insert (self->files_by_monitor,
                       g_object_ref (monitor),
                       g_object_ref (dir));

  g_signal_connect_object (monitor,
                           "changed",
                           G_CALLBACK (ide_recursive_file_monitor_changed),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
ide_recursive_file_monitor_start_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  IdeRecursiveFileMonitor *self = (IdeRecursiveFileMonitor *)object;
  g_autoptr(GPtrArray) dirs = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = user_data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_RECURSIVE_FILE_MONITOR (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  dirs = ide_recursive_file_monitor_collect_finish (self, result, &error);

  if (dirs == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  for (guint i = 0; i < dirs->len; i++)
    {
      GFile *dir = g_ptr_array_index (dirs, i);
      g_autoptr(GFileMonitor) monitor = NULL;

      g_assert (G_IS_FILE (dir));

      if (ide_recursive_file_monitor_ignored (self, dir))
        {
          /*
           * Skip ahead to the next directory that does not have this directory
           * as a prefix. We can do this because we know the descendants are
           * guaranteed to immediately follow this directory.
           */

          for (guint j = i + 1; j < dirs->len; j++, i++)
            {
              GFile *next = g_ptr_array_index (dirs, j);

              if (!g_file_has_prefix (next, dir))
                break;
            }

          continue;
        }

      monitor = g_file_monitor_directory (dir, MONITOR_FLAGS, self->cancellable, &error);

      if (monitor == NULL)
        {
          g_warning ("Failed to monitor directory: %s", error->message);
          g_clear_error (&error);
          continue;
        }

      ide_recursive_file_monitor_track (self, dir, monitor);
    }

  g_task_return_boolean (task, TRUE);
}

void
ide_recursive_file_monitor_start_async (IdeRecursiveFileMonitor *self,
                                        GCancellable            *cancellable,
                                        GAsyncReadyCallback      callback,
                                        gpointer                 user_data)
{
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (IDE_IS_RECURSIVE_FILE_MONITOR (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_recursive_file_monitor_start_async);
  g_task_set_return_on_cancel (task, TRUE);
  g_task_set_task_data (task, g_object_ref (self->root), g_object_unref);
  g_task_set_priority (task, G_PRIORITY_LOW);

  if (self->root == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_INVAL,
                               "Cannot start file monitor, no root directory set");
      return;
    }

  ide_recursive_file_monitor_collect (self,
                                      self->root,
                                      self->cancellable,
                                      ide_recursive_file_monitor_start_cb,
                                      g_steal_pointer (&task));
}

gboolean
ide_recursive_file_monitor_start_finish (IdeRecursiveFileMonitor  *self,
                                         GAsyncResult             *result,
                                         GError                  **error)
{
  g_return_val_if_fail (IDE_IS_RECURSIVE_FILE_MONITOR (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);
  g_return_val_if_fail (g_task_is_valid (G_TASK (result), self), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
ide_recursive_file_monitor_constructed (GObject *object)
{
  IdeRecursiveFileMonitor *self = (IdeRecursiveFileMonitor *)object;

  G_OBJECT_CLASS (ide_recursive_file_monitor_parent_class)->constructed (object);

  if (self->root == NULL)
    g_warning ("%s created without a root directory", G_OBJECT_TYPE_NAME (self));
}

static void
ide_recursive_file_monitor_dispose (GObject *object)
{
  IdeRecursiveFileMonitor *self = (IdeRecursiveFileMonitor *)object;

  g_cancellable_cancel (self->cancellable);
  ide_recursive_file_monitor_set_ignore_func (self, NULL, NULL, NULL);

  g_hash_table_remove_all (self->files_by_monitor);
  g_hash_table_remove_all (self->monitors_by_file);

  G_OBJECT_CLASS (ide_recursive_file_monitor_parent_class)->dispose (object);
}

static void
ide_recursive_file_monitor_finalize (GObject *object)
{
  IdeRecursiveFileMonitor *self = (IdeRecursiveFileMonitor *)object;

  g_clear_object (&self->root);
  g_clear_object (&self->cancellable);

  g_clear_pointer (&self->files_by_monitor, g_hash_table_unref);
  g_clear_pointer (&self->monitors_by_file, g_hash_table_unref);

  G_OBJECT_CLASS (ide_recursive_file_monitor_parent_class)->finalize (object);
}

static void
ide_recursive_file_monitor_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  IdeRecursiveFileMonitor *self = IDE_RECURSIVE_FILE_MONITOR (object);

  switch (prop_id)
    {
    case PROP_ROOT:
      g_value_set_object (value, ide_recursive_file_monitor_get_root (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_recursive_file_monitor_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  IdeRecursiveFileMonitor *self = IDE_RECURSIVE_FILE_MONITOR (object);

  switch (prop_id)
    {
    case PROP_ROOT:
      self->root = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_recursive_file_monitor_class_init (IdeRecursiveFileMonitorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ide_recursive_file_monitor_constructed;
  object_class->dispose = ide_recursive_file_monitor_dispose;
  object_class->finalize = ide_recursive_file_monitor_finalize;
  object_class->get_property = ide_recursive_file_monitor_get_property;
  object_class->set_property = ide_recursive_file_monitor_set_property;

  properties [PROP_ROOT] =
    g_param_spec_object ("root",
                         "Root",
                         "The root directory to monitor",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * IdeRecursiveFileMonitor::changed:
   * @self: a #IdeRecursiveFileMonitor
   * @file: a #GFile
   * @other_file: (nullable): a #GFile for the other file when applicable
   * @event: the #GFileMonitorEvent event
   *
   * This event is similar to #GFileMonitor::changed but can be fired from
   * any of the monitored directories in the recursive mount.
   *
   * Since: 3.28
   */
  signals [CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 3, G_TYPE_FILE, G_TYPE_FILE, G_TYPE_FILE_MONITOR_EVENT);
}

static void
ide_recursive_file_monitor_init (IdeRecursiveFileMonitor *self)
{
  self->cancellable = g_cancellable_new ();
  self->files_by_monitor = g_hash_table_new_full (NULL, NULL, g_object_unref, g_object_unref);
  self->monitors_by_file = g_hash_table_new_full (g_file_hash,
                                                  (GEqualFunc) g_file_equal,
                                                  g_object_unref,
                                                  g_object_unref);
}

IdeRecursiveFileMonitor *
ide_recursive_file_monitor_new (GFile *file)
{
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  return g_object_new (IDE_TYPE_RECURSIVE_FILE_MONITOR,
                       "root", file,
                       NULL);
}

/**
 * ide_recursive_file_monitor_cancel:
 * @self: a #IdeRecursiveFileMonitor
 *
 * Cancels the recursive file monitor.
 *
 * Since: 3.28
 */
void
ide_recursive_file_monitor_cancel (IdeRecursiveFileMonitor *self)
{
  g_return_if_fail (IDE_IS_RECURSIVE_FILE_MONITOR (self));

  g_object_run_dispose (G_OBJECT (self));
}

/**
 * ide_recursive_file_monitor_get_root:
 * @self: a #IdeRecursiveFileMonitor
 *
 * Gets the root directory used forthe file monitor.
 *
 * Returns: (transfer none): a #GFile
 *
 * Since: 3.28
 */
GFile *
ide_recursive_file_monitor_get_root (IdeRecursiveFileMonitor *self)
{
  g_return_val_if_fail (IDE_IS_RECURSIVE_FILE_MONITOR (self), NULL);

  return self->root;
}

/**
 * ide_recursive_file_monitor_set_ignore_func:
 * @self: a #IdeRecursiveFileMonitor
 * @ignore_func: (scope async): a #IdeRecursiveIgnoreFunc
 * @ignore_func_data: closure data for @ignore_func
 * @ignore_func_data_destroy: destroy notify for @ignore_func_data
 *
 * Sets a callback function to determine if a #GFile should be ignored
 * from signal emission.
 *
 * @ignore_func will always be called from the applications main thread.
 *
 * If @ignore_func is %NULL, it is set to the default which does not
 * ignore any files or directories.
 *
 * Since: 3.28
 */
void
ide_recursive_file_monitor_set_ignore_func (IdeRecursiveFileMonitor *self,
                                            IdeRecursiveIgnoreFunc   ignore_func,
                                            gpointer                 ignore_func_data,
                                            GDestroyNotify           ignore_func_data_destroy)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_RECURSIVE_FILE_MONITOR (self));

  if (ignore_func == NULL)
    {
      ignore_func_data = NULL;
      ignore_func_data_destroy = NULL;
    }

  if (self->ignore_func_data && self->ignore_func_data_destroy)
    {
      gpointer data = self->ignore_func_data;
      GDestroyNotify notify = self->ignore_func_data_destroy;

      self->ignore_func = NULL;
      self->ignore_func_data = NULL;
      self->ignore_func_data_destroy = NULL;

      notify (data);
    }

  self->ignore_func = ignore_func;
  self->ignore_func_data = ignore_func_data;
  self->ignore_func_data_destroy = ignore_func_data_destroy;
}
