/* gbp-git-buffer-addin.c
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

#define G_LOG_DOMAIN "gbp-git-buffer-addin"

#include "config.h"

#include <libide-vcs.h>

#include "gbp-git-buffer-addin.h"
#include "gbp-git-buffer-change-monitor.h"
#include "gbp-git-vcs.h"

struct _GbpGitBufferAddin
{
  GObject                 parent_instance;
  IdeBufferChangeMonitor *monitor;
};

static void
gbp_git_buffer_addin_file_loaded (IdeBufferAddin *addin,
                                  IdeBuffer      *buffer,
                                  GFile          *file)
{
  GbpGitBufferAddin *self = (GbpGitBufferAddin *)addin;
  g_autoptr(IdeBufferChangeMonitor) monitor = NULL;
  g_autoptr(IdeContext) context = NULL;
  IpcGitRepository *repository;
  IdeObjectBox *box;
  IdeVcs *vcs;

  g_assert (GBP_IS_GIT_BUFFER_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_FILE (file));

  if (!(context = ide_buffer_ref_context (buffer)) ||
      !(vcs = ide_context_peek_child_typed (context, IDE_TYPE_VCS)) ||
      !GBP_IS_GIT_VCS (vcs) ||
      !(repository = gbp_git_vcs_get_repository (GBP_GIT_VCS (vcs))) ||
      !(monitor = gbp_git_buffer_change_monitor_new (buffer, repository, file, NULL, NULL)))
    return;

  ide_clear_and_destroy_object (&self->monitor);
  self->monitor = g_steal_pointer (&monitor);

  box = ide_object_box_from_object (G_OBJECT (buffer));
  ide_object_append (IDE_OBJECT (box), IDE_OBJECT (self->monitor));
  ide_buffer_set_change_monitor (buffer, self->monitor);
}

static void
gbp_git_buffer_addin_unload (IdeBufferAddin *addin,
                             IdeBuffer      *buffer)
{
  GbpGitBufferAddin *self = (GbpGitBufferAddin *)addin;

  g_assert (GBP_IS_GIT_BUFFER_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  if (self->monitor != NULL)
    {
      ide_buffer_set_change_monitor (buffer, NULL);
      ide_clear_and_destroy_object (&self->monitor);
    }
}

static void
gbp_git_buffer_addin_settle_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  GbpGitBufferChangeMonitor *monitor = (GbpGitBufferChangeMonitor *)object;
  g_autoptr(IdeTask) task = user_data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GIT_BUFFER_CHANGE_MONITOR (monitor));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  gbp_git_buffer_change_monitor_wait_finish (monitor, result, NULL);
  ide_task_return_boolean (task, TRUE);
}

static void
gbp_git_buffer_addin_settle_async (IdeBufferAddin      *addin,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  GbpGitBufferAddin *self = (GbpGitBufferAddin *)addin;
  g_autoptr(IdeTask) task = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER_ADDIN (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_git_buffer_addin_settle_async);

  if (self->monitor == NULL)
    ide_task_return_boolean (task, TRUE);
  else
    gbp_git_buffer_change_monitor_wait_async (GBP_GIT_BUFFER_CHANGE_MONITOR (self->monitor),
                                              cancellable,
                                              gbp_git_buffer_addin_settle_cb,
                                              g_steal_pointer (&task));
}

static gboolean
gbp_git_buffer_addin_settle_finish (IdeBufferAddin  *addin,
                                    GAsyncResult    *result,
                                    GError         **error)
{
  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
buffer_addin_iface_init (IdeBufferAddinInterface *iface)
{
  iface->file_loaded = gbp_git_buffer_addin_file_loaded;
  iface->unload = gbp_git_buffer_addin_unload;
  iface->settle_async = gbp_git_buffer_addin_settle_async;
  iface->settle_finish = gbp_git_buffer_addin_settle_finish;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpGitBufferAddin, gbp_git_buffer_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_BUFFER_ADDIN, buffer_addin_iface_init))

static void
gbp_git_buffer_addin_class_init (GbpGitBufferAddinClass *klass)
{
}

static void
gbp_git_buffer_addin_init (GbpGitBufferAddin *self)
{
}
