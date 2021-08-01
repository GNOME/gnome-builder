/* gbp-git-progress.c
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

#define G_LOG_DOMAIN "gbp-git-progress"

#include "gbp-git-progress.h"

struct _GbpGitProgress
{
  IpcGitProgressSkeleton parent_instance;
  IdeNotification *notif;
  guint withdraw : 1;
};

G_DEFINE_FINAL_TYPE (GbpGitProgress, gbp_git_progress, IPC_TYPE_GIT_PROGRESS_SKELETON)

IpcGitProgress *
gbp_git_progress_new (GDBusConnection  *connection,
                      IdeNotification  *notif,
                      GCancellable     *cancellable,
                      GError          **error)
{
  g_autofree gchar *guid = NULL;
  g_autofree gchar *path = NULL;
  g_autoptr(GbpGitProgress) ret = NULL;

  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), NULL);
  g_return_val_if_fail (!notif || IDE_IS_NOTIFICATION (notif), NULL);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), NULL);

  ret = g_object_new (GBP_TYPE_GIT_PROGRESS, NULL);

  if (notif != NULL)
    {
      ret->notif = g_object_ref (notif);
      ide_notification_set_has_progress (notif, TRUE);
      ide_notification_set_icon_name (notif, "builder-vcs-git-symbolic");
    }

  guid = g_dbus_generate_guid ();
  path = g_strdup_printf ("/org/gnome/Builder/Git/Progress/%s", guid);

  if (g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (ret),
                                        connection,
                                        path,
                                        error))
    {
      if (notif != NULL)
        {
          g_object_bind_property (ret, "fraction", notif, "progress", 0);
          g_object_bind_property (ret, "message", notif, "body", 0);
        }

      return IPC_GIT_PROGRESS (g_steal_pointer (&ret));
    }

  return NULL;
}

static void
gbp_git_progress_finalize (GObject *object)
{
  GbpGitProgress *self = (GbpGitProgress *)object;

  if (self->notif)
    {
      if (self->withdraw)
        ide_notification_withdraw (self->notif);
      g_clear_object (&self->notif);
    }

  G_OBJECT_CLASS (gbp_git_progress_parent_class)->finalize (object);
}

static void
gbp_git_progress_class_init (GbpGitProgressClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_git_progress_finalize;
}

static void
gbp_git_progress_init (GbpGitProgress *self)
{
}

void
gbp_git_progress_set_withdraw (GbpGitProgress *self,
                               gboolean        withdraw)
{
  g_return_if_fail (GBP_IS_GIT_PROGRESS (self));

  self->withdraw = !!withdraw;
}
