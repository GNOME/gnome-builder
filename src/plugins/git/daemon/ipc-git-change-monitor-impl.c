/* ipc-git-change-monitor-impl.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ipc-git-change-monitor-impl"

#include <glib/gi18n.h>

#include "ipc-git-change-monitor-impl.h"
#include "line-cache.h"

/* Some code from this file is loosely based around the git-diff
 * plugin from Atom. Namely, API usage for iterating through hunks
 * containing changes. It's license is provided below.
 */

/*
 * Copyright (c) 2014 GitHub Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

struct _IpcGitChangeMonitorImpl
{
  IpcGitChangeMonitorSkeleton  parent;
  gchar                       *path;
  GgitRepository              *repository;
  GBytes                      *contents;
  GgitObject                  *blob;
};

typedef struct
{
  gint old_start;
  gint old_lines;
  gint new_start;
  gint new_lines;
} Range;

static gint
diff_hunk_cb (GgitDiffDelta *delta,
              GgitDiffHunk  *hunk,
              gpointer       user_data)
{
  GArray *ranges = user_data;
  Range range;

  g_assert (delta != NULL);
  g_assert (hunk != NULL);
  g_assert (ranges != NULL);

  range.old_start = ggit_diff_hunk_get_old_start (hunk);
  range.old_lines = ggit_diff_hunk_get_old_lines (hunk);
  range.new_start = ggit_diff_hunk_get_new_start (hunk);
  range.new_lines = ggit_diff_hunk_get_new_lines (hunk);

  g_array_append_val (ranges, range);

  return 0;
}

static GgitObject *
ipc_git_change_monitor_impl_load_blob (IpcGitChangeMonitorImpl  *self,
                                       GError                  **error)
{
  g_autofree gchar *path = NULL;
  GgitOId *entry_oid = NULL;
  GgitOId *oid = NULL;
  GgitObject *blob = NULL;
  GgitObject *commit = NULL;
  GgitRef *head = NULL;
  GgitTree *tree = NULL;
  GgitTreeEntry *entry = NULL;

  g_assert (IPC_IS_GIT_CHANGE_MONITOR_IMPL (self));

  if (self->blob != NULL)
    return g_object_ref (self->blob);

  g_object_get (self, "path", &path, NULL);

  if (self->repository == NULL || path == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   _("No repository to access file contents"));
      return NULL;
    }

  if (!(head = ggit_repository_get_head (self->repository, error)) ||
      !(oid = ggit_ref_get_target (head)) ||
      !(commit = ggit_repository_lookup (self->repository, oid, GGIT_TYPE_COMMIT, error)) ||
      !(tree = ggit_commit_get_tree (GGIT_COMMIT (commit))) ||
      !(entry = ggit_tree_get_by_path (tree, path, error)) ||
      !(entry_oid = ggit_tree_entry_get_id (entry)) ||
      !(blob = ggit_repository_lookup (self->repository, entry_oid, GGIT_TYPE_BLOB, error)))
    goto cleanup;

  g_set_object (&self->blob, blob);

cleanup:
  g_clear_pointer (&entry_oid, ggit_oid_free);
  g_clear_pointer (&entry, ggit_tree_entry_unref);
  g_clear_object (&tree);
  g_clear_object (&commit);
  g_clear_pointer (&oid, ggit_oid_free);
  g_clear_object (&head);

  return g_steal_pointer (&blob);
}

static gboolean
ipc_git_change_monitor_impl_handle_update_content (IpcGitChangeMonitor   *monitor,
                                                   GDBusMethodInvocation *invocation,
                                                   const gchar           *contents)
{
  IpcGitChangeMonitorImpl *self = (IpcGitChangeMonitorImpl *)monitor;

  g_assert (IPC_IS_GIT_CHANGE_MONITOR_IMPL (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (contents != NULL);

  /* Make a copy, but retain the trailing \0 */
  g_clear_pointer (&self->contents, g_bytes_unref);
  self->contents = g_bytes_new_take (g_strdup (contents), strlen (contents));

  ipc_git_change_monitor_complete_update_content (monitor, invocation);

  return TRUE;
}

static gboolean
ipc_git_change_monitor_impl_handle_list_changes (IpcGitChangeMonitor   *monitor,
                                                 GDBusMethodInvocation *invocation)
{
  IpcGitChangeMonitorImpl *self = (IpcGitChangeMonitorImpl *)monitor;
  g_autoptr(GgitDiffOptions) options = NULL;
  g_autoptr(GgitObject) blob = NULL;
  g_autoptr(GArray) ranges = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(LineCache) cache = NULL;
  g_autoptr(GVariant) ret = NULL;
  const guint8 *data;
  gsize len = 0;

  g_assert (IPC_IS_GIT_CHANGE_MONITOR_IMPL (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));

  if (self->contents == NULL)
    {
      g_set_error (&error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_INITIALIZED,
                   _("No contents have been set to diff"));
      goto gerror;
    }

  if (!(blob = ipc_git_change_monitor_impl_load_blob (self, &error)))
    goto gerror;

  ranges = g_array_new (FALSE, FALSE, sizeof (Range));
  options = ggit_diff_options_new ();
  ggit_diff_options_set_n_context_lines (options, 0);

  data = g_bytes_get_data (self->contents, &len);

  ggit_diff_blob_to_buffer (GGIT_BLOB (blob),
                            self->path,
                            data,
                            len,
                            self->path,
                            options,
                            NULL,         /* File Callback */
                            NULL,         /* Binary Callback */
                            diff_hunk_cb, /* Hunk Callback */
                            NULL,
                            ranges,
                            &error);

  if (error != NULL)
    goto gerror;

  cache = line_cache_new ();

  for (guint i = 0; i < ranges->len; i++)
    {
      const Range *range = &g_array_index (ranges, Range, i);
      gint start_line = range->new_start - 1;
      gint end_line = range->new_start + range->new_lines - 1;

      if (range->old_lines == 0 && range->new_lines > 0)
        {
          line_cache_mark_range (cache, start_line, end_line, LINE_MARK_ADDED);
        }
      else if (range->new_lines == 0 && range->old_lines > 0)
        {
          if (start_line < 0)
            line_cache_mark_range (cache, 0, 0, LINE_MARK_PREVIOUS_REMOVED);
          else
            line_cache_mark_range (cache, start_line + 1, start_line + 1, LINE_MARK_REMOVED);
        }
      else
        {
          line_cache_mark_range (cache, start_line, end_line, LINE_MARK_CHANGED);
        }
    }

  ret = line_cache_to_variant (cache);

  g_assert (ret != NULL);
  g_assert (g_variant_is_of_type (ret, G_VARIANT_TYPE ("au")));

gerror:
  g_assert (ret != NULL || error != NULL);

  if (g_error_matches (error, GGIT_ERROR, GIT_ENOTFOUND))
    {
      g_dbus_method_invocation_return_dbus_error (invocation,
                                                  "org.freedesktop.DBus.Error.FileNotFound",
                                                  "No such file");
      return TRUE;
    }

  if (error != NULL && error->domain != G_IO_ERROR)
    {
      g_autoptr(GError) wrapped = g_steal_pointer (&error);

      g_set_error (&error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   _("The operation failed. The original error was \"%s\""),
                   wrapped->message);
    }

  if (error != NULL)
    g_dbus_method_invocation_return_gerror (invocation, error);
  else
    ipc_git_change_monitor_complete_list_changes (monitor, invocation, ret);

  return TRUE;
}

static gboolean
ipc_git_change_monitor_impl_handle_close (IpcGitChangeMonitor   *monitor,
                                          GDBusMethodInvocation *invocation)
{
  g_assert (IPC_IS_GIT_CHANGE_MONITOR_IMPL (monitor));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));

  /* Repository will drop it's reference from the hashtable */
  ipc_git_change_monitor_emit_closed (monitor);

  g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (monitor));
  ipc_git_change_monitor_complete_close (monitor, invocation);

  return TRUE;
}

static void
git_change_monitor_iface_init (IpcGitChangeMonitorIface *iface)
{
  iface->handle_update_content = ipc_git_change_monitor_impl_handle_update_content;
  iface->handle_list_changes = ipc_git_change_monitor_impl_handle_list_changes;
  iface->handle_close = ipc_git_change_monitor_impl_handle_close;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (IpcGitChangeMonitorImpl, ipc_git_change_monitor_impl, IPC_TYPE_GIT_CHANGE_MONITOR_SKELETON,
                               G_IMPLEMENT_INTERFACE (IPC_TYPE_GIT_CHANGE_MONITOR, git_change_monitor_iface_init))

static void
ipc_git_change_monitor_impl_finalize (GObject *object)
{
  IpcGitChangeMonitorImpl *self = (IpcGitChangeMonitorImpl *)object;

  g_clear_object (&self->blob);
  g_clear_object (&self->repository);
  g_clear_pointer (&self->contents, g_bytes_unref);
  g_clear_pointer (&self->path, g_free);

  G_OBJECT_CLASS (ipc_git_change_monitor_impl_parent_class)->finalize (object);
}

static void
ipc_git_change_monitor_impl_class_init (IpcGitChangeMonitorImplClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ipc_git_change_monitor_impl_finalize;
}

static void
ipc_git_change_monitor_impl_init (IpcGitChangeMonitorImpl *self)
{
}

IpcGitChangeMonitor *
ipc_git_change_monitor_impl_new (GgitRepository *repository,
                                 const gchar    *path)
{
  IpcGitChangeMonitorImpl *ret;

  ret = g_object_new (IPC_TYPE_GIT_CHANGE_MONITOR_IMPL,
                      "path", path,
                      NULL);
  ret->repository = g_object_ref (repository);

  return IPC_GIT_CHANGE_MONITOR (g_steal_pointer (&ret));
}

void
ipc_git_change_monitor_impl_reset (IpcGitChangeMonitorImpl *self)
{
  g_return_if_fail (IPC_IS_GIT_CHANGE_MONITOR_IMPL (self));

  g_clear_object (&self->blob);
}
