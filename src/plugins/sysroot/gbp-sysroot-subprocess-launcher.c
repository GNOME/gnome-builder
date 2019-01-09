/* gbp-sysroot-subprocess-launcher.c
 *
 * Copyright 2018 Corentin Noël <corentin.noel@collabora.com>
 * Copyright 2018 Collabora Ltd.
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

#define G_LOG_DOMAIN "gbp-sysroot-subprocess-launcher"

#include <glib/gi18n.h>

#include "gbp-sysroot-subprocess-launcher.h"

struct _GbpSysrootSubprocessLauncher
{
  IdeSubprocessLauncher parent_instance;
};

G_DEFINE_TYPE (GbpSysrootSubprocessLauncher,
               gbp_sysroot_subprocess_launcher,
               IDE_TYPE_SUBPROCESS_LAUNCHER)

IdeSubprocessLauncher *
gbp_sysroot_subprocess_launcher_new (GSubprocessFlags flags)
{
  return g_object_new (GBP_TYPE_SYSROOT_SUBPROCESS_LAUNCHER,
                       "flags", flags,
                       NULL);
}

static IdeSubprocess *
gbp_sysroot_subprocess_launcher_spawn (IdeSubprocessLauncher  *self,
                                       GCancellable           *cancellable,
                                       GError                 **error)
{
  const gchar * const *args;
  g_autoptr(GString) cmd = NULL;

  g_assert (GBP_IS_SYSROOT_SUBPROCESS_LAUNCHER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  /* don't prepend `sh -c` twice */
  args = ide_subprocess_launcher_get_argv (self);
  if (args[0] != NULL && g_strcmp0 (args[0], "sh") == 0 && g_strcmp0 (args[1], "-c") == 0)
    return IDE_SUBPROCESS_LAUNCHER_CLASS (gbp_sysroot_subprocess_launcher_parent_class)->spawn (self, cancellable, error);

  cmd = g_string_new (NULL);
  for (guint i = 0; args[i] != NULL; i++)
    {
      g_autofree gchar *quoted = g_shell_quote (args[i]);
      g_string_append (cmd, quoted);
      g_string_append_c (cmd, ' ');
    }

  ide_subprocess_launcher_set_argv (self, NULL);
  ide_subprocess_launcher_push_argv (self, "sh");
  ide_subprocess_launcher_push_argv (self, "-c");
  ide_subprocess_launcher_push_argv (self, cmd->str);

  return IDE_SUBPROCESS_LAUNCHER_CLASS (gbp_sysroot_subprocess_launcher_parent_class)->spawn (self, cancellable, error);
}

static void
gbp_sysroot_subprocess_launcher_class_init (GbpSysrootSubprocessLauncherClass *klass)
{
  IdeSubprocessLauncherClass *subprocess_launcher_class = IDE_SUBPROCESS_LAUNCHER_CLASS (klass);

  subprocess_launcher_class->spawn = gbp_sysroot_subprocess_launcher_spawn;
}

static void
gbp_sysroot_subprocess_launcher_init (GbpSysrootSubprocessLauncher *self)
{
}
