/* gbp-flatpak-application-addin.c
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

#include "gbp-flatpak-application-addin.h"
#include "gbp-flatpak-runtime.h"

struct _GbpFlatpakApplicationAddin
{
  GObject parent_instance;
};

static void application_addin_iface_init (IdeApplicationAddinInterface *iface);

G_DEFINE_TYPE_EXTENDED (GbpFlatpakApplicationAddin,
                        gbp_flatpak_application_addin,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_APPLICATION_ADDIN, application_addin_iface_init))

static void
gbp_flatpak_application_addin_load (IdeApplicationAddin *addin,
                                    IdeApplication      *application)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) process = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (GBP_IS_FLATPAK_APPLICATION_ADDIN (addin));
  g_assert (IDE_IS_APPLICATION (application));

  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_SILENCE | G_SUBPROCESS_FLAGS_STDERR_SILENCE);
  ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
  ide_subprocess_launcher_push_argv (launcher, "flatpak");
  ide_subprocess_launcher_push_argv (launcher, "remote-delete");
  ide_subprocess_launcher_push_argv (launcher, "--user");
  ide_subprocess_launcher_push_argv (launcher, "--force");
  ide_subprocess_launcher_push_argv (launcher, FLATPAK_REPO_NAME);
  process = ide_subprocess_launcher_spawn (launcher, NULL, &error);
  if (process == NULL)
    {
      g_warning ("%s", error->message);
      return;
    }
  ide_subprocess_wait (process, NULL, NULL);
}

static void
gbp_flatpak_application_addin_class_init (GbpFlatpakApplicationAddinClass *klass)
{
}

static void
gbp_flatpak_application_addin_init (GbpFlatpakApplicationAddin *self)
{
}

static void
application_addin_iface_init (IdeApplicationAddinInterface *iface)
{
  iface->load = gbp_flatpak_application_addin_load;
}
