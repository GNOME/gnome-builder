/* ide-host-subprocess-launcher.c
 *
 * Copyright (C) 2018 Corentin Noël <corentin.noel@collabora.com>
 * Copyright (C) 2018 Collabora Ltd.
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

#define G_LOG_DOMAIN "ide-host-subprocess-launcher"

#include <glib/gi18n.h>

#include "ide-host-subprocess-launcher.h"

struct _IdeHostSubprocessLauncher
{
  IdeSubprocessLauncher parent_instance;
};

G_DEFINE_TYPE (IdeHostSubprocessLauncher,
               ide_host_subprocess_launcher,
               IDE_TYPE_SUBPROCESS_LAUNCHER)

IdeHostSubprocessLauncher *
ide_host_subprocess_launcher_new (GSubprocessFlags flags)
{
  return g_object_new (IDE_TYPE_HOST_SUBPROCESS_LAUNCHER,
                       "flags", flags,
                       NULL);
}

static IdeSubprocess *
ide_hostsubprocess_launcher_spawn (IdeSubprocessLauncher  *self,
                                   GCancellable           *cancellable,
                                   GError                **error)
{
  gchar *argv = NULL;
  const gchar * const *args = NULL;
  const gchar * const *environ = NULL;
  GString *cmd = NULL;

  g_assert (IDE_IS_SUBPROCESS_LAUNCHER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  // don't prepend `sh -c` twice
  args = ide_subprocess_launcher_get_argv (self);
  if (g_strv_length ((gchar **)args) >= 2)
    {
      if (g_strcmp0 (args[0], "sh") == 0 && g_strcmp0 (args[1], "-c") == 0)
        {
          return IDE_SUBPROCESS_LAUNCHER_CLASS (ide_host_subprocess_launcher_parent_class)->spawn (self, cancellable, error);
        }
    }

  argv = ide_subprocess_launcher_pop_argv (self);
  cmd = g_string_new (argv);
  g_free (argv);

  while ((argv = ide_subprocess_launcher_pop_argv (self)) != NULL)
    {
      g_string_prepend (cmd, " ");
      g_string_prepend (cmd, argv);
      g_free (argv);
    }

  ide_subprocess_launcher_push_argv (self, "sh");
  ide_subprocess_launcher_push_argv (self, "-c");
  ide_subprocess_launcher_push_argv (self, g_string_free (cmd, FALSE));

  return IDE_SUBPROCESS_LAUNCHER_CLASS (ide_host_subprocess_launcher_parent_class)->spawn (self, cancellable, error);
}

static void
ide_host_subprocess_launcher_class_init (IdeHostSubprocessLauncherClass *klass)
{
  IdeSubprocessLauncherClass *subprocess_launcher_class = IDE_SUBPROCESS_LAUNCHER_CLASS (klass);

  subprocess_launcher_class->spawn = ide_hostsubprocess_launcher_spawn;
}

static void
ide_host_subprocess_launcher_init (IdeHostSubprocessLauncher *self)
{
  
}
