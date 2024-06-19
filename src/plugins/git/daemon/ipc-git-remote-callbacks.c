/* gnome-builder-git.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#define G_LOG_DOMAIN "ipc-git-remote-callbacks"

#include <glib/gi18n.h>

#include "ipc-git-remote-callbacks.h"

struct _IpcGitRemoteCallbacks
{
  GgitRemoteCallbacks  parent_instance;
  IpcGitProgress      *progress;
  double               fraction;
  guint                queued_progress;
  GgitCredtype         tried;
  int                  pty_fd;
  guint                cancelled : 1;
  guint                fraction_changed : 1;
};

G_DEFINE_FINAL_TYPE (IpcGitRemoteCallbacks, ipc_git_remote_callbacks, GGIT_TYPE_REMOTE_CALLBACKS)

static gboolean
ipc_git_remote_callbacks_update_progress (gpointer data)
{
  IpcGitRemoteCallbacks *self = data;

  g_assert (IPC_IS_GIT_REMOTE_CALLBACKS (self));

  if (self->progress != NULL)
    {
      if (self->fraction_changed)
        ipc_git_progress_set_fraction (self->progress, self->fraction);
    }

  self->fraction_changed = FALSE;
  self->queued_progress = 0;

  return G_SOURCE_REMOVE;
}

static void
ipc_git_remote_callbacks_queue_progress (IpcGitRemoteCallbacks *self)
{
  if (self->queued_progress == 0)
    self->queued_progress = g_timeout_add (200, /* up to 5x per second */
                                           ipc_git_remote_callbacks_update_progress,
                                           self);
}

GgitRemoteCallbacks *
ipc_git_remote_callbacks_new (IpcGitProgress *progress,
                              int             pty_fd)
{
  IpcGitRemoteCallbacks *ret;

  ret = g_object_new (IPC_TYPE_GIT_REMOTE_CALLBACKS, NULL);
  ret->progress = progress ? g_object_ref (progress) : NULL;
  ret->pty_fd = pty_fd;

  return GGIT_REMOTE_CALLBACKS (g_steal_pointer (&ret));
}

static GgitCred *
ipc_git_remote_callbacks_real_credentials (GgitRemoteCallbacks  *callbacks,
                                           const gchar          *url,
                                           const gchar          *username_from_url,
                                           GgitCredtype          allowed_types,
                                           GError              **error)
{
  IpcGitRemoteCallbacks *self = (IpcGitRemoteCallbacks *)callbacks;
  GgitCred *ret = NULL;

  g_assert (IPC_IS_GIT_REMOTE_CALLBACKS (self));
  g_assert (url != NULL);

  g_debug ("username=%s url=%s", username_from_url ?: "", url);

  if (self->cancelled)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_CANCELLED,
                   _("The operation has been cancelled"));
      return NULL;
    }

  allowed_types &= ~self->tried;

  if ((allowed_types & GGIT_CREDTYPE_SSH_KEY) != 0)
    {
      GgitCredSshKeyFromAgent *cred;

      cred = ggit_cred_ssh_key_from_agent_new (username_from_url, error);
      ret = GGIT_CRED (cred);
      self->tried |= GGIT_CREDTYPE_SSH_KEY;

      if (ret != NULL)
        return g_steal_pointer (&ret);
    }

  if ((allowed_types & GGIT_CREDTYPE_SSH_INTERACTIVE) != 0)
    {
      GgitCredSshInteractive *cred;

      cred = ggit_cred_ssh_interactive_new (username_from_url, error);
      ret = GGIT_CRED (cred);
      self->tried |= GGIT_CREDTYPE_SSH_INTERACTIVE;

      if (ret != NULL)
        return g_steal_pointer (&ret);
    }

  g_set_error (error,
               G_IO_ERROR,
               G_IO_ERROR_NOT_SUPPORTED,
               _("Builder failed to provide appropriate credentials when cloning the repository."));

  return NULL;
}

static void
ipc_git_remote_callbacks_progress (GgitRemoteCallbacks *callbacks,
                                   const gchar         *message)
{
  IpcGitRemoteCallbacks *self = (IpcGitRemoteCallbacks *)callbacks;

  g_assert (IPC_IS_GIT_REMOTE_CALLBACKS (self));

  if (self->pty_fd != -1)
    write (self->pty_fd, message, strlen (message));
}

static void
ipc_git_remote_callbacks_transfer_progress (GgitRemoteCallbacks  *callbacks,
                                            GgitTransferProgress *stats)
{
  IpcGitRemoteCallbacks *self = (IpcGitRemoteCallbacks *)callbacks;

  g_assert (IPC_IS_GIT_REMOTE_CALLBACKS (self));

  if (self->progress != NULL)
    {
      gdouble fraction = 0.0;
      guint total = ggit_transfer_progress_get_total_objects (stats);
      guint acked = ggit_transfer_progress_get_received_objects (stats);

      if (total > 0)
        fraction = (gdouble)acked / (gdouble)total;

      if (fraction != self->fraction)
        {
          self->fraction = fraction;
          self->fraction_changed = TRUE;
          ipc_git_remote_callbacks_queue_progress (self);
        }
    }
}

static void
ipc_git_remote_callbacks_update_tips (GgitRemoteCallbacks *callbacks,
                                      const gchar         *refname,
                                      const GgitOId       *a,
                                      const GgitOId       *b)
{
}

static void
ipc_git_remote_callbacks_finalize (GObject *object)
{
  IpcGitRemoteCallbacks *self = (IpcGitRemoteCallbacks *)object;

  g_clear_object (&self->progress);
  g_clear_handle_id (&self->queued_progress, g_source_remove);

  self->pty_fd = -1;

  G_OBJECT_CLASS (ipc_git_remote_callbacks_parent_class)->finalize (object);
}

static void
ipc_git_remote_callbacks_class_init (IpcGitRemoteCallbacksClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GgitRemoteCallbacksClass *callbacks_class = GGIT_REMOTE_CALLBACKS_CLASS (klass);

  object_class->finalize = ipc_git_remote_callbacks_finalize;

  callbacks_class->credentials = ipc_git_remote_callbacks_real_credentials;
  callbacks_class->progress = ipc_git_remote_callbacks_progress;
  callbacks_class->transfer_progress = ipc_git_remote_callbacks_transfer_progress;
  callbacks_class->update_tips = ipc_git_remote_callbacks_update_tips;
}

static void
ipc_git_remote_callbacks_init (IpcGitRemoteCallbacks *self)
{
  self->pty_fd = -1;
}

/**
 * ipc_git_remote_callbacks_cancel:
 *
 * This function should be called when a clone was canceled so that we can
 * avoid dispatching more events.
 */
void
ipc_git_remote_callbacks_cancel (IpcGitRemoteCallbacks *self)
{
  g_return_if_fail (IPC_IS_GIT_REMOTE_CALLBACKS (self));

  self->cancelled  = TRUE;
}
