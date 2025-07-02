/* gbp-flatpak-config-provider.c
 *
 * Copyright 2016 Matthew Leeds <mleeds@redhat.com>
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

#define G_LOG_DOMAIN "gbp-flatpak-config-provider"

#include <glib/gi18n.h>
#include <json-glib/json-glib.h>
#include <libide-vcs.h>
#include <string.h>

#include "gbp-flatpak-client.h"
#include "gbp-flatpak-config-provider.h"
#include "gbp-flatpak-manifest.h"

#define DISCOVERY_MAX_DEPTH 4
#define MAX_MANIFEST_SIZE_IN_BYTES (1024L*256L) /* 256kb */

struct _GbpFlatpakConfigProvider
{
  IdeObject          parent_instance;
  IpcFlatpakService *service;
  GPtrArray         *configs;
};

static void manifest_save_tick    (IdeTask                         *task);
static void manifest_needs_reload (GbpFlatpakConfigProvider *self,
                                   GbpFlatpakManifest              *manifest);

static void
gbp_flatpak_config_provider_save_cb (GObject      *object,
                                            GAsyncResult *result,
                                            gpointer      user_data)
{
  GbpFlatpakManifest *manifest = (GbpFlatpakManifest *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (GBP_IS_FLATPAK_MANIFEST (manifest));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!gbp_flatpak_manifest_save_finish (manifest, result, &error))
    g_warning ("Failed to save manifest: %s", error->message);

  manifest_save_tick (task);
}

static void
manifest_save_tick (IdeTask *task)
{
  g_autoptr(GbpFlatpakManifest) manifest = NULL;
  GPtrArray *manifests;

  g_assert (IDE_IS_TASK (task));

  manifests = ide_task_get_task_data (task);
  g_assert (manifests != NULL);

  if (manifests->len == 0)
    {
      ide_task_return_boolean (task, TRUE);
      return;
    }

  manifest = g_object_ref (g_ptr_array_index (manifests, manifests->len - 1));
  g_assert (GBP_IS_FLATPAK_MANIFEST (manifest));
  g_ptr_array_remove_index (manifests, manifests->len - 1);

  gbp_flatpak_manifest_save_async (manifest,
                                   ide_task_get_cancellable (task),
                                   gbp_flatpak_config_provider_save_cb,
                                   g_object_ref (task));
}

static void
gbp_flatpak_config_provider_save_async (IdeConfigProvider   *provider,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  GbpFlatpakConfigProvider *self = (GbpFlatpakConfigProvider *)provider;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GPtrArray) ar = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_CONFIG_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_flatpak_config_provider_save_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  if (self->configs == NULL || self->configs->len == 0)
    {
      ide_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  g_assert (self->configs != NULL);
  g_assert (self->configs->len > 0);

  ar = g_ptr_array_new_with_free_func (g_object_unref);

  for (guint i = 0; i < self->configs->len; i++)
    {
      GbpFlatpakManifest *manifest = g_ptr_array_index (self->configs, i);

      if (ide_config_get_dirty (IDE_CONFIG (manifest)))
        g_ptr_array_add (ar, g_object_ref (manifest));
    }

  ide_task_set_task_data (task, g_steal_pointer (&ar), g_ptr_array_unref);

  manifest_save_tick (task);

  IDE_EXIT;
}

static gboolean
gbp_flatpak_config_provider_save_finish (IdeConfigProvider  *provider,
                                         GAsyncResult       *result,
                                         GError            **error)
{
  g_assert (GBP_IS_FLATPAK_CONFIG_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
load_manifest_worker (IdeTask      *task,
                      gpointer      source_object,
                      gpointer      task_data,
                      GCancellable *cancellable)
{
  GbpFlatpakConfigProvider *self = source_object;
  g_autoptr(GbpFlatpakManifest) manifest = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *name = NULL;
  GFile *file = task_data;

  g_assert (IDE_IS_TASK (task));
  g_assert (GBP_IS_FLATPAK_CONFIG_PROVIDER (self));
  g_assert (G_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  name = g_file_get_basename (file);
  manifest = gbp_flatpak_manifest_new (file, name);
  ide_object_append (IDE_OBJECT (self), IDE_OBJECT (manifest));

  if (!g_initable_init (G_INITABLE (manifest), cancellable, &error))
    {
      ide_clear_and_destroy_object (&manifest);
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_signal_connect_object (manifest,
                           "needs-reload",
                           G_CALLBACK (manifest_needs_reload),
                           self,
                           G_CONNECT_SWAPPED);

  ide_task_return_pointer (task, g_steal_pointer (&manifest), g_object_unref);
}

static void
load_manifest_async (GbpFlatpakConfigProvider *self,
                     GFile                           *file,
                     GCancellable                    *cancellable,
                     GAsyncReadyCallback              callback,
                     gpointer                         user_data)
{
  g_autoptr(IdeTask) task = NULL;

  g_assert (GBP_IS_FLATPAK_CONFIG_PROVIDER (self));
  g_assert (G_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, load_manifest_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);
  ide_task_set_task_data (task, g_object_ref (file), g_object_unref);
  ide_task_run_in_thread (task, load_manifest_worker);
}

static GbpFlatpakManifest *
load_manifest_finish (GbpFlatpakConfigProvider  *self,
                      GAsyncResult                     *result,
                      GError                          **error)
{
  g_assert (GBP_IS_FLATPAK_CONFIG_PROVIDER (self));
  g_assert (IDE_IS_TASK (result));
  g_assert (ide_task_is_valid (IDE_TASK (result), self));

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static void
reload_manifest_cb (GObject      *object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  GbpFlatpakConfigProvider *self = (GbpFlatpakConfigProvider *)object;
  g_autoptr(GbpFlatpakManifest) old_manifest = user_data;
  g_autoptr(GbpFlatpakManifest) new_manifest = NULL;
  g_autoptr(GError) error = NULL;
  IdeConfigManager *manager;
  IdeConfig *current;
  IdeContext *context;
  gboolean is_active;

  g_assert (GBP_IS_FLATPAK_CONFIG_PROVIDER (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_FLATPAK_MANIFEST (old_manifest));

  new_manifest = load_manifest_finish (self, result, &error);

  if (new_manifest == NULL)
    {
      g_warning ("Failed to reload manifest: %s", error->message);

      /* Watch for future changes */
      g_signal_connect_object (old_manifest,
                               "needs-reload",
                               G_CALLBACK (manifest_needs_reload),
                               self,
                               G_CONNECT_SWAPPED);
      return;
    }

  g_ptr_array_remove (self->configs, old_manifest);
  g_ptr_array_add (self->configs, g_object_ref (new_manifest));

  context = ide_object_get_context (IDE_OBJECT (self));
  manager = ide_config_manager_from_context (context);
  current = ide_config_manager_get_current (manager);

  is_active = current == IDE_CONFIG (old_manifest);

  ide_config_provider_emit_added (IDE_CONFIG_PROVIDER (self),
                                  IDE_CONFIG (new_manifest));

  if (is_active)
    ide_config_manager_set_current (manager,
                                    IDE_CONFIG (new_manifest));

  ide_config_provider_emit_removed (IDE_CONFIG_PROVIDER (self),
                                    IDE_CONFIG (old_manifest));
}

static void
manifest_needs_reload (GbpFlatpakConfigProvider *self,
                       GbpFlatpakManifest       *manifest)
{
  GFile *file;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_CONFIG_PROVIDER (self));
  g_assert (GBP_IS_FLATPAK_MANIFEST (manifest));

  g_signal_handlers_disconnect_by_func (manifest,
                                        G_CALLBACK (manifest_needs_reload),
                                        self);

  file = gbp_flatpak_manifest_get_file (manifest);

  load_manifest_async (self,
                       file,
                       NULL,
                       reload_manifest_cb,
                       g_object_ref (manifest));

  IDE_EXIT;
}

static int
sort_by_path (gconstpointer a,
              gconstpointer b)
{
  GbpFlatpakManifest *manifest_a = *(GbpFlatpakManifest **)a;
  GbpFlatpakManifest *manifest_b = *(GbpFlatpakManifest **)b;
  GFile *file_a = gbp_flatpak_manifest_get_file (manifest_a);
  GFile *file_b = gbp_flatpak_manifest_get_file (manifest_b);
  const char *path_a = g_file_peek_path (file_a);
  const char *path_b = g_file_peek_path (file_b);
  gboolean is_devel_a = strstr (path_a, ".Devel.") != NULL;
  gboolean is_devel_b = strstr (path_b, ".Devel.") != NULL;

  if (is_devel_a && !is_devel_b)
    return -1;

  if (!is_devel_a && is_devel_b)
    return 1;

  return g_utf8_collate (path_a, path_b);
}

static void
gbp_flatpak_config_provider_load_worker (IdeTask      *task,
                                         gpointer      source_object,
                                         gpointer      task_data,
                                         GCancellable *cancellable)
{
  GbpFlatpakConfigProvider *self = source_object;
  g_autoptr(GPtrArray) manifests = NULL;
  GPtrArray *files = task_data;

  IDE_ENTRY;

  g_assert (IDE_IS_TASK (task));
  g_assert (GBP_IS_FLATPAK_CONFIG_PROVIDER (self));
  g_assert (files != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  manifests = g_ptr_array_new_with_free_func (g_object_unref);

  for (guint i = 0; i < files->len; i++)
    {
      GFile *file = g_ptr_array_index (files, i);
      g_autoptr(GbpFlatpakManifest) manifest = NULL;
      g_autoptr(GFileInfo) info = NULL;
      g_autoptr(GError) error = NULL;
      g_autofree gchar *name = NULL;

      g_assert (G_IS_FILE (file));

      name = g_file_get_basename (file);

      if (!(info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_SIZE, 0, cancellable, NULL)) ||
          g_file_info_get_size (info) > MAX_MANIFEST_SIZE_IN_BYTES)
        {
          g_debug ("Ignoring %s as potential manifest, file size too large", name);
          continue;
        }

      manifest = gbp_flatpak_manifest_new (file, name);
      ide_object_append (IDE_OBJECT (self), IDE_OBJECT (manifest));

      if (!g_initable_init (G_INITABLE (manifest), cancellable, &error))
        {
          ide_clear_and_destroy_object (&manifest);
          g_message ("%s is not a flatpak manifest, skipping: %s",
                     name, error->message);
          continue;
        }

      g_assert (ide_config_get_dirty (IDE_CONFIG (manifest)) == FALSE);

      g_ptr_array_add (manifests, g_steal_pointer (&manifest));
    }

  g_ptr_array_sort (manifests, sort_by_path);

  ide_task_return_pointer (task,
                           g_steal_pointer (&manifests),
                           g_ptr_array_unref);

  IDE_EXIT;
}

static void
load_find_files_cb (GObject      *object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  GFile *file = (GFile *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GPtrArray) ret = NULL;

  IDE_ENTRY;

  g_assert (G_IS_FILE (file));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  ret = ide_g_file_find_finish (file, result, &error);
  IDE_PTR_ARRAY_SET_FREE_FUNC (ret, g_object_unref);

  if (ret == NULL)
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  ide_task_set_task_data (task, g_steal_pointer (&ret), g_ptr_array_unref);
  ide_task_run_in_thread (task, gbp_flatpak_config_provider_load_worker);

  IDE_EXIT;
}

static gboolean
contains_file (GbpFlatpakConfigProvider *self,
               GFile                    *file)
{
  g_autofree gchar *path = NULL;

  g_assert (GBP_IS_FLATPAK_CONFIG_PROVIDER (self));
  g_assert (G_IS_FILE (file));

  path = g_file_get_path (file);
  g_debug ("Checking for existing configuration: %s", path);

  for (guint i = 0; i < self->configs->len; i++)
    {
      GbpFlatpakManifest *manifest = g_ptr_array_index (self->configs, i);
      g_autofree gchar *loc_path = NULL;
      GFile *loc;

      g_assert (GBP_IS_FLATPAK_MANIFEST (manifest));

      loc = gbp_flatpak_manifest_get_file (manifest);

      loc_path = g_file_get_path (loc);
      g_debug ("  [%u] = %s", i, loc_path);

      if (g_file_equal (loc, file))
        return TRUE;
    }

  return FALSE;
}

static void
gbp_flatpak_config_provider_monitor_changed (GbpFlatpakConfigProvider *self,
                                             GFile                    *file,
                                             GFile                    *other_file,
                                             GFileMonitorEvent         event,
                                             IdeVcsMonitor            *monitor)
{
  g_assert (GBP_IS_FLATPAK_CONFIG_PROVIDER (self));
  g_assert (G_IS_FILE (file));
  g_assert (!other_file || G_IS_FILE (other_file));
  g_assert (IDE_IS_VCS_MONITOR (monitor));

  if (ide_object_in_destruction (IDE_OBJECT (self)) ||
      ide_object_in_destruction (IDE_OBJECT (monitor)))
    return;

  if (event == G_FILE_MONITOR_EVENT_CREATED)
    {
      g_autofree gchar *name = g_file_get_basename (file);

      if (name != NULL &&
          (g_str_has_suffix (name, ".json") ||
           g_str_has_suffix (name, ".yaml") ||
           g_str_has_suffix (name, ".yml")) &&
          !contains_file (self, file))
        {
          g_autoptr(GbpFlatpakManifest) manifest = NULL;
          g_autoptr(GError) error = NULL;

          manifest = gbp_flatpak_manifest_new (file, name);
          ide_object_append (IDE_OBJECT (self), IDE_OBJECT (manifest));

          if (!g_initable_init (G_INITABLE (manifest), NULL, &error))
            {
              ide_clear_and_destroy_object (&manifest);
              g_message ("%s is not a flatpak manifest, skipping: %s",
                         name, error->message);
              return;
            }

          g_signal_connect_object (manifest,
                                   "needs-reload",
                                   G_CALLBACK (manifest_needs_reload),
                                   self,
                                   G_CONNECT_SWAPPED);

          g_ptr_array_add (self->configs, g_object_ref (manifest));

          ide_config_provider_emit_added (IDE_CONFIG_PROVIDER (self),
                                                 IDE_CONFIG (manifest));
        }
    }
}

static void
gbp_flatpak_config_provider_load_client_cb (GObject      *object,
                                            GAsyncResult *result,
                                            gpointer      user_data)
{
  GbpFlatpakClient *client = (GbpFlatpakClient *)object;
  g_autoptr(IpcFlatpakService) service = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  GbpFlatpakConfigProvider *self;
  IdeVcsMonitor *monitor;
  IdeContext *context;
  IdeVcs *vcs;
  GFile *workdir;
  /* expect a.b.json at least, if not
   * a.b.c.json or a.b.c.d.json or more.
   */
  const gchar *patterns[4] = {
    "*.*.json",
    "*.*.yaml",
    "*.*.yml",
    NULL
  };

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_FLATPAK_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!(service = gbp_flatpak_client_get_service_finish (client, result, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  self = ide_task_get_source_object (task);
  context = ide_object_get_context (IDE_OBJECT (self));
  vcs = ide_vcs_from_context (context);
  workdir = ide_vcs_get_workdir (vcs);
  monitor = ide_context_peek_child_typed (context, IDE_TYPE_VCS_MONITOR);

  g_set_object (&self->service, service);

  g_signal_connect_object (monitor,
                           "changed",
                           G_CALLBACK (gbp_flatpak_config_provider_monitor_changed),
                           self,
                           G_CONNECT_SWAPPED);

  ide_g_file_find_multiple_with_depth_async (workdir,
                                             patterns,
                                             DISCOVERY_MAX_DEPTH,
                                             ide_task_get_cancellable (task),
                                             load_find_files_cb,
                                             g_object_ref (task));

  IDE_EXIT;
}

static void
gbp_flatpak_config_provider_load_async (IdeConfigProvider   *provider,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  GbpFlatpakConfigProvider *self = (GbpFlatpakConfigProvider *)provider;
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_FLATPAK_CONFIG_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (provider, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_flatpak_config_provider_load_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  gbp_flatpak_client_get_service_async (gbp_flatpak_client_get_default (),
                                        NULL,
                                        gbp_flatpak_config_provider_load_client_cb,
                                        g_steal_pointer (&task));

  IDE_EXIT;
}

static IdeConfig *
guess_best_config (GPtrArray *ar)
{
  g_assert (ar != NULL);
  g_assert (ar->len > 0);

  for (guint i = 0; i < ar->len; i++)
    {
      GbpFlatpakManifest *config = g_ptr_array_index (ar, i);
      g_autofree gchar *path = gbp_flatpak_manifest_get_path (config);

      if (strstr (path, "-unstable.json") != NULL)
        return IDE_CONFIG (config);
      else if (strstr (path, "-unstable.yml") != NULL)
        return IDE_CONFIG (config);
      else if (strstr (path, "-unstable.yaml") != NULL)
        return IDE_CONFIG (config);
    }

  for (guint i = 0; i < ar->len; i++)
    {
      GbpFlatpakManifest *config = g_ptr_array_index (ar, i);
      g_autofree gchar *path = gbp_flatpak_manifest_get_path (config);
      g_autofree gchar *base = g_path_get_basename (path);
      const gchar *app_id = ide_config_get_app_id (IDE_CONFIG (config));
      g_autofree gchar *app_id_manifest = NULL;

      if (g_str_has_suffix(base, ".json"))
        app_id_manifest = g_strdup_printf ("%s.json", app_id);
      else if (g_str_has_suffix(base, ".yml"))
        app_id_manifest = g_strdup_printf ("%s.yml", app_id);
      else if (g_str_has_suffix(base, ".yaml"))
        app_id_manifest = g_strdup_printf ("%s.yaml", app_id);
      else
        continue;

      /* If appid.json is the same as the filename, that is the
       * best match (after unstable) we can have. Use it.
       */
      if (ide_str_equal0 (app_id_manifest, base))
        return IDE_CONFIG (config);
    }

  return g_ptr_array_index (ar, 0);
}

static void
gbp_flatpak_config_provider_resolve_cb (GObject      *object,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
  GbpFlatpakManifest *manifest = (GbpFlatpakManifest *)object;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_FLATPAK_MANIFEST (manifest));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!gbp_flatpak_manifest_resolve_extensions_finish (manifest, result, &error))
    g_warning ("Failed to resolve SDK extensions: %s", error->message);

  IDE_EXIT;
}

static gboolean
gbp_flatpak_config_provider_load_finish (IdeConfigProvider  *provider,
                                         GAsyncResult       *result,
                                         GError            **error)
{
  GbpFlatpakConfigProvider *self = (GbpFlatpakConfigProvider *)provider;
  g_autoptr(GPtrArray) configs = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_FLATPAK_CONFIG_PROVIDER (self));
  g_assert (IDE_IS_TASK (result));
  g_assert (ide_task_is_valid (IDE_TASK (result), provider));

  configs = ide_task_propagate_pointer (IDE_TASK (result), error);

  if (configs == NULL)
    return FALSE;

  g_clear_pointer (&self->configs, g_ptr_array_unref);
  self->configs = g_ptr_array_ref (configs);

  for (guint i = 0; i < configs->len; i++)
    {
      IdeConfig *config = g_ptr_array_index (configs, i);

      g_assert (GBP_IS_FLATPAK_MANIFEST (config));
      g_assert (ide_config_get_dirty (config) == FALSE);

      g_signal_connect_object (config,
                               "needs-reload",
                               G_CALLBACK (manifest_needs_reload),
                               self,
                               G_CONNECT_SWAPPED);

      ide_config_provider_emit_added (provider, config);

      gbp_flatpak_manifest_resolve_extensions_async (GBP_FLATPAK_MANIFEST (config),
                                                     self->service,
                                                     NULL,
                                                     gbp_flatpak_config_provider_resolve_cb,
                                                     NULL);
    }

  if (configs->len > 0)
    {
      IdeConfig *config = guess_best_config (configs);
      IdeContext *context = ide_object_get_context (IDE_OBJECT (self));
      IdeConfigManager *manager = ide_config_manager_from_context (context);

      g_assert (IDE_IS_CONFIG (config));

      /* TODO: We should have a GSetting for this, in config-manager */
      ide_config_manager_set_current (manager, config);
    }

  return TRUE;
}

static void
gbp_flatpak_config_provider_unload (IdeConfigProvider *provider)
{
  GbpFlatpakConfigProvider *self = (GbpFlatpakConfigProvider *)provider;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_CONFIG_PROVIDER (self));

  if (self->configs != NULL)
    {
      for (guint i = self->configs->len; i > 0; i--)
        {
          g_autoptr(IdeConfig) config = NULL;

          config = g_object_ref (g_ptr_array_index (self->configs, i - 1));
          g_signal_handlers_disconnect_by_func (config,
                                                G_CALLBACK (manifest_needs_reload),
                                                self);
          g_ptr_array_remove_index (self->configs, i - 1);

          ide_config_provider_emit_removed (provider, config);
        }

      g_clear_pointer (&self->configs, g_ptr_array_unref);
    }

  IDE_EXIT;
}

static void
gbp_flatpak_config_provider_duplicate (IdeConfigProvider *provider,
                                       IdeConfig         *configuration)
{
  GbpFlatpakManifest *manifest = (GbpFlatpakManifest *)configuration;
  g_autofree gchar *path = NULL;
  g_autofree gchar *base = NULL;
  g_autoptr(GFile) parent = NULL;
  const gchar *extension = ".json";
  gchar *dot;
  GFile *file;

  g_assert (GBP_IS_FLATPAK_CONFIG_PROVIDER (provider));
  g_assert (GBP_IS_FLATPAK_MANIFEST (manifest));

  file = gbp_flatpak_manifest_get_file (manifest);
  path = g_file_get_path (file);
  base = g_file_get_basename (file);
  parent = g_file_get_parent (file);

  if ((dot = strrchr (base, '.'))) {
    if (ide_str_equal0 (dot, ".yaml"))
      extension = ".yaml";
    else if (ide_str_equal0 (dot, ".yml"))
      extension = ".yml";

    *dot = '\0';
  }

  for (guint i = 2; i <= 10; i++)
    {
      g_autofree gchar *name = g_strdup_printf ("%s-%u%s", base, i, extension);
      g_autoptr(GFile) dest = g_file_get_child (parent, name);

      if (!g_file_query_exists (dest, NULL))
        {
          g_file_copy (file, dest,
                       G_FILE_COPY_ALL_METADATA,
                       NULL, NULL, NULL, NULL);
          break;
        }
    }
}

static void
gbp_flatpak_config_provider_delete (IdeConfigProvider *provider,
                                    IdeConfig         *configuration)
{
  GbpFlatpakConfigProvider *self = (GbpFlatpakConfigProvider *)provider;
  GbpFlatpakManifest *manifest = (GbpFlatpakManifest *)configuration;
  g_autoptr(IdeConfig) hold = NULL;
  g_autoptr(GFile) file = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *name = NULL;

  g_assert (GBP_IS_FLATPAK_CONFIG_PROVIDER (self));
  g_assert (GBP_IS_FLATPAK_MANIFEST (manifest));

  hold = g_object_ref (configuration);
  file = g_object_ref (gbp_flatpak_manifest_get_file (manifest));
  name = g_file_get_basename (file);

  if (g_ptr_array_remove (self->configs, hold))
    {
      ide_config_provider_emit_removed (provider, hold);
      if (!g_file_delete (file, NULL, &error))
        ide_object_warning (provider, _("Failed to remove flatpak manifest: %s"), name);
    }
}

static void
configuration_provider_iface_init (IdeConfigProviderInterface *iface)
{
  iface->load_async = gbp_flatpak_config_provider_load_async;
  iface->load_finish = gbp_flatpak_config_provider_load_finish;
  iface->unload = gbp_flatpak_config_provider_unload;
  iface->save_async = gbp_flatpak_config_provider_save_async;
  iface->save_finish = gbp_flatpak_config_provider_save_finish;
  iface->duplicate = gbp_flatpak_config_provider_duplicate;
  iface->delete = gbp_flatpak_config_provider_delete;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpFlatpakConfigProvider,
                         gbp_flatpak_config_provider,
                         IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_CONFIG_PROVIDER,
                                                configuration_provider_iface_init))

static void
gbp_flatpak_config_provider_finalize (GObject *object)
{
  GbpFlatpakConfigProvider *self = (GbpFlatpakConfigProvider *)object;

  g_clear_pointer (&self->configs, g_ptr_array_unref);
  g_clear_object (&self->service);

  G_OBJECT_CLASS (gbp_flatpak_config_provider_parent_class)->finalize (object);
}

static void
gbp_flatpak_config_provider_class_init (GbpFlatpakConfigProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_flatpak_config_provider_finalize;
}

static void
gbp_flatpak_config_provider_init (GbpFlatpakConfigProvider *self)
{
  self->configs = g_ptr_array_new_with_free_func (g_object_unref);
}
