/* gbp-flatpak-subprocess-launcher.c
 *
 * Copyright © 2016 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-flatpak-subprocess-launcher"

#include "gbp-flatpak-subprocess-launcher.h"

struct _GbpFlatpakSubprocessLauncher
{
  IdeSubprocessLauncher parent_instance;
};

G_DEFINE_TYPE (GbpFlatpakSubprocessLauncher, gbp_flatpak_subprocess_launcher, IDE_TYPE_SUBPROCESS_LAUNCHER)

static IdeSubprocess *
gbp_flatpak_subprocess_launcher_spawn (IdeSubprocessLauncher  *launcher,
                                       GCancellable           *cancellable,
                                       GError                **error)
{
  const gchar * const * envp;
  IdeSubprocess *ret;
  const gchar * const * argv;
  guint argpos = 0;
  g_autofree gchar *build_dir_option = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_SUBPROCESS_LAUNCHER (launcher));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));


  /*
   * The "flatpak build" command will filter out all of our environment variables
   * from the subprocess, and change the current directory to the build dir.
   * So we need to look at our configured environment and convert the
   * KEY=VALUE pairs into --env=key=value command line arguments, and set the appropriate
   * --build-dir
   */

  argv = ide_subprocess_launcher_get_argv (launcher);

  /*
   * Locate the position after our ["flatpak", "build"] arguments.
   */
  for (argpos = 0; argv[argpos] != NULL; argpos++)
    {
      if (g_strcmp0 (argv[argpos], "flatpak") == 0)
        break;
    }
  for (; argv[argpos] != NULL; argpos++)
    {
      if (g_strcmp0 (argv[argpos], "build") == 0)
        {
          argpos++;
          break;
        }
    }

  build_dir_option = g_strdup_printf ("--build-dir=%s",
                                      ide_subprocess_launcher_get_cwd (launcher));

  /*
   * Since this can be called multiple times, we have to avoid re-adding
   * the --build-dir= parameters a second (or third, or fourth) time.
   */
  if (!g_strv_contains (argv, build_dir_option))
    ide_subprocess_launcher_insert_argv (launcher, argpos, build_dir_option);

  envp = ide_subprocess_launcher_get_environ (launcher);

  if (envp != NULL)
    {
      /*
       * Since this can be called multiple times, we have to avoid re-adding
       * the --env= parameters a second (or third, or fourth) time.
       */
      for (guint i = 0; envp[i] != NULL; i++)
        {
          g_autofree gchar *arg = g_strdup_printf ("--env=%s", envp[i]);
          argv = ide_subprocess_launcher_get_argv (launcher);
          if (!g_strv_contains (argv, arg))
            ide_subprocess_launcher_insert_argv (launcher, argpos, arg);
        }

      ide_subprocess_launcher_setenv (launcher, "PATH", NULL, TRUE);
    }

  ret = IDE_SUBPROCESS_LAUNCHER_CLASS (gbp_flatpak_subprocess_launcher_parent_class)->spawn (launcher, cancellable, error);

  IDE_RETURN (ret);
}

static void
gbp_flatpak_subprocess_launcher_class_init (GbpFlatpakSubprocessLauncherClass *klass)
{
  IdeSubprocessLauncherClass *launcher_class = IDE_SUBPROCESS_LAUNCHER_CLASS (klass);

  launcher_class->spawn = gbp_flatpak_subprocess_launcher_spawn;
}

static void
gbp_flatpak_subprocess_launcher_init (GbpFlatpakSubprocessLauncher *self)
{
  ide_subprocess_launcher_setenv (IDE_SUBPROCESS_LAUNCHER (self), "PATH", "/app/bin:/usr/bin", TRUE);
}

IdeSubprocessLauncher *
gbp_flatpak_subprocess_launcher_new (GSubprocessFlags flags)
{
  return g_object_new (GBP_TYPE_FLATPAK_SUBPROCESS_LAUNCHER,
                       "flags", flags,
                       NULL);
}
