/* ide-git-remote-callbacks.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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
 */

#include <glib/gi18n.h>

#include "egg-animation.h"
#include "ide-debug.h"
#include "ide-git-remote-callbacks.h"
#include "ide-macros.h"
#include "ide-progress.h"

#define ANIMATION_DURATION_MSEC 250

struct _IdeGitRemoteCallbacks
{
  GgitRemoteCallbacks  parent_instance;

  EggAnimation        *animation;
  IdeProgress         *progress;
  gdouble              fraction;
};

G_DEFINE_TYPE (IdeGitRemoteCallbacks, ide_git_remote_callbacks, GGIT_TYPE_REMOTE_CALLBACKS)

enum {
  PROP_0,
  PROP_FRACTION,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

GgitRemoteCallbacks *
ide_git_remote_callbacks_new (void)
{
  return g_object_new (IDE_TYPE_GIT_REMOTE_CALLBACKS, NULL);
}

/**
 * ide_git_remote_callbacks_get_progress:
 *
 * Gets the #IdeProgress for the operation.
 *
 * Returns: (transfer none): An #IdeProgress.
 */
IdeProgress *
ide_git_remote_callbacks_get_progress (IdeGitRemoteCallbacks *self)
{
  g_return_val_if_fail (IDE_IS_GIT_REMOTE_CALLBACKS (self), NULL);

  return self->progress;
}

/**
 * ide_git_remote_callbacks_get_fraction:
 *
 * Gets the fraction of the current operation. This should typically be bound using
 * g_object_bind_property() to GtkProgressBar:fraction or similar progress widget.
 *
 * Returns: The operation completion percentage, as a fraction between 0 and 1.
 */
gdouble
ide_git_remote_callbacks_get_fraction (IdeGitRemoteCallbacks *self)
{
  g_return_val_if_fail (IDE_IS_GIT_REMOTE_CALLBACKS (self), 0.0);

  return self->fraction;
}

static gboolean
ide_git_remote_callbacks__notify_fraction_cb (gpointer data)
{
  g_autoptr(IdeGitRemoteCallbacks) self = data;
  EggAnimation *animation;

  g_assert (IDE_IS_GIT_REMOTE_CALLBACKS (self));

  if ((animation = self->animation))
    {
      ide_clear_weak_pointer (&self->animation);
      egg_animation_stop (animation);
    }

  animation = egg_object_animate (self->progress,
                                  EGG_ANIMATION_EASE_IN_OUT_QUAD,
                                  ANIMATION_DURATION_MSEC,
                                  NULL,
                                  "fraction", self->fraction,
                                  NULL);
  ide_set_weak_pointer (&self->animation, animation);

  g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_FRACTION]);

  return G_SOURCE_REMOVE;
}

static void
ide_git_remote_callbacks_real_transfer_progress (GgitRemoteCallbacks  *callbacks,
                                                 GgitTransferProgress *stats)
{
  IdeGitRemoteCallbacks *self = (IdeGitRemoteCallbacks *)callbacks;
  guint total;
  guint received;

  g_assert (IDE_IS_GIT_REMOTE_CALLBACKS (self));
  g_assert (stats != NULL);

  total = ggit_transfer_progress_get_total_objects (stats);
  received = ggit_transfer_progress_get_received_objects (stats);
  if (total == 0)
    return;

  self->fraction = (gdouble)received / (gdouble)total;

  /* Emit notify::fraction from the gtk+ main loop */
  g_timeout_add (0, ide_git_remote_callbacks__notify_fraction_cb, g_object_ref (self));
}

static GgitCred *
ide_git_remote_callbacks_real_credentials (GgitRemoteCallbacks  *callbacks,
                                           const gchar          *url,
                                           const gchar          *username_from_url,
                                           GgitCredtype          allowed_types,
                                           GError              **error)
{
  GgitCred *ret = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_GIT_REMOTE_CALLBACKS (callbacks));
  g_assert (url != NULL);

  IDE_TRACE_MSG ("username=%s url=%s", username_from_url ?: "", url);

  if ((allowed_types & GGIT_CREDTYPE_SSH_KEY) != 0)
    {
      GgitCredSshKeyFromAgent *cred;

      cred = ggit_cred_ssh_key_from_agent_new (username_from_url, error);
      ret = GGIT_CRED (cred);
    }

  if ((allowed_types & GGIT_CREDTYPE_SSH_INTERACTIVE) != 0)
    {
      GgitCredSshInteractive *cred;

      cred = ggit_cred_ssh_interactive_new (username_from_url, error);
      ret = GGIT_CRED (cred);
    }

  IDE_RETURN (ret);
}

static void
ide_git_remote_callbacks_finalize (GObject *object)
{
  IdeGitRemoteCallbacks *self = (IdeGitRemoteCallbacks *)object;

  g_clear_object (&self->progress);

  G_OBJECT_CLASS (ide_git_remote_callbacks_parent_class)->finalize (object);
}

static void
ide_git_remote_callbacks_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  IdeGitRemoteCallbacks *self = IDE_GIT_REMOTE_CALLBACKS (object);

  switch (prop_id)
    {
    case PROP_FRACTION:
      g_value_set_double (value, ide_git_remote_callbacks_get_fraction (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_git_remote_callbacks_class_init (IdeGitRemoteCallbacksClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GgitRemoteCallbacksClass *callbacks_class = GGIT_REMOTE_CALLBACKS_CLASS (klass);

  object_class->finalize = ide_git_remote_callbacks_finalize;
  object_class->get_property = ide_git_remote_callbacks_get_property;

  callbacks_class->transfer_progress = ide_git_remote_callbacks_real_transfer_progress;
  callbacks_class->credentials = ide_git_remote_callbacks_real_credentials;

  gParamSpecs [PROP_FRACTION] =
    g_param_spec_double ("fraction",
                         _("Fraction"),
                         _("A fraction containing the operation progress."),
                         0,
                         1.0,
                         0.0,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);
}

static void
ide_git_remote_callbacks_init (IdeGitRemoteCallbacks *self)
{
  self->progress = g_object_new (IDE_TYPE_PROGRESS, NULL);
}
