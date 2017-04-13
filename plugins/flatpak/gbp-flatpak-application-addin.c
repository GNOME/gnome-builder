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

#define G_LOG_DOMAIN "gbp-flatpak-application-addin"

#include <flatpak.h>

#include "gbp-flatpak-application-addin.h"
#include "gbp-flatpak-runtime.h"

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
  IdeProgress         *progress;
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
};

typedef struct
{
  const gchar *name;
  const gchar *url;
} BuiltinFlatpakRepo;

enum {
  RUNTIME_ADDED,
  N_SIGNALS
};

static GbpFlatpakApplicationAddin *instance;
static guint signals [N_SIGNALS];
static BuiltinFlatpakRepo builtin_flatpak_repos[] = {
  { "gnome",         "https://sdk.gnome.org/gnome.flatpakrepo" },
  { "gnome-nightly", "https://sdk.gnome.org/gnome-nightly.flatpakrepo" },
};

static void gbp_flatpak_application_addin_reload (GbpFlatpakApplicationAddin *self);

static gboolean
is_ignored (FlatpakRef *ref)
{
  const gchar *name = flatpak_ref_get_name (ref);

  return g_str_has_suffix (name, ".Locale") ||
         g_str_has_suffix (name, ".Debug") ||
         g_str_has_suffix (name, ".Var");
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

  g_assert (G_IS_FILE_MONITOR (monitor));
  g_assert (G_IS_FILE (file));
  g_assert (!other_file || G_IS_FILE (other_file));
  g_assert (info != NULL);

  self = g_object_ref (info->self);

  gbp_flatpak_application_addin_reload (self);

  IDE_EXIT;
}

static void
install_info_free (InstallInfo *info)
{
  g_assert (info != NULL);
  g_assert (!info->installation || FLATPAK_IS_INSTALLATION (info->installation));
  g_assert (!info->monitor || G_IS_FILE_MONITOR (info->monitor));

  if (info->monitor != NULL)
    {
      g_signal_handlers_disconnect_by_func (info->monitor,
                                            G_CALLBACK (install_info_installation_changed),
                                            info);
    }

  ide_clear_weak_pointer (&info->self);
  g_clear_object (&info->monitor);
  g_clear_object (&info->installation);

  g_slice_free (InstallInfo, info);
}

static InstallInfo *
install_info_new (GbpFlatpakApplicationAddin *self,
                  FlatpakInstallation        *installation)
{
  InstallInfo *info;

  g_assert (GBP_IS_FLATPAK_APPLICATION_ADDIN (self));
  g_assert (FLATPAK_IS_INSTALLATION (installation));

  info = g_slice_new0 (InstallInfo);
  info->installation = g_object_ref (installation);
  info->monitor = flatpak_installation_create_monitor (installation, NULL, NULL);
  ide_set_weak_pointer (&info->self, self);

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

static gboolean
gbp_flatpak_application_addin_remove_old_repo (GbpFlatpakApplicationAddin  *self,
                                               GCancellable                *cancellable,
                                               GError                     **error)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) process = NULL;
  gboolean ret = FALSE;

  IDE_ENTRY;

  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_SILENCE | G_SUBPROCESS_FLAGS_STDERR_SILENCE);

  ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
  ide_subprocess_launcher_push_argv (launcher, "flatpak");
  ide_subprocess_launcher_push_argv (launcher, "remote-delete");
  ide_subprocess_launcher_push_argv (launcher, "--user");
  ide_subprocess_launcher_push_argv (launcher, "--force");
  ide_subprocess_launcher_push_argv (launcher, FLATPAK_REPO_NAME);

  process = ide_subprocess_launcher_spawn (launcher, cancellable, error);

  if (process != NULL)
    ret = ide_subprocess_wait (process, cancellable, error);

  IDE_RETURN (ret);
}

static void
gbp_flatpak_application_addin_reload (GbpFlatpakApplicationAddin *self)
{
  g_autofree gchar *user_path = NULL;
  g_autoptr(GFile) user_file = NULL;
  g_autoptr(GPtrArray) system_installs = NULL;
  g_autoptr(GPtrArray) runtimes = NULL;
  g_autoptr(FlatpakInstallation) user = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_APPLICATION_ADDIN (self));

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

  IDE_EXIT;
}

static void
gbp_flatpak_application_addin_load (IdeApplicationAddin *addin,
                                    IdeApplication      *application)
{
  GbpFlatpakApplicationAddin *self = (GbpFlatpakApplicationAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_APPLICATION_ADDIN (self));
  g_assert (IDE_IS_APPLICATION (application));

  instance = self;

  gbp_flatpak_application_addin_remove_old_repo (self, NULL, NULL);
  gbp_flatpak_application_addin_reload (self);

  IDE_EXIT;
}

static void
gbp_flatpak_application_addin_unload (IdeApplicationAddin *addin,
                                      IdeApplication      *application)
{
  GbpFlatpakApplicationAddin *self = (GbpFlatpakApplicationAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_APPLICATION_ADDIN (self));
  g_assert (IDE_IS_APPLICATION (application));

  instance = NULL;

  g_clear_pointer (&self->installations, g_ptr_array_unref);
  gbp_flatpak_application_addin_remove_old_repo (self, NULL, NULL);

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

  g_assert (GBP_IS_FLATPAK_APPLICATION_ADDIN (self));

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

              if (!is_ignored (FLATPAK_REF (ref)))
                g_ptr_array_add (ret, g_object_ref (ref));
            }
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
                                                 GTask                      *task)
{
  InstallRequest *request;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_APPLICATION_ADDIN (self));
  g_assert (pspec != NULL);
  g_assert (G_IS_TASK (task));

  request = g_task_get_task_data (task);

  if (request->ref != NULL && !request->did_added)
    {
      request->did_added = TRUE;
      g_signal_emit (self, signals[RUNTIME_ADDED], 0, request->ref);
    }

  IDE_EXIT;
}

static void
gbp_flatpak_application_addin_install_runtime_worker (GTask        *task,
                                                      gpointer      source_object,
                                                      gpointer      task_data,
                                                      GCancellable *cancellable)
{
  GbpFlatpakApplicationAddin *self = source_object;
  InstallRequest *request = task_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (G_IS_TASK (task));
  g_assert (GBP_IS_FLATPAK_APPLICATION_ADDIN (self));
  g_assert (request != NULL);
  g_assert (request->id != NULL);
  g_assert (request->arch != NULL);
  g_assert (request->branch != NULL);
  g_assert (request->installations != NULL);

  if (!ensure_remotes_exist_sync (cancellable, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
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
              g_strcmp0 (request->branch, branch) == 0 &&
              g_strcmp0 (request->arch, arch) == 0)
            {
              request->ref = flatpak_installation_update (installation,
                                                          FLATPAK_UPDATE_FLAGS_NONE,
                                                          FLATPAK_REF_KIND_RUNTIME,
                                                          request->id,
                                                          request->arch,
                                                          request->branch,
                                                          ide_progress_flatpak_progress_callback,
                                                          request->progress,
                                                          cancellable,
                                                          &error);

              if (request->ref == NULL)
                g_task_return_error (task, g_steal_pointer (&error));
              else
                g_task_return_boolean (task, TRUE);

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
                  g_strcmp0 (request->arch, arch) == 0 &&
                  g_strcmp0 (request->branch, branch) == 0)
                {
                  request->ref = flatpak_installation_install (installation,
                                                               name,
                                                               FLATPAK_REF_KIND_RUNTIME,
                                                               request->id,
                                                               request->arch,
                                                               request->branch,
                                                               ide_progress_flatpak_progress_callback,
                                                               request->progress,
                                                               cancellable,
                                                               &error);

                  if (request->ref != NULL)
                    g_task_return_boolean (task, TRUE);
                  else
                    g_task_return_error (task, g_steal_pointer (&error));

                  IDE_EXIT;
                }
            }
        }
    }

  g_task_return_new_error (task,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_FOUND,
                           "Failed to locate runtime within installed flatpak remotes");

  IDE_EXIT;
}

void
gbp_flatpak_application_addin_install_runtime_async (GbpFlatpakApplicationAddin  *self,
                                                     const gchar                 *runtime_id,
                                                     const gchar                 *arch,
                                                     const gchar                 *branch,
                                                     GCancellable                *cancellable,
                                                     IdeProgress                **progress,
                                                     GAsyncReadyCallback          callback,
                                                     gpointer                     user_data)
{
  g_autoptr(GTask) task = NULL;
  InstallRequest *request;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_APPLICATION_ADDIN (self));
  g_assert (runtime_id != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (self->installations != NULL);

  if (arch == NULL)
    arch = flatpak_get_default_arch ();

  if (branch == NULL)
    branch = "master";

  request = g_slice_new0 (InstallRequest);
  request->id = g_strdup (runtime_id);
  request->arch = g_strdup (arch);
  request->branch = g_strdup (branch);
  request->installations = g_ptr_array_ref (self->installations);
  request->progress = ide_progress_new ();

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gbp_flatpak_application_addin_install_runtime_async);
  g_task_set_task_data (task, request, (GDestroyNotify)install_request_free);

  g_signal_connect_object (task,
                           "notify::completed",
                           G_CALLBACK (gbp_flatpak_application_addin_install_completed),
                           self,
                           G_CONNECT_SWAPPED);

  if (progress != NULL)
    *progress = g_object_ref (request->progress);

  g_task_run_in_thread (task, gbp_flatpak_application_addin_install_runtime_worker);

  IDE_EXIT;
}

gboolean
gbp_flatpak_application_addin_install_runtime_finish (GbpFlatpakApplicationAddin  *self,
                                                      GAsyncResult                *result,
                                                      GError                     **error)
{
  InstallRequest *request;
  g_autoptr(GError) local_error = NULL;

  g_return_val_if_fail (GBP_IS_FLATPAK_APPLICATION_ADDIN (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  request = g_task_get_task_data (G_TASK (result));

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

  if (!g_task_propagate_boolean (G_TASK (result), &local_error))
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

  g_assert (GBP_IS_FLATPAK_APPLICATION_ADDIN (self));
  g_assert (id != NULL);
  g_assert (arch != NULL);
  g_assert (branch != NULL);

  IDE_TRACE_MSG ("Looking for runtime %s/%s/%s", id, arch, branch);

  ar = gbp_flatpak_application_addin_get_runtimes (self);

  if (ar != NULL)
    {
      for (guint i = 0; i < ar->len; i++)
        {
          FlatpakInstalledRef *ref = g_ptr_array_index (ar, i);
          const gchar *ref_id = flatpak_ref_get_name (FLATPAK_REF (ref));
          const gchar *ref_arch = flatpak_ref_get_arch (FLATPAK_REF (ref));
          const gchar *ref_branch = flatpak_ref_get_branch (FLATPAK_REF (ref));

          if (g_strcmp0 (id, ref_id) == 0 &&
              g_strcmp0 (arch, ref_arch) == 0 &&
              g_strcmp0 (branch, ref_branch) == 0)
            IDE_RETURN (TRUE);
        }
    }

  IDE_RETURN (FALSE);
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
                        0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_APPLICATION_ADDIN, application_addin_iface_init))

static void
gbp_flatpak_application_addin_class_init (GbpFlatpakApplicationAddinClass *klass)
{
  /**
   * GbpFlatpakApplicationAddin::runtime-added:
   * @self: An #GbpFlatpakApplicationAddin
   * @runtime: A #FlatpakInstalledRef
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
}

static void
gbp_flatpak_application_addin_init (GbpFlatpakApplicationAddin *self)
{
}

static void
gbp_flatpak_application_addin_locate_sdk_worker (GTask        *task,
                                                 gpointer      source_object,
                                                 gpointer      task_data,
                                                 GCancellable *cancellable)
{
  LocateSdk *locate = task_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (G_IS_TASK (task));
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
                  g_task_return_error (task, g_steal_pointer (&error));
                  IDE_EXIT;
                }

              idstr = g_key_file_get_string (keyfile, "Runtime", "sdk", NULL);

              if (idstr != NULL)
                {
                  g_auto(GStrv) parts = g_strsplit (idstr, "/", 3);

                  if (g_strv_length (parts) != 3)
                    {
                      g_task_return_new_error (task,
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

              g_task_return_boolean (task, TRUE);

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
      g_task_return_error (task, g_steal_pointer (&error));
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
                      g_task_return_error (task, g_steal_pointer (&error));
                      IDE_EXIT;
                    }

                  keyfile = g_key_file_new ();
                  data = (gchar *)g_bytes_get_data (bytes, &len);

                  if (!g_key_file_load_from_data (keyfile, data, len, 0, &error))
                    {
                      g_task_return_error (task, g_steal_pointer (&error));
                      IDE_EXIT;
                    }

                  idstr = g_key_file_get_string (keyfile, "Runtime", "sdk", NULL);

                  if (idstr != NULL)
                    {
                      g_auto(GStrv) parts = g_strsplit (idstr, "/", 3);

                      if (g_strv_length (parts) != 3)
                        {
                          g_task_return_new_error (task,
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

                  g_task_return_boolean (task, TRUE);

                  IDE_EXIT;
                }
            }
        }
    }

  g_task_return_new_error (task,
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
  g_autoptr(GTask) task = NULL;
  LocateSdk *locate;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_APPLICATION_ADDIN (self));
  g_assert (runtime_id != NULL);
  g_assert (arch != NULL);
  g_assert (branch != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gbp_flatpak_application_addin_locate_sdk_async);

  locate = g_slice_new0 (LocateSdk);
  locate->id = g_strdup (runtime_id);
  locate->arch = g_strdup (arch);
  locate->branch = g_strdup (branch);
  locate->installations = g_ptr_array_ref (self->installations);

  g_task_set_task_data (task, locate, (GDestroyNotify)locate_sdk_free);
  g_task_run_in_thread (task, gbp_flatpak_application_addin_locate_sdk_worker);

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
  g_assert (G_IS_TASK (result));

  ret = g_task_propagate_boolean (G_TASK (result), error);

  if (ret)
    {
      LocateSdk *state = g_task_get_task_data (G_TASK (result));

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

              if (g_strcmp0 (id, flatpak_ref_get_name (ref)) == 0 &&
                  g_strcmp0 (arch, flatpak_ref_get_arch (ref)) == 0 &&
                  g_strcmp0 (branch, flatpak_ref_get_branch (ref)) == 0)
                return g_object_ref (ref);
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
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_SUBPROCESS (subprocess));
  g_return_if_fail (G_IS_ASYNC_RESULT (result));
  g_return_if_fail (G_IS_TASK (task));

  if (!ide_subprocess_wait_check_finish (subprocess, result, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_boolean (task, TRUE);

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
  g_autoptr(GTask) task = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_return_if_fail (GBP_IS_FLATPAK_APPLICATION_ADDIN (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gbp_flatpak_application_addin_check_sysdeps_async);

  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_SILENCE);
  ide_subprocess_launcher_set_clear_env (launcher, FALSE);
  ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
  ide_subprocess_launcher_push_argv (launcher, "which");
  ide_subprocess_launcher_push_argv (launcher, "flatpak-builder");

  subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, &error);

  if (subprocess == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
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

  g_return_val_if_fail (GBP_IS_FLATPAK_APPLICATION_ADDIN (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  ret = g_task_propagate_boolean (G_TASK (result), error);

  IDE_RETURN (ret);
}
