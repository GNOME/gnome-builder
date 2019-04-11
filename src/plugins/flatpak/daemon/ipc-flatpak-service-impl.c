/* ipc-flatpak-service-impl.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#include "ipc-flatpak-service-impl.h"
#include "ipc-flatpak-util.h"

typedef struct
{
  FlatpakInstallation *installation;
  gchar *name;
  gchar *arch;
  gchar *branch;
  gchar *sdk_name;
  gchar *sdk_branch;
  gboolean sdk_extension : 1;
} Runtime;

typedef struct
{
  FlatpakInstallation *installation;
  GFileMonitor *monitor;
} Install;

typedef struct
{
  GDBusMethodInvocation *invocation;
  GPtrArray             *installs;
  FlatpakRef            *target;
} IsKnown;

struct _IpcFlatpakServiceImpl
{
  IpcFlatpakServiceSkeleton parent;
  GHashTable *installs;
  GPtrArray *runtimes;
};

static void      ipc_flatpak_service_impl_install_changed_cb (IpcFlatpakServiceImpl  *self,
                                                              GFile                  *file,
                                                              GFile                  *other_file,
                                                              GFileMonitorEvent       event,
                                                              GFileMonitor           *monitor);
static gboolean  ipc_flatpak_service_impl_add_installation   (IpcFlatpakService      *service,
                                                              GDBusMethodInvocation  *invocation,
                                                              const gchar            *path,
                                                              gboolean                is_user);
static void      add_runtime                                 (IpcFlatpakServiceImpl  *service,
                                                              Runtime                *runtime);
static Install  *install_new                                 (IpcFlatpakServiceImpl  *self,
                                                              FlatpakInstallation    *installation,
                                                              GError                **error);
static void      install_free                                (Install                *install);
static void      install_reload                              (IpcFlatpakServiceImpl  *self,
                                                              Install                *install);
static GVariant *runtime_to_variant                          (const Runtime          *runtime);
static void      runtime_free                                (Runtime                *runtime);
static gboolean  runtime_equal                               (const Runtime          *a,
                                                              const Runtime          *b);
static void      is_known_free                               (IsKnown                *state);

static void
is_known_free (IsKnown *state)
{
  g_clear_pointer (&state->installs, g_ptr_array_unref);
  g_clear_object (&state->invocation);
  g_clear_object (&state->target);
  g_slice_free (IsKnown, state);
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

static void
runtime_free (Runtime *runtime)
{
  g_clear_pointer (&runtime->name, g_free);
  g_clear_pointer (&runtime->arch, g_free);
  g_clear_pointer (&runtime->branch, g_free);
  g_clear_pointer (&runtime->sdk_name, g_free);
  g_clear_pointer (&runtime->sdk_branch, g_free);
  g_slice_free (Runtime, runtime);
}

static gboolean
runtime_equal (const Runtime *a,
               const Runtime *b)
{
  return g_str_equal (a->name, b->name) &&
         g_str_equal (a->arch, b->arch) &&
         g_str_equal (a->branch, b->branch);
}

static GVariant *
runtime_to_variant (const Runtime *runtime)
{
  return g_variant_take_ref (g_variant_new ("(sssssb)",
                                            runtime->name,
                                            runtime->arch,
                                            runtime->branch,
                                            runtime->sdk_name,
                                            runtime->sdk_branch,
                                            runtime->sdk_extension));
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

static void
install_reload (IpcFlatpakServiceImpl *self,
                Install               *install)
{
  g_autoptr(GPtrArray) refs = NULL;

  g_assert (IPC_IS_FLATPAK_SERVICE_IMPL (self));
  g_assert (install != NULL);

  if (!(refs = flatpak_installation_list_installed_refs_by_kind (install->installation,
                                                                 FLATPAK_REF_KIND_RUNTIME,
                                                                 NULL, NULL)))
    return;

  for (guint i = 0; i < refs->len; i++)
    {
      FlatpakInstalledRef *ref = g_ptr_array_index (refs, i);
      g_autoptr(FlatpakRef) sdk_ref = NULL;
      g_autoptr(GBytes) bytes = NULL;
      g_autoptr(GKeyFile) keyfile = g_key_file_new ();
      g_autofree gchar *sdk_full_ref = NULL;
      g_autofree gchar *name = NULL;
      g_autofree gchar *runtime = NULL;
      g_autofree gchar *sdk = NULL;
      g_autofree gchar *exten_of = NULL;
      Runtime *state;

      if (!(bytes = flatpak_installed_ref_load_metadata (ref, NULL, NULL)) ||
          !g_key_file_load_from_bytes (keyfile, bytes, G_KEY_FILE_NONE, NULL))
        continue;

      if (!(name = g_key_file_get_string (keyfile, "Runtime", "name", NULL)) ||
          !(runtime = g_key_file_get_string (keyfile, "Runtime", "runtime", NULL)) ||
          !(sdk = g_key_file_get_string (keyfile, "Runtime", "sdk", NULL)))
        continue;

      if (g_key_file_has_group (keyfile, "ExtensionOf"))
        {
          /* Skip if this item is an extension, but not an SDK extension */
          if (!g_key_file_has_key (keyfile, "ExtensionOf", "ref", NULL) ||
              strstr (name, ".Extension.") == NULL)
            continue;

          exten_of = g_key_file_get_string (keyfile, "ExtensionOf", "ref", NULL);
        }

      if (!g_str_has_prefix (sdk, "runtime/"))
        sdk_full_ref = g_strdup_printf ("runtime/%s", sdk);
      else
        sdk_full_ref = g_strdup (sdk);

      /* Make sure we can parse the SDK reference */
      if (!(sdk_ref = flatpak_ref_parse (sdk_full_ref, NULL)))
        continue;

      state = g_slice_new0 (Runtime);
      state->installation = g_object_ref (install->installation);
      state->name = g_strdup (flatpak_ref_get_name (FLATPAK_REF (ref)));
      state->arch = g_strdup (flatpak_ref_get_arch (FLATPAK_REF (ref)));
      state->branch = g_strdup (flatpak_ref_get_branch (FLATPAK_REF (ref)));
      state->sdk_name = g_strdup (flatpak_ref_get_name (sdk_ref));
      state->sdk_branch = g_strdup (flatpak_ref_get_branch (sdk_ref));
      state->sdk_extension = exten_of != NULL;

      add_runtime (self, g_steal_pointer (&state));
    }
}

static gboolean
ipc_flatpak_service_impl_add_installation (IpcFlatpakService     *service,
                                           GDBusMethodInvocation *invocation,
                                           const gchar           *path,
                                           gboolean               is_user)
{
  IpcFlatpakServiceImpl *self = (IpcFlatpakServiceImpl *)service;
  g_autoptr(FlatpakInstallation) installation = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) file = NULL;

  g_assert (IPC_IS_FLATPAK_SERVICE_IMPL (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (path != NULL);

  file = g_file_new_for_path (path);

  if (!g_hash_table_contains (self->installs, file))
    {
      Install *install;

      if (!(installation = flatpak_installation_new_for_path (file, is_user, NULL, &error)) ||
          !(install = install_new (self, installation, &error)))
        return complete_wrapped_error (invocation, error);

      install_reload (self, install);

      g_hash_table_insert (self->installs,
                           g_steal_pointer (&file),
                           g_steal_pointer (&install));
    }

  ipc_flatpak_service_complete_add_installation (service, invocation);

  return TRUE;
}

static gboolean
ipc_flatpak_service_impl_list_runtimes (IpcFlatpakService     *service,
                                        GDBusMethodInvocation *invocation)
{
  IpcFlatpakServiceImpl *self = (IpcFlatpakServiceImpl *)service;
  g_autoptr(GError) error = NULL;
  GVariantBuilder builder;

  g_assert (IPC_IS_FLATPAK_SERVICE_IMPL (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(sssssb)"));

  for (guint i = 0; i < self->runtimes->len; i++)
    {
      const Runtime *runtime = g_ptr_array_index (self->runtimes, i);
      g_autoptr(GVariant) item = runtime_to_variant (runtime);
      g_variant_builder_add_value (&builder, item);
    }

  ipc_flatpak_service_complete_list_runtimes (service,
                                              invocation,
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
  const gchar *ref_name;
  const gchar *ref_arch;
  const gchar *ref_branch;
  gint64 download_size = 0;
  gboolean found = FALSE;

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

  for (guint z = 0; z < state->installs->len; z++)
    {
      FlatpakInstallation *install = g_ptr_array_index (state->installs, z);

      if (!(remotes = flatpak_installation_list_remotes (install, NULL, &error)))
        goto finish;

      for (guint i = 0; i < remotes->len; i++)
        {
          FlatpakRemote *remote = g_ptr_array_index (remotes, i);
          const gchar *remote_name = flatpak_remote_get_name (remote);
          g_autoptr(GPtrArray) refs = NULL;

          if (!(refs = flatpak_installation_list_remote_refs_sync (install, remote_name, NULL, NULL)))
            continue;

          for (guint j = 0; j < refs->len; j++)
            {
              FlatpakRemoteRef *remote_ref = g_ptr_array_index (refs, j);

              if (g_str_equal (ref_name, flatpak_ref_get_name (FLATPAK_REF (remote_ref))) &&
                  g_str_equal (ref_arch, flatpak_ref_get_arch (FLATPAK_REF (remote_ref))) &&
                  g_str_equal (ref_branch, flatpak_ref_get_branch (FLATPAK_REF (remote_ref))))
                {
                  found = TRUE;
                  download_size = flatpak_remote_ref_get_download_size (remote_ref);
                  goto finish;
                }
            }
        }
    }

finish:
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
                                           const gchar           *name)
{
  IpcFlatpakServiceImpl *self = (IpcFlatpakServiceImpl *)service;
  g_autofree gchar *full_name = NULL;
  g_autoptr(FlatpakRef) ref = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = NULL;
  const gchar *ref_name;
  const gchar *ref_arch;
  const gchar *ref_branch;
  GHashTableIter iter;
  Install *install;
  IsKnown *state;

  g_assert (IPC_IS_FLATPAK_SERVICE_IMPL (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (name != NULL);

  /* Homogenize names into runtime/name/arch/branch */
  if (g_str_has_prefix (name, "runtime/"))
    name += strlen ("runtime/");
  full_name = g_strdup_printf ("runtime/%s", name);

  /* Parse the ref, so we can try to locate it */
  if (!(ref = flatpak_ref_parse (full_name, &error)))
    return complete_wrapped_error (invocation, error);

  ref_name = flatpak_ref_get_name (ref);
  ref_arch = flatpak_ref_get_arch (ref);
  ref_branch = flatpak_ref_get_branch (ref);

  /* First check if we know about the runtime from those installed */
  for (guint i = 0; i < self->runtimes->len; i++)
    {
      const Runtime *runtime = g_ptr_array_index (self->runtimes, i);

      if (g_str_equal (ref_name, runtime->name) &&
          g_str_equal (ref_arch, runtime->arch) &&
          g_str_equal (ref_branch, runtime->branch))
        {
          ipc_flatpak_service_complete_runtime_is_known (service, invocation, TRUE, 0);
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

  /* Now check remote refs */
  g_hash_table_iter_init (&iter, self->installs);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&install))
    g_ptr_array_add (state->installs, g_object_ref (install->installation));

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

static gboolean
ipc_flatpak_service_impl_install (IpcFlatpakService     *service,
                                  GDBusMethodInvocation *invocation,
                                  const gchar           *full_ref_name)
{
  g_autoptr(FlatpakRef) ref = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IPC_IS_FLATPAK_SERVICE_IMPL (service));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (full_ref_name != NULL);

  if (!(ref = flatpak_ref_parse (full_ref_name, &error)))
    return complete_wrapped_error (invocation, error);

  ipc_flatpak_service_complete_install (service, invocation, "");

  return TRUE;
}

static void
service_iface_init (IpcFlatpakServiceIface *iface)
{
  iface->handle_add_installation = ipc_flatpak_service_impl_add_installation;
  iface->handle_list_runtimes = ipc_flatpak_service_impl_list_runtimes;
  iface->handle_runtime_is_known = ipc_flatpak_service_impl_runtime_is_known;
  iface->handle_install = ipc_flatpak_service_impl_install;
}

G_DEFINE_TYPE_WITH_CODE (IpcFlatpakServiceImpl, ipc_flatpak_service_impl, IPC_TYPE_FLATPAK_SERVICE_SKELETON,
                         G_IMPLEMENT_INTERFACE (IPC_TYPE_FLATPAK_SERVICE, service_iface_init))

static void
ipc_flatpak_service_impl_finalize (GObject *object)
{
  IpcFlatpakServiceImpl *self = (IpcFlatpakServiceImpl *)object;

  g_clear_pointer (&self->installs, g_hash_table_unref);
  g_clear_pointer (&self->runtimes, g_ptr_array_unref);

  G_OBJECT_CLASS (ipc_flatpak_service_impl_parent_class)->finalize (object);
}

static void
ipc_flatpak_service_impl_class_init (IpcFlatpakServiceImplClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ipc_flatpak_service_impl_finalize;
}

static void
ipc_flatpak_service_impl_init (IpcFlatpakServiceImpl *self)
{
  self->installs = g_hash_table_new_full (g_file_hash,
                                               (GEqualFunc) g_file_equal,
                                               g_object_unref,
                                               (GDestroyNotify) install_free);
  self->runtimes = g_ptr_array_new_with_free_func ((GDestroyNotify) runtime_free);
}

IpcFlatpakService *
ipc_flatpak_service_impl_new (void)
{
  return g_object_new (IPC_TYPE_FLATPAK_SERVICE_IMPL, NULL);
}
