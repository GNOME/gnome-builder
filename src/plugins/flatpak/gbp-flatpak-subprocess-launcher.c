/* gbp-flatpak-subprocess-launcher.c
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-flatpak-subprocess-launcher"

#include "gbp-flatpak-subprocess-launcher.h"

struct _GbpFlatpakSubprocessLauncher
{
  IdeSubprocessLauncher parent_instance;
  gchar *ref;
  guint use_run : 1;
};

G_DEFINE_TYPE (GbpFlatpakSubprocessLauncher, gbp_flatpak_subprocess_launcher, IDE_TYPE_SUBPROCESS_LAUNCHER)

static IdeSubprocess *
gbp_flatpak_subprocess_launcher_spawn (IdeSubprocessLauncher  *launcher,
                                       GCancellable           *cancellable,
                                       GError                **error)
{
  GbpFlatpakSubprocessLauncher *self = (GbpFlatpakSubprocessLauncher *)launcher;
  g_autofree gchar *build_dir_option = NULL;
  const gchar * const * envp;
  const gchar * const * argv;
  IdeSubprocess *ret;
  guint argpos = 0;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_SUBPROCESS_LAUNCHER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (self->use_run)
    {
      g_autofree gchar *newval = NULL;
      const gchar *oldval;
      guint savepos;

      ide_subprocess_launcher_insert_argv (launcher, argpos++, "flatpak");
      ide_subprocess_launcher_insert_argv (launcher, argpos++, "run");
      ide_subprocess_launcher_insert_argv (launcher, argpos++, "--allow=devel");
      ide_subprocess_launcher_insert_argv (launcher, argpos++, "--device=dri");
      ide_subprocess_launcher_insert_argv (launcher, argpos++, "--filesystem=home");
      ide_subprocess_launcher_insert_argv (launcher, argpos++, "--share=ipc");
      ide_subprocess_launcher_insert_argv (launcher, argpos++, "--share=network");
      ide_subprocess_launcher_insert_argv (launcher, argpos++, "--socket=wayland");
      ide_subprocess_launcher_insert_argv (launcher, argpos++, "--socket=fallback-x11");
      ide_subprocess_launcher_insert_argv (launcher, argpos++, "--socket=pulseaudio");
      ide_subprocess_launcher_insert_argv (launcher, argpos++, "--socket=system-bus");
      ide_subprocess_launcher_insert_argv (launcher, argpos++, "--socket=session-bus");
      ide_subprocess_launcher_insert_argv (launcher, argpos++, "--socket=ssh-auth");
#if 0
      ide_subprocess_launcher_insert_argv (launcher, argpos++, "--verbose");
#endif

      savepos = argpos;

      oldval = ide_subprocess_launcher_get_arg (launcher, argpos);
      newval = g_strdup_printf ("--command=%s", oldval);
      ide_subprocess_launcher_replace_argv (launcher, argpos++, newval);
      ide_subprocess_launcher_insert_argv (launcher, argpos++, self->ref);

      argpos = savepos;

      goto apply_env;
    }

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

apply_env:

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
gbp_flatpak_subprocess_launcher_finalize (GObject *object)
{
  GbpFlatpakSubprocessLauncher *self = (GbpFlatpakSubprocessLauncher *)object;

  g_clear_pointer (&self->ref, g_free);

  G_OBJECT_CLASS (gbp_flatpak_subprocess_launcher_parent_class)->finalize (object);
}

static void
gbp_flatpak_subprocess_launcher_class_init (GbpFlatpakSubprocessLauncherClass *klass)
{
  IdeSubprocessLauncherClass *launcher_class = IDE_SUBPROCESS_LAUNCHER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_flatpak_subprocess_launcher_finalize;

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

void
gbp_flatpak_subprocess_launcher_use_run (GbpFlatpakSubprocessLauncher *self,
                                         const gchar                  *ref)
{
  g_return_if_fail (GBP_IS_FLATPAK_SUBPROCESS_LAUNCHER (self));
  g_return_if_fail (ref != NULL);
  g_return_if_fail (self->ref == NULL);

  self->use_run = TRUE;
  self->ref = g_strdup (ref);

  ide_subprocess_launcher_set_argv (IDE_SUBPROCESS_LAUNCHER (self), NULL);
}
