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
split_id (const gchar  *str,
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
add_installation (IpcFlatpakServiceImpl  *self,
                  FlatpakInstallation    *installation,
                  GError                **error)
{
  g_autoptr(GFile) file = NULL;
  Install *install;

  g_assert (IPC_IS_FLATPAK_SERVICE_IMPL (self));
  g_assert (FLATPAK_IS_INSTALLATION (installation));

  file = flatpak_installation_get_path (installation);

  if (!(install = install_new (self, installation, error)))
    return FALSE;

  install_reload (self, install);

  g_hash_table_insert (self->installs,
                       g_steal_pointer (&file),
                       g_steal_pointer (&install));

  return TRUE;
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
      if (!(installation = flatpak_installation_new_for_path (file, is_user, NULL, &error)) ||
          !add_installation (self, installation, &error))
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

typedef struct
{
  const gchar *ref;
  const gchar *extension;
} ResolveExtension;

G_GNUC_PRINTF (2, 3)
static const gchar *
chunk_insert (GStringChunk *strings,
              const gchar *format,
              ...)
{
  char formatted[256];
  const gchar *ret = NULL;
  va_list args;

  va_start (args, format);
  if (g_vsnprintf (formatted, sizeof formatted, format, args) < sizeof formatted)
    ret = g_string_chunk_insert_const (strings, formatted);
  va_end (args);

  return ret;
}

static gchar *
resolve_extension (GPtrArray   *installations,
                   const gchar *sdk,
                   const gchar *extension)
{
  g_autofree gchar *sdk_id = NULL;
  g_autofree gchar *sdk_arch = NULL;
  g_autofree gchar *sdk_branch = NULL;
  g_autoptr(GArray) maybe_extention_of = NULL;
  g_autoptr(GArray) runtime_extensions = NULL;
  g_autoptr(GStringChunk) strings = NULL;

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
          const gchar *name = flatpak_remote_get_name (remote);
          g_autoptr(GPtrArray) refs = NULL;

          refs = flatpak_installation_list_remote_refs_sync_full (installation,
                                                                  name,
                                                                  FLATPAK_QUERY_FLAGS_ONLY_CACHED,
                                                                  NULL,
                                                                  NULL);

          if (refs == NULL)
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
                  const gchar *group = groups[l];
                  g_autofree gchar *version = NULL;
                  g_autofree gchar *runtime = NULL;
                  g_autofree gchar *match = NULL;
                  g_autofree gchar *refstr = NULL;

                  /* This might be our extension */
                  if (str_equal0 (group, "ExtensionOf") &&
                      str_equal0 (id, extension))
                    {
                      runtime = g_key_file_get_string (keyfile, group, "runtime", NULL);
                      refstr = g_key_file_get_string (keyfile, group, "ref", NULL);

                      if (ref != NULL && g_str_has_prefix (refstr, "runtime/"))
                        {
                          g_autofree gchar *ref_id = NULL;
                          g_autofree gchar *ref_arch = NULL;
                          g_autofree gchar *ref_branch = NULL;

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
                      const gchar *extname = group + strlen ("Extension ");

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
          g_autofree gchar *rname = NULL;

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
  GHashTableIter iter;
  Install *install;

  g_assert (IPC_IS_FLATPAK_SERVICE_IMPL (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (sdk != NULL);
  g_assert (extension != NULL);

  state = g_slice_new0 (ResolveExtensionState);
  state->installs = g_ptr_array_new_with_free_func (g_object_unref);
  state->invocation = g_steal_pointer (&invocation);
  state->sdk = g_strdup (sdk);
  state->extension = g_strdup (extension);

  g_hash_table_iter_init (&iter, self->installs);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&install))
    g_ptr_array_add (state->installs, g_object_ref (install->installation));

  task = g_task_new (self, NULL, NULL, NULL);
  g_task_set_source_tag (task, ipc_flatpak_service_impl_resolve_extension);
  g_task_set_task_data (task, state, (GDestroyNotify) resolve_extension_state_free);
  g_task_run_in_thread (task, resolve_extension_worker);

  return TRUE;
}

static void
service_iface_init (IpcFlatpakServiceIface *iface)
{
  iface->handle_add_installation = ipc_flatpak_service_impl_add_installation;
  iface->handle_list_runtimes = ipc_flatpak_service_impl_list_runtimes;
  iface->handle_runtime_is_known = ipc_flatpak_service_impl_runtime_is_known;
  iface->handle_install = ipc_flatpak_service_impl_install;
  iface->handle_resolve_extension = ipc_flatpak_service_impl_resolve_extension;
}

G_DEFINE_TYPE_WITH_CODE (IpcFlatpakServiceImpl, ipc_flatpak_service_impl, IPC_TYPE_FLATPAK_SERVICE_SKELETON,
                         G_IMPLEMENT_INTERFACE (IPC_TYPE_FLATPAK_SERVICE, service_iface_init))

static void
ipc_flatpak_service_impl_constructed (GObject *object)
{
  IpcFlatpakServiceImpl *self = (IpcFlatpakServiceImpl *)object;
  g_autoptr(GPtrArray) installations = NULL;
  g_autoptr(FlatpakInstallation) user = NULL;

  G_OBJECT_CLASS (ipc_flatpak_service_impl_parent_class)->constructed (object);

  if ((user = flatpak_installation_new_user (NULL, NULL)))
    add_installation (self, user, NULL);

  if ((installations = flatpak_get_system_installations (NULL, NULL)))
    {
      for (guint i = 0; i < installations->len; i++)
        add_installation (self, g_ptr_array_index (installations, i), NULL);
    }
}

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

  object_class->constructed = ipc_flatpak_service_impl_constructed;
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
