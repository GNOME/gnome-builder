/* ipc-flatpak-repo.c
 *
 * Copyright 2021 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ipc-flatpak-repo"

#include "ipc-flatpak-repo.h"

struct _IpcFlatpakRepo
{
  GObject parent_instance;
  FlatpakInstallation *installation;
};

G_DEFINE_TYPE (IpcFlatpakRepo, ipc_flatpak_repo, G_TYPE_OBJECT)

static const char filter_file_contents[] = "\
deny *\n\
allow runtime/org.freedesktop.*\n\
allow runtime/org.gnome.*\n\
allow runtime/io.elementary.*\n\
allow runtime/org.kde.*\n\
";

static const char *remotes[] = { "flathub", "flathub-beta", "gnome-nightly" };
static char *repo_data_dir;
static IpcFlatpakRepo *instance;

static void
ipc_flatpak_repo_constructed (GObject *object)
{
  IpcFlatpakRepo *self = (IpcFlatpakRepo *)object;
  g_autofree gchar *gnome_builder_conf_data = NULL;
  g_autoptr(GFile) etc = NULL;
  g_autoptr(GFile) installations_d = NULL;
  g_autoptr(GFile) gnome_builder_conf = NULL;
  g_autoptr(GFile) filter_file = NULL;
  g_autoptr(GFile) flatpak = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GKeyFile) keyfile = NULL;

  g_assert (IPC_IS_FLATPAK_REPO (self));

  G_OBJECT_CLASS (ipc_flatpak_repo_parent_class)->constructed (object);

  flatpak = g_file_new_build_filename (repo_data_dir, "flatpak", NULL);
  filter_file = g_file_get_child (flatpak, "filter");
  etc = g_file_get_child (flatpak, "etc");
  installations_d = g_file_get_child (etc, "installations.d");
  gnome_builder_conf = g_file_get_child (installations_d, "gnome-builder.conf");

  /* Create installation if it doesn't exist */
  if (!(self->installation = flatpak_installation_new_for_path (flatpak, TRUE, NULL, &error)))
    {
      g_warning ("Failed to create private flatpak installation: %s", error->message);
      return;
    }

  /* Create filter list to only allow runtimes */
  if (!g_file_replace_contents (filter_file, filter_file_contents, strlen (filter_file_contents),
                                NULL, FALSE, G_FILE_CREATE_REPLACE_DESTINATION, NULL, NULL, &error))
    {
      g_warning ("Failed to create repository filter file: %s", error->message);
      return;
    }

  g_assert (FLATPAK_IS_INSTALLATION (self->installation));

  /* Add repos we need for development to private installation, but filtered to
   * only include runtimes.
   */
  for (guint i = 0; i < G_N_ELEMENTS (remotes); i++)
    {
      g_autoptr(FlatpakRemote) remote = NULL;
      g_autoptr(GPtrArray) refs = NULL;

      if (!(remote = flatpak_installation_get_remote_by_name (self->installation, remotes[i], NULL, NULL)))
        {
          g_autofree char *title = g_strdup_printf ("Builder (%s)", remotes[i]);
          g_autofree char *resource = g_strdup_printf ("/flatpak/%s.flatpakrepo", remotes[i]);
          g_autoptr(GBytes) bytes = g_resources_lookup_data (resource, 0, NULL);

          g_assert (bytes != NULL);

          if (!(remote = flatpak_remote_new_from_file (remotes[i], bytes, &error)))
            {
              g_warning ("Failed to add remote %s to flatpak installation: %s",
                         remotes[i], error->message);
              g_clear_error (&error);
              continue;
            }

          flatpak_remote_set_filter (remote, g_file_peek_path (filter_file));

          if (!flatpak_installation_add_remote (self->installation, remote, TRUE, NULL, &error))
            {
              g_warning ("Failed to add remote %s to flatpak installation: %s",
                         remotes[i], error->message);
              g_clear_error (&error);
            }
        }

      g_debug ("Updating remote %s", remotes[i]);
      refs = flatpak_installation_list_remote_refs_sync (self->installation, remotes[i], NULL, &error);
      if (error != NULL)
        g_warning ("Failed to update remote %s: %s", remotes[i], error->message);
      else
        g_debug ("Found %u refs", refs->len);
      g_clear_error (&error);
    }

#define INSTALLATION_NAME "Installation \"gnome-builder-private\""

  keyfile = g_key_file_new ();
  g_key_file_set_string (keyfile, INSTALLATION_NAME, "Path", g_file_peek_path (flatpak));
  g_key_file_set_string (keyfile, INSTALLATION_NAME, "DisplayName", "GNOME Builder");
  g_key_file_set_integer (keyfile, INSTALLATION_NAME, "Priority", 0);
  g_key_file_set_string (keyfile, INSTALLATION_NAME, "StorageType", "harddisk");
  gnome_builder_conf_data = g_key_file_to_data (keyfile, NULL, NULL);

  /* Now setup a configuration file that points to all the installations we
   * know about so that we can use FLATPAK_CONFIG_DIR to initialize them.
   */
  if ((!g_file_query_exists (installations_d, NULL) &&
       !g_file_make_directory_with_parents (installations_d, NULL, &error)) ||
      !g_file_replace_contents (gnome_builder_conf, gnome_builder_conf_data, strlen (gnome_builder_conf_data),
                                NULL, FALSE, G_FILE_CREATE_REPLACE_DESTINATION, NULL, NULL, &error))
    {
      g_warning ("Failed to create flatpak site configuration: %s", error->message);
      return;
    }
}

static void
ipc_flatpak_repo_finalize (GObject *object)
{
  IpcFlatpakRepo *self = (IpcFlatpakRepo *)object;

  g_clear_object (&self->installation);

  G_OBJECT_CLASS (ipc_flatpak_repo_parent_class)->finalize (object);
}

static void
ipc_flatpak_repo_class_init (IpcFlatpakRepoClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ipc_flatpak_repo_constructed;
  object_class->finalize = ipc_flatpak_repo_finalize;

  if (repo_data_dir == NULL)
    repo_data_dir = g_build_filename (g_get_user_data_dir (),
                                      "gnome-builder",
                                      NULL);
}

static void
ipc_flatpak_repo_init (IpcFlatpakRepo *self)
{
}

IpcFlatpakRepo *
ipc_flatpak_repo_get_default (void)
{
  if (!instance)
    {
      instance = g_object_new (IPC_TYPE_FLATPAK_REPO, NULL);
      g_object_add_weak_pointer (G_OBJECT (instance), (gpointer *)&instance);
    }

  return instance;
}

FlatpakInstallation *
ipc_flatpak_repo_get_installation (IpcFlatpakRepo *self)
{
  g_return_val_if_fail (IPC_IS_FLATPAK_REPO (self), NULL);

  return self->installation;
}

char *
ipc_flatpak_repo_get_path (IpcFlatpakRepo *self)
{
  g_autoptr(GFile) path = NULL;

  g_return_val_if_fail (IPC_IS_FLATPAK_REPO (self), NULL);

  if (self->installation == NULL)
    return NULL;

  if (!(path = flatpak_installation_get_path (self->installation)))
    return NULL;

  return g_file_get_path (path);
}

char *
ipc_flatpak_repo_get_config_dir (IpcFlatpakRepo *self)
{
  g_return_val_if_fail (IPC_IS_FLATPAK_REPO (self), NULL);

  return g_build_filename (repo_data_dir,
                           "etc",
                           "flatpak",
                           NULL);
}

void
ipc_flatpak_repo_load (const char *data_dir)
{
  if (instance != NULL)
    {
      g_critical ("Cannot load repo, already loaded");
      return;
    }

  if (g_strcmp0 (data_dir, repo_data_dir) != 0)
    {
      g_free (repo_data_dir);
      repo_data_dir = g_strdup (data_dir);
    }

  (void)ipc_flatpak_repo_get_default ();

  g_return_if_fail (IPC_IS_FLATPAK_REPO (instance));
}
