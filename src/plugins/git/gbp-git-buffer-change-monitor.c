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

#include <glib/gi18n.h>
#include <string.h>

#include "daemon/ipc-git-change-monitor.h"
#include "daemon/line-cache.h"

#include "gbp-git-buffer-change-monitor.h"
#include "gbp-git-vcs.h"

struct _GbpGitBufferChangeMonitor
{
  IdeBufferChangeMonitor  parent;
  IpcGitChangeMonitor    *proxy;
  GSignalGroup           *vcs_signals;
  LineCache              *cache;
  GWeakRef                buffer_wr;
  guint                   commit_notify;
  guint                   last_change_count;
  guint                   queued_source;
  guint                   delete_range_requires_recalculation : 1;
  guint                   not_found : 1;
};

enum { SLOW, FAST };
static const guint g_delay[] = { 750, 50 };

G_DEFINE_FINAL_TYPE (GbpGitBufferChangeMonitor, gbp_git_buffer_change_monitor, IDE_TYPE_BUFFER_CHANGE_MONITOR)

static gboolean
queued_update_source_cb (GbpGitBufferChangeMonitor *self)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GIT_BUFFER_CHANGE_MONITOR (self));

  self->queued_source = 0;

  gbp_git_buffer_change_monitor_wait_async (self, NULL, NULL, NULL);

  return G_SOURCE_REMOVE;
}

static void
gbp_git_buffer_change_monitor_queue_update (GbpGitBufferChangeMonitor *self,
                                            gboolean                  fast)
{
  guint delay;

  g_assert (GBP_IS_GIT_BUFFER_CHANGE_MONITOR (self));

  fast = !!fast;

  /* Re-use existing source if this is slow */
  if (fast == SLOW && self->queued_source)
    return;

  delay = g_delay[fast];

  g_clear_handle_id (&self->queued_source, g_source_remove);

  self->queued_source =
    g_timeout_add_full (G_PRIORITY_HIGH,
                        delay,
                        (GSourceFunc) queued_update_source_cb,
                        g_object_ref (self),
                        g_object_unref);
}

static void
gbp_git_buffer_change_monitor_commit_notify (GtkTextBuffer            *buffer,
                                             GtkTextBufferNotifyFlags  flags,
                                             guint                     position,
                                             guint                     length,
                                             gpointer                  user_data)
{
  GbpGitBufferChangeMonitor *self = user_data;

  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (flags != 0);
  g_assert (GBP_IS_GIT_BUFFER_CHANGE_MONITOR (self));

  if (flags == GTK_TEXT_BUFFER_NOTIFY_BEFORE_INSERT)
    {
      /* Do nothing */
    }
  else if (flags == GTK_TEXT_BUFFER_NOTIFY_AFTER_INSERT)
    {
      GtkTextIter begin;
      GtkTextIter end;
      guint begin_line;
      guint end_line;

      /*
       * We need to recalculate the diff when text is inserted if:
       *
       * 1) A newline is included in the text.
       * 2) The line currently has flags of NONE.
       *
       * Technically we need to do it on every change to be more correct, but that
       * wastes a lot of power. So instead, we'll be a bit lazy about it here and
       * pick up the other changes on a much more conservative timeout
       */

      gtk_text_buffer_get_iter_at_offset (buffer, &begin, position);
      gtk_text_buffer_get_iter_at_offset (buffer, &end, position + length);

      begin_line = gtk_text_iter_get_line (&begin);
      end_line = gtk_text_iter_get_line (&end);

      if (begin_line != end_line ||
          self->cache == NULL ||
          !line_cache_get_mark (self->cache, begin_line))
        gbp_git_buffer_change_monitor_queue_update (self, FAST);
      else
        gbp_git_buffer_change_monitor_queue_update (self, SLOW);
    }
  else if (flags == GTK_TEXT_BUFFER_NOTIFY_BEFORE_DELETE)
    {
      GtkTextIter begin;
      GtkTextIter end;
      guint begin_line;
      guint end_line;

      /*
       * We need to recalculate the diff when text is deleted if:
       *
       * 1) The range includes a newline.
       * 2) The current line change is set to NONE.
       *
       * Technically we need to do it on every change to be more correct, but that
       * wastes a lot of power. So instead, we'll be a bit lazy about it here and
       * pick up the other changes on a much more conservative timeout, generated
       * by gbp_git_buffer_change_monitor__buffer_changed_cb().
       */

      gtk_text_buffer_get_iter_at_offset (buffer, &begin, position);
      gtk_text_buffer_get_iter_at_offset (buffer, &end, position + length);

      begin_line = gtk_text_iter_get_line (&begin);
      end_line = gtk_text_iter_get_line (&end);

      if (begin_line != end_line ||
          self->cache == NULL ||
          !line_cache_get_mark (self->cache, begin_line))
        self->delete_range_requires_recalculation = TRUE;
    }
  else if (flags == GTK_TEXT_BUFFER_NOTIFY_AFTER_DELETE)
    {
      if (self->delete_range_requires_recalculation)
        {
          self->delete_range_requires_recalculation = FALSE;
          gbp_git_buffer_change_monitor_queue_update (self, FAST);
        }
    }
}

static void
gbp_git_buffer_change_monitor_destroy (IdeObject *object)
{
  GbpGitBufferChangeMonitor *self = (GbpGitBufferChangeMonitor *)object;
  g_autoptr(IdeBuffer) buffer = NULL;

  if ((buffer = g_weak_ref_get (&self->buffer_wr)))
    {
      gtk_text_buffer_remove_commit_notify (GTK_TEXT_BUFFER (buffer),
                                            self->commit_notify);
      self->commit_notify = 0;
      g_weak_ref_set (&self->buffer_wr, NULL);
    }

  if (self->vcs_signals)
    {
      g_signal_group_set_target (self->vcs_signals, NULL);
      g_clear_object (&self->vcs_signals);
    }

  if (self->proxy != NULL)
    {
      ipc_git_change_monitor_call_close (self->proxy, NULL, NULL, NULL);
      g_clear_object (&self->proxy);
    }

  g_clear_pointer (&self->cache, line_cache_free);
  g_clear_handle_id (&self->queued_source, g_source_remove);

  IDE_OBJECT_CLASS (gbp_git_buffer_change_monitor_parent_class)->destroy (object);
}

static void
gbp_git_buffer_change_monitor_load (IdeBufferChangeMonitor *monitor,
                                    IdeBuffer              *buffer)
{
  GbpGitBufferChangeMonitor *self = (GbpGitBufferChangeMonitor *)monitor;
  g_autoptr(IdeContext) context = NULL;
  IdeVcs *vcs;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GIT_BUFFER_CHANGE_MONITOR (self));
  g_assert (IDE_IS_BUFFER (buffer));

  context = ide_buffer_ref_context (buffer);
  vcs = ide_vcs_from_context (context);

  g_signal_group_set_target (self->vcs_signals, vcs);

  g_weak_ref_set (&self->buffer_wr, buffer);

  self->commit_notify = gtk_text_buffer_add_commit_notify (GTK_TEXT_BUFFER (buffer),
                                                           (GTK_TEXT_BUFFER_NOTIFY_BEFORE_INSERT |
                                                            GTK_TEXT_BUFFER_NOTIFY_AFTER_INSERT |
                                                            GTK_TEXT_BUFFER_NOTIFY_BEFORE_DELETE |
                                                            GTK_TEXT_BUFFER_NOTIFY_AFTER_DELETE),
                                                           gbp_git_buffer_change_monitor_commit_notify,
                                                           self, NULL);

  gbp_git_buffer_change_monitor_queue_update (self, FAST);
}

static void
gbp_git_buffer_change_monitor_reload (IdeBufferChangeMonitor *monitor)
{
  GbpGitBufferChangeMonitor *self = (GbpGitBufferChangeMonitor *)monitor;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GIT_BUFFER_CHANGE_MONITOR (self));

  gbp_git_buffer_change_monitor_queue_update (self, FAST);

  IDE_EXIT;
}

static void
foreach_cb (gpointer data,
            gpointer user_data)
{
  const LineEntry *entry = data;
  struct {
    IdeBufferChangeMonitorForeachFunc func;
    gpointer user_data;
  } *state = user_data;
  IdeBufferLineChange change = 0;

  if (entry->mark & LINE_MARK_ADDED)
    change |= IDE_BUFFER_LINE_CHANGE_ADDED;

  if (entry->mark & LINE_MARK_REMOVED)
    change |= IDE_BUFFER_LINE_CHANGE_DELETED;

  if (entry->mark & LINE_MARK_PREVIOUS_REMOVED)
    change |= IDE_BUFFER_LINE_CHANGE_PREVIOUS_DELETED;

  if (entry->mark & LINE_MARK_CHANGED)
    change |= IDE_BUFFER_LINE_CHANGE_CHANGED;

  state->func (entry->line, change, state->user_data);
}

static void
gbp_git_buffer_change_monitor_foreach_change (IdeBufferChangeMonitor            *monitor,
                                              guint                              begin_line,
                                              guint                              end_line,
                                              IdeBufferChangeMonitorForeachFunc  callback,
                                              gpointer                           user_data)
{
  GbpGitBufferChangeMonitor *self = (GbpGitBufferChangeMonitor *)monitor;
  struct {
    IdeBufferChangeMonitorForeachFunc func;
    gpointer user_data;
  } state = { callback, user_data };

  g_assert (GBP_IS_GIT_BUFFER_CHANGE_MONITOR (self));
  g_assert (callback != NULL);

  if (end_line == G_MAXUINT)
    end_line--;

  if (self->cache == NULL)
    {
      guint change;

      if (self->not_found)
        change = IDE_BUFFER_LINE_CHANGE_ADDED;
      else
        change = IDE_BUFFER_LINE_CHANGE_NONE;

      for (guint i = begin_line; i < end_line; i++)
        callback (i, change, user_data);

      return;
    }

  line_cache_foreach_in_range (self->cache, begin_line, end_line, foreach_cb, &state);
}

static IdeBufferLineChange
gbp_git_buffer_change_monitor_get_change (IdeBufferChangeMonitor *monitor,
                                          guint                   line)
{
  GbpGitBufferChangeMonitor *self = (GbpGitBufferChangeMonitor *)monitor;
  guint mark;

  g_assert (GBP_IS_GIT_BUFFER_CHANGE_MONITOR (self));

  if (self->cache == NULL)
    return IDE_BUFFER_LINE_CHANGE_ADDED;

  mark = line_cache_get_mark (self->cache, line + 1);

  if (mark & LINE_MARK_ADDED)
    return IDE_BUFFER_LINE_CHANGE_ADDED;
  else if (mark & LINE_MARK_REMOVED)
    return IDE_BUFFER_LINE_CHANGE_DELETED;
  else if (mark & LINE_MARK_CHANGED)
    return IDE_BUFFER_LINE_CHANGE_CHANGED;
  else
    return IDE_BUFFER_LINE_CHANGE_NONE;
}

static void
vcs_changed_cb (GbpGitBufferChangeMonitor *self,
                IdeVcs                    *vcs)
{
  g_assert (IDE_IS_BUFFER_CHANGE_MONITOR (self));
  g_assert (IDE_IS_VCS (vcs));

  gbp_git_buffer_change_monitor_queue_update (self, FAST);
}

static void
gbp_git_buffer_change_monitor_class_init (GbpGitBufferChangeMonitorClass *klass)
{
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);
  IdeBufferChangeMonitorClass *monitor_class = IDE_BUFFER_CHANGE_MONITOR_CLASS (klass);

  monitor_class->load = gbp_git_buffer_change_monitor_load;
  monitor_class->reload = gbp_git_buffer_change_monitor_reload;
  monitor_class->get_change = gbp_git_buffer_change_monitor_get_change;
  monitor_class->foreach_change = gbp_git_buffer_change_monitor_foreach_change;

  i_object_class->destroy = gbp_git_buffer_change_monitor_destroy;
}

static void
gbp_git_buffer_change_monitor_init (GbpGitBufferChangeMonitor *self)
{
  self->vcs_signals = g_signal_group_new (IDE_TYPE_VCS);
  g_signal_group_connect_object (self->vcs_signals,
                                 "changed",
                                 G_CALLBACK (vcs_changed_cb),
                                 self,
                                 G_CONNECT_SWAPPED | G_CONNECT_AFTER);
}

IdeBufferChangeMonitor *
gbp_git_buffer_change_monitor_new (IdeBuffer         *buffer,
                                   IpcGitRepository  *repository,
                                   GFile             *file,
                                   GCancellable      *cancellable,
                                   GError           **error)
{
  GbpGitBufferChangeMonitor *ret;
  g_autoptr(IpcGitChangeMonitor) proxy = NULL;
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(GFile) workdir = NULL;
  g_autofree gchar *relative_path = NULL;
  g_autofree gchar *obj_path = NULL;
  GDBusConnection *connection;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_BUFFER (buffer), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), NULL);

  context = ide_buffer_ref_context (buffer);
  workdir = ide_context_ref_workdir (context);

  if (!g_file_has_prefix (file, workdir))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_SUPPORTED,
                   _("Cannot monitor files outside the working directory"));
      return NULL;
    }

  relative_path = g_file_get_relative_path (workdir, file);

  if (!ipc_git_repository_call_create_change_monitor_sync (repository,
                                                           relative_path,
                                                           &obj_path,
                                                           cancellable,
                                                           error))
    return NULL;

  connection = g_dbus_proxy_get_connection (G_DBUS_PROXY (repository));

  if (!(proxy = ipc_git_change_monitor_proxy_new_sync (connection,
                                                       G_DBUS_PROXY_FLAGS_NONE,
                                                       NULL,
                                                       obj_path,
                                                       cancellable,
                                                       error)))
    return NULL;

  ret = g_object_new (GBP_TYPE_GIT_BUFFER_CHANGE_MONITOR,
                      "buffer", buffer,
                      NULL);
  ret->proxy = g_steal_pointer (&proxy);

  return IDE_BUFFER_CHANGE_MONITOR (g_steal_pointer (&ret));
}

static void
gbp_git_buffer_change_monitor_wait_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  IpcGitChangeMonitor *proxy = (IpcGitChangeMonitor *)object;
  g_autoptr(GVariant) changes = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  GbpGitBufferChangeMonitor *self;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IPC_IS_GIT_CHANGE_MONITOR (proxy));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);

  if (!ipc_git_change_monitor_call_list_changes_finish (proxy, &changes, result, &error))
    {
      g_clear_pointer (&self->cache, line_cache_free);
      self->not_found = TRUE;

      if (g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_FILE_NOT_FOUND))
        ide_task_return_boolean (task, TRUE);
      else
        ide_task_return_error (task, g_steal_pointer (&error));
    }
  else
    {
      g_clear_pointer (&self->cache, line_cache_free);
      self->not_found = FALSE;
      self->cache = line_cache_new_from_variant (changes);
      ide_buffer_change_monitor_emit_changed (IDE_BUFFER_CHANGE_MONITOR (self));
      ide_task_return_boolean (task, TRUE);
    }
}

void
gbp_git_buffer_change_monitor_wait_async (GbpGitBufferChangeMonitor *self,
                                          GCancellable              *cancellable,
                                          GAsyncReadyCallback        callback,
                                          gpointer                   user_data)
{
  g_autoptr(IdeTask) task = NULL;
  IdeBuffer *buffer;
  guint change_count;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (GBP_IS_GIT_BUFFER_CHANGE_MONITOR (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_git_buffer_change_monitor_wait_async);

  if (ide_task_return_error_if_cancelled (task))
    return;

  buffer = ide_buffer_change_monitor_get_buffer (IDE_BUFFER_CHANGE_MONITOR (self));
  change_count = ide_buffer_get_change_count (buffer);

  /* Update the peer of buffer contents immediately in-case it does
   * not yet have teh newest version.
   */
  if (change_count != self->last_change_count)
    {
      g_autoptr(GBytes) bytes = ide_buffer_dup_content (buffer);

      self->last_change_count = change_count;
      ipc_git_change_monitor_call_update_content (self->proxy,
                                                  (const gchar *)g_bytes_get_data (bytes, NULL),
                                                  NULL, NULL, NULL);
    }

  ipc_git_change_monitor_call_list_changes (self->proxy,
                                            cancellable,
                                            gbp_git_buffer_change_monitor_wait_cb,
                                            g_steal_pointer (&task));
}

gboolean
gbp_git_buffer_change_monitor_wait_finish (GbpGitBufferChangeMonitor  *self,
                                           GAsyncResult               *result,
                                           GError                    **error)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (GBP_IS_GIT_BUFFER_CHANGE_MONITOR (self), FALSE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}
