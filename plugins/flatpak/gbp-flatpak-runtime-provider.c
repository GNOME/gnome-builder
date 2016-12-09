/* gbp-flatpak-runtime-provider.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-flatpak-runtime-provider"

#include <string.h>
#include <flatpak.h>
#include <json-glib/json-glib.h>

#include "util/ide-posix.h"

#include "gbp-flatpak-runtime.h"
#include "gbp-flatpak-runtime-provider.h"

struct _GbpFlatpakRuntimeProvider
{
  GObject              parent_instance;
  IdeRuntimeManager   *manager;
  FlatpakInstallation *user_installation;
  FlatpakInstallation *system_installation;
  GCancellable        *cancellable;
  GPtrArray           *runtimes;
  GFileMonitor        *system_flatpak_monitor;
  GFileMonitor        *user_flatpak_monitor;
};

typedef struct
{
  gchar *platform;
  gchar *branch;
  gchar *sdk;
  gchar *app_id;
  gchar *primary_module;
  GFile *file;
} FlatpakManifest;

static void runtime_provider_iface_init (IdeRuntimeProviderInterface *);

G_DEFINE_TYPE_EXTENDED (GbpFlatpakRuntimeProvider, gbp_flatpak_runtime_provider, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_RUNTIME_PROVIDER,
                                               runtime_provider_iface_init))

static void gbp_flatpak_runtime_provider_load (IdeRuntimeProvider *provider, IdeRuntimeManager *manager);
static void gbp_flatpak_runtime_provider_unload (IdeRuntimeProvider *provider, IdeRuntimeManager *manager);

static inline void
sanitize_name (gchar *name)
{
  gchar *tmp = strchr (name, '/');

  if (tmp != NULL)
    *tmp = '\0';
}

static gboolean
contains_id (GPtrArray   *ar,
             const gchar *id)
{
  g_assert (ar != NULL);
  g_assert (id != NULL);

  for (guint i = 0; i < ar->len; i++)
    {
      IdeRuntime *runtime = g_ptr_array_index (ar, i);

      g_assert (IDE_IS_RUNTIME (runtime));

      if (ide_str_equal0 (id, ide_runtime_get_id (runtime)))
        return TRUE;
    }

  return FALSE;
}

static void
flatpak_manifest_free (void *data)
{
  FlatpakManifest *manifest = data;

  g_free (manifest->branch);
  g_free (manifest->sdk);
  g_free (manifest->platform);
  g_free (manifest->app_id);
  g_free (manifest->primary_module);
  g_clear_object (&manifest->file);
  g_slice_free (FlatpakManifest, manifest);
}

static gboolean
gbp_flatpak_runtime_provider_load_refs (GbpFlatpakRuntimeProvider  *self,
                                        FlatpakInstallation        *installation,
                                        GPtrArray                  *runtimes,
                                        GCancellable               *cancellable,
                                        GError                    **error)
{
  g_autofree gchar *host_type = ide_get_system_arch ();
  g_autoptr(GPtrArray) ar = NULL;
  IdeContext *context;

  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (self));
  g_assert (FLATPAK_IS_INSTALLATION (installation));

  context = ide_object_get_context (IDE_OBJECT (self->manager));
  ar = flatpak_installation_list_installed_refs_by_kind (installation,
                                                         FLATPAK_REF_KIND_RUNTIME,
                                                         cancellable,
                                                         error);

  if (ar == NULL)
    return FALSE;

  IDE_TRACE_MSG ("Found %u installation refs", ar->len);

  for (guint i = 0; i < ar->len; i++)
    {
      FlatpakInstalledRef *ref = g_ptr_array_index (ar, i);
      g_autofree gchar *str = NULL;
      g_autofree gchar *id = NULL;
      g_autofree gchar *name = NULL;
      const gchar *arch;
      const gchar *branch;
      g_autoptr(GBytes) metadata = NULL;
      g_autofree gchar *sdk = NULL;
      g_autoptr(GKeyFile) key_file = NULL;
      const gchar *metadata_data;
      gsize metadata_len;
      g_autoptr(GError) local_error = NULL;

      g_assert (FLATPAK_IS_INSTALLED_REF (ref));
      g_assert (flatpak_ref_get_kind (FLATPAK_REF (ref)) == FLATPAK_REF_KIND_RUNTIME);

      name = g_strdup (flatpak_ref_get_name (FLATPAK_REF (ref)));

      /* Ignore runtimes like org.gnome.Platform.Locale */
      if (name && g_str_has_suffix (name, ".Locale"))
        continue;

      sanitize_name (name);

      arch = flatpak_ref_get_arch (FLATPAK_REF (ref));
      branch = flatpak_ref_get_branch (FLATPAK_REF (ref));

      id = g_strdup_printf ("flatpak:%s/%s/%s", name, branch, arch);

      if (contains_id (runtimes, id))
        continue;

      if (g_strcmp0 (host_type, arch) == 0)
        str = g_strdup_printf ("%s <b>%s</b>", name, branch);
      else
        str = g_strdup_printf ("%s <b>%s</b> <sup>%s</sup>", name, branch, arch);

      metadata = flatpak_installed_ref_load_metadata (FLATPAK_INSTALLED_REF (ref), cancellable, &local_error);

      if (metadata == NULL)
        {
          g_warning ("%s", local_error->message);
          continue;
        }

      metadata_data = g_bytes_get_data (metadata, &metadata_len);

      key_file = g_key_file_new ();

      if (!g_key_file_load_from_data (key_file, metadata_data, metadata_len, G_KEY_FILE_NONE, &local_error))
        {
          /*
           * If this is not really a runtime, but something like a locale, then
           * the metadata file will not exist.
           */
          if (!g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
            g_warning ("%s", local_error->message);

          continue;
        }

      if (!(sdk = g_key_file_get_string (key_file, "Runtime", "sdk", NULL)))
        sdk = g_strdup (name);

      sanitize_name (sdk);

      IDE_TRACE_MSG ("Discovered flatpak runtime %s/%s/%s", name, branch, arch);

      g_ptr_array_add (runtimes,
                       g_object_new (GBP_TYPE_FLATPAK_RUNTIME,
                                     "branch", branch,
                                     "sdk", sdk,
                                     "platform", name,
                                     "context", context,
                                     "id", id,
                                     "display-name", str,
                                     NULL));
    }

  return TRUE;
}

static gchar *
guess_primary_module (JsonObject *manifest_object,
                      GFile      *directory)
{
  JsonArray *modules = NULL;
  gchar *dir_name;
  guint num_modules;

  g_assert (manifest_object != NULL);
  g_assert (G_IS_FILE (directory));

  dir_name = g_file_get_basename (directory);
  g_assert (!ide_str_empty0 (dir_name));
  modules = json_object_get_array_member (manifest_object, "modules");
  g_assert (modules != NULL);

  num_modules = json_array_get_length (modules);
  for (guint i = 0; i < num_modules; i++)
    {
      JsonNode *module;
      module = json_array_get_element (modules, i);
      if (JSON_NODE_HOLDS_OBJECT (module))
        {
          const gchar *module_name;
          module_name = json_object_get_string_member (json_node_get_object (module), "name");
          if (num_modules == 1 || g_strcmp0 (module_name, dir_name) == 0)
            return g_strdup (module_name);
        }
    }

  g_warning ("Unable to determine the primary module in the flatpak manifest");
  return NULL;
}

static GPtrArray *
gbp_flatpak_runtime_provider_find_flatpak_manifests (GbpFlatpakRuntimeProvider *self,
                                                     GCancellable              *cancellable,
                                                     GFile                     *directory,
                                                     GError                   **error)
{
  g_autoptr(GFileEnumerator) enumerator = NULL;
  GFileInfo *file_info = NULL;
  GPtrArray *ar;

  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (G_IS_FILE (directory));

  ar = g_ptr_array_new ();
  g_ptr_array_set_free_func (ar, flatpak_manifest_free);

  enumerator = g_file_enumerate_children (directory,
                                          G_FILE_ATTRIBUTE_STANDARD_NAME,
                                          G_FILE_QUERY_INFO_NONE,
                                          cancellable,
                                          error);
  if (!enumerator)
    return NULL;

  while ((file_info = g_file_enumerator_next_file (enumerator, cancellable, NULL)))
    {
      GFileType file_type;
      g_autofree gchar *name = NULL;
      g_autofree gchar *path = NULL;
      g_autoptr(GRegex) filename_regex = NULL;
      g_autoptr(GMatchInfo) match_info = NULL;
      g_autoptr(JsonParser) parser = NULL;
      JsonNode *root_node = NULL;
      JsonNode *app_id_node = NULL;
      JsonNode *id_node = NULL;
      JsonNode *runtime_node = NULL;
      JsonNode *runtime_version_node = NULL;
      JsonNode *sdk_node = NULL;
      JsonNode *modules_node = NULL;
      JsonObject *root_object = NULL;
      g_autoptr(GError) local_error = NULL;
      g_autoptr(GFile) file = NULL;
      FlatpakManifest *manifest;

      file_type = g_file_info_get_file_type (file_info);
      name = g_strdup (g_file_info_get_name (file_info));
      g_clear_object (&file_info);

      if (name == NULL || file_type == G_FILE_TYPE_DIRECTORY)
        continue;

      file = g_file_get_child (directory, name);

      /* This regex is based on https://wiki.gnome.org/HowDoI/ChooseApplicationID */
      filename_regex = g_regex_new ("^[[:alnum:]-_]+\\.[[:alnum:]-_]+(\\.[[:alnum:]-_]+)*\\.json$",
                                    0, 0, NULL);

      g_regex_match (filename_regex, name, 0, &match_info);
      if (!g_match_info_matches (match_info))
        continue;

      /* Check if the contents look like a flatpak manifest */
      path = g_file_get_path (file);
      parser = json_parser_new ();
      json_parser_load_from_file (parser, path, &local_error);
      if (local_error != NULL)
        continue;

      root_node = json_parser_get_root (parser);
      if (!JSON_NODE_HOLDS_OBJECT (root_node))
        continue;

      root_object = json_node_get_object (root_node);
      app_id_node = json_object_get_member (root_object, "app-id");
      id_node = json_object_get_member (root_object, "id");
      runtime_node = json_object_get_member (root_object, "runtime");
      runtime_version_node = json_object_get_member (root_object, "runtime-version");
      sdk_node = json_object_get_member (root_object, "sdk");
      modules_node = json_object_get_member (root_object, "modules");

      if ((!JSON_NODE_HOLDS_VALUE (app_id_node) && !JSON_NODE_HOLDS_VALUE (id_node)) ||
           !JSON_NODE_HOLDS_VALUE (runtime_node) ||
           !JSON_NODE_HOLDS_VALUE (sdk_node) ||
           !JSON_NODE_HOLDS_ARRAY (modules_node))
        continue;

      IDE_TRACE_MSG ("Discovered flatpak manifest at %s", path);

      manifest = g_slice_new0 (FlatpakManifest);
      manifest->file = g_steal_pointer (&file);
      manifest->platform = json_node_dup_string (runtime_node);
      if (!JSON_NODE_HOLDS_VALUE (runtime_version_node) || ide_str_empty0 (json_node_get_string (runtime_version_node)))
        manifest->branch = g_strdup ("master");
      else
        manifest->branch = json_node_dup_string (runtime_version_node);
      manifest->sdk = json_node_dup_string (sdk_node);
      if (JSON_NODE_HOLDS_VALUE (app_id_node))
        manifest->app_id = json_node_dup_string (app_id_node);
      else
        manifest->app_id = json_node_dup_string (id_node);
      manifest->primary_module = guess_primary_module (root_object, directory);

      g_ptr_array_add (ar, manifest);
    }

  return ar;
}

static gboolean
gbp_flatpak_runtime_provider_load_manifests (GbpFlatpakRuntimeProvider  *self,
                                             GPtrArray                  *runtimes,
                                             GCancellable               *cancellable,
                                             GError                    **error)
{
  g_autoptr(GPtrArray) ar = NULL;
  g_autoptr(GFileInfo) file_info = NULL;
  IdeContext *context;
  GFile *project_file;
  g_autoptr(GFile) project_dir = NULL;

  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (self));

  context = ide_object_get_context (IDE_OBJECT (self->manager));
  project_file = ide_context_get_project_file (context);

  g_assert (G_IS_FILE (project_file));

  file_info = g_file_query_info (project_file,
                                 G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                 G_FILE_QUERY_INFO_NONE,
                                 cancellable,
                                 error);

  if (file_info == NULL)
    return FALSE;

  if (g_file_info_get_file_type (file_info) == G_FILE_TYPE_DIRECTORY)
    project_dir = g_object_ref (project_file);
  else
    project_dir = g_file_get_parent (project_file);

  ar = gbp_flatpak_runtime_provider_find_flatpak_manifests (self, cancellable, project_dir, error);

  if (ar == NULL)
    return FALSE;

  IDE_TRACE_MSG ("Found %u flatpak manifests", ar->len);

  for (guint i = 0; i < ar->len; i++)
    {
      FlatpakManifest *manifest = g_ptr_array_index (ar, i);
      g_autofree gchar *filename = NULL;
      g_autofree gchar *hash = NULL;
      g_autofree gchar *id = NULL;
      g_autofree gchar *manifest_data = NULL;
      g_autofree gchar *path = NULL;
      gsize manifest_data_len = 0;

      path = g_file_get_path (manifest->file);

      if (g_file_get_contents (path, &manifest_data, &manifest_data_len, NULL))
        {
          g_autoptr(GChecksum) checksum = NULL;

          checksum = g_checksum_new (G_CHECKSUM_SHA1);
          g_checksum_update (checksum, (const guint8 *)manifest_data, manifest_data_len);
          hash = g_strdup (g_checksum_get_string (checksum));
        }

      filename = g_file_get_basename (manifest->file);

      if (hash != NULL)
        id = g_strdup_printf ("%s@%s", filename, hash);
      else
        id = g_strdup (filename);

      if (contains_id (runtimes, id))
        continue;

      g_ptr_array_add (runtimes,
                       g_object_new (GBP_TYPE_FLATPAK_RUNTIME,
                                     "branch", manifest->branch,
                                     "sdk", manifest->sdk,
                                     "platform", manifest->platform,
                                     "manifest", manifest->file,
                                     "primary-module", manifest->primary_module,
                                     "app-id", manifest->app_id,
                                     "context", context,
                                     "id", id,
                                     "display-name", filename,
                                     NULL));
    }

  return TRUE;
}

static void
on_flatpak_installation_changed (GbpFlatpakRuntimeProvider *self,
                                 GFile                     *file,
                                 GFile                     *other_file,
                                 GFileMonitorEvent          event_type,
                                 GFileMonitor              *monitor)
{
  IdeRuntimeManager *manager;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (self));
  g_assert (G_IS_FILE_MONITOR (self));
  g_assert (!file || G_IS_FILE (file));
  g_assert (!other_file || G_IS_FILE (other_file));

  /* Save a pointer to manager before unload() wipes it out */
  manager = self->manager;

  gbp_flatpak_runtime_provider_unload (IDE_RUNTIME_PROVIDER (self), manager);
  gbp_flatpak_runtime_provider_load (IDE_RUNTIME_PROVIDER (self), manager);

  IDE_EXIT;
}

static void
gbp_flatpak_runtime_provider_load_worker (GTask        *task,
                                          gpointer      source_object,
                                          gpointer      task_data,
                                          GCancellable *cancellable)
{
  GbpFlatpakRuntimeProvider *self = source_object;
  g_autoptr(GPtrArray) ret = NULL;
  g_autoptr(GFile) file = NULL;
  g_autofree gchar *path = NULL;
  GError *error = NULL;

  IDE_ENTRY;

  g_assert (G_IS_TASK (task));
  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (self));
  g_assert (IDE_IS_RUNTIME_MANAGER (self->manager));

  ret = g_ptr_array_new_with_free_func (g_object_unref);

  /* Load system flatpak runtimes */
  if (NULL == (self->system_installation = flatpak_installation_new_system (cancellable, &error)) ||
      !gbp_flatpak_runtime_provider_load_refs (self, self->system_installation, ret, cancellable, &error))
    {
      g_warning ("Failed to load system installation: %s", error->message);
      g_clear_error (&error);
    }

  /* Load user flatpak runtimes */
  path = g_build_filename (g_get_home_dir (), ".local", "share", "flatpak", NULL);
  file = g_file_new_for_path (path);

  if (NULL == (self->user_installation = flatpak_installation_new_for_path (file, TRUE, cancellable, &error)) ||
      !gbp_flatpak_runtime_provider_load_refs (self, self->user_installation, ret, cancellable, &error))
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
    }

  /* Load flatpak manifests in the repo */
  if (!gbp_flatpak_runtime_provider_load_manifests (self, ret, cancellable, &error))
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
    }

  /* Set up file monitors so the list of runtimes refreshes when necessary */
  if (self->system_installation != NULL)
    {
      if (NULL == (self->system_flatpak_monitor = flatpak_installation_create_monitor (self->system_installation,
                                                                                       cancellable, &error)))
        {
          g_warning ("Failed to create flatpak installation file monitor: %s", error->message);
          g_clear_error (&error);
        }
      else
        {
          g_signal_connect_object (self->system_flatpak_monitor,
                                   "changed",
                                   G_CALLBACK (on_flatpak_installation_changed),
                                   self,
                                   G_CONNECT_SWAPPED);
        }
    }

  if (self->user_installation != NULL)
    {
      if (NULL == (self->user_flatpak_monitor = flatpak_installation_create_monitor (self->user_installation,
                                                                                     cancellable, &error)))
        {
          g_warning ("Failed to create flatpak installation file monitor: %s", error->message);
          g_clear_error (&error);
        }
      else
        {
          g_signal_connect_object (self->user_flatpak_monitor,
                                   "changed",
                                   G_CALLBACK (on_flatpak_installation_changed),
                                   self,
                                   G_CONNECT_SWAPPED);
        }
    }

  g_task_return_pointer (task, g_steal_pointer (&ret), (GDestroyNotify)g_ptr_array_unref);

  IDE_EXIT;
}

static void
gbp_flatpak_runtime_provider_load_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  GbpFlatpakRuntimeProvider *self = (GbpFlatpakRuntimeProvider *)object;
  GPtrArray *ret;
  GError *error = NULL;
  guint i;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (self));
  g_assert (G_IS_TASK (result));

  if (!(ret = g_task_propagate_pointer (G_TASK (result), &error)))
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
      IDE_EXIT;
    }

  for (i = 0; i < ret->len; i++)
    {
      IdeRuntime *runtime = g_ptr_array_index (ret, i);

      ide_runtime_manager_add (self->manager, runtime);
    }

  self->runtimes = ret;

  IDE_EXIT;
}

static void
gbp_flatpak_runtime_provider_load (IdeRuntimeProvider *provider,
                                   IdeRuntimeManager  *manager)
{
  GbpFlatpakRuntimeProvider *self = (GbpFlatpakRuntimeProvider *)provider;
  g_autoptr(GTask) task = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (self));
  g_assert (IDE_IS_RUNTIME_MANAGER (manager));

  ide_set_weak_pointer (&self->manager, manager);

  self->cancellable = g_cancellable_new ();

  task = g_task_new (self, self->cancellable, gbp_flatpak_runtime_provider_load_cb, NULL);
  g_task_run_in_thread (task, gbp_flatpak_runtime_provider_load_worker);

  IDE_EXIT;
}

static void
gbp_flatpak_runtime_provider_unload (IdeRuntimeProvider *provider,
                                     IdeRuntimeManager  *manager)
{
  GbpFlatpakRuntimeProvider *self = (GbpFlatpakRuntimeProvider *)provider;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (self));
  g_assert (IDE_IS_RUNTIME_MANAGER (manager));

  if (self->system_flatpak_monitor != NULL)
    {
      g_file_monitor_cancel (self->system_flatpak_monitor);
      g_clear_object (&self->system_flatpak_monitor);
    }
  if (self->user_flatpak_monitor != NULL)
    {
      g_file_monitor_cancel (self->user_flatpak_monitor);
      g_clear_object (&self->user_flatpak_monitor);
    }

  if (self->runtimes != NULL)
    {
      for (guint i= 0; i < self->runtimes->len; i++)
        {
          IdeRuntime *runtime = g_ptr_array_index (self->runtimes, i);

          ide_runtime_manager_remove (manager, runtime);
        }
    }

  g_clear_pointer (&self->runtimes, g_ptr_array_unref);

  if (self->cancellable != NULL)
    g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  g_clear_object (&self->system_installation);
  g_clear_object (&self->user_installation);

  ide_clear_weak_pointer (&self->manager);

  IDE_EXIT;
}

static void
gbp_flatpak_runtime_provider_class_init (GbpFlatpakRuntimeProviderClass *klass)
{
}

static void
gbp_flatpak_runtime_provider_init (GbpFlatpakRuntimeProvider *self)
{
}

static void
runtime_provider_iface_init (IdeRuntimeProviderInterface *iface)
{
  iface->load = gbp_flatpak_runtime_provider_load;
  iface->unload = gbp_flatpak_runtime_provider_unload;
}
