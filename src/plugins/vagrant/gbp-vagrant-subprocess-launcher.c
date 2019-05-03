/* gbp-vagrant-subprocess-launcher.c
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

#define G_LOG_DOMAIN "gbp-vagrant-subprocess-launcher"

#include "config.h"

#include "gbp-vagrant-subprocess-launcher.h"

struct _GbpVagrantSubprocessLauncher
{
  GObject parent_instance;
  gchar *dir;
};

G_DEFINE_TYPE (GbpVagrantSubprocessLauncher, gbp_vagrant_subprocess_launcher, IDE_TYPE_SUBPROCESS_LAUNCHER)

static IdeSubprocess *
gbp_vagrant_subprocess_launcher_spawn (IdeSubprocessLauncher  *launcher,
                                       GCancellable           *cancellable,
                                       GError                **error)
{
  GbpVagrantSubprocessLauncher *self = (GbpVagrantSubprocessLauncher *)launcher;
  const gchar * const *argv;
  const gchar *cwd;

  g_assert (GBP_IS_VAGRANT_SUBPROCESS_LAUNCHER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  argv = ide_subprocess_launcher_get_argv (launcher);

  for (guint i = 0; argv[i] != NULL; i++)
    {
      if (ide_str_equal (argv[i], GBP_VAGRANT_SUBPROCESS_LAUNCHER_C_OPT))
        {
          ide_subprocess_launcher_replace_argv (launcher, i, "-c");
          ide_subprocess_launcher_join_args_for_sh_c (launcher, i + 1);
        }
    }

  /* TODO: We have to "cd some-dir/; before our other commands */

  /* Ignore any CWD, since we have to run from the project tree */
  cwd = ide_subprocess_launcher_get_cwd (launcher);
  if (!g_str_has_prefix (cwd, self->dir))
    ide_subprocess_launcher_set_cwd (launcher, self->dir);

  return IDE_SUBPROCESS_LAUNCHER_CLASS (gbp_vagrant_subprocess_launcher_parent_class)->spawn (launcher, cancellable, error);
}

static void
gbp_vagrant_subprocess_launcher_finalize (GObject *object)
{
  G_OBJECT_CLASS (gbp_vagrant_subprocess_launcher_parent_class)->finalize (object);
}

static void
gbp_vagrant_subprocess_launcher_class_init (GbpVagrantSubprocessLauncherClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeSubprocessLauncherClass *launcher_class = IDE_SUBPROCESS_LAUNCHER_CLASS (klass);

  launcher_class->spawn = gbp_vagrant_subprocess_launcher_spawn;

  object_class->finalize = gbp_vagrant_subprocess_launcher_finalize;
}

static void
gbp_vagrant_subprocess_launcher_init (GbpVagrantSubprocessLauncher *self)
{
}

IdeSubprocessLauncher *
gbp_vagrant_subprocess_launcher_new (const gchar *dir)
{
  GbpVagrantSubprocessLauncher *self;

  self = g_object_new (GBP_TYPE_VAGRANT_SUBPROCESS_LAUNCHER,
                       "cwd", dir,
                       NULL);
  self->dir = g_strdup (dir);

  return IDE_SUBPROCESS_LAUNCHER (g_steal_pointer (&self));
}
