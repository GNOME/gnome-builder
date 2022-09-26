/* ipc-git-index-monitor.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#define G_LOG_DOMAIN "ipc-git-index-monitor"

#include "ipc-git-index-monitor.h"

#define CHANGED_DELAY_MSEC 500

struct _IpcGitIndexMonitor
{
  GObject       parent_instance;
  GFileMonitor *refs_heads_monitor;
  GFileMonitor *dot_git_monitor;
  guint         changed_source;
};

G_DEFINE_FINAL_TYPE (IpcGitIndexMonitor, ipc_git_index_monitor, G_TYPE_OBJECT)

enum {
  CHANGED,
  N_SIGNALS
};

static guint signals [N_SIGNALS];
static GHashTable *index_changed_files;
static const char *index_changed_file_names[] = {
  "index",
  "index.lock",
  "HEAD",
  "HEAD.lock",
  "ORIG_HEAD",
  "FETCH_HEAD",
  "COMMIT_EDITMSG",
  "PREPARE_COMMIT_MSG",
  "config",
};

static void
ipc_git_index_monitor_dispose (GObject *object)
{
  IpcGitIndexMonitor *self = (IpcGitIndexMonitor *)object;

  g_clear_handle_id (&self->changed_source, g_source_remove);

  if (self->refs_heads_monitor != NULL)
    {
      g_file_monitor_cancel (self->refs_heads_monitor);
      g_clear_object (&self->refs_heads_monitor);
    }

  if (self->dot_git_monitor != NULL)
    {
      g_file_monitor_cancel (self->dot_git_monitor);
      g_clear_object (&self->dot_git_monitor);
    }

  G_OBJECT_CLASS (ipc_git_index_monitor_parent_class)->dispose (object);
}

static void
ipc_git_index_monitor_class_init (IpcGitIndexMonitorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ipc_git_index_monitor_dispose;

  signals [CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  g_signal_set_va_marshaller (signals [CHANGED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__VOIDv);

  index_changed_files = g_hash_table_new (g_str_hash, g_str_equal);
  for (guint i = 0; i < G_N_ELEMENTS (index_changed_file_names); i++)
    g_hash_table_add (index_changed_files, (char *)index_changed_file_names[i]);
}

static void
ipc_git_index_monitor_init (IpcGitIndexMonitor *self)
{
}

static gboolean
ipc_git_index_monitor_queue_changed_cb (gpointer data)
{
  IpcGitIndexMonitor *self = data;

  g_assert (IPC_IS_GIT_INDEX_MONITOR (self));

  self->changed_source = 0;

  g_signal_emit (self, signals [CHANGED], 0);

  return G_SOURCE_REMOVE;
}

static void
ipc_git_index_monitor_queue_changed (IpcGitIndexMonitor *self)
{
  g_assert (IPC_IS_GIT_INDEX_MONITOR (self));

  g_clear_handle_id (&self->changed_source, g_source_remove);
  self->changed_source = g_timeout_add_full (G_PRIORITY_LOW,
                                             CHANGED_DELAY_MSEC,
                                             ipc_git_index_monitor_queue_changed_cb,
                                             g_object_ref (self),
                                             g_object_unref);
}

static void
ipc_git_index_monitor_dot_git_changed_cb (IpcGitIndexMonitor *self,
                                          GFile              *file,
                                          GFile              *other_file,
                                          GFileMonitorEvent   event,
                                          GFileMonitor       *monitor)
{
  g_autofree char *name = NULL;

  g_assert (IPC_IS_GIT_INDEX_MONITOR (self));
  g_assert (G_IS_FILE (file));
  g_assert (!other_file || G_IS_FILE (other_file));
  g_assert (G_IS_FILE_MONITOR (monitor));

  name = g_file_get_basename (file);

  if (g_hash_table_contains (index_changed_files, name))
    goto queue_changed_signal;

  if (other_file != NULL)
    {
      g_autofree char *other_name = g_file_get_basename (other_file);

      if (g_hash_table_contains (index_changed_files, other_name))
        goto queue_changed_signal;
    }

  return;

queue_changed_signal:
  ipc_git_index_monitor_queue_changed (self);
}

static void
ipc_git_index_monitor_refs_heads_changed (IpcGitIndexMonitor *self,
                                          GFile              *file,
                                          GFile              *other_file,
                                          GFileMonitorEvent   event,
                                          GFileMonitor       *monitor)
{
  g_assert (IPC_IS_GIT_INDEX_MONITOR (self));
  g_assert (G_IS_FILE (file));
  g_assert (!other_file || G_IS_FILE (other_file));
  g_assert (G_IS_FILE_MONITOR (monitor));

  ipc_git_index_monitor_queue_changed (self);
}

IpcGitIndexMonitor *
ipc_git_index_monitor_new (GFile *location)
{
  IpcGitIndexMonitor *self;
  g_autofree gchar *name = NULL;
  g_autoptr(GFile) dot_git_dir = NULL;
  g_autoptr(GFile) refs_heads_dir = NULL;
  g_autoptr(GError) error = NULL;

  g_return_val_if_fail (G_IS_FILE (location), NULL);
  g_return_val_if_fail (g_file_is_native (location), NULL);

  self = g_object_new (IPC_TYPE_GIT_INDEX_MONITOR, NULL);

  name = g_file_get_basename (location);

  if (g_strcmp0 (name, ".git") == 0)
    {
      dot_git_dir = g_object_ref (location);
    }
  else
    {
      const gchar *path = g_file_peek_path (location);
      g_autofree gchar *new_path = NULL;
      const gchar *dot_git;

      /* Can't find .git directory, bail */
      if (NULL == (dot_git = strstr (path, ".git"G_DIR_SEPARATOR_S)))
        {
          g_critical ("Failed to locate .git directory, cannot monitor repository");
          return g_steal_pointer (&self);
        }

      new_path = g_strndup (path, dot_git - path + 4);
      dot_git_dir = g_file_new_for_path (new_path);
    }

  self->dot_git_monitor = g_file_monitor_directory (dot_git_dir,
                                                    G_FILE_MONITOR_WATCH_MOVES,
                                                    NULL,
                                                    &error);

  if (self->dot_git_monitor == NULL)
    {
      g_critical ("Failed to monitor git repository, no changes will be detected: %s",
                  error->message);
      g_clear_error (&error);
    }
  else
    {
      g_signal_connect_object (self->dot_git_monitor,
                               "changed",
                               G_CALLBACK (ipc_git_index_monitor_dot_git_changed_cb),
                               self,
                               G_CONNECT_SWAPPED);
    }

  refs_heads_dir = g_file_get_child (dot_git_dir, "refs/heads");
  self->refs_heads_monitor = g_file_monitor_directory (refs_heads_dir,
                                                       G_FILE_MONITOR_NONE,
                                                       NULL,
                                                       &error);

  if (self->refs_heads_monitor == NULL)
    {
      g_critical ("Failed to monitor git repository, no changes will be detected: %s",
                  error->message);
      g_clear_error (&error);
    }
  else
    {
      g_signal_connect_object (self->refs_heads_monitor,
                               "changed",
                               G_CALLBACK (ipc_git_index_monitor_refs_heads_changed),
                               self,
                               G_CONNECT_SWAPPED);
    }

  return g_steal_pointer (&self);
}
