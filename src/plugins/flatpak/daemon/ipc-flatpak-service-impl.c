/* ipc-flatpak-service-impl.c
 *
 * Copyright 2019-2023 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ipc-flatpak-service-impl"

#include "config.h"

#include <flatpak/flatpak.h>
#include <glib/gi18n.h>

#include "ipc-flatpak-repo.h"
#include "ipc-flatpak-service-impl.h"
#include "ipc-flatpak-transfer.h"
#include "ipc-flatpak-util.h"

typedef struct
{
  FlatpakInstallation *installation;
  char *name;
  char *arch;
  char *branch;
  char *sdk_name;
  char *sdk_branch;
  char *deploy_dir;
  GBytes *metadata;
  gboolean sdk_extension : 1;
} Runtime;

typedef struct
{
  FlatpakInstallation *installation;
  GFileMonitor *monitor;
  guint is_private : 1;
} Install;

typedef struct
{
  GDBusMethodInvocation *invocation;
  GPtrArray             *installs;
  FlatpakRef            *target;
} IsKnown;

typedef struct
{
  GDBusMethodInvocation *invocation;
  GPtrArray             *installs;
  char                  *sdk;
  char                  *extension;
} ResolveExtensionState;

struct _IpcFlatpakServiceImpl
{
  IpcFlatpakServiceSkeleton parent;
  GHashTable *installs;
  GPtrArray *runtimes;
  GPtrArray *installs_ordered;
  GHashTable *known;
  guint ignore_system_installations : 1;
};

static void      ipc_flatpak_service_impl_install_changed_cb (IpcFlatpakServiceImpl  *self,
                                                              GFile                  *file,
                                                              GFile                  *other_file,
                                                              GFileMonitorEvent       event,
                                                              GFileMonitor           *monitor);
static gboolean  ipc_flatpak_service_impl_add_installation   (IpcFlatpakService      *service,
                                                              GDBusMethodInvocation  *invocation,
                                                              const char             *path,
                                                              gboolean                is_user);
static void      add_runtime                                 (IpcFlatpakServiceImpl  *service,
                                                              Runtime                *runtime);
static Install  *install_new                                 (IpcFlatpakServiceImpl  *self,
                                                              FlatpakInstallation    *installation,
                                                              GError                **error);
static void      install_free                                (Install                *install);
static void      install_reload                              (IpcFlatpakServiceImpl  *self,
                                                              Install                *install);
static void      runtime_free                                (Runtime                *runtime);
static gboolean  runtime_equal                               (const Runtime          *a,
                                                              const Runtime          *b);
static void      is_known_free                               (IsKnown                *state);

enum {
  PROP_0,
  PROP_IGNORE_SYSTEM_INSTALLATIONS,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
resolve_extension_state_free (ResolveExtensionState *state)
{
  g_clear_pointer (&state->sdk, g_free);
  g_clear_pointer (&state->extension, g_free);
  g_clear_pointer (&state->installs, g_ptr_array_unref);
  g_clear_object (&state->invocation);
  g_slice_free (ResolveExtensionState, state);
}

static inline gboolean
str_equal0 (const char *a,
            const char *b)
{
  return g_strcmp0 (a, b) == 0;
}

static inline gboolean
str_empty0 (const char *s)
{
  return !s || !*s;
}

gboolean
split_id (const char  *str,
          char       **id,
          char       **arch,
          char       **branch)
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
      if (branch != NULL && !str_empty0 (parts[i]))
        *branch = g_strdup (parts[i]);
    }

  return TRUE;
}

static void
is_known_free (IsKnown *state)
{
  g_clear_pointer (&state->installs, g_ptr_array_unref);
  g_clear_object (&state->invocation);
  g_clear_object (&state->target);
  g_slice_free (IsKnown, state);
}

static GVariant *
runtime_to_variant (const Runtime *runtime)
{
  return runtime_variant_new (runtime->name,
                              runtime->arch,
                              runtime->branch,
                              runtime->sdk_name,
                              runtime->sdk_branch,
                              runtime->deploy_dir,
                              (const char *)g_bytes_get_data (runtime->metadata, NULL),
                              runtime->sdk_extension);
}

static void
add_runtime (IpcFlatpakServiceImpl *self,
             Runtime               *runtime)
{
  g_autoptr(GVariant) variant = NULL;

  g_assert (IPC_IS_FLATPAK_SERVICE_IMPL (self));
  g_assert (runtime != NULL);

  for (guint i = 0; i < self->runtimes->len; i++)
    {
      const Runtime *other = g_ptr_array_index (self->runtimes, i);

      /* Ignore if we know about it already */
      if (runtime_equal (other, runtime))
        return;
    }

  variant = runtime_to_variant (runtime);
  g_ptr_array_add (self->runtimes, g_steal_pointer (&runtime));

  ipc_flatpak_service_emit_runtime_added (IPC_FLATPAK_SERVICE (self), variant);
}

static gboolean
is_installed (IpcFlatpakServiceImpl *self,
              FlatpakRef            *ref)
{
  g_autofree char *key = NULL;
  const char *lookup_name;
  const char *lookup_arch;
  const char *lookup_branch;

  g_assert (IPC_IS_FLATPAK_SERVICE_IMPL (self));
  g_assert (FLATPAK_IS_REF (ref));

  lookup_name = flatpak_ref_get_name (ref);
  lookup_arch = flatpak_ref_get_arch (ref);
  lookup_branch = flatpak_ref_get_branch (ref);

  key = g_strdup_printf ("%s/%s/%s", lookup_name, lookup_arch, lookup_branch);

  if (g_hash_table_contains (self->known, key))
    return TRUE;

  for (guint i = 0; i < self->runtimes->len; i++)
    {
      const Runtime *r = g_ptr_array_index (self->runtimes, i);

      if (str_equal0 (r->name, lookup_name) &&
          str_equal0 (r->arch, lookup_arch) &&
          str_equal0 (r->branch, lookup_branch))
        return TRUE;
    }

  if (flatpak_ref_get_kind (ref) == FLATPAK_REF_KIND_APP)
    {
      g_autoptr(GPtrArray) remotes = NULL;
      g_autoptr(GError) error = NULL;

      for (guint i = 0; i < self->installs_ordered->len; i++)
        {
          g_autoptr(FlatpakInstalledRef) installed_ref = NULL;
          const Install *install = g_ptr_array_index (self->installs_ordered, i);

          installed_ref = flatpak_installation_get_installed_ref (install->installation,
                                                                  FLATPAK_REF_KIND_APP,
                                                                  lookup_name,
                                                                  lookup_arch,
                                                                  lookup_branch,
                                                                  NULL,
                                                                  NULL);
          if (installed_ref)
            return TRUE;
        }
    }

  return FALSE;
}

static void
runtime_free (Runtime *runtime)
{
  g_clear_pointer (&runtime->name, g_free);
  g_clear_pointer (&runtime->arch, g_free);
  g_clear_pointer (&runtime->branch, g_free);
  g_clear_pointer (&runtime->sdk_name, g_free);
  g_clear_pointer (&runtime->sdk_branch, g_free);
  g_clear_pointer (&runtime->deploy_dir, g_free);
  g_clear_pointer (&runtime->metadata, g_bytes_unref);
  g_slice_free (Runtime, runtime);
}

static gboolean
runtime_equal (const Runtime *a,
               const Runtime *b)
{
  return str_equal0 (a->name, b->name) &&
         str_equal0 (a->arch, b->arch) &&
         str_equal0 (a->branch, b->branch);
}

static Install *
install_new (IpcFlatpakServiceImpl  *self,
             FlatpakInstallation    *installation,
             GError                **error)
{
  g_autoptr(GFileMonitor) monitor = NULL;
  Install *ret;

  if (!(monitor = flatpak_installation_create_monitor (installation, NULL, error)))
    return NULL;

  ret = g_slice_new0 (Install);
  ret->installation = g_object_ref (installation);
  ret->monitor = g_steal_pointer (&monitor);

  g_signal_connect_object (ret->monitor,
                           "changed",
                           G_CALLBACK (ipc_flatpak_service_impl_install_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  return g_steal_pointer (&ret);
}

static void
install_free (Install *install)
{
  g_clear_object (&install->installation);
  g_clear_object (&install->monitor);
  g_slice_free (Install, install);
}

static FlatpakInstalledRef *
find_sdk (GPtrArray  *runtimes,
          FlatpakRef *match)
{
  for (guint i = 0; i < runtimes->len; i++)
    {
      FlatpakInstalledRef *ref = g_ptr_array_index (runtimes, i);

      if (g_strcmp0 (flatpak_ref_get_name (FLATPAK_REF (ref)),
                     flatpak_ref_get_name (match)) == 0 &&
          g_strcmp0 (flatpak_ref_get_arch (FLATPAK_REF (ref)),
                     flatpak_ref_get_arch (match)) == 0 &&
          g_strcmp0 (flatpak_ref_get_branch (FLATPAK_REF (ref)),
                     flatpak_ref_get_branch (match)) == 0)
        return ref;
    }

  return NULL;
}

static void
install_reload (IpcFlatpakServiceImpl *self,
                Install               *install)
{
  g_autoptr(GPtrArray) refs = NULL;

  g_assert (IPC_IS_FLATPAK_SERVICE_IMPL (self));
  g_assert (install != NULL);

  /* XXX: This current ignores removing known runtimes. For now, if you do that,
   *      just restart Builder to have that picked up.
   */

  if (!(refs = flatpak_installation_list_installed_refs_by_kind (install->installation,
                                                                 FLATPAK_REF_KIND_RUNTIME,
                                                                 NULL, NULL)))
    return;

  for (guint i = 0; i < refs->len; i++)
    {
      FlatpakInstalledRef *ref = g_ptr_array_index (refs, i);
      FlatpakInstalledRef *installed_sdk;
      g_autoptr(FlatpakRef) sdk_ref = NULL;
      g_autoptr(GBytes) bytes = NULL;
      g_autoptr(GKeyFile) keyfile = g_key_file_new ();
      const char *deploy_dir = NULL;
      g_autofree char *sdk_full_ref = NULL;
      g_autofree char *name = NULL;
      g_autofree char *runtime = NULL;
      g_autofree char *sdk = NULL;
      g_autofree char *exten_of = NULL;
      Runtime *state;

      g_hash_table_insert (self->known,
                           g_strdup_printf ("%s/%s/%s",
                                            flatpak_ref_get_name (FLATPAK_REF (ref)),
                                            flatpak_ref_get_arch (FLATPAK_REF (ref)),
                                            flatpak_ref_get_branch (FLATPAK_REF (ref))),
                           NULL);

      if (!(bytes = flatpak_installed_ref_load_metadata (ref, NULL, NULL)) ||
          !g_key_file_load_from_bytes (keyfile, bytes, G_KEY_FILE_NONE, NULL))
        continue;

      if (!(name = g_key_file_get_string (keyfile, "Runtime", "name", NULL)) ||
          !(runtime = g_key_file_get_string (keyfile, "Runtime", "runtime", NULL)) ||
          !(sdk = g_key_file_get_string (keyfile, "Runtime", "sdk", NULL)))
        {
          /* Special case .Docs so they show up in our list of runtimes
           * but treat them as if they were just the SDK. That way we can
           * extract information from them for manuals/etc but also filter
           * them from the user selecting as an option.
           */
          if (name != NULL && g_str_has_suffix (name, ".Docs"))
            {
              g_autofree char *shortname = g_strndup (name, strlen (name) - strlen (".Docs"));

              g_clear_pointer (&runtime, g_free);
              g_clear_pointer (&sdk, g_free);

              runtime = g_strdup_printf ("%s/%s/%s",
                                         flatpak_ref_get_name (FLATPAK_REF (ref)),
                                         flatpak_ref_get_arch (FLATPAK_REF (ref)),
                                         flatpak_ref_get_branch (FLATPAK_REF (ref)));
              sdk = g_strdup (runtime);

              goto special_case_docs;
            }

          continue;
        }

      if (g_key_file_has_group (keyfile, "ExtensionOf"))
        {
          /* Skip if this item is an extension, but not an SDK extension */
          if (!g_key_file_has_key (keyfile, "ExtensionOf", "ref", NULL) ||
              strstr (name, ".Extension.") == NULL)
            continue;

          exten_of = g_key_file_get_string (keyfile, "ExtensionOf", "ref", NULL);
        }

special_case_docs:

      if (!g_str_has_prefix (sdk, "runtime/"))
        sdk_full_ref = g_strdup_printf ("runtime/%s", sdk);
      else
        sdk_full_ref = g_strdup (sdk);

      /* Make sure we can parse the SDK reference */
      if (!(sdk_ref = flatpak_ref_parse (sdk_full_ref, NULL)))
        continue;

      /* Try to locate the installed SDK so that we can get its deploy
       * directory instead of the runtime (or the application will not
       * be able to locate includes/pkg-config/etc when building).
       */
      if ((installed_sdk = find_sdk (refs, sdk_ref)) && exten_of == NULL)
        deploy_dir = flatpak_installed_ref_get_deploy_dir (installed_sdk);
      else
        deploy_dir = flatpak_installed_ref_get_deploy_dir (ref);

      state = g_slice_new0 (Runtime);
      state->installation = g_object_ref (install->installation);
      state->name = g_strdup (flatpak_ref_get_name (FLATPAK_REF (ref)));
      state->arch = g_strdup (flatpak_ref_get_arch (FLATPAK_REF (ref)));
      state->branch = g_strdup (flatpak_ref_get_branch (FLATPAK_REF (ref)));
      state->sdk_name = g_strdup (flatpak_ref_get_name (sdk_ref));
      state->sdk_branch = g_strdup (flatpak_ref_get_branch (sdk_ref));
      state->deploy_dir = g_strdup (deploy_dir);
      state->sdk_extension = exten_of != NULL;
      state->metadata = g_bytes_ref (bytes);

      add_runtime (self, g_steal_pointer (&state));
    }
}

static gboolean
is_private_install (const Install *install)
{
  return g_strcmp0 ("gnome-builder-private",
                    flatpak_installation_get_id (install->installation)) == 0;
}

static gboolean
add_installation (IpcFlatpakServiceImpl  *self,
                  FlatpakInstallation    *installation,
                  Install               **out_install,
                  GError                **error)
{
  g_autoptr(GFile) file = NULL;
  Install *install;
  int position = -1;

  g_assert (IPC_IS_FLATPAK_SERVICE_IMPL (self));
  g_assert (FLATPAK_IS_INSTALLATION (installation));

  if (out_install != NULL)
    *out_install = NULL;

  file = flatpak_installation_get_path (installation);

  if (!(install = install_new (self, installation, error)))
    return FALSE;

  for (guint i = 0; i < self->installs_ordered->len; i++)
    {
      const Install *other = g_ptr_array_index (self->installs_ordered, i);

      if ((flatpak_installation_get_is_user (install->installation) &&
           !flatpak_installation_get_is_user (other->installation)) ||
          is_private_install (other))
        {
          position = i;
          break;
        }
    }

  if (position > -1)
    g_ptr_array_insert (self->installs_ordered, position, install);
  else
    g_ptr_array_add (self->installs_ordered, install);

  g_hash_table_insert (self->installs,
                       g_steal_pointer (&file),
                       install);

  install_reload (self, install);

  if (out_install != NULL)
    *out_install = install;

  return TRUE;
}

static gboolean
ipc_flatpak_service_impl_add_installation (IpcFlatpakService     *service,
                                           GDBusMethodInvocation *invocation,
                                           const char            *path,
                                           gboolean               is_user)
{
  IpcFlatpakServiceImpl *self = (IpcFlatpakServiceImpl *)service;
  g_autoptr(FlatpakInstallation) installation = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) file = NULL;

  g_assert (IPC_IS_FLATPAK_SERVICE_IMPL (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (path != NULL);

  g_debug ("AddInstallation(%s, is_user=%d)", path, is_user);

  file = g_file_new_for_path (path);

  if (!g_hash_table_contains (self->installs, file))
    {
      if (!(installation = flatpak_installation_new_for_path (file, is_user, NULL, &error)) ||
          !add_installation (self, installation, NULL, &error))
        return complete_wrapped_error (invocation, error);
    }

  ipc_flatpak_service_complete_add_installation (service, invocation);

  return TRUE;
}

static gboolean
ipc_flatpak_service_impl_list_runtimes (IpcFlatpakService     *service,
                                        GDBusMethodInvocation *invocation)
{
  IpcFlatpakServiceImpl *self = (IpcFlatpakServiceImpl *)service;
  GVariantBuilder builder;

  g_assert (IPC_IS_FLATPAK_SERVICE_IMPL (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));

  g_debug ("ListRuntimes()");

  g_variant_builder_init (&builder, RUNTIME_ARRAY_VARIANT_TYPE);

  for (guint i = 0; i < self->runtimes->len; i++)
    {
      const Runtime *runtime = g_ptr_array_index (self->runtimes, i);
      g_autoptr(GVariant) item = runtime_to_variant (runtime);
      g_variant_builder_add_value (&builder, item);
    }

  ipc_flatpak_service_complete_list_runtimes (service,
                                              g_steal_pointer (&invocation),
                                              g_variant_builder_end (&builder));

  return TRUE;
}

static void
is_known_worker (GTask        *task,
                 gpointer      source_object,
                 gpointer      task_data,
                 GCancellable *cancellable)
{
  g_autoptr(GPtrArray) remotes = NULL;
  g_autoptr(GError) error = NULL;
  IsKnown *state = task_data;
  const char *ref_name;
  const char *ref_arch;
  const char *ref_branch;
  gint64 download_size = 0;
  gboolean found = FALSE;
  FlatpakQueryFlags flags = 0;

  g_assert (G_IS_TASK (task));
  g_assert (IPC_IS_FLATPAK_SERVICE_IMPL (source_object));
  g_assert (state != NULL);
  g_assert (state->installs != NULL);
  g_assert (G_IS_DBUS_METHOD_INVOCATION (state->invocation));
  g_assert (FLATPAK_IS_REF (state->target));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  ref_name = flatpak_ref_get_name (state->target);
  ref_arch = flatpak_ref_get_arch (state->target);
  ref_branch = flatpak_ref_get_branch (state->target);

  if (str_equal0 (ref_arch, flatpak_get_default_arch ()))
    flags |= FLATPAK_QUERY_FLAGS_ONLY_CACHED;
#if FLATPAK_CHECK_VERSION(1, 11, 2)
  else
    flags |= FLATPAK_QUERY_FLAGS_ALL_ARCHES;
#endif

  g_debug ("%u installs available for IsKnown query",
           state->installs->len);

  for (guint z = 0; z < state->installs->len; z++)
    {
      FlatpakInstallation *install = g_ptr_array_index (state->installs, z);

      if (!(remotes = flatpak_installation_list_remotes (install, NULL, &error)))
        continue;

      g_debug ("%u remotes found for installation %u", remotes->len, z);

      for (guint i = 0; i < remotes->len; i++)
        {
          FlatpakRemote *remote = g_ptr_array_index (remotes, i);
          const char *remote_name = flatpak_remote_get_name (remote);
          g_autoptr(GPtrArray) refs = NULL;

          g_debug ("Checking remote \"%s\" with flags 0x%x", remote_name, flags);
          if (!(refs = flatpak_installation_list_remote_refs_sync_full (install, remote_name, flags, NULL, NULL)))
            {
              g_debug ("Failed to access refs, skipping");
              continue;
            }

          g_debug ("%u remote refs found", refs->len);

          for (guint j = 0; j < refs->len; j++)
            {
              FlatpakRemoteRef *remote_ref = g_ptr_array_index (refs, j);

#if 0
              g_debug ("Found remote ref: %s/%s/%s",
                       flatpak_ref_get_name (FLATPAK_REF (remote_ref)),
                       flatpak_ref_get_arch (FLATPAK_REF (remote_ref)),
                       flatpak_ref_get_branch (FLATPAK_REF (remote_ref)));
#endif

              if (str_equal0 (ref_name, flatpak_ref_get_name (FLATPAK_REF (remote_ref))) &&
                  str_equal0 (ref_arch, flatpak_ref_get_arch (FLATPAK_REF (remote_ref))) &&
                  str_equal0 (ref_branch, flatpak_ref_get_branch (FLATPAK_REF (remote_ref))))
                {
                  found = TRUE;
                  download_size = flatpak_remote_ref_get_download_size (remote_ref);
                  goto finish;
                }
            }
        }
    }

finish:
  g_debug ("RuntimeIsKnown => (%d, %"G_GINT64_FORMAT")", found, download_size);
  ipc_flatpak_service_complete_runtime_is_known (g_task_get_source_object (task),
                                                 g_steal_pointer (&state->invocation),
                                                 found,
                                                 download_size);

  if (error != NULL)
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_boolean (task, TRUE);
}

static gboolean
ipc_flatpak_service_impl_runtime_is_known (IpcFlatpakService     *service,
                                           GDBusMethodInvocation *invocation,
                                           const char            *name)
{
  IpcFlatpakServiceImpl *self = (IpcFlatpakServiceImpl *)service;
  g_autofree char *full_name = NULL;
  g_autoptr(FlatpakRef) ref = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = NULL;
  const char *ref_name;
  const char *ref_arch;
  const char *ref_branch;
  IsKnown *state;

  g_debug ("RuntimeIsKnown(%s)", name);

  g_assert (IPC_IS_FLATPAK_SERVICE_IMPL (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (name != NULL);

  /* Homogenize names into runtime/name/arch/branch */
  if (!g_str_has_prefix (name, "runtime/") && !g_str_has_prefix (name, "app/"))
    {
      full_name = g_strdup_printf ("runtime/%s", name);
      name = full_name;
    }

  /* Parse the ref, so we can try to locate it */
  if (!(ref = flatpak_ref_parse (name, &error)))
    return complete_wrapped_error (g_steal_pointer (&invocation), error);

  ref_name = flatpak_ref_get_name (ref);
  ref_arch = flatpak_ref_get_arch (ref);
  ref_branch = flatpak_ref_get_branch (ref);

  /* First check if we know about the runtime from those installed */
  for (guint i = 0; i < self->runtimes->len; i++)
    {
      const Runtime *runtime = g_ptr_array_index (self->runtimes, i);

      if (str_equal0 (ref_name, runtime->name) &&
          str_equal0 (ref_arch, runtime->arch) &&
          str_equal0 (ref_branch, runtime->branch))
        {
          ipc_flatpak_service_complete_runtime_is_known (service,
                                                         g_steal_pointer (&invocation),
                                                         TRUE, 0);
          return TRUE;
        }
    }

  state = g_slice_new0 (IsKnown);
  state->installs = g_ptr_array_new_with_free_func (g_object_unref);
  state->target = g_object_ref (ref);
  state->invocation = g_steal_pointer (&invocation);

  task = g_task_new (self, NULL, NULL, NULL);
  g_task_set_source_tag (task, ipc_flatpak_service_impl_runtime_is_known);
  g_task_set_task_data (task, state, (GDestroyNotify) is_known_free);

  for (guint i = 0; i < self->installs_ordered->len; i++)
    {
      const Install *install = g_ptr_array_index (self->installs_ordered, i);
      g_ptr_array_add (state->installs, g_object_ref (install->installation));
    }

  g_task_run_in_thread (task, is_known_worker);

  return TRUE;
}

static void
ipc_flatpak_service_impl_install_changed_cb (IpcFlatpakServiceImpl *self,
                                             GFile                 *file,
                                             GFile                 *other_file,
                                             GFileMonitorEvent      event,
                                             GFileMonitor          *monitor)
{
  GHashTableIter iter;
  Install *install;

  g_assert (IPC_IS_FLATPAK_SERVICE_IMPL (self));
  g_assert (G_IS_FILE (file));
  g_assert (!other_file || G_IS_FILE (other_file));
  g_assert (G_IS_FILE_MONITOR (monitor));

  g_hash_table_iter_init (&iter, self->installs);

  while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&install))
    {
      if (install->monitor == monitor)
        {
          install_reload (self, install);
          break;
        }
    }
}

typedef struct
{
  FlatpakRef *fref;
  char *ref;
  char *remote;
  guint update : 1;
} InstallRef;

typedef struct
{
  GPtrArray             *installations;
  GDBusMethodInvocation *invocation;
  IpcFlatpakTransfer    *transfer;
  char                  *parent_window;
  GArray                *refs;
  GCancellable          *cancellable;
} InstallState;

static void
install_state_free (InstallState *state)
{
  g_clear_pointer (&state->installations, g_ptr_array_unref);
  g_clear_object (&state->invocation);
  g_clear_object (&state->transfer);
  g_clear_object (&state->cancellable);
  g_clear_pointer (&state->refs, g_array_unref);
  g_clear_pointer (&state->parent_window, g_free);
  g_slice_free (InstallState, state);
}

static void
on_progress_changed (FlatpakTransactionProgress *progress,
                     IpcFlatpakTransfer         *transfer)
{
  g_autofree char *message = NULL;
  int val;

  g_assert (FLATPAK_IS_TRANSACTION_PROGRESS (progress));
  g_assert (IPC_IS_FLATPAK_TRANSFER (transfer));

  val = flatpak_transaction_progress_get_progress (progress);
  message = flatpak_transaction_progress_get_status (progress);

  ipc_flatpak_transfer_set_message (transfer, message);
  ipc_flatpak_transfer_set_fraction (transfer, (double)val / 100.0);
}

static void
on_new_operation_cb (FlatpakTransaction          *transaction,
                     FlatpakTransactionOperation *operation,
                     FlatpakTransactionProgress  *progress,
                     IpcFlatpakTransfer          *transfer)
{
  g_assert (FLATPAK_IS_TRANSACTION (transaction));
  g_assert (FLATPAK_IS_TRANSACTION_OPERATION (operation));
  g_assert (FLATPAK_IS_TRANSACTION_PROGRESS (progress));
  g_assert (IPC_IS_FLATPAK_TRANSFER (transfer));

  g_signal_connect_object (progress,
                           "changed",
                           G_CALLBACK (on_progress_changed),
                           transfer,
                           0);
  on_progress_changed (progress, transfer);
}

static void
install_worker (GTask        *task,
                gpointer      source_object,
                gpointer      task_data,
                GCancellable *cancellable)
{
  InstallState *state = task_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GPtrArray) ref_ids = NULL;
  g_autoptr(GHashTable) transactions = NULL;
  GHashTableIter iter;
  gpointer k, v;

  g_assert (G_IS_TASK (task));
  g_assert (IPC_IS_FLATPAK_SERVICE_IMPL (source_object));
  g_assert (state != NULL);
  g_assert (state->refs != NULL);
  g_assert (G_IS_DBUS_METHOD_INVOCATION (state->invocation));
  g_assert (IPC_IS_FLATPAK_TRANSFER (state->transfer));
  g_assert (state->installations != NULL);
  g_assert (state->installations->len == state->refs->len);
  g_assert (state->installations->len > 0);

  transactions = g_hash_table_new_full (NULL, NULL, NULL, g_object_unref);

  ipc_flatpak_transfer_set_fraction (state->transfer, 0.0);
  ipc_flatpak_transfer_set_message (state->transfer, "");

  /* Get list of refs to user confirmation */
  ref_ids = g_ptr_array_new ();
  for (guint i = 0; i < state->refs->len; i++)
    g_ptr_array_add (ref_ids, (gpointer)g_array_index (state->refs, InstallRef, i).ref);
  g_ptr_array_add (ref_ids, NULL);

  /* Ask for user confirmation we can go ahead and install these */
  if (!ipc_flatpak_transfer_call_confirm_sync (state->transfer, (const char * const *)ref_ids->pdata, state->cancellable, &error))
    {
      g_warning ("User confirmation failed: %s", error->message);
      complete_wrapped_error (g_steal_pointer (&state->invocation), g_error_copy (error));
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  /* Add refs to transactions, one transaction per FlatpakInstallation */
  for (guint i = 0; i < state->refs->len; i++)
    {
      const InstallRef *ir = &g_array_index (state->refs, InstallRef, i);
      FlatpakInstallation *installation = g_ptr_array_index (state->installations, i);
      FlatpakTransaction *transaction;

      if (!(transaction = g_hash_table_lookup (transactions, installation)))
        {
          if (!(transaction = flatpak_transaction_new_for_installation (installation, state->cancellable, &error)))
            {
              g_warning ("Failed to create transaction: %s", error->message);
              complete_wrapped_error (g_steal_pointer (&state->invocation), g_error_copy (error));
              g_task_return_error (task, g_steal_pointer (&error));
              return;
            }

          flatpak_transaction_set_disable_related (transaction, TRUE);

          g_signal_connect_object (transaction,
                                   "new-operation",
                                   G_CALLBACK (on_new_operation_cb),
                                   state->transfer,
                                   0);

          g_hash_table_insert (transactions, installation, transaction);
        }

      /* Add ref as install or update to installation's transaction */
      if (ir->update)
        {
          if (!flatpak_transaction_add_update (transaction, ir->ref, NULL, NULL, &error))
            {
              g_warning ("Failed to add update ref to transaction: %s", error->message);
              complete_wrapped_error (g_steal_pointer (&state->invocation), g_error_copy (error));
              g_task_return_error (task, g_steal_pointer (&error));
              return;
            }
        }
      else
        {
          if (!flatpak_transaction_add_install (transaction, ir->remote, ir->ref, NULL, &error))
            {
              g_warning ("Failed to add install ref to transaction: %s", error->message);
              complete_wrapped_error (g_steal_pointer (&state->invocation), g_error_copy (error));
              g_task_return_error (task, g_steal_pointer (&error));
              return;
            }
        }
    }

  /* Now process all of the transactions */
  g_hash_table_iter_init (&iter, transactions);
  while (g_hash_table_iter_next (&iter, &k, &v))
    {
      G_GNUC_UNUSED FlatpakInstallation *installation = k;
      FlatpakTransaction *transaction = v;

      g_assert (FLATPAK_IS_INSTALLATION (installation));
      g_assert (FLATPAK_IS_TRANSACTION (transaction));

      if (!flatpak_transaction_run (transaction, state->cancellable, &error))
        {
          g_warning ("Failed to execute transaction: %s", error->message);
          ipc_flatpak_transfer_set_fraction (state->transfer, 1.0);
          ipc_flatpak_transfer_set_message (state->transfer, _("Installation failed"));
          complete_wrapped_error (g_steal_pointer (&state->invocation), g_error_copy (error));
          g_task_return_error (task, g_steal_pointer (&error));
          return;
        }
    }

  g_debug ("Installation complete");

  ipc_flatpak_transfer_set_fraction (state->transfer, 1.0);
  ipc_flatpak_transfer_set_message (state->transfer, _("Installation complete"));
  g_task_return_boolean (task, TRUE);

  /* GDBusMethodInvocation completes on the main thread */
}

enum {
  MODE_PRIVATE = 0,
  MODE_USER = 1,
  MODE_SYSTEM = 2,
};

static char *
find_remote_for_ref (IpcFlatpakServiceImpl  *self,
                     FlatpakRef             *ref,
                     FlatpakInstallation   **installation)
{
  FlatpakQueryFlags flags = FLATPAK_QUERY_FLAGS_NONE;
  g_autoptr(GPtrArray) installs = NULL;
  g_autoptr(GPtrArray) suffix = NULL;
  g_autoptr(GSettings) settings = NULL;
  g_autofree char *preferred = NULL;
  const char *ref_name;
  const char *ref_arch;
  const char *ref_branch;
  int mode = 0;

  g_assert (IPC_IS_FLATPAK_SERVICE_IMPL (self));
  g_assert (FLATPAK_IS_REF (ref));

  ref_name = flatpak_ref_get_name (ref);
  ref_arch = flatpak_ref_get_arch (ref);
  ref_branch = flatpak_ref_get_branch (ref);

#if FLATPAK_CHECK_VERSION(1, 11, 2)
  /* If this is not the default architecture, we need to force that we've
   * loaded sub-summaries or we won't find any matches for the arch. Otherwise
   * the cached form is fine (and faster).
   */
  if (!str_equal0 (flatpak_get_default_arch (), flatpak_ref_get_arch (ref)))
    flags |= FLATPAK_QUERY_FLAGS_ALL_ARCHES;
#else
# warning "Flatpak is too old, searching for alternate arches will not work"
#endif

  settings = g_settings_new ("org.gnome.builder.flatpak");
  preferred = g_settings_get_string (settings, "preferred-installation");

  installs = g_ptr_array_new ();
  suffix = g_ptr_array_new ();

  if (g_strcmp0 (preferred, "private") == 0)
    mode = MODE_PRIVATE;
  else if (g_strcmp0 (preferred, "system") == 0)
    mode = MODE_SYSTEM;
  else
    mode = MODE_USER;

  for (guint i = 0; i < self->installs_ordered->len; i++)
    {
      Install *install = g_ptr_array_index (self->installs_ordered, i);

      if ((mode == MODE_PRIVATE && install->is_private) ||
          (mode == MODE_SYSTEM && !install->is_private && !flatpak_installation_get_is_user (install->installation)) ||
          (mode == MODE_USER && !install->is_private && flatpak_installation_get_is_user (install->installation)))
        g_ptr_array_add (installs, install);
      else
        g_ptr_array_add (suffix, install);
    }

  g_ptr_array_extend (installs, suffix, NULL, NULL);

  /* Someday we might want to prompt the user for which remote to install from,
   * but for now we'll just take the first.
   */
  for (guint i = 0; i < installs->len; i++)
    {
      Install *install = g_ptr_array_index (installs, i);
      g_autoptr(GPtrArray) remotes = flatpak_installation_list_remotes (install->installation, NULL, NULL);

      for (guint j = 0; j < remotes->len; j++)
        {
          FlatpakRemote *remote = g_ptr_array_index (remotes, j);
          const char *name = flatpak_remote_get_name (remote);
          g_autoptr(GPtrArray) refs = flatpak_installation_list_remote_refs_sync_full (install->installation, name, flags, NULL, NULL);

          if (refs == NULL)
            continue;

          for (guint k = 0; k < refs->len; k++)
            {
              FlatpakRef *remote_ref = g_ptr_array_index (refs, k);

              if (str_equal0 (ref_name, flatpak_ref_get_name (remote_ref)) &&
                  str_equal0 (ref_arch, flatpak_ref_get_arch (remote_ref)) &&
                  str_equal0 (ref_branch, flatpak_ref_get_branch (remote_ref)))
                {
                  if (installation != NULL)
                    *installation = g_object_ref (install->installation);
                  return g_strdup (name);
                }
            }
        }
    }

  if (installation != NULL)
    *installation = NULL;

  return NULL;
}

static void
clear_install_ref (gpointer data)
{
  InstallRef *r = data;

  g_clear_object (&r->fref);
  g_free (r->ref);
  g_free (r->remote);
}

static GPtrArray *
find_installations_for_refs (IpcFlatpakServiceImpl *self,
                             GArray                *refs)
{
  Install *private_install = NULL;
  GPtrArray *installations;

  g_assert (IPC_IS_FLATPAK_SERVICE_IMPL (self));
  g_assert (self->installs_ordered->len > 0);
  g_assert (refs != NULL);

  for (guint i = 0; i < self->installs_ordered->len; i++)
    {
      Install *install = g_ptr_array_index (self->installs_ordered, i);

      if (install->is_private)
        {
          private_install = install;
          break;
        }
    }

  g_assert (private_install != NULL);
  g_assert (private_install->is_private == TRUE);

  installations = g_ptr_array_new_with_free_func (g_object_unref);

  for (guint i = 0; i < refs->len; i++)
    {
      InstallRef *ir = &g_array_index (refs, InstallRef, i);
      const char *name = flatpak_ref_get_name (ir->fref);
      const char *arch = flatpak_ref_get_arch (ir->fref);
      const char *branch = flatpak_ref_get_branch (ir->fref);
      g_autoptr(FlatpakInstallation) install = NULL;
      g_autofree char *remote = NULL;

      /* First try to find if the ref is already installed */
      for (guint j = 0; j < self->runtimes->len; j++)
        {
          const Runtime *r = g_ptr_array_index (self->runtimes, j);

          /* Check for matching ref, but also that installation is going
           * to be writable to us from the sandbox.
           */
          if (str_equal0 (name, r->name) &&
              str_equal0 (arch, r->arch) &&
              str_equal0 (branch, r->branch))
            {
              g_ptr_array_add (installations, g_object_ref (r->installation));
              goto next_ref;
            }
        }

      /* Now see if it is found in a configured remote. */
      if ((remote = find_remote_for_ref (self, ir->fref, &install)))
        {
          g_ptr_array_add (installations, g_steal_pointer (&install));
          goto next_ref;
        }

      /* Default to our internal private installation */
      g_ptr_array_add (installations, g_object_ref (private_install->installation));

    next_ref:
      continue;
    }

  return installations;
}

static void
on_install_completed_cb (IpcFlatpakServiceImpl *self,
                         GParamSpec            *pspec,
                         GTask                 *task)
{
  g_autoptr(GDBusMethodInvocation) invocation = NULL;
  GHashTableIter iter;
  Install *install;

  g_assert (IPC_IS_FLATPAK_SERVICE_IMPL (self));
  g_assert (G_IS_TASK (task));

  /* Steal the invocation object */
  invocation = g_object_ref (g_object_get_data (G_OBJECT (task), "INVOCATION"));
  g_object_set_data (G_OBJECT (task), "INVOCATION", NULL);

  if (g_task_had_error (task))
    return;

  /* Reload installations so that we pick up new runtimes
   * immediately before we notify the install completed. Otherwise
   * we risk not being able to access it from the UI when the
   * D-Bus completion comes in.
   */
  g_hash_table_iter_init (&iter, self->installs);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&install))
    {
      if (flatpak_installation_get_is_user (install->installation))
        install_reload (self, install);
    }

  /* Now notify the client */
  ipc_flatpak_service_complete_install (IPC_FLATPAK_SERVICE (self),
                                        g_steal_pointer (&invocation));
}

static gboolean
ipc_flatpak_service_impl_install (IpcFlatpakService     *service,
                                  GDBusMethodInvocation *invocation,
                                  const char * const    *full_ref_names,
                                  gboolean               if_not_exists,
                                  const char            *transfer_path,
                                  const char            *parent_window)
{
  IpcFlatpakServiceImpl *self = (IpcFlatpakServiceImpl *)service;
  g_autoptr(IpcFlatpakTransfer) transfer = NULL;
  g_autoptr(GArray) refs = NULL;
  g_autoptr(GTask) task = NULL;
  g_autofree char *debug_str = NULL;
  GDBusConnection *connection;
  InstallState *state;

  g_assert (IPC_IS_FLATPAK_SERVICE_IMPL (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (full_ref_names != NULL);
  g_assert (transfer_path != NULL);
  g_assert (parent_window != NULL);

  debug_str = g_strjoinv (", ", (char **)full_ref_names);
  g_debug ("Install([%s], if_not_exists=%d, transfer=%s, parent_window=%s)",
           debug_str, if_not_exists, transfer_path, parent_window);

  refs = g_array_new (FALSE, FALSE, sizeof (InstallRef));
  g_array_set_clear_func (refs, clear_install_ref);

  connection = g_dbus_method_invocation_get_connection (invocation);
  transfer = ipc_flatpak_transfer_proxy_new_sync (connection,
                                                  G_DBUS_PROXY_FLAGS_NONE,
                                                  NULL,
                                                  transfer_path,
                                                  NULL, NULL);

  g_dbus_proxy_set_default_timeout (G_DBUS_PROXY (transfer), G_MAXINT);

  if (full_ref_names[0] == NULL)
    {
      g_dbus_method_invocation_return_error (g_steal_pointer (&invocation),
                                             G_DBUS_ERROR,
                                             G_DBUS_ERROR_FAILED,
                                             "No refs to install");
      return TRUE;
    }

  for (guint i = 0; full_ref_names[i]; i++)
    {
      const char *name = full_ref_names[i];
      g_autoptr(FlatpakRef) ref = NULL;
      g_autofree char *adjusted = NULL;
      InstallRef iref = {0};

      if (!g_str_has_prefix (name, "runtime/") && !g_str_has_prefix (name, "app/"))
        name = adjusted = g_strdup_printf ("runtime/%s", full_ref_names[i]);

      if ((ref = flatpak_ref_parse (name, NULL)))
        {
          iref.update = is_installed (self, ref);
          if (if_not_exists && iref.update)
            continue;
        }

      if (ref == NULL ||
          !(iref.remote = find_remote_for_ref (self, ref, NULL)))
        {
          g_dbus_method_invocation_return_error (g_steal_pointer (&invocation),
                                                 G_DBUS_ERROR,
                                                 G_DBUS_ERROR_FAILED,
                                                 "No configured remote contains ref \"%s\"",
                                                 full_ref_names[i]);
          return TRUE;
        }

      iref.ref = flatpak_ref_format_ref (ref);
      iref.fref = g_steal_pointer (&ref);
      g_array_append_val (refs, iref);
    }

  if (refs->len == 0)
    {
      ipc_flatpak_service_complete_install (service, g_steal_pointer (&invocation));
      return TRUE;
    }

  state = g_slice_new0 (InstallState);
  state->cancellable = g_cancellable_new ();
  state->invocation = g_object_ref (invocation);
  state->refs = g_array_ref (refs);
  state->parent_window = parent_window[0] ? g_strdup (parent_window) : NULL;
  state->transfer = g_object_ref (transfer);
  state->installations = find_installations_for_refs (self, refs);

  g_assert (refs->len > 0);
  g_assert (state->installations->len == refs->len);

  g_signal_connect_object (transfer,
                           "cancel",
                           G_CALLBACK (g_cancellable_cancel),
                           state->cancellable,
                           G_CONNECT_SWAPPED);

  task = g_task_new (self, state->cancellable, NULL, NULL);
  g_object_set_data_full (G_OBJECT (task),
                          "INVOCATION",
                          g_object_ref (invocation),
                          g_object_unref);
  g_signal_connect_object (task,
                           "notify::completed",
                           G_CALLBACK (on_install_completed_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_task_set_source_tag (task, ipc_flatpak_service_impl_install);
  g_task_set_task_data (task, state, (GDestroyNotify)install_state_free);
  g_task_run_in_thread (task, install_worker);

  g_object_unref (invocation);

  return TRUE;
}

typedef struct
{
  const char *ref;
  const char *extension;
} ResolveExtension;

G_GNUC_PRINTF (2, 3)
static const char *
chunk_insert (GStringChunk *strings,
              const char   *format,
              ...)
{
  char formatted[256];
  const char *ret = NULL;
  va_list args;

  va_start (args, format);
  if (g_vsnprintf (formatted, sizeof formatted, format, args) < sizeof formatted)
    ret = g_string_chunk_insert_const (strings, formatted);
  va_end (args);

  return ret;
}

static char *
resolve_extension (GPtrArray  *installations,
                   const char *sdk,
                   const char *extension)
{
  g_autofree char *sdk_id = NULL;
  g_autofree char *sdk_arch = NULL;
  g_autofree char *sdk_branch = NULL;
  g_autoptr(GArray) maybe_extention_of = NULL;
  g_autoptr(GArray) runtime_extensions = NULL;
  g_autoptr(GStringChunk) strings = NULL;
  FlatpakQueryFlags flags = FLATPAK_QUERY_FLAGS_NONE;

  g_assert (installations != NULL);
  g_assert (sdk != NULL);
  g_assert (extension != NULL);

  /* It would be very nice to do this asynchronously someday, but we try to
   * only use cached contents so it's not quite as bad as it could be.
   */

  if (!split_id (sdk, &sdk_id, &sdk_arch, &sdk_branch))
    return NULL;

  if (sdk_arch == NULL)
    sdk_arch = g_strdup (flatpak_get_default_arch ());

#if FLATPAK_CHECK_VERSION(1, 11, 2)
  if (!str_equal0 (sdk_arch, flatpak_get_default_arch ()))
    flags |= FLATPAK_QUERY_FLAGS_ALL_ARCHES;
#else
# warning "Flatpak is too old, searching for alternate arches will not work"
#endif

  strings = g_string_chunk_new (4096);
  maybe_extention_of = g_array_new (FALSE, FALSE, sizeof (ResolveExtension));
  runtime_extensions = g_array_new (FALSE, FALSE, sizeof (ResolveExtension));

  for (guint i = 0; i < installations->len; i++)
    {
      FlatpakInstallation *installation = g_ptr_array_index (installations, i);
      g_autoptr(GPtrArray) remotes = flatpak_installation_list_remotes (installation, NULL, NULL);

      if (remotes == NULL)
        continue;

      for (guint j = 0; j < remotes->len; j++)
        {
          FlatpakRemote *remote = g_ptr_array_index (remotes, j);
          const char *name = flatpak_remote_get_name (remote);
          g_autoptr(GPtrArray) refs = NULL;

          if (!(refs = flatpak_installation_list_remote_refs_sync_full (installation, name, flags, NULL, NULL)))
            continue;

          for (guint k = 0; k < refs->len; k++)
            {
              FlatpakRemoteRef *ref = g_ptr_array_index (refs, k);
              const char *id = flatpak_ref_get_name (FLATPAK_REF (ref));
              const char *branch = flatpak_ref_get_branch (FLATPAK_REF (ref));
              const char *arch = flatpak_ref_get_arch (FLATPAK_REF (ref));
              g_autoptr(GKeyFile) keyfile = NULL;
              g_auto(GStrv) groups = NULL;
              GBytes *bytes;

              if (flatpak_ref_get_kind (FLATPAK_REF (ref)) != FLATPAK_REF_KIND_RUNTIME ||
                  !str_equal0 (arch, sdk_arch) ||
                  !(bytes = flatpak_remote_ref_get_metadata (ref)))
                continue;

              keyfile = g_key_file_new ();
              if (!g_key_file_load_from_bytes (keyfile, bytes, 0, NULL))
                continue;

              groups = g_key_file_get_groups (keyfile, NULL);

              for (guint l = 0; groups[l]; l++)
                {
                  const char *group = groups[l];
                  g_autofree char *version = NULL;
                  g_autofree char *runtime = NULL;
                  g_autofree char *refstr = NULL;

                  /* This might be our extension */
                  if (str_equal0 (group, "ExtensionOf") &&
                      str_equal0 (id, extension))
                    {
                      runtime = g_key_file_get_string (keyfile, group, "runtime", NULL);
                      refstr = g_key_file_get_string (keyfile, group, "ref", NULL);

                      if (ref != NULL && g_str_has_prefix (refstr, "runtime/"))
                        {
                          g_autofree char *ref_id = NULL;
                          g_autofree char *ref_arch = NULL;
                          g_autofree char *ref_branch = NULL;

                          if (split_id (refstr + strlen ("runtime/"), &ref_id, &ref_arch, &ref_branch))
                            {
                              g_clear_pointer (&runtime, g_free);

                              /* https://gitlab.gnome.org/GNOME/gnome-builder/issues/1437
                               *
                               * Some extensions report an incorrect ref (or a ref that is
                               * for another architecture than the current). For example,
                               * org.freedesktop.Sdk.Compat.i386/x86_64/19.08 will report
                               * a ref of org.freedesktop.Sdk/i386/19.08.
                               *
                               * To work around this, we can simply swap the arch for the
                               * arch of the runtime extension we're looking at.
                               */
                              runtime = g_strdup_printf ("%s/%s/%s", ref_id, arch, ref_branch);
                            }
                        }

                      if (runtime != NULL)
                        {
                          ResolveExtension re = {
                            chunk_insert (strings, "%s/%s/%s", id, arch, branch),
                            g_string_chunk_insert_const (strings, runtime) };

                          g_array_append_val (maybe_extention_of, re);
                        }
                    }

                  /* This might provide the extension */
                  if (g_str_has_prefix (group, "Extension "))
                    {
                      const char *extname = group + strlen ("Extension ");

                      /* Only track extensions to the runtime itself unless it is
                       * for our target runtime/SDK.
                       */
                      if (!g_str_has_prefix (extname, id))
                        {
                          if (!str_equal0 (id, sdk_id) ||
                              !str_equal0 (branch, sdk_branch))
                            continue;
                        }

                      if (!(version = g_key_file_get_string (keyfile, group, "version", NULL)))
                        version = g_strdup (branch);

                      if (version != NULL)
                        {
                          ResolveExtension re = {
                            chunk_insert (strings, "%s/%s/%s", id, arch, branch),
                            chunk_insert (strings, "%s/%s/%s", extname, arch, version) };

                          g_array_append_val (runtime_extensions, re);
                        }
                    }
                }
            }
        }
    }

  for (guint i = 0; i < maybe_extention_of->len; i++)
    {
      const ResolveExtension *maybe = &g_array_index (maybe_extention_of, ResolveExtension, i);

      /* First find any runtime matching the ExtensionOf (such as
       * ExtensionOf=org.freedesktop.Sdk/x86_64/20.08.
       */

      for (guint j = 0; j < runtime_extensions->len; j++)
        {
          const ResolveExtension *re = &g_array_index (runtime_extensions, ResolveExtension, j);
          g_autofree char *rname = NULL;

          if (!str_equal0 (re->ref, maybe->extension))
            continue;

          if (!split_id (re->extension, &rname, NULL, NULL))
            continue;

          /* Now we need to find any runtime that matches the extension
           * that is in re->extension (such as
           * org.freedesktop.Sdk.Extension/x86_64/20.08).
           */

          for (guint k = 0; k < runtime_extensions->len; k++)
            {
              const ResolveExtension *target = &g_array_index (runtime_extensions, ResolveExtension, k);

              if (!str_equal0 (re->extension, target->extension))
                continue;

              if (str_equal0 (target->ref, sdk))
                {
                  char *ret = g_strdup (maybe->ref);
                  return ret;
                }
            }
        }
    }

  return NULL;
}

static void
resolve_extension_worker (GTask        *task,
                          gpointer      source_object,
                          gpointer      task_data,
                          GCancellable *cancellable)
{
  g_autofree char *resolved = NULL;
  ResolveExtensionState *state = task_data;

  g_assert (G_IS_TASK (task));
  g_assert (IPC_IS_FLATPAK_SERVICE_IMPL (source_object));
  g_assert (state != NULL);
  g_assert (state->installs != NULL);
  g_assert (G_IS_DBUS_METHOD_INVOCATION (state->invocation));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  resolved = resolve_extension (state->installs, state->sdk, state->extension);

  if (resolved == NULL)
    g_dbus_method_invocation_return_error (g_steal_pointer (&state->invocation),
                                           G_DBUS_ERROR,
                                           G_DBUS_ERROR_FAILED,
                                           "Failed to resolve extension");
  else
    ipc_flatpak_service_complete_resolve_extension (source_object,
                                                    g_steal_pointer (&state->invocation),
                                                    resolved);

  g_task_return_boolean (task, TRUE);
}

static gboolean
ipc_flatpak_service_impl_resolve_extension (IpcFlatpakService     *service,
                                            GDBusMethodInvocation *invocation,
                                            const char            *sdk,
                                            const char            *extension)
{
  IpcFlatpakServiceImpl *self = (IpcFlatpakServiceImpl *)service;
  ResolveExtensionState *state;
  g_autoptr(GTask) task = NULL;

  g_assert (IPC_IS_FLATPAK_SERVICE_IMPL (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (sdk != NULL);
  g_assert (extension != NULL);

  g_debug ("ResolveExtension(%s, %s)", sdk, extension);

  if (g_str_has_prefix (sdk, "runtime/"))
    sdk += strlen ("runtime/");

  if (g_str_has_prefix (extension, "runtime/"))
    extension += strlen ("runtime/");

  state = g_slice_new0 (ResolveExtensionState);
  state->installs = g_ptr_array_new_with_free_func (g_object_unref);
  state->invocation = g_steal_pointer (&invocation);
  state->sdk = g_strdup (sdk);
  state->extension = g_strdup (extension);

  for (guint i = 0; i < self->installs_ordered->len; i++)
    {
      const Install *install = g_ptr_array_index (self->installs_ordered, i);
      g_ptr_array_add (state->installs, g_object_ref (install->installation));
    }

  task = g_task_new (self, NULL, NULL, NULL);
  g_task_set_source_tag (task, ipc_flatpak_service_impl_resolve_extension);
  g_task_set_task_data (task, state, (GDestroyNotify) resolve_extension_state_free);
  g_task_run_in_thread (task, resolve_extension_worker);

  return TRUE;
}

static gboolean
ipc_flatpak_service_impl_get_runtime (IpcFlatpakService     *service,
                                      GDBusMethodInvocation *invocation,
                                      const char            *runtime_id)
{
  IpcFlatpakServiceImpl *self = (IpcFlatpakServiceImpl *)service;
  g_autoptr(FlatpakRef) ref = NULL;
  g_autofree char *full_name = NULL;

  g_assert (IPC_IS_FLATPAK_SERVICE_IMPL (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (runtime_id != NULL);

  g_debug ("GetRuntime(%s)", runtime_id);

  /* Homogenize names into runtime/name/arch/branch */
  if (g_str_has_prefix (runtime_id, "runtime/"))
    runtime_id += strlen ("runtime/");
  full_name = g_strdup_printf ("runtime/%s", runtime_id);

  if (!(ref = flatpak_ref_parse (full_name, NULL)))
    {
      g_dbus_method_invocation_return_error (g_steal_pointer (&invocation),
                                             G_DBUS_ERROR,
                                             G_DBUS_ERROR_FAILED,
                                             "Invalid runtime id %s",
                                             full_name);
      return TRUE;
    }

  for (guint i = 0; i < self->runtimes->len; i++)
    {
      const Runtime *runtime = g_ptr_array_index (self->runtimes, i);

      if (str_equal0 (flatpak_ref_get_name (ref), runtime->name) &&
          str_equal0 (flatpak_ref_get_arch (ref), runtime->arch) &&
          str_equal0 (flatpak_ref_get_branch (ref), runtime->branch))
        {
          g_autoptr(GVariant) ret = runtime_to_variant (runtime);
          ipc_flatpak_service_complete_get_runtime (service,
                                                    g_steal_pointer (&invocation),
                                                    ret);
          return TRUE;
        }
    }

  g_dbus_method_invocation_return_error (g_steal_pointer (&invocation),
                                         G_DBUS_ERROR,
                                         G_DBUS_ERROR_FAILED,
                                         "No such runtime %s",
                                         full_name);

  return TRUE;
}

static void
service_iface_init (IpcFlatpakServiceIface *iface)
{
  iface->handle_add_installation = ipc_flatpak_service_impl_add_installation;
  iface->handle_list_runtimes = ipc_flatpak_service_impl_list_runtimes;
  iface->handle_runtime_is_known = ipc_flatpak_service_impl_runtime_is_known;
  iface->handle_install = ipc_flatpak_service_impl_install;
  iface->handle_get_runtime = ipc_flatpak_service_impl_get_runtime;
  iface->handle_resolve_extension = ipc_flatpak_service_impl_resolve_extension;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (IpcFlatpakServiceImpl, ipc_flatpak_service_impl, IPC_TYPE_FLATPAK_SERVICE_SKELETON,
                               G_IMPLEMENT_INTERFACE (IPC_TYPE_FLATPAK_SERVICE, service_iface_init))

static void
ipc_flatpak_service_impl_constructed (GObject *object)
{
  IpcFlatpakServiceImpl *self = (IpcFlatpakServiceImpl *)object;
  IpcFlatpakRepo *repo = ipc_flatpak_repo_get_default ();
  g_autoptr(GPtrArray) installations = NULL;
  FlatpakInstallation *priv_install;

  G_OBJECT_CLASS (ipc_flatpak_service_impl_parent_class)->constructed (object);

  ipc_flatpak_service_set_default_arch (IPC_FLATPAK_SERVICE (self),
                                        flatpak_get_default_arch ());

  /* Add system installations unless disabled */
  if (!self->ignore_system_installations)
    {
      if ((installations = flatpak_get_system_installations (NULL, NULL)))
        {
          for (guint i = 0; i < installations->len; i++)
            add_installation (self, g_ptr_array_index (installations, i), NULL, NULL);
        }
    }

  /* Fallback for SDKs not available elsewhere */
  if ((priv_install = ipc_flatpak_repo_get_installation (repo)))
    {
      g_autofree char *config_dir = ipc_flatpak_repo_get_config_dir (repo);
      g_autofree char *path = ipc_flatpak_repo_get_path (repo);
      Install *priv_install_ptr = NULL;

      add_installation (self, priv_install, &priv_install_ptr, NULL);
      ipc_flatpak_service_set_config_dir (IPC_FLATPAK_SERVICE (self), config_dir);

      priv_install_ptr->is_private = TRUE;

      g_debug ("Added installation at %s and FLATPAK_CONFIG_DIR %s", config_dir, path);
    }
}

static void
ipc_flatpak_service_impl_finalize (GObject *object)
{
  IpcFlatpakServiceImpl *self = (IpcFlatpakServiceImpl *)object;

  g_clear_pointer (&self->installs_ordered, g_ptr_array_unref);
  g_clear_pointer (&self->installs, g_hash_table_unref);
  g_clear_pointer (&self->runtimes, g_ptr_array_unref);
  g_clear_pointer (&self->known, g_hash_table_unref);

  G_OBJECT_CLASS (ipc_flatpak_service_impl_parent_class)->finalize (object);
}

static void
ipc_flatpak_service_impl_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  IpcFlatpakServiceImpl *self = IPC_FLATPAK_SERVICE_IMPL (object);

  switch (prop_id)
    {
    case PROP_IGNORE_SYSTEM_INSTALLATIONS:
      g_value_set_boolean (value, self->ignore_system_installations);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ipc_flatpak_service_impl_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  IpcFlatpakServiceImpl *self = IPC_FLATPAK_SERVICE_IMPL (object);

  switch (prop_id)
    {
    case PROP_IGNORE_SYSTEM_INSTALLATIONS:
      self->ignore_system_installations = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ipc_flatpak_service_impl_class_init (IpcFlatpakServiceImplClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ipc_flatpak_service_impl_constructed;
  object_class->finalize = ipc_flatpak_service_impl_finalize;
  object_class->get_property = ipc_flatpak_service_impl_get_property;
  object_class->set_property = ipc_flatpak_service_impl_set_property;

  properties [PROP_IGNORE_SYSTEM_INSTALLATIONS] =
    g_param_spec_boolean ("ignore-system-installations",
                          "Ignore System Installations",
                          "Ignore System Installations",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ipc_flatpak_service_impl_init (IpcFlatpakServiceImpl *self)
{
  self->installs_ordered = g_ptr_array_new ();
  self->installs = g_hash_table_new_full (g_file_hash,
                                          (GEqualFunc) g_file_equal,
                                          g_object_unref,
                                          (GDestroyNotify) install_free);
  self->runtimes = g_ptr_array_new_with_free_func ((GDestroyNotify) runtime_free);
  self->known = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
}

IpcFlatpakService *
ipc_flatpak_service_impl_new (gboolean ignore_system_installations)
{
  return g_object_new (IPC_TYPE_FLATPAK_SERVICE_IMPL,
                       "ignore-system-installations", ignore_system_installations,
                       NULL);
}
