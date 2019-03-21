/* gbp-git-index-monitor.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN       "gbp-git-index-monitor"
#define CHANGED_DELAY_MSEC 250

#include "config.h"

#include <libide-core.h>

#include "gbp-git-index-monitor.h"

struct _GbpGitIndexMonitor
{
  GObject       parent_instance;
  GFile        *repository_dir;
  GFileMonitor *monitor;
  guint         changed_source;
};

G_DEFINE_TYPE (GbpGitIndexMonitor, gbp_git_index_monitor, G_TYPE_OBJECT)

enum {
  CHANGED,
  N_SIGNALS
};

static guint signals [N_SIGNALS];

static void
gbp_git_index_monitor_dispose (GObject *object)
{
  GbpGitIndexMonitor *self = (GbpGitIndexMonitor *)object;

  g_clear_handle_id (&self->changed_source, g_source_remove);
  g_clear_object (&self->repository_dir);

  if (self->monitor != NULL)
    {
      g_file_monitor_cancel (self->monitor);
      g_clear_object (&self->monitor);
    }

  G_OBJECT_CLASS (gbp_git_index_monitor_parent_class)->dispose (object);
}

static void
gbp_git_index_monitor_class_init (GbpGitIndexMonitorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gbp_git_index_monitor_dispose;

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
}

static void
gbp_git_index_monitor_init (GbpGitIndexMonitor *self)
{
}

static gboolean
gbp_git_index_monitor_queue_changed_cb (gpointer data)
{
  GbpGitIndexMonitor *self = data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GIT_INDEX_MONITOR (self));

  self->changed_source = 0;

  g_signal_emit (self, signals [CHANGED], 0);

  return G_SOURCE_REMOVE;
}

static void
gbp_git_index_monitor_queue_changed (GbpGitIndexMonitor *self)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GIT_INDEX_MONITOR (self));

  g_clear_handle_id (&self->changed_source, g_source_remove);
  self->changed_source = g_timeout_add_full (G_PRIORITY_LOW,
                                             CHANGED_DELAY_MSEC,
                                             gbp_git_index_monitor_queue_changed_cb,
                                             g_object_ref (self),
                                             g_object_unref);
}

static void
gbp_git_index_monitor_changed_cb (GbpGitIndexMonitor *self,
                                  GFile              *file,
                                  GFile              *other_file,
                                  GFileMonitorEvent   event,
                                  GFileMonitor       *monitor)
{
  g_autofree gchar *name = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_GIT_INDEX_MONITOR (self));
  g_assert (G_IS_FILE (file));
  g_assert (!other_file || G_IS_FILE (other_file));
  g_assert (G_IS_FILE_MONITOR (monitor));

  name = g_file_get_basename (file);

  if (ide_str_equal0 (name, "index") ||
      ide_str_equal0 (name, "HEAD") ||
      ide_str_equal0 (name, "HEAD.lock") ||
      ide_str_equal0 (name, "ORIG_HEAD") ||
      ide_str_equal0 (name, "FETCH_HEAD") ||
      ide_str_equal0 (name, "COMMIT_EDITMSG") ||
      ide_str_equal0 (name, "config"))
    gbp_git_index_monitor_queue_changed (self);

  IDE_EXIT;
}

GbpGitIndexMonitor *
gbp_git_index_monitor_new (GFile *repository_dir)
{
  GbpGitIndexMonitor *self;
  g_autoptr(GError) error = NULL;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (G_IS_FILE (repository_dir), NULL);

  self = g_object_new (GBP_TYPE_GIT_INDEX_MONITOR, NULL);
  self->repository_dir = g_object_ref (repository_dir);
  self->monitor = g_file_monitor_directory (repository_dir,
                                            G_FILE_MONITOR_NONE,
                                            NULL,
                                            &error);

  if (error != NULL)
    g_critical ("Failed to monitor git repository, no changes will be detected: %s",
                error->message);
  else
    g_signal_connect_object (self->monitor,
                             "changed",
                             G_CALLBACK (gbp_git_index_monitor_changed_cb),
                             self,
                             G_CONNECT_SWAPPED);

  return g_steal_pointer (&self);
}
