/* gbp-podman-subprocess-launcher.c
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

#define G_LOG_DOMAIN "gbp-podman-subprocess-launcher"

#include "config.h"

#include "gbp-podman-subprocess-launcher.h"

struct _GbpPodmanSubprocessLauncher
{
  IdeSubprocessLauncher parent_instance;
  gchar *id;
};

enum {
  PROP_0,
  PROP_ID,
  N_PROPS
};

G_DEFINE_TYPE (GbpPodmanSubprocessLauncher, gbp_podman_subprocess_launcher, IDE_TYPE_SUBPROCESS_LAUNCHER)

static GParamSpec *properties [N_PROPS];

static void
copy_envvar (IdeSubprocessLauncher *launcher,
             guint                  position,
             const gchar           *key)
{
  const gchar *val;

  if ((val = g_getenv (key)))
    {
      g_autofree gchar *arg = g_strdup_printf ("--env=%s=%s", key, val);
      ide_subprocess_launcher_insert_argv (launcher, position, arg);
    }
}

static IdeSubprocess *
gbp_podman_subprocess_launcher_spawn (IdeSubprocessLauncher  *launcher,
                                      GCancellable           *cancellable,
                                      GError                **error)
{
  GbpPodmanSubprocessLauncher *self = (GbpPodmanSubprocessLauncher *)launcher;
  const gchar * const *argv;

  g_assert (GBP_IS_PODMAN_SUBPROCESS_LAUNCHER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (self->id != NULL);

  /* Override any plugin setting, we need to prefix "podman" from host */
  ide_subprocess_launcher_set_run_on_host (launcher, TRUE);

  argv = ide_subprocess_launcher_get_argv (launcher);

  if (!g_strv_contains (argv, "podman"))
    {
      const gchar * const *environ_;
      const gchar *cwd;
      guint i = 0;
      gint max_fd;

      ide_subprocess_launcher_insert_argv (launcher, i++, "podman");
      ide_subprocess_launcher_insert_argv (launcher, i++, "exec");
      ide_subprocess_launcher_insert_argv (launcher, i++, "--privileged");

      if (ide_subprocess_launcher_get_needs_tty (launcher))
        ide_subprocess_launcher_insert_argv (launcher, i++, "--tty");

      if ((cwd = ide_subprocess_launcher_get_cwd (launcher)))
        {
          g_autofree gchar *cwd_absolute = g_canonicalize_filename (cwd, NULL);

          ide_subprocess_launcher_insert_argv (launcher, i++, "--workdir");
          ide_subprocess_launcher_insert_argv (launcher, i++, cwd_absolute);
        }

      /* Determine how many FDs we need to preserve.
       *
       * From man podman-exec:
       *
       * Pass down to the process N additional file descriptors (in addition to
       * 0, 1, 2).  The total FDs will be 3+N.
       */
      if ((max_fd = ide_subprocess_launcher_get_max_fd (launcher)) > 2)
        {
          g_autofree gchar *max_fd_param = NULL;

          max_fd_param = g_strdup_printf ("--preserve-fds=%d", max_fd - 2);
          ide_subprocess_launcher_insert_argv (launcher, i++, max_fd_param);
        }

      if (!ide_subprocess_launcher_get_clear_env (launcher))
        {
          copy_envvar (launcher, i++, "COLORTERM");
          copy_envvar (launcher, i++, "DBUS_SESSION_BUS_ADDRESS");
          copy_envvar (launcher, i++, "DESKTOP_SESSION");
          copy_envvar (launcher, i++, "DISPLAY");
          copy_envvar (launcher, i++, "LANG");
          copy_envvar (launcher, i++, "SSH_AUTH_SOCK");
          copy_envvar (launcher, i++, "WAYLAND_DISPLAY");
          copy_envvar (launcher, i++, "XDG_CURRENT_DESKTOP");
          copy_envvar (launcher, i++, "XDG_SEAT");
          copy_envvar (launcher, i++, "XDG_SESSION_DESKTOP");
          copy_envvar (launcher, i++, "XDG_SESSION_ID");
          copy_envvar (launcher, i++, "XDG_SESSION_TYPE");
          copy_envvar (launcher, i++, "XDG_VTNR");
        }

      if ((environ_ = ide_subprocess_launcher_get_environ (launcher)))
        {
          for (guint j = 0; environ_[j]; j++)
            {
              ide_subprocess_launcher_insert_argv (launcher, i++, "--env");
              ide_subprocess_launcher_insert_argv (launcher, i++, environ_[j]);
            }

          ide_subprocess_launcher_set_environ (launcher, NULL);
        }

      ide_subprocess_launcher_insert_argv (launcher, i++, self->id);
    }

  return IDE_SUBPROCESS_LAUNCHER_CLASS (gbp_podman_subprocess_launcher_parent_class)->spawn (launcher, cancellable, error);
}

static void
gbp_podman_subprocess_launcher_finalize (GObject *object)
{
  GbpPodmanSubprocessLauncher *self = (GbpPodmanSubprocessLauncher *)object;

  g_clear_pointer (&self->id, g_free);

  G_OBJECT_CLASS (gbp_podman_subprocess_launcher_parent_class)->finalize (object);
}

static void
gbp_podman_subprocess_launcher_get_property (GObject    *object,
                                             guint       prop_id,
                                             GValue     *value,
                                             GParamSpec *pspec)
{
  GbpPodmanSubprocessLauncher *self = GBP_PODMAN_SUBPROCESS_LAUNCHER (object);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_set_string (value, self->id);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_podman_subprocess_launcher_set_property (GObject      *object,
                                             guint         prop_id,
                                             const GValue *value,
                                             GParamSpec   *pspec)
{
  GbpPodmanSubprocessLauncher *self = GBP_PODMAN_SUBPROCESS_LAUNCHER (object);

  switch (prop_id)
    {
    case PROP_ID:
      self->id = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_podman_subprocess_launcher_class_init (GbpPodmanSubprocessLauncherClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeSubprocessLauncherClass *launcher_class = IDE_SUBPROCESS_LAUNCHER_CLASS (klass);

  object_class->finalize = gbp_podman_subprocess_launcher_finalize;
  object_class->get_property = gbp_podman_subprocess_launcher_get_property;
  object_class->set_property = gbp_podman_subprocess_launcher_set_property;

  launcher_class->spawn = gbp_podman_subprocess_launcher_spawn;

  properties [PROP_ID] =
    g_param_spec_string ("id",
                         "Id",
                         "The identifier for the podman runtime",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_podman_subprocess_launcher_init (GbpPodmanSubprocessLauncher *self)
{
  ide_subprocess_launcher_set_run_on_host (IDE_SUBPROCESS_LAUNCHER (self), TRUE);
}
