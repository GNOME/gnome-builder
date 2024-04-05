/* gbp-flatpak-util.c
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

#define G_LOG_DOMAIN "gbp-flatpak-util"

#include <string.h>
#include <libide-foundry.h>
#include <libide-vcs.h>

#include "gbp-flatpak-client.h"
#include "gbp-flatpak-util.h"

gchar *
gbp_flatpak_get_repo_dir (IdeContext *context)
{
  return ide_context_cache_filename (context, "flatpak", "repo", NULL);
}

gchar *
gbp_flatpak_get_staging_dir (IdePipeline *pipeline)
{
  g_autofree char *branch = NULL;
  g_autofree char *name = NULL;
  g_autofree char *arch = NULL;
  g_autoptr (IdeTriplet) triplet = NULL;
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(IdeVcs) vcs = NULL;
  g_autoptr(IdeToolchain) toolchain = NULL;

  g_assert (IDE_IS_PIPELINE (pipeline));

  context = ide_object_ref_context (IDE_OBJECT (pipeline));
  vcs = ide_vcs_ref_from_context (context);
  branch = ide_vcs_get_branch_name (vcs);
  arch = ide_pipeline_dup_arch (pipeline);
  name = g_strdup_printf ("%s-%s", arch, branch);

  g_strdelimit (name, G_DIR_SEPARATOR_S, '-');

  return ide_context_cache_filename (context, "flatpak", "staging", name, NULL);
}

gboolean
gbp_flatpak_is_ignored (const gchar *name)
{
  if (name == NULL)
    return TRUE;

  return g_str_has_suffix (name, ".Locale") ||
         g_str_has_suffix (name, ".Debug") ||
         g_str_has_suffix (name, ".Docs") ||
         g_str_has_suffix (name, ".Sources") ||
         g_str_has_suffix (name, ".Var") ||
         g_str_has_prefix (name, "org.gtk.Gtk3theme.") ||
         strstr (name, ".GL.nvidia") != NULL ||
         strstr (name, ".GL32.nvidia") != NULL ||
         strstr (name, ".VAAPI") != NULL ||
         strstr (name, ".Icontheme") != NULL ||
         strstr (name, ".Extension") != NULL ||
         strstr (name, ".Gtk3theme") != NULL ||
         strstr (name, ".KStyle") != NULL ||
         strstr (name, ".PlatformTheme") != NULL ||
         strstr (name, ".openh264") != NULL;
}

static gboolean
_gbp_flatpak_split_id (const gchar  *str,
                       gchar       **id,
                       gchar       **arch,
                       gchar       **branch)
{
  g_auto(GStrv) parts = g_strsplit (str, "/", 0);
  guint i = 0;

  if (id)
    *id = NULL;

  if (arch)
    *arch = NULL;

  if (branch)
    *branch = NULL;

  if (parts[i] != NULL)
    {
      if (id != NULL)
        *id = g_strdup (parts[i]);
    }
  else
    {
      /* we require at least a runtime/app ID */
      return FALSE;
    }

  i++;

  if (parts[i] != NULL)
    {
      if (arch != NULL)
        *arch = g_strdup (parts[i]);
    }
  else
    return TRUE;

  i++;

  if (parts[i] != NULL)
    {
      if (branch != NULL && !ide_str_empty0 (parts[i]))
        *branch = g_strdup (parts[i]);
    }

  return TRUE;
}

gboolean
gbp_flatpak_split_id (const gchar  *str,
                      gchar       **id,
                      gchar       **arch,
                      gchar       **branch)
{
  if (g_str_has_prefix (str, "runtime/"))
    str += strlen ("runtime/");
  else if (g_str_has_prefix (str, "app/"))
    str += strlen ("app/");

  return _gbp_flatpak_split_id (str, id, arch, branch);
}

static char *
_gbp_flatpak_get_default_arch (void)
{
  GbpFlatpakClient *client = gbp_flatpak_client_get_default ();
  IpcFlatpakService *service = gbp_flatpak_client_get_service (client, NULL, NULL);

  if (service != NULL)
    return g_strdup (ipc_flatpak_service_get_default_arch (service));

  return ide_get_system_arch ();
}

const char *
gbp_flatpak_get_default_arch (void)
{
  static char *default_arch;

  if (default_arch == NULL)
    default_arch = _gbp_flatpak_get_default_arch ();

  return default_arch;
}

const char *
gbp_flatpak_get_config_dir (void)
{
  static char *config_dir;

  if (!config_dir)
    config_dir = g_build_filename (g_get_user_data_dir (),
                                   "gnome-builder",
                                   "flatpak",
                                   "etc",
                                   NULL);

  return config_dir;
}

static char *
_gbp_flatpak_get_a11y_bus (void)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *stdout_buf = NULL;
  g_autofree char *a11y_bus = NULL;
  g_autoptr(GVariant) variant = NULL;

  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE);
  ide_subprocess_launcher_push_args (launcher,
                                     IDE_STRV_INIT ("gdbus",
                                                    "call",
                                                    "--session",
                                                    "--dest=org.a11y.Bus",
                                                    "--object-path=/org/a11y/bus",
                                                    "--method=org.a11y.Bus.GetAddress"));
  ide_subprocess_launcher_set_clear_env (launcher, FALSE);
  ide_subprocess_launcher_set_run_on_host (launcher, TRUE);

  if (!(subprocess = ide_subprocess_launcher_spawn (launcher, NULL, &error)))
    goto handle_error;

  if (!ide_subprocess_communicate_utf8 (subprocess, NULL, NULL, &stdout_buf, NULL, &error))
    goto handle_error;

  if (!(variant = g_variant_parse (G_VARIANT_TYPE ("(s)"), stdout_buf, NULL, NULL, &error)))
    goto handle_error;

  g_variant_take_ref (variant);
  g_variant_get (variant, "(s)", &a11y_bus, NULL);

  g_debug ("Accessibility bus discovered at %s", a11y_bus);

  return g_steal_pointer (&a11y_bus);

handle_error:
  g_critical ("Failed to detect a11y bus on host: %s", error->message);

  return NULL;
}

const char *
gbp_flatpak_get_a11y_bus (const char **out_unix_path,
                          const char **out_address_suffix)
{
  static char *a11y_bus;
  static char *a11y_bus_path;
  static const char *a11y_bus_suffix;

  if (g_once_init_enter (&a11y_bus))
    {
      char *address = _gbp_flatpak_get_a11y_bus ();

      if (address != NULL && g_str_has_prefix (address, "unix:path="))
        {
          const char *skip = address + strlen ("unix:path=");

          if ((a11y_bus_suffix = strchr (skip, ',')))
            a11y_bus_path = g_strndup (skip, a11y_bus_suffix - skip);
          else
            a11y_bus_path = g_strdup (skip);
        }

      g_once_init_leave (&a11y_bus, address);
    }

#if 0
  g_print ("a11y_bus=%s\n", a11y_bus);
  g_print ("a11y_bus_path=%s\n", a11y_bus_path);
  g_print ("a11y_bus_suffix=%s\n", a11y_bus_suffix);
#endif

  if (out_unix_path != NULL)
    *out_unix_path = a11y_bus_path;

  if (out_address_suffix != NULL)
    *out_address_suffix = a11y_bus_suffix;

  return a11y_bus;
}
