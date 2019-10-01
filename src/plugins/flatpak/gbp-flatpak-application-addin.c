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

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <libide-greeter.h>
#include <libide-gui.h>
#include <errno.h>
#include <flatpak.h>
#include <string.h>
#include <unistd.h>

#include "gbp-flatpak-application-addin.h"
#include "gbp-flatpak-clone-widget.h"
#include "gbp-flatpak-runtime.h"
#include "gbp-flatpak-util.h"

typedef struct
{
  FlatpakInstallation        *installation;
  GFileMonitor               *monitor;
  GbpFlatpakApplicationAddin *self;
} InstallInfo;

typedef struct
{
  gchar               *id;
  gchar               *arch;
  gchar               *branch;
  GPtrArray           *installations;
  IdeNotification     *progress;
  FlatpakInstalledRef *ref;
  guint                did_added : 1;
} InstallRequest;

typedef struct
{
  gchar     *id;
  gchar     *arch;
  gchar     *branch;
  gchar     *sdk_id;
  gchar     *sdk_arch;
  gchar     *sdk_branch;
  GPtrArray *installations;
} LocateSdk;

struct _GbpFlatpakApplicationAddin
{
  GObject    parent_instance;

  /*
   * @installations is never modified after creation. Whenever we reload
   * the runtimes we reload completely, so that threaded operations that
   * are accessing this structure (albeit with a full reference to the
   * ptrarray) will not be affected.
   */
  GPtrArray *installations;

  /* The addin attempts to delay loading any flatpak information until
   * it has been requested (by the runtime provider for example). Doing
   * so helps speed up initial application startup at the cost of a bit
   * slower project setup time.
   */
  guint      has_loaded : 1;
};

typedef struct
{
  const gchar *name;
  const gchar *url;
} BuiltinFlatpakRepo;

enum {
  RUNTIME_ADDED,
  RELOAD,
  N_SIGNALS
};

static GbpFlatpakApplicationAddin *instance;
static guint signals [N_SIGNALS];
static BuiltinFlatpakRepo builtin_flatpak_repos[] = {
  { "flathub",       "https://flathub.org/repo/flathub.flatpakrepo" },
  { "gnome-nightly", "https://nightly.gnome.org/gnome-nightly.flatpakrepo" },
};

static void gbp_flatpak_application_addin_lazy_reload (GbpFlatpakApplicationAddin *self);

static void
copy_devhelp_docs_into_user_data_dir_worker (IdeTask      *task,
                                             gpointer      source_object,
                                             gpointer      task_data,
                                             GCancellable *cancellable)
{
  GPtrArray *paths = task_data;
  g_autofree gchar *dest_dir = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_APPLICATION_ADDIN (source_object));
  g_assert (paths != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  dest_dir = g_build_filename (g_get_user_data_dir (), "gtk-doc", "html", NULL);

  if (!g_file_test (dest_dir, G_FILE_TEST_IS_DIR))
    g_mkdir_with_parents (dest_dir, 0750);

  for (guint i = 0; i < paths->len; i++)
    {
      const gchar *basedir = g_ptr_array_index (paths, i);
      g_autofree gchar *parent = g_build_filename (basedir, "files", "gtk-doc", "html", NULL);
      g_autoptr(GDir) dir = g_dir_open (parent, 0, NULL);
      const gchar *name;

      if (dir == NULL)
        continue;

      while (NULL != (name = g_dir_read_name (dir)))
        {
          g_autofree gchar *src = g_build_filename (parent, name, NULL);
          g_autofree gchar *dst = g_build_filename (dest_dir, name, NULL);

          if (g_file_test (dst, G_FILE_TEST_IS_SYMLINK))
            g_unlink (dst);

          errno = 0;
          if (symlink (src, dst) == -1)
            g_warning ("Failed to copy documentation: %s (%s)",
                       g_strerror (errno), src);
        }
    }

  ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
copy_devhelp_docs_into_user_data_dir (GbpFlatpakApplicationAddin *self)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GPtrArray) paths = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_FLATPAK_APPLICATION_ADDIN (self));

  if (self->installations == NULL)
    IDE_EXIT;

  /* HACK:
   *
   * Devhelp does not provide us adequate API to be able to set the
   * search path for runtimes. That means that we have no documentation
   * for users from Devhelp. While we can improve this in the future via
   * "doc packs" in Devhelp, we need something that can be shipped
   * immediately for 3.26.
   *
   * The hack that seems to be the minimal amount of work which improves
   * the situation on both Host-installed and Flatpak-installed variants
   * is to copy documentation from the newest version of runtimes into
   * the $XDG_DATA_DIR/and will work
   */

  paths = g_ptr_array_new_with_free_func (g_free);

  task = ide_task_new (self, NULL, NULL, NULL);
  ide_task_set_source_tag (task, copy_devhelp_docs_into_user_data_dir);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  /*
   * Collect the paths to all of the .Docs runtimes.
   *
   * TODO: We should try to sort these by importance, so that master
   *       docs are preferred and if that is not available, the highest
   *       version number.
   */
  for (guint i = 0; i < self->installations->len; i++)
    {
      InstallInfo *info = g_ptr_array_index (self->installations, i);
      g_autoptr(GPtrArray) refs = NULL;

      refs = flatpak_installation_list_installed_refs_by_kind (info->installation,
                                                               FLATPAK_REF_KIND_RUNTIME,
                                                               ide_task_get_cancellable (task),
                                                               NULL);
      if (refs == NULL)
        continue;

      for (guint j = 0; j < refs->len; j++)
        {
          FlatpakInstalledRef *ref = g_ptr_array_index (refs, j);
          const gchar *name = flatpak_ref_get_name (FLATPAK_REF (ref));

          if (name == NULL || !g_str_has_suffix (name, ".Docs"))
            continue;

          g_ptr_array_add (paths, g_strdup (flatpak_installed_ref_get_deploy_dir (ref)));
        }
    }

  ide_task_set_task_data (task, g_steal_pointer (&paths), g_ptr_array_unref);

  /* Now go copy the the docs over */
  ide_task_run_in_thread (task, copy_devhelp_docs_into_user_data_dir_worker);

  IDE_EXIT;
}

static void
install_info_installation_changed (GFileMonitor      *monitor,
                                   GFile             *file,
                                   GFile             *other_file,
                                   GFileMonitorEvent  event_type,
                                   InstallInfo       *info)
{
  g_autoptr(GbpFlatpakApplicationAddin) self = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (G_IS_FILE_MONITOR (monitor));
  g_assert (G_IS_FILE (file));
  g_assert (!other_file || G_IS_FILE (other_file));
  g_assert (info != NULL);

  self = g_object_ref (info->self);

  gbp_flatpak_application_addin_lazy_reload (self);

  IDE_EXIT;
}

static void
install_info_free (InstallInfo *info)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (info != NULL);
  g_assert (!info->installation || FLATPAK_IS_INSTALLATION (info->installation));
  g_assert (!info->monitor || G_IS_FILE_MONITOR (info->monitor));

  if (info->monitor != NULL)
    {
      g_signal_handlers_disconnect_by_func (info->monitor,
                                            G_CALLBACK (install_info_installation_changed),
                                            info);
    }

  g_clear_weak_pointer (&info->self);
  g_clear_object (&info->monitor);
  g_clear_object (&info->installation);

  g_slice_free (InstallInfo, info);
}

static InstallInfo *
install_info_new (GbpFlatpakApplicationAddin *self,
                  FlatpakInstallation        *installation)
{
  InstallInfo *info;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_FLATPAK_APPLICATION_ADDIN (self));
  g_assert (FLATPAK_IS_INSTALLATION (installation));

  info = g_slice_new0 (InstallInfo);
  info->installation = g_object_ref (installation);
  info->monitor = flatpak_installation_create_monitor (installation, NULL, NULL);
  g_set_weak_pointer (&info->self, self);

  if (info->monitor != NULL)
    {
      g_signal_connect (info->monitor,
                        "changed",
                        G_CALLBACK (install_info_installation_changed),
                        info);
    }

  return info;
}

static void
install_request_free (InstallRequest *request)
{
  g_clear_pointer (&request->id, g_free);
  g_clear_pointer (&request->arch, g_free);
  g_clear_pointer (&request->branch, g_free);
  g_clear_pointer (&request->installations, g_ptr_array_unref);
  g_clear_object (&request->progress);
  g_clear_object (&request->ref);
  g_slice_free (InstallRequest, request);
}

static void
locate_sdk_free (LocateSdk *locate)
{
  g_clear_pointer (&locate->id, g_free);
  g_clear_pointer (&locate->arch, g_free);
  g_clear_pointer (&locate->branch, g_free);
  g_clear_pointer (&locate->sdk_id, g_free);
  g_clear_pointer (&locate->sdk_arch, g_free);
  g_clear_pointer (&locate->sdk_branch, g_free);
  g_clear_pointer (&locate->installations, g_ptr_array_unref);
  g_slice_free (LocateSdk, locate);
}

static void
gbp_flatpak_application_addin_lazy_reload (GbpFlatpakApplicationAddin *self)
{
  g_autofree gchar *user_path = NULL;
  g_autoptr(GFile) user_file = NULL;
  g_autoptr(GPtrArray) system_installs = NULL;
  g_autoptr(GPtrArray) runtimes = NULL;
  g_autoptr(FlatpakInstallation) user = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_FLATPAK_APPLICATION_ADDIN (self));

  self->has_loaded = TRUE;

  /* Clear any previous installations */
  g_clear_pointer (&self->installations, g_ptr_array_unref);
  self->installations = g_ptr_array_new_with_free_func ((GDestroyNotify)install_info_free);

  /*
   * First we want to load the user installation so that it is at index 0.
   * This naturally prefers the user installation for various operations
   * which is precisely what we want.
   *
   * We can't use flatpak_installation_new_user() since that will not map to
   * the user's real flatpak user installation. It will instead map to the
   * reidrected XDG_DATA_DIRS version. Therefore, we synthesize the path to the
   * location we know it should be at.
   */
  user_path = g_build_filename (g_get_home_dir (), ".local", "share", "flatpak", NULL);
  user_file = g_file_new_for_path (user_path);
  user = flatpak_installation_new_for_path (user_file, TRUE, NULL, NULL);
  if (user != NULL)
    g_ptr_array_add (self->installations, install_info_new (self, user));

  /*
   * Now load any of the system installations. As of more recent flatpak
   * versions, we can have multiple system installations. So try to load all of
   * them.
   */
  system_installs = flatpak_get_system_installations (NULL, NULL);

  if (system_installs != NULL)
    {
      for (guint i = 0; i < system_installs->len; i++)
        {
          FlatpakInstallation *installation = g_ptr_array_index (system_installs, i);
          g_ptr_array_add (self->installations, install_info_new (self, installation));
        }
    }

  /*
   * Now notify any listeners of new runtimes. They are responsible for
   * dealing with deduplicating by id/arch/branch.
   */
  runtimes = gbp_flatpak_application_addin_get_runtimes (self);

  if (runtimes != NULL)
    {
      for (guint i = 0; i < runtimes->len; i++)
        {
          FlatpakRef *ref = g_ptr_array_index (runtimes, i);

          g_signal_emit (self, signals[RUNTIME_ADDED], 0, ref);
        }
    }

  copy_devhelp_docs_into_user_data_dir (self);

  g_signal_emit (self, signals[RELOAD], 0);

  IDE_EXIT;
}

static void
gbp_flatpak_application_addin_load (IdeApplicationAddin *addin,
                                    IdeApplication      *application)
{
  GbpFlatpakApplicationAddin *self = (GbpFlatpakApplicationAddin *)addin;
  g_autoptr(GSettings) settings = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_FLATPAK_APPLICATION_ADDIN (self));
  g_assert (IDE_IS_APPLICATION (application));

  instance = self;

  settings = g_settings_new ("org.gnome.builder");

  if (g_settings_get_boolean (settings, "clear-cache-at-startup"))
    {
      g_autoptr(DzlDirectoryReaper) reaper = NULL;
      g_autoptr(GFile) builds_dir = NULL;
      g_autofree gchar *path = NULL;

      path = g_build_filename (g_get_user_cache_dir (),
                               ide_get_program_name (),
                               "flatpak-builder",
                               "build",
                               NULL);

      IDE_TRACE_MSG ("Clearing flatpak build cache \"%s\"", path);

      /*
       * Cleanup old build data to avoid an ever-growing cache directory.
       * Any build data older than 3 days can be wiped.
       */

      reaper = dzl_directory_reaper_new ();
      builds_dir = g_file_new_for_path (path);
      dzl_directory_reaper_add_directory (reaper, builds_dir, G_TIME_SPAN_DAY * 3);
      dzl_directory_reaper_execute_async (reaper, NULL, NULL, NULL);
    }

  IDE_EXIT;
}

static void
gbp_flatpak_application_addin_unload (IdeApplicationAddin *addin,
                                      IdeApplication      *application)
{
  GbpFlatpakApplicationAddin *self = (GbpFlatpakApplicationAddin *)addin;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_FLATPAK_APPLICATION_ADDIN (self));
  g_assert (IDE_IS_APPLICATION (application));

  instance = NULL;

  g_clear_pointer (&self->installations, g_ptr_array_unref);

  IDE_EXIT;
}

/**
 * gbp_flatpak_application_addin_get_runtimes:
 *
 * Gets an array of runtimes available on the system.
 *
 * Returns: (transfer container) (element-type Flatpak.InstalledRef): Array of runtimes.
 */
GPtrArray *
gbp_flatpak_application_addin_get_runtimes (GbpFlatpakApplicationAddin *self)
{
  GPtrArray *ret;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_FLATPAK_APPLICATION_ADDIN (self));

  if (!self->has_loaded)
    gbp_flatpak_application_addin_lazy_reload (self);

  ret = g_ptr_array_new_with_free_func (g_object_unref);

  for (guint i = 0; i < self->installations->len; i++)
    {
      InstallInfo *info = g_ptr_array_index (self->installations, i);
      g_autoptr(GPtrArray) ar = NULL;

      ar = flatpak_installation_list_installed_refs_by_kind (info->installation,
                                                             FLATPAK_REF_KIND_RUNTIME,
                                                             NULL,
                                                             NULL);

      if (ar != NULL)
        {
          for (guint j = 0; j < ar->len; j++)
            {
              FlatpakInstalledRef *ref = g_ptr_array_index (ar, j);

              g_ptr_array_add (ret, g_object_ref (ref));
            }
        }
    }

  IDE_RETURN (ret);
}

/**
 * gbp_flatpak_application_addin_get_installations:
 *
 * Gets an array of flatpak installations on the system.
 *
 * Returns: (transfer container) (element-type Flatpak.Installation): Array of installations
 */
GPtrArray *
gbp_flatpak_application_addin_get_installations (GbpFlatpakApplicationAddin *self)
{
  GPtrArray *ret;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_FLATPAK_APPLICATION_ADDIN (self));

  if (!self->has_loaded)
    gbp_flatpak_application_addin_lazy_reload (self);

  ret = g_ptr_array_new_with_free_func (g_object_unref);

  /* Might be NULL before things have loaded at startup */
  if (self->installations != NULL)
    {
      for (guint i = 0; i < self->installations->len; i++)
        {
          InstallInfo *info = g_ptr_array_index (self->installations, i);

          g_assert (info != NULL);
          g_assert (FLATPAK_IS_INSTALLATION (info->installation));

          g_ptr_array_add (ret, g_object_ref (info->installation));
        }
    }

  IDE_RETURN (ret);
}

GbpFlatpakApplicationAddin *
gbp_flatpak_application_addin_get_default (void)
{
  return instance;
}

/*
* Ensure we have our repositories that we need to locate various
* runtimes for GNOME.
*/
static gboolean
ensure_remotes_exist_sync (GCancellable  *cancellable,
                           GError       **error)
{
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

      subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, error);

      if (subprocess == NULL || !ide_subprocess_wait_check (subprocess, cancellable, error))
        return FALSE;
    }
  return TRUE;
}

static void
gbp_flatpak_application_addin_install_completed (GbpFlatpakApplicationAddin *self,
                                                 GParamSpec                 *pspec,
                                                 IdeTask                    *task)
{
  InstallRequest *request;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_APPLICATION_ADDIN (self));
  g_assert (pspec != NULL);
  g_assert (IDE_IS_TASK (task));

  request = ide_task_get_task_data (task);

  if (request->ref != NULL && !request->did_added)
    {
      request->did_added = TRUE;
      g_signal_emit (self, signals[RUNTIME_ADDED], 0, request->ref);
    }

  IDE_EXIT;
}

static void
gbp_flatpak_application_addin_install_runtime_worker (IdeTask      *task,
                                                      gpointer      source_object,
                                                      gpointer      task_data,
                                                      GCancellable *cancellable)
{
  InstallRequest *request = task_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_TASK (task));
  g_assert (GBP_IS_FLATPAK_APPLICATION_ADDIN (source_object));
  g_assert (request != NULL);
  g_assert (request->id != NULL);
  g_assert (request->arch != NULL);
  g_assert (request->branch == NULL || *request->branch != 0);
  g_assert (request->installations != NULL);

  if (!ensure_remotes_exist_sync (cancellable, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  /*
   * First we want to try to locate the runtime within a previous install.
   * If so, we will just update from that.
   */
  for (guint i = 0; i < request->installations->len; i++)
    {
      InstallInfo *info = g_ptr_array_index (request->installations, i);
      FlatpakInstallation *installation = info->installation;
      g_autoptr(GPtrArray) refs = NULL;

      g_assert (FLATPAK_IS_INSTALLATION (installation));

      refs = flatpak_installation_list_installed_refs (installation, cancellable, NULL);
      if (refs == NULL)
        continue;

      for (guint j = 0; j < refs->len; j++)
        {
          FlatpakInstalledRef *ref = g_ptr_array_index (refs, j);
          const gchar *id = flatpak_ref_get_name (FLATPAK_REF (ref));
          const gchar *arch = flatpak_ref_get_arch (FLATPAK_REF (ref));
          const gchar *branch = flatpak_ref_get_branch (FLATPAK_REF (ref));

          g_assert (FLATPAK_IS_INSTALLED_REF (ref));

          if (g_strcmp0 (request->id, id) == 0 &&
              (!request->branch || g_strcmp0 (request->branch, branch) == 0) &&
              g_strcmp0 (request->arch, arch) == 0)
            {
              request->ref = flatpak_installation_update (installation,
                                                          FLATPAK_UPDATE_FLAGS_NONE,
                                                          FLATPAK_REF_KIND_RUNTIME,
                                                          id,
                                                          arch,
                                                          branch,
                                                          ide_notification_flatpak_progress_callback,
                                                          request->progress,
                                                          cancellable,
                                                          &error);

              if (request->ref == NULL)
                ide_task_return_error (task, g_steal_pointer (&error));
              else
                ide_task_return_boolean (task, TRUE);

              IDE_EXIT;
            }
        }
    }

  /*
   * We failed to locate a previous install, so instead let's discover the
   * ref from a remote summary description.
   */
  for (guint i = 0; i < request->installations->len; i++)
    {
      InstallInfo *info = g_ptr_array_index (request->installations, i);
      FlatpakInstallation *installation = info->installation;
      g_autoptr(GPtrArray) remotes = NULL;

      g_assert (FLATPAK_IS_INSTALLATION (installation));

      /* Refresh in case a new remote was added */
      flatpak_installation_drop_caches (installation, cancellable, NULL);

      remotes = flatpak_installation_list_remotes (installation, cancellable, NULL);
      if (remotes == NULL)
        continue;

      for (guint j = 0; j < remotes->len; j++)
        {
          FlatpakRemote *remote = g_ptr_array_index (remotes, j);
          const gchar *name = flatpak_remote_get_name (remote);
          g_autoptr(GPtrArray) refs = NULL;

          g_assert (FLATPAK_IS_REMOTE (remote));

          refs = flatpak_installation_list_remote_refs_sync (installation, name, cancellable, NULL);
          if (refs == NULL)
            continue;

          for (guint k = 0; k < refs->len; k++)
            {
              FlatpakRemoteRef *ref = g_ptr_array_index (refs, k);
              const gchar *id = flatpak_ref_get_name (FLATPAK_REF (ref));
              const gchar *arch = flatpak_ref_get_arch (FLATPAK_REF (ref));
              const gchar *branch = flatpak_ref_get_branch (FLATPAK_REF (ref));

              g_assert (FLATPAK_IS_REMOTE_REF (ref));

              if (g_strcmp0 (request->id, id) == 0 &&
                  (!request->branch || g_strcmp0 (request->branch, branch) == 0) &&
                  g_strcmp0 (request->arch, arch) == 0)
                {
                  request->ref = flatpak_installation_install (installation,
                                                               name,
                                                               FLATPAK_REF_KIND_RUNTIME,
                                                               id,
                                                               arch,
                                                               branch,
                                                               ide_notification_flatpak_progress_callback,
                                                               request->progress,
                                                               cancellable,
                                                               &error);

                  if (request->ref != NULL)
                    ide_task_return_boolean (task, TRUE);
                  else
                    ide_task_return_error (task, g_steal_pointer (&error));

                  IDE_EXIT;
                }
            }
        }
    }

  ide_task_return_new_error (task,
                             G_IO_ERROR,
                             G_IO_ERROR_NOT_FOUND,
                             "Failed to locate runtime \"%s/%s/%s\" within configured flatpak remotes",
                             request->id, request->arch ?: "", request->branch ?: "");

  IDE_EXIT;
}

void
gbp_flatpak_application_addin_install_runtime_async (GbpFlatpakApplicationAddin  *self,
                                                     const gchar                 *runtime_id,
                                                     const gchar                 *arch,
                                                     const gchar                 *branch,
                                                     GCancellable                *cancellable,
                                                     IdeNotification            **progress,
                                                     GAsyncReadyCallback          callback,
                                                     gpointer                     user_data)
{
  g_autoptr(IdeTask) task = NULL;
  InstallRequest *request;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_FLATPAK_APPLICATION_ADDIN (self));
  g_assert (runtime_id != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (self->installations != NULL);

  if (arch == NULL || *arch == 0)
    arch = flatpak_get_default_arch ();

  /* NULL branch indicates to accept any match */
  if (branch != NULL && *branch == 0)
    branch = NULL;

  request = g_slice_new0 (InstallRequest);
  request->id = g_strdup (runtime_id);
  request->arch = g_strdup (arch);
  request->branch = g_strdup (branch);
  request->installations = g_ptr_array_ref (self->installations);
  request->progress = ide_notification_new ();

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_flatpak_application_addin_install_runtime_async);
  ide_task_set_task_data (task, request, install_request_free);

  g_signal_connect_object (task,
                           "notify::completed",
                           G_CALLBACK (gbp_flatpak_application_addin_install_completed),
                           self,
                           G_CONNECT_SWAPPED);

  if (progress != NULL)
    *progress = g_object_ref (request->progress);

  ide_task_run_in_thread (task, gbp_flatpak_application_addin_install_runtime_worker);

  IDE_EXIT;
}

gboolean
gbp_flatpak_application_addin_install_runtime_finish (GbpFlatpakApplicationAddin  *self,
                                                      GAsyncResult                *result,
                                                      GError                     **error)
{
  InstallRequest *request;
  g_autoptr(GError) local_error = NULL;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (GBP_IS_FLATPAK_APPLICATION_ADDIN (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  request = ide_task_get_task_data (IDE_TASK (result));

  /*
   * We might want to immediately notify about the ref so that the
   * caller can access the runtime after calling this. Otherwise our
   * notify::completed might not have yet run.
   */
  if (request->ref != NULL && !request->did_added)
    {
      request->did_added = TRUE;
      g_signal_emit (self, signals[RUNTIME_ADDED], 0, request->ref);
    }

  if (!ide_task_propagate_boolean (IDE_TASK (result), &local_error))
    {
      /* Ignore "already installed" errors. */
      if (!g_error_matches (local_error, FLATPAK_ERROR, FLATPAK_ERROR_ALREADY_INSTALLED))
        {
          g_propagate_error (error,  g_steal_pointer (&local_error));
          return FALSE;
        }

      g_clear_error (&local_error);
    }

  return TRUE;
}

gboolean
gbp_flatpak_application_addin_has_runtime (GbpFlatpakApplicationAddin *self,
                                           const gchar                *id,
                                           const gchar                *arch,
                                           const gchar                *branch)
{
  g_autoptr(GPtrArray) ar = NULL;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (GBP_IS_FLATPAK_APPLICATION_ADDIN (self), FALSE);

  if (id == NULL)
    return FALSE;

  if (arch == NULL)
    arch = flatpak_get_default_arch ();

  IDE_TRACE_MSG ("Looking for runtime %s/%s/%s", id, arch, branch ?: "");

  ar = gbp_flatpak_application_addin_get_runtimes (self);

  if (ar != NULL)
    {
      for (guint i = 0; i < ar->len; i++)
        {
          FlatpakInstalledRef *ref = g_ptr_array_index (ar, i);
          const gchar *ref_id = flatpak_ref_get_name (FLATPAK_REF (ref));
          const gchar *ref_arch = flatpak_ref_get_arch (FLATPAK_REF (ref));
          const gchar *ref_branch = flatpak_ref_get_branch (FLATPAK_REF (ref));

          if (ide_str_equal0 (id, ref_id) &&
              ide_str_equal0 (arch, ref_arch) &&
              (ide_str_empty0 (branch) || ide_str_equal0 (branch, ref_branch)))
            IDE_RETURN (TRUE);
        }
    }

  IDE_RETURN (FALSE);
}

static void
gbp_flatpak_application_addin_add_option_entries (IdeApplicationAddin *addin,
                                                  IdeApplication      *app)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_FLATPAK_APPLICATION_ADDIN (addin));
  g_assert (G_IS_APPLICATION (app));

  g_application_add_main_option (G_APPLICATION (app),
                                 "manifest",
                                 'm',
                                 G_OPTION_FLAG_IN_MAIN,
                                 G_OPTION_ARG_FILENAME,
                                 _("Clone a project using flatpak manifest"),
                                 _("MANIFEST"));
}

static void
gbp_flatpak_application_addin_clone_cb (GObject      *object,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
  GbpFlatpakCloneWidget *clone = (GbpFlatpakCloneWidget *)object;
  g_autoptr(IdeGreeterWorkspace) workspace = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_FLATPAK_CLONE_WIDGET (clone));
  g_assert (IDE_IS_GREETER_WORKSPACE (workspace));

  if (!gbp_flatpak_clone_widget_clone_finish (clone, result, &error))
    g_warning ("%s", error->message);

  ide_greeter_workspace_end (workspace);
}

static void
gbp_flatpak_application_addin_handle_command_line (IdeApplicationAddin     *addin,
                                                   IdeApplication          *application,
                                                   GApplicationCommandLine *cmdline)
{
  g_autofree gchar *manifest = NULL;
  GbpFlatpakCloneWidget *clone;
  IdeGreeterWorkspace *workspace;
  IdeWorkbench *workbench;
  GVariantDict *options;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_FLATPAK_APPLICATION_ADDIN (addin));
  g_assert (IDE_IS_APPLICATION (application));
  g_assert (G_IS_APPLICATION_COMMAND_LINE (cmdline));

  if (!(options = g_application_command_line_get_options_dict (cmdline)) ||
      !g_variant_dict_contains (options, "manifest") ||
      !g_variant_dict_lookup (options, "manifest", "^ay", &manifest))
    return;

  workbench = ide_workbench_new ();
  ide_application_add_workbench (application, workbench);

  workspace = ide_greeter_workspace_new (application);
  ide_workbench_add_workspace (workbench, IDE_WORKSPACE (workspace));

  clone = g_object_new (GBP_TYPE_FLATPAK_CLONE_WIDGET,
                        "manifest", manifest,
                        "visible", TRUE,
                        NULL);
  ide_workspace_add_surface (IDE_WORKSPACE (workspace), IDE_SURFACE (clone));
  ide_workspace_set_visible_surface (IDE_WORKSPACE (workspace), IDE_SURFACE (clone));

  ide_workbench_focus_workspace (workbench, IDE_WORKSPACE (workspace));

  ide_greeter_workspace_begin (workspace);
  gbp_flatpak_clone_widget_clone_async (clone,
                                        NULL,
                                        gbp_flatpak_application_addin_clone_cb,
                                        g_object_ref (workspace));
}

static void
application_addin_iface_init (IdeApplicationAddinInterface *iface)
{
  iface->load = gbp_flatpak_application_addin_load;
  iface->unload = gbp_flatpak_application_addin_unload;
  iface->add_option_entries = gbp_flatpak_application_addin_add_option_entries;
  iface->handle_command_line = gbp_flatpak_application_addin_handle_command_line;
}

G_DEFINE_TYPE_EXTENDED (GbpFlatpakApplicationAddin,
                        gbp_flatpak_application_addin,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_APPLICATION_ADDIN, application_addin_iface_init))

static void
gbp_flatpak_application_addin_class_init (GbpFlatpakApplicationAddinClass *klass)
{
  /**
   * GbpFlatpakApplicationAddin::runtime-added:
   * @self: An #GbpFlatpakApplicationAddin
   * @runtime: a #FlatpakInstalledRef
   *
   * This signal is emitted when a new runtime is discovered. No deduplication
   * is dealt with here, so consumers will need to ensure they have not seen
   * the runtime before by deduplicating with id/arch/branch.
   */
  signals [RUNTIME_ADDED] = g_signal_new ("runtime-added",
                                          G_TYPE_FROM_CLASS (klass),
                                          G_SIGNAL_RUN_LAST,
                                          0,
                                          NULL, NULL, NULL,
                                          G_TYPE_NONE, 1, FLATPAK_TYPE_INSTALLED_REF);

  /**
   * GbpFlatpakApplicationAddin::reload:
   * @self: An #GbpFlatpakApplicationAddin
   *
   * This signal is emitted when the addin reloads, which is generally
   * triggered by one of the flatpak installations changing, so other
   * components can indirectly monitor that.
   */
  signals [RELOAD] = g_signal_new ("reload",
                                   G_TYPE_FROM_CLASS (klass),
                                   G_SIGNAL_RUN_LAST,
                                   0,
                                   NULL, NULL, NULL,
                                   G_TYPE_NONE, 0);
}

static void
gbp_flatpak_application_addin_init (GbpFlatpakApplicationAddin *self)
{
}

static void
gbp_flatpak_application_addin_locate_sdk_worker (IdeTask      *task,
                                                 gpointer      source_object,
                                                 gpointer      task_data,
                                                 GCancellable *cancellable)
{
  LocateSdk *locate = task_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_TASK (task));
  g_assert (GBP_IS_FLATPAK_APPLICATION_ADDIN (source_object));
  g_assert (locate != NULL);
  g_assert (locate->id != NULL);
  g_assert (locate->arch != NULL);
  g_assert (locate->branch != NULL);
  g_assert (locate->installations != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_TRACE_MSG ("Locating SDK for %s/%s/%s",
                 locate->id, locate->arch, locate->branch);

  /*
   * First we'll try to resolve things by locating local items. This allows
   * us to avoid network traffic.
   */

  for (guint i = 0; i < locate->installations->len; i++)
    {
      InstallInfo *info = g_ptr_array_index (locate->installations, i);
      FlatpakInstallation *installation = info->installation;
      g_autoptr(GPtrArray) refs = NULL;

      g_assert (FLATPAK_IS_INSTALLATION (installation));

      refs = flatpak_installation_list_installed_refs_by_kind (installation,
                                                               FLATPAK_REF_KIND_RUNTIME,
                                                               cancellable, NULL);
      if (refs == NULL)
        continue;

      for (guint j = 0; j < refs->len; j++)
        {
          FlatpakInstalledRef *ref = g_ptr_array_index (refs, j);
          const gchar *id = flatpak_ref_get_name (FLATPAK_REF (ref));
          const gchar *arch = flatpak_ref_get_arch (FLATPAK_REF (ref));
          const gchar *branch = flatpak_ref_get_branch (FLATPAK_REF (ref));

          if (g_strcmp0 (locate->id, id) == 0 &&
              g_strcmp0 (locate->arch, arch) == 0 &&
              g_strcmp0 (locate->branch, branch) == 0)
            {
              g_autoptr(GBytes) bytes = NULL;
              g_autoptr(GKeyFile) keyfile = NULL;
              g_autofree gchar *idstr = NULL;
              const gchar *data;
              gsize len;

              bytes = flatpak_installed_ref_load_metadata (ref, cancellable, NULL);

              keyfile = g_key_file_new ();
              data = (gchar *)g_bytes_get_data (bytes, &len);

              if (!g_key_file_load_from_data (keyfile, data, len, 0, &error))
                {
                  ide_task_return_error (task, g_steal_pointer (&error));
                  IDE_EXIT;
                }

              idstr = g_key_file_get_string (keyfile, "Runtime", "sdk", NULL);

              if (idstr != NULL)
                {
                  g_auto(GStrv) parts = g_strsplit (idstr, "/", 3);

                  if (g_strv_length (parts) != 3)
                    {
                      ide_task_return_new_error (task,
                                                 G_IO_ERROR,
                                                 G_IO_ERROR_INVALID_DATA,
                                                 "Invalid runtime id %s",
                                                 idstr);
                      IDE_EXIT;
                    }

                  locate->sdk_id = g_strdup (parts[0]);
                  locate->sdk_arch = g_strdup (parts[1]);
                  locate->sdk_branch = g_strdup (parts[2]);
                }

              ide_task_return_boolean (task, TRUE);

              IDE_EXIT;
            }
        }
    }

  /*
   * Look through all of our remote refs and see if we find a match for
   * the runtime for which we need to locate the SDK. Afterwards, we need
   * to get the metadata for that runtime so that we can find the sdk field
   * which maps to another runtime.
   *
   * We might have to make a request to the server for the ref if we do not
   * have a cached copy of the file.
   */

  if (!ensure_remotes_exist_sync (cancellable, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  for (guint i = 0; i < locate->installations->len; i++)
    {
      InstallInfo *info = g_ptr_array_index (locate->installations, i);
      FlatpakInstallation *installation = info->installation;
      g_autoptr(GPtrArray) remotes = NULL;

      g_assert (FLATPAK_IS_INSTALLATION (installation));

      /* Refresh in case a new remote was added */
      flatpak_installation_drop_caches (installation, cancellable, NULL);

      remotes = flatpak_installation_list_remotes (installation, cancellable, NULL);
      if (remotes == NULL)
        continue;

      for (guint j = 0; j < remotes->len; j++)
        {
          FlatpakRemote *remote = g_ptr_array_index (remotes, j);
          const gchar *name = flatpak_remote_get_name (remote);
          g_autoptr(GPtrArray) refs = NULL;

          g_assert (FLATPAK_IS_REMOTE (remote));

          refs = flatpak_installation_list_remote_refs_sync (installation, name, cancellable, NULL);
          if (refs == NULL)
            continue;

          for (guint k = 0; k < refs->len; k++)
            {
              FlatpakRemoteRef *ref = g_ptr_array_index (refs, k);
              const gchar *id = flatpak_ref_get_name (FLATPAK_REF (ref));
              const gchar *arch = flatpak_ref_get_arch (FLATPAK_REF (ref));
              const gchar *branch = flatpak_ref_get_branch (FLATPAK_REF (ref));

              if (g_strcmp0 (locate->id, id) == 0 &&
                  g_strcmp0 (locate->arch, arch) == 0 &&
                  g_strcmp0 (locate->branch, branch) == 0)
                {
                  g_autoptr(GBytes) bytes = NULL;
                  g_autoptr(GKeyFile) keyfile = NULL;
                  g_autofree gchar *idstr = NULL;
                  const gchar *data;
                  gsize len;

                  bytes = flatpak_installation_fetch_remote_metadata_sync (installation,
                                                                           name,
                                                                           FLATPAK_REF (ref),
                                                                           cancellable,
                                                                           &error);

                  if (bytes == NULL)
                    {
                      ide_task_return_error (task, g_steal_pointer (&error));
                      IDE_EXIT;
                    }

                  keyfile = g_key_file_new ();
                  data = (gchar *)g_bytes_get_data (bytes, &len);

                  if (!g_key_file_load_from_data (keyfile, data, len, 0, &error))
                    {
                      ide_task_return_error (task, g_steal_pointer (&error));
                      IDE_EXIT;
                    }

                  idstr = g_key_file_get_string (keyfile, "Runtime", "sdk", NULL);

                  if (idstr != NULL)
                    {
                      g_auto(GStrv) parts = g_strsplit (idstr, "/", 3);

                      if (g_strv_length (parts) != 3)
                        {
                          ide_task_return_new_error (task,
                                                     G_IO_ERROR,
                                                     G_IO_ERROR_INVALID_DATA,
                                                     "Invalid runtime id %s",
                                                     idstr);
                          IDE_EXIT;
                        }

                      locate->sdk_id = g_strdup (parts[0]);
                      locate->sdk_arch = g_strdup (parts[1]);
                      locate->sdk_branch = g_strdup (parts[2]);
                    }

                  ide_task_return_boolean (task, TRUE);

                  IDE_EXIT;
                }
            }
        }
    }

  ide_task_return_new_error (task,
                             G_IO_ERROR,
                             G_IO_ERROR_NOT_FOUND,
                             "Failed to locate corresponding SDK");

  IDE_EXIT;
}

void
gbp_flatpak_application_addin_locate_sdk_async (GbpFlatpakApplicationAddin  *self,
                                                const gchar                 *runtime_id,
                                                const gchar                 *arch,
                                                const gchar                 *branch,
                                                GCancellable                *cancellable,
                                                GAsyncReadyCallback          callback,
                                                gpointer                     user_data)
{
  g_autoptr(IdeTask) task = NULL;
  LocateSdk *locate;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_FLATPAK_APPLICATION_ADDIN (self));
  g_assert (runtime_id != NULL);
  g_assert (arch != NULL);
  g_assert (branch != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_flatpak_application_addin_locate_sdk_async);
  ide_task_set_release_on_propagate (task, FALSE);

  locate = g_slice_new0 (LocateSdk);
  locate->id = g_strdup (runtime_id);
  locate->arch = g_strdup (arch);
  locate->branch = g_strdup (branch);
  locate->installations = g_ptr_array_ref (self->installations);

  ide_task_set_task_data (task, locate, locate_sdk_free);
  ide_task_run_in_thread (task, gbp_flatpak_application_addin_locate_sdk_worker);

  IDE_EXIT;
}

gboolean
gbp_flatpak_application_addin_locate_sdk_finish (GbpFlatpakApplicationAddin  *self,
                                                 GAsyncResult                *result,
                                                 gchar                      **sdk_id,
                                                 gchar                      **sdk_arch,
                                                 gchar                      **sdk_branch,
                                                 GError                     **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_APPLICATION_ADDIN (self));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  if (ret)
    {
      LocateSdk *state = ide_task_get_task_data (IDE_TASK (result));

      if (sdk_id)
        *sdk_id = g_strdup (state->sdk_id);

      if (sdk_arch)
        *sdk_arch = g_strdup (state->sdk_arch);

      if (sdk_branch)
        *sdk_branch = g_strdup (state->sdk_branch);
    }

  IDE_RETURN (ret);
}

static FlatpakInstalledRef *
gbp_flatpak_application_addin_find_ref (GbpFlatpakApplicationAddin *self,
                                        const gchar                *id,
                                        const gchar                *arch,
                                        const gchar                *branch)
{
  g_assert (IDE_IS_MAIN_THREAD ());

  for (guint i = 0; i < self->installations->len; i++)
    {
      InstallInfo *info = g_ptr_array_index (self->installations, i);
      g_autoptr(GPtrArray) ar = NULL;

      ar = flatpak_installation_list_installed_refs_by_kind (info->installation,
                                                             FLATPAK_REF_KIND_RUNTIME,
                                                             NULL,
                                                             NULL);

      if (ar != NULL)
        {
          for (guint j = 0; j < ar->len; j++)
            {
              FlatpakRef *ref = g_ptr_array_index (ar, j);

              if (g_strcmp0 (id, flatpak_ref_get_name (ref)) != 0)
                continue;

              if (arch && g_strcmp0 (arch, flatpak_ref_get_arch (ref)) != 0)
                continue;

              if (branch && g_strcmp0 (branch, flatpak_ref_get_branch (ref)) != 0)
                continue;

              return g_object_ref (FLATPAK_INSTALLED_REF (ref));
            }
        }
    }

  return NULL;
}

gchar *
gbp_flatpak_application_addin_get_deploy_dir (GbpFlatpakApplicationAddin *self,
                                              const gchar                *id,
                                              const gchar                *arch,
                                              const gchar                *branch)
{
  g_autoptr(FlatpakInstalledRef) ref = NULL;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (GBP_IS_FLATPAK_APPLICATION_ADDIN (self), NULL);
  g_return_val_if_fail (id, NULL);
  g_return_val_if_fail (arch, NULL);
  g_return_val_if_fail (branch, NULL);

  ref = gbp_flatpak_application_addin_find_ref (self, id, arch, branch);

  if (ref != NULL)
    return g_strdup (flatpak_installed_ref_get_deploy_dir (ref));

  return NULL;
}

static void
gbp_flatpak_application_addin_check_sysdeps_cb (GObject      *object,
                                                GAsyncResult *result,
                                                gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_SUBPROCESS (subprocess));
  g_return_if_fail (G_IS_ASYNC_RESULT (result));
  g_return_if_fail (IDE_IS_TASK (task));

  if (!ide_subprocess_wait_check_finish (subprocess, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

void
gbp_flatpak_application_addin_check_sysdeps_async (GbpFlatpakApplicationAddin *self,
                                                   GCancellable               *cancellable,
                                                   GAsyncReadyCallback         callback,
                                                   gpointer                    user_data)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (GBP_IS_FLATPAK_APPLICATION_ADDIN (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_flatpak_application_addin_check_sysdeps_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  if (ide_is_flatpak ())
    {
      /* We bundle flatpak-builder with Builder from flatpak */
      ide_task_return_boolean (task, TRUE);
      return;
    }

  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_SILENCE);
  ide_subprocess_launcher_set_clear_env (launcher, FALSE);
  ide_subprocess_launcher_push_argv (launcher, "which");
  ide_subprocess_launcher_push_argv (launcher, "flatpak-builder");

  subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, &error);

  if (subprocess == NULL)
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_GOTO (failure);
    }

  ide_subprocess_wait_check_async (subprocess,
                                   cancellable,
                                   gbp_flatpak_application_addin_check_sysdeps_cb,
                                   g_steal_pointer (&task));

failure:
  IDE_EXIT;
}

gboolean
gbp_flatpak_application_addin_check_sysdeps_finish (GbpFlatpakApplicationAddin  *self,
                                                    GAsyncResult                *result,
                                                    GError                     **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (GBP_IS_FLATPAK_APPLICATION_ADDIN (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

FlatpakInstalledRef *
gbp_flatpak_application_addin_find_extension (GbpFlatpakApplicationAddin *self,
                                              const gchar                *name)
{
  g_autofree gchar *pname = NULL;
  g_autofree gchar *parch = NULL;
  g_autofree gchar *pversion = NULL;

  g_return_val_if_fail (GBP_IS_FLATPAK_APPLICATION_ADDIN (self), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  if (strchr (name, '/') != NULL)
    {
      if (gbp_flatpak_split_id (name, &pname, &parch, &pversion))
        return gbp_flatpak_application_addin_find_ref (self, pname, parch, pversion);
    }

  return gbp_flatpak_application_addin_find_ref (self, name, NULL, NULL);
}
