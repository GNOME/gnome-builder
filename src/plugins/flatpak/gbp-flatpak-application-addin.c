/* gbp-flatpak-application-addin.c
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

#define G_LOG_DOMAIN "gbp-flatpak-application-addin"

#include "config.h"

#include "gbp-flatpak-application-addin.h"

struct _GbpFlatpakApplicationAddin
{
  GObject    parent_instance;
};

typedef struct
{
  const gchar *name;
  const gchar *url;
} BuiltinFlatpakRepo;

static BuiltinFlatpakRepo builtin_flatpak_repos[] = {
  { "flathub",       "https://flathub.org/repo/flathub.flatpakrepo" },
  { "gnome-nightly", "https://nightly.gnome.org/gnome-nightly.flatpakrepo" },
};

/*
* Ensure we have our repositories that we need to locate various
* runtimes for GNOME.
*/
static gboolean
ensure_remotes_exist_sync (GError **error)
{
  IDE_ENTRY;

  for (guint i = 0; i < G_N_ELEMENTS (builtin_flatpak_repos); i++)
    {
      g_autoptr(IdeSubprocessLauncher) launcher = NULL;
      g_autoptr(IdeSubprocess) subprocess = NULL;
      const gchar *name = builtin_flatpak_repos[i].name;
      const gchar *url = builtin_flatpak_repos[i].url;

      launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                              G_SUBPROCESS_FLAGS_STDERR_PIPE);
      ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
      ide_subprocess_launcher_set_clear_env (launcher, FALSE);
      ide_subprocess_launcher_push_argv (launcher, "flatpak");
      ide_subprocess_launcher_push_argv (launcher, "remote-add");
      ide_subprocess_launcher_push_argv (launcher, "--user");
      ide_subprocess_launcher_push_argv (launcher, "--if-not-exists");
      ide_subprocess_launcher_push_argv (launcher, "--from");
      ide_subprocess_launcher_push_argv (launcher, name);
      ide_subprocess_launcher_push_argv (launcher, url);

      subprocess = ide_subprocess_launcher_spawn (launcher, NULL, error);

      if (subprocess == NULL || !ide_subprocess_wait_check (subprocess, NULL, error))
        IDE_RETURN (FALSE);
    }
  IDE_RETURN (TRUE);
}

static void
gbp_flatpak_application_addin_load (IdeApplicationAddin *addin,
                                    IdeApplication      *application)
{
  GbpFlatpakApplicationAddin *self = (GbpFlatpakApplicationAddin *)addin;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_FLATPAK_APPLICATION_ADDIN (self));
  g_assert (IDE_IS_APPLICATION (application));

  if (!ensure_remotes_exist_sync (&error))
    g_warning ("Failed to add required flatpak remotes: %s", error->message);
}

static void
gbp_flatpak_application_addin_unload (IdeApplicationAddin *addin,
                                      IdeApplication      *application)
{
  GbpFlatpakApplicationAddin *self = (GbpFlatpakApplicationAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_FLATPAK_APPLICATION_ADDIN (self));
  g_assert (IDE_IS_APPLICATION (application));
}

static void
application_addin_iface_init (IdeApplicationAddinInterface *iface)
{
  iface->load = gbp_flatpak_application_addin_load;
  iface->unload = gbp_flatpak_application_addin_unload;
}

G_DEFINE_TYPE_EXTENDED (GbpFlatpakApplicationAddin,
                        gbp_flatpak_application_addin,
                        G_TYPE_OBJECT,
                        G_TYPE_FLAG_FINAL,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_APPLICATION_ADDIN, application_addin_iface_init))

static void
gbp_flatpak_application_addin_class_init (GbpFlatpakApplicationAddinClass *klass)
{
}

static void
gbp_flatpak_application_addin_init (GbpFlatpakApplicationAddin *self)
{
}

