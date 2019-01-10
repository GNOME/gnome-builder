/* gbp-git-remote-callbacks.c
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "gbp-git-remote-callbacks"

#include <dazzle.h>
#include <glib/gi18n.h>

#include "gbp-git-remote-callbacks.h"

#define ANIMATION_DURATION_MSEC 250

struct _GbpGitRemoteCallbacks
{
  GgitRemoteCallbacks  parent_instance;

  IdeNotification     *progress;
  GString             *body;
  GgitCredtype         tried;
  guint                cancelled : 1;
};

G_DEFINE_TYPE (GbpGitRemoteCallbacks, gbp_git_remote_callbacks, GGIT_TYPE_REMOTE_CALLBACKS)

enum {
  PROP_0,
  PROP_PROGRESS,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

GgitRemoteCallbacks *
gbp_git_remote_callbacks_new (IdeNotification *progress)
{
  g_return_val_if_fail (IDE_IS_NOTIFICATION (progress), NULL);

  return g_object_new (GBP_TYPE_GIT_REMOTE_CALLBACKS,
                       "progress", progress,
                       NULL);
}

/**
 * gbp_git_remote_callbacks_get_progress:
 *
 * Gets the #IdeNotification for the operation.
 *
 * Returns: (transfer none): An #IdeNotification.
 *
 * Since: 3.32
 */
IdeNotification *
gbp_git_remote_callbacks_get_progress (GbpGitRemoteCallbacks *self)
{
  g_return_val_if_fail (GBP_IS_GIT_REMOTE_CALLBACKS (self), NULL);

  return self->progress;
}

static void
gbp_git_remote_callbacks_real_progress (GgitRemoteCallbacks *callbacks,
                                        const gchar         *message)
{
  GbpGitRemoteCallbacks *self = (GbpGitRemoteCallbacks *)callbacks;

  g_assert (GBP_IS_GIT_REMOTE_CALLBACKS (self));

  if (self->body == NULL)
    self->body = g_string_new (message);
  else
    g_string_append (self->body, message);

  ide_notification_set_body (self->progress, self->body->str);
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

  ide_notification_set_progress (self->progress, (gdouble)received / (gdouble)total);
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
    }

  if ((allowed_types & GGIT_CREDTYPE_SSH_INTERACTIVE) != 0)
    {
      GgitCredSshInteractive *cred;

      cred = ggit_cred_ssh_interactive_new (username_from_url, error);
      ret = GGIT_CRED (cred);
      self->tried |= GGIT_CREDTYPE_SSH_INTERACTIVE;
    }

  if (ret == NULL)
    g_set_error (error,
                 G_IO_ERROR,
                 G_IO_ERROR_NOT_SUPPORTED,
                 _("Builder failed to provide appropriate credentials when cloning repository."));

  IDE_RETURN (ret);
}

static void
gbp_git_remote_callbacks_finalize (GObject *object)
{
  GbpGitRemoteCallbacks *self = (GbpGitRemoteCallbacks *)object;

  g_clear_object (&self->progress);

  g_string_free (self->body, TRUE);
  self->body = NULL;

  G_OBJECT_CLASS (gbp_git_remote_callbacks_parent_class)->finalize (object);
}

static void
gbp_git_remote_callbacks_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  GbpGitRemoteCallbacks *self = GBP_GIT_REMOTE_CALLBACKS (object);

  switch (prop_id)
    {
    case PROP_PROGRESS:
      g_value_set_object (value, gbp_git_remote_callbacks_get_progress (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_git_remote_callbacks_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  GbpGitRemoteCallbacks *self = GBP_GIT_REMOTE_CALLBACKS (object);

  switch (prop_id)
    {
    case PROP_PROGRESS:
      g_clear_object (&self->progress);
      self->progress = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_git_remote_callbacks_class_init (GbpGitRemoteCallbacksClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GgitRemoteCallbacksClass *callbacks_class = GGIT_REMOTE_CALLBACKS_CLASS (klass);

  object_class->finalize = gbp_git_remote_callbacks_finalize;
  object_class->get_property = gbp_git_remote_callbacks_get_property;
  object_class->set_property = gbp_git_remote_callbacks_set_property;

  callbacks_class->transfer_progress = gbp_git_remote_callbacks_real_transfer_progress;
  callbacks_class->progress = gbp_git_remote_callbacks_real_progress;
  callbacks_class->credentials = gbp_git_remote_callbacks_real_credentials;

  properties [PROP_PROGRESS] =
    g_param_spec_object ("progress",
                         "Progress",
                         "An IdeNotification instance containing the operation progress.",
                         IDE_TYPE_NOTIFICATION,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
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
