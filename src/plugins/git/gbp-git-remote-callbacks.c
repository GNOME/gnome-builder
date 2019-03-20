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

#include <glib/gi18n.h>

#include "gbp-git-remote-callbacks.h"

struct _GbpGitRemoteCallbacks
{
  GgitRemoteCallbacks parent_instance;
  GgitCredtype        tried;
  guint               cancelled : 1;
};

G_DEFINE_TYPE (GbpGitRemoteCallbacks, gbp_git_remote_callbacks, GGIT_TYPE_REMOTE_CALLBACKS)

GgitRemoteCallbacks *
gbp_git_remote_callbacks_new (void)
{
  return g_object_new (GBP_TYPE_GIT_REMOTE_CALLBACKS, NULL);
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

  g_assert (GBP_IS_GIT_REMOTE_CALLBACKS (self));
  g_assert (url != NULL);

  g_debug ("username=%s url=%s", username_from_url ?: "", url);

  if (self->cancelled)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_CANCELLED,
                   "The operation has been canceled");
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
gbp_git_remote_callbacks_class_init (GbpGitRemoteCallbacksClass *klass)
{
  GgitRemoteCallbacksClass *callbacks_class = GGIT_REMOTE_CALLBACKS_CLASS (klass);

  callbacks_class->credentials = gbp_git_remote_callbacks_real_credentials;
}

static void
gbp_git_remote_callbacks_init (GbpGitRemoteCallbacks *self)
{
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
