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

#define G_LOG_DOMAIN "gbp-git-remote-callbacks"

#include <dazzle.h>
#include <glib/gi18n.h>

#include "gbp-git-remote-callbacks.h"

#define ANIMATION_DURATION_MSEC 250

struct _GbpGitRemoteCallbacks
{
  GgitRemoteCallbacks    parent_instance;

  GMutex                 mutex;
  GString               *body;
  gdouble                progress;
  guint                  status_source;

  /* bitflags of what we've tried */
  GgitCredtype           tried;

  guint                  cancelled : 1;
};

G_DEFINE_TYPE (GbpGitRemoteCallbacks, gbp_git_remote_callbacks, GGIT_TYPE_REMOTE_CALLBACKS)

enum {
  STATUS,
  N_SIGNALS
};

static guint signals [N_SIGNALS];

GgitRemoteCallbacks *
gbp_git_remote_callbacks_new (void)
{
  return g_object_new (GBP_TYPE_GIT_REMOTE_CALLBACKS, NULL);
}

static gboolean
gbp_git_remote_callbacks_do_emit_status (GbpGitRemoteCallbacks *self)
{
  g_autofree gchar *message = NULL;
  gdouble progress = 0.0;

  g_assert (GBP_IS_GIT_REMOTE_CALLBACKS (self));

  g_mutex_lock (&self->mutex);

  self->status_source = 0;

  progress = self->progress;

  if (self->body->len > 1)
    {
      const gchar *endptr;

      endptr = &self->body->str[self->body->len - 1];

      if (*endptr == '\n')
        endptr--;

      while (endptr >= self->body->str)
        {
          if (*endptr == '\n' || *endptr == '\r')
            {
              message = g_strdup (endptr + 1);
              break;
            }

          endptr--;
        }
    }

  g_mutex_unlock (&self->mutex);

  g_signal_emit (self, signals [STATUS], 0, message, progress);

  return G_SOURCE_REMOVE;
}

static void
gbp_git_remote_callbacks_emit_status (GbpGitRemoteCallbacks *self)
{
  g_assert (GBP_IS_GIT_REMOTE_CALLBACKS (self));

  g_mutex_lock (&self->mutex);
  if (self->status_source == 0)
    self->status_source = g_idle_add_full (G_PRIORITY_HIGH,
                                           (GSourceFunc)gbp_git_remote_callbacks_do_emit_status,
                                           g_object_ref (self),
                                           g_object_unref);
  g_mutex_unlock (&self->mutex);
}

static void
gbp_git_remote_callbacks_real_progress (GgitRemoteCallbacks *callbacks,
                                        const gchar         *message)
{
  GbpGitRemoteCallbacks *self = (GbpGitRemoteCallbacks *)callbacks;

  g_assert (GBP_IS_GIT_REMOTE_CALLBACKS (self));

  g_mutex_lock (&self->mutex);
  if (self->body == NULL)
    self->body = g_string_new (message);
  else
    g_string_append (self->body, message);
  g_mutex_unlock (&self->mutex);

  gbp_git_remote_callbacks_emit_status (self);
}

static void
gbp_git_remote_callbacks_real_transfer_progress (GgitRemoteCallbacks  *callbacks,
                                                 GgitTransferProgress *stats)
{
  GbpGitRemoteCallbacks *self = (GbpGitRemoteCallbacks *)callbacks;
  guint total;
  guint received;

  g_assert (GBP_IS_GIT_REMOTE_CALLBACKS (self));
  g_assert (stats != NULL);

  if (self->cancelled)
    return;

  total = ggit_transfer_progress_get_total_objects (stats);
  received = ggit_transfer_progress_get_received_objects (stats);
  if (total == 0)
    return;

  g_mutex_lock (&self->mutex);
  self->progress = (gdouble)received / (gdouble)total;
  g_mutex_unlock (&self->mutex);

  gbp_git_remote_callbacks_emit_status (self);
}

static GgitCred *
gbp_git_remote_callbacks_real_credentials (GgitRemoteCallbacks  *callbacks,
                                           const gchar          *url,
                                           const gchar          *username_from_url,
                                           GgitCredtype          allowed_types,
                                           GError              **error)
{
  GbpGitRemoteCallbacks *self = (GbpGitRemoteCallbacks *)callbacks;
  GgitCred *ret = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_GIT_REMOTE_CALLBACKS (self));
  g_assert (url != NULL);

  IDE_TRACE_MSG ("username=%s url=%s", username_from_url ?: "", url);

  if (self->cancelled)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_CANCELLED,
                   "The operation has been canceled");
      IDE_RETURN (NULL);
    }

  allowed_types &= ~self->tried;

  if ((allowed_types & GGIT_CREDTYPE_SSH_KEY) != 0)
    {
      GgitCredSshKeyFromAgent *cred;

      cred = ggit_cred_ssh_key_from_agent_new (username_from_url, error);
      ret = GGIT_CRED (cred);
      self->tried |= GGIT_CREDTYPE_SSH_KEY;

      if (ret != NULL)
        IDE_RETURN (ret);
    }

  if ((allowed_types & GGIT_CREDTYPE_SSH_INTERACTIVE) != 0)
    {
      GgitCredSshInteractive *cred;

      cred = ggit_cred_ssh_interactive_new (username_from_url, error);
      ret = GGIT_CRED (cred);
      self->tried |= GGIT_CREDTYPE_SSH_INTERACTIVE;

      if (ret != NULL)
        IDE_RETURN (ret);
    }

  g_set_error (error,
               G_IO_ERROR,
               G_IO_ERROR_NOT_SUPPORTED,
               _("Builder failed to provide appropriate credentials when cloning repository."));

  IDE_RETURN (NULL);
}

static void
gbp_git_remote_callbacks_finalize (GObject *object)
{
  GbpGitRemoteCallbacks *self = (GbpGitRemoteCallbacks *)object;

  g_clear_object (&self->progress);

  if (self->body != NULL)
    {
      g_string_free (self->body, TRUE);
      self->body = NULL;
    }

  g_clear_handle_id (&self->status_source, g_source_remove);

  g_mutex_clear (&self->mutex);

  G_OBJECT_CLASS (gbp_git_remote_callbacks_parent_class)->finalize (object);
}

static void
gbp_git_remote_callbacks_class_init (GbpGitRemoteCallbacksClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GgitRemoteCallbacksClass *callbacks_class = GGIT_REMOTE_CALLBACKS_CLASS (klass);

  object_class->finalize = gbp_git_remote_callbacks_finalize;

  callbacks_class->transfer_progress = gbp_git_remote_callbacks_real_transfer_progress;
  callbacks_class->progress = gbp_git_remote_callbacks_real_progress;
  callbacks_class->credentials = gbp_git_remote_callbacks_real_credentials;

  /**
   * GbpGitRemoteCallbacks::status:
   * @self: a GbpGitRemoteCallbacks
   * @message: the status message string
   * @progress: the progress for the operation
   *
   * This signal is emitted when the progress or the status message changes
   * for the operation.
   *
   * Since: 3.34
   */
  signals [STATUS] =
    g_signal_new ("status",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE,
                  2,
                  G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
                  G_TYPE_DOUBLE);
}

static void
gbp_git_remote_callbacks_init (GbpGitRemoteCallbacks *self)
{
  g_mutex_init (&self->mutex);
}

/**
 * gbp_git_remote_callbacks_cancel:
 *
 * This function should be called when a clone was canceled so that we can
 * avoid dispatching more events.
 *
 * Since: 3.32
 */
void
gbp_git_remote_callbacks_cancel (GbpGitRemoteCallbacks *self)
{
  g_return_if_fail (GBP_IS_GIT_REMOTE_CALLBACKS (self));

  self->cancelled  = TRUE;
}
