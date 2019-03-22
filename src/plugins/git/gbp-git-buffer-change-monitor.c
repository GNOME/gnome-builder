/* gbp-git-buffer-change-monitor.c
 *
 * Copyright 2015-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-git-buffer-change-monitor"

#include "config.h"

#include <dazzle.h>

#include "gbp-git-client.h"
#include "gbp-git-buffer-change-monitor.h"
#include "line-cache.h"

#define DELAY_CHANGED_SEC 1

struct _GbpGitBufferChangeMonitor
{
  IdeBufferChangeMonitor  parent_instance;
  DzlSignalGroup         *signals;
  LineCache              *cache;
  guint                   delete_range_requires_recalculation : 1;
};

G_DEFINE_TYPE (GbpGitBufferChangeMonitor, gbp_git_buffer_change_monitor, IDE_TYPE_BUFFER_CHANGE_MONITOR)

static void
gbp_git_buffer_change_monitor_recalculate (GbpGitBufferChangeMonitor *self)
{
  g_assert (GBP_IS_GIT_BUFFER_CHANGE_MONITOR (self));

  gbp_git_buffer_change_monitor_wait_async (self, NULL, NULL, NULL);
}

static void
gbp_git_buffer_change_monitor_load (IdeBufferChangeMonitor *monitor,
                                    IdeBuffer              *buffer)
{
  GbpGitBufferChangeMonitor *self = (GbpGitBufferChangeMonitor *)monitor;

  g_assert (GBP_IS_GIT_BUFFER_CHANGE_MONITOR (self));
  g_assert (DZL_IS_SIGNAL_GROUP (self->signals));
  g_assert (IDE_IS_BUFFER (buffer));

  dzl_signal_group_set_target (self->signals, buffer);
}

static void
gbp_git_buffer_change_monitor_reload (IdeBufferChangeMonitor *monitor)
{
  GbpGitBufferChangeMonitor *self = (GbpGitBufferChangeMonitor *)monitor;

  g_assert (GBP_IS_GIT_BUFFER_CHANGE_MONITOR (monitor));

  g_clear_pointer (&self->cache, line_cache_free);
}

static IdeBufferLineChange
translate_mark (LineMark mark)
{
  IdeBufferLineChange change = 0;

  if (mark & LINE_MARK_ADDED)
    change |= IDE_BUFFER_LINE_CHANGE_ADDED;

  if (mark & LINE_MARK_REMOVED)
    change |= IDE_BUFFER_LINE_CHANGE_DELETED;

  if (mark & LINE_MARK_PREVIOUS_REMOVED)
    change |= IDE_BUFFER_LINE_CHANGE_PREVIOUS_DELETED;

  if (mark & LINE_MARK_CHANGED)
    change |= IDE_BUFFER_LINE_CHANGE_CHANGED;

  return change;
}

static IdeBufferLineChange
gbp_git_buffer_change_monitor_get_change (IdeBufferChangeMonitor *monitor,
                                          guint                   line)
{
  GbpGitBufferChangeMonitor *self = (GbpGitBufferChangeMonitor *)monitor;

  g_assert (GBP_IS_GIT_BUFFER_CHANGE_MONITOR (self));

  return self->cache ? translate_mark (line_cache_get_mark (self->cache, line)) : 0;
}

static void
gbp_git_buffer_change_monitor_foreach_change_cb (gpointer data,
                                                 gpointer user_data)
{
  const LineEntry *entry = data;
  const struct {
    IdeBufferChangeMonitorForeachFunc func;
    gpointer data;
  } *state = user_data;

  g_assert (entry != NULL);
  g_assert (state != NULL);
  g_assert (state->func != NULL);

  state->func (entry->line, translate_mark (entry->mark), state->data);
}

static void
gbp_git_buffer_change_monitor_foreach_change (IdeBufferChangeMonitor            *monitor,
                                              guint                              line_begin,
                                              guint                              line_end,
                                              IdeBufferChangeMonitorForeachFunc  callback,
                                              gpointer                           user_data)
{
  GbpGitBufferChangeMonitor *self = (GbpGitBufferChangeMonitor *)monitor;

  g_assert (GBP_IS_GIT_BUFFER_CHANGE_MONITOR (self));
  g_assert (callback != NULL);

  if (self->cache != NULL)
    {
      struct {
        IdeBufferChangeMonitorForeachFunc func;
        gpointer data;
      } state = { callback, user_data };

      line_cache_foreach_in_range (self->cache,
                                   line_begin,
                                   line_end,
                                   gbp_git_buffer_change_monitor_foreach_change_cb,
                                   &state);
    }
}

static void
gbp_git_buffer_change_monitor_insert_text_after_cb (GbpGitBufferChangeMonitor *self,
                                                    const GtkTextIter         *location,
                                                    gchar                     *text,
                                                    gint                       len,
                                                    IdeBuffer                 *buffer)
{
  IdeBufferLineChange change;
  guint line;

  g_assert (GBP_IS_GIT_BUFFER_CHANGE_MONITOR (self));
  g_assert (location != NULL);
  g_assert (text != NULL);
  g_assert (IDE_IS_BUFFER (buffer));

  /*
   * We need to recalculate the diff when text is inserted if:
   *
   * 1) A newline is included in the text.
   * 2) The line currently has flags of NONE.
   *
   * Technically we need to do it on every change to be more correct, but that wastes a lot of
   * power. So instead, we'll be a bit lazy about it here and pick up the other changes on a much
   * more conservative timeout, generated by gbp_git_buffer_change_monitor__buffer_changed_cb().
   */

  if (NULL != memmem (text, len, "\n", 1))
    goto recalculate;

  line = gtk_text_iter_get_line (location);
  change = gbp_git_buffer_change_monitor_get_change (IDE_BUFFER_CHANGE_MONITOR (self), line);
  if (change == IDE_BUFFER_LINE_CHANGE_NONE)
    goto recalculate;

  return;

recalculate:
  gbp_git_buffer_change_monitor_recalculate (self);
}

static void
gbp_git_buffer_change_monitor_delete_range_after_cb (GbpGitBufferChangeMonitor *self,
                                                     GtkTextIter               *begin,
                                                     GtkTextIter               *end,
                                                     IdeBuffer                 *buffer)
{
  g_assert (GBP_IS_GIT_BUFFER_CHANGE_MONITOR (self));
  g_assert (begin != NULL);
  g_assert (end != NULL);
  g_assert (IDE_IS_BUFFER (buffer));

  if (self->delete_range_requires_recalculation)
    {
      self->delete_range_requires_recalculation = FALSE;
      gbp_git_buffer_change_monitor_recalculate (self);
    }
}

static void
gbp_git_buffer_change_monitor_delete_range_cb (GbpGitBufferChangeMonitor *self,
                                               GtkTextIter               *begin,
                                               GtkTextIter               *end,
                                               IdeBuffer                 *buffer)
{
  IdeBufferLineChange change;
  guint line;

  g_assert (GBP_IS_GIT_BUFFER_CHANGE_MONITOR (self));
  g_assert (begin != NULL);
  g_assert (end != NULL);
  g_assert (IDE_IS_BUFFER (buffer));

  /*
   * We need to recalculate the diff when text is deleted if:
   *
   * 1) The range includes a newline.
   * 2) The current line change is set to NONE.
   *
   * Technically we need to do it on every change to be more correct, but
   * that wastes a lot of power. So instead, we'll be a bit lazy about it
   * here and pick up the other changes on a much more conservative timeout,
   * generated by gbp_git_buffer_change_monitor__buffer_changed_cb().
   */

  if (gtk_text_iter_get_line (begin) != gtk_text_iter_get_line (end))
    goto recalculate;

  line = gtk_text_iter_get_line (begin);
  change = gbp_git_buffer_change_monitor_get_change (IDE_BUFFER_CHANGE_MONITOR (self), line);
  if (change == IDE_BUFFER_LINE_CHANGE_NONE)
    goto recalculate;

  return;

recalculate:
  /*
   * We need to wait for the delete to occur, so mark it as necessary and let
   * gbp_git_buffer_change_monitor__buffer_delete_range_after_cb perform the operation.
   */
  self->delete_range_requires_recalculation = TRUE;
}

static void
gbp_git_buffer_change_monitor_change_settled_cb (GbpGitBufferChangeMonitor *self,
                                                 IdeBuffer                 *buffer)
{
  g_assert (IDE_IS_BUFFER_CHANGE_MONITOR (self));
  g_assert (IDE_IS_BUFFER (buffer));

  gbp_git_buffer_change_monitor_recalculate (self);
}

static void
gbp_git_buffer_change_monitor_destroy (IdeObject *object)
{
  GbpGitBufferChangeMonitor *self = (GbpGitBufferChangeMonitor *)object;

  g_clear_pointer (&self->cache, line_cache_free);

  if (self->signals != NULL)
    {
      dzl_signal_group_set_target (self->signals, NULL);
      g_clear_object (&self->signals);
    }

  IDE_OBJECT_CLASS (gbp_git_buffer_change_monitor_parent_class)->destroy (object);
}

static void
gbp_git_buffer_change_monitor_class_init (GbpGitBufferChangeMonitorClass *klass)
{
  IdeBufferChangeMonitorClass *monitor_class = IDE_BUFFER_CHANGE_MONITOR_CLASS (klass);
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  i_object_class->destroy = gbp_git_buffer_change_monitor_destroy;

  monitor_class->load = gbp_git_buffer_change_monitor_load;
  monitor_class->reload = gbp_git_buffer_change_monitor_reload;
  monitor_class->get_change = gbp_git_buffer_change_monitor_get_change;
  monitor_class->foreach_change = gbp_git_buffer_change_monitor_foreach_change;
}

static void
gbp_git_buffer_change_monitor_init (GbpGitBufferChangeMonitor *self)
{
  self->signals = dzl_signal_group_new (IDE_TYPE_BUFFER);

  dzl_signal_group_connect_object (self->signals,
                                   "insert-text",
                                   G_CALLBACK (gbp_git_buffer_change_monitor_insert_text_after_cb),
                                   self,
                                   G_CONNECT_SWAPPED | G_CONNECT_AFTER);
  dzl_signal_group_connect_object (self->signals,
                                   "delete-range",
                                   G_CALLBACK (gbp_git_buffer_change_monitor_delete_range_cb),
                                   self,
                                   G_CONNECT_SWAPPED);
  dzl_signal_group_connect_object (self->signals,
                                   "delete-range",
                                   G_CALLBACK (gbp_git_buffer_change_monitor_delete_range_after_cb),
                                   self,
                                   G_CONNECT_SWAPPED | G_CONNECT_AFTER);
  dzl_signal_group_connect_object (self->signals,
                                   "change-settled",
                                   G_CALLBACK (gbp_git_buffer_change_monitor_change_settled_cb),
                                   self,
                                   G_CONNECT_SWAPPED | G_CONNECT_AFTER);
}

static void
gbp_git_buffer_change_monitor_wait_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  GbpGitClient *client = (GbpGitClient *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  GbpGitBufferChangeMonitor *self;
  LineCache *cache;

  g_assert (GBP_IS_GIT_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!(cache = gbp_git_client_get_changes_finish (client, result, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  self = ide_task_get_source_object (task);
  g_assert (GBP_IS_GIT_BUFFER_CHANGE_MONITOR (self));
  g_clear_pointer (&self->cache, line_cache_free);
  self->cache = g_steal_pointer (&cache);

  ide_task_return_boolean (task, TRUE);

  ide_buffer_change_monitor_emit_changed (IDE_BUFFER_CHANGE_MONITOR (self));
}

void
gbp_git_buffer_change_monitor_wait_async (GbpGitBufferChangeMonitor *self,
                                          GCancellable              *cancellable,
                                          GAsyncReadyCallback        callback,
                                          gpointer                   user_data)
{
  g_autofree gchar *path = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(GFile) workdir = NULL;
  GbpGitClient *client;
  IdeContext *context;
  IdeBuffer *buffer;
  GFile *file;

  g_return_if_fail (GBP_IS_GIT_BUFFER_CHANGE_MONITOR (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  self->delete_range_requires_recalculation = FALSE;

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_git_buffer_change_monitor_wait_async);

  buffer = dzl_signal_group_get_target (self->signals);
  context = ide_object_get_context (IDE_OBJECT (self));
  workdir = ide_context_ref_workdir (context);
  file = ide_buffer_get_file (buffer);

  if (!g_file_has_prefix (file, workdir))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "File is not within project directory");
      return;
    }

  path = g_file_get_relative_path (workdir, file);
  bytes = ide_buffer_dup_content (buffer);
  client = gbp_git_client_from_context (context);

  gbp_git_client_get_changes_async (client,
                                    path,
                                    (const gchar *)g_bytes_get_data (bytes, NULL),
                                    cancellable,
                                    gbp_git_buffer_change_monitor_wait_cb,
                                    g_steal_pointer (&task));
}

gboolean
gbp_git_buffer_change_monitor_wait_finish (GbpGitBufferChangeMonitor  *self,
                                           GAsyncResult               *result,
                                           GError                    **error)
{
  g_return_val_if_fail (GBP_IS_GIT_BUFFER_CHANGE_MONITOR (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}
