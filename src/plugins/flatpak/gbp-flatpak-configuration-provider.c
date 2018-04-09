/* gbp-flatpak-configuration-provider.c
 *
 * Copyright © 2016 Matthew Leeds <mleeds@redhat.com>
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

#define G_LOG_DOMAIN "gbp-flatpak-configuration-provider"

#include <flatpak.h>
#include <glib/gi18n.h>
#include <json-glib/json-glib.h>
#include <string.h>

#include "gbp-flatpak-configuration-provider.h"
#include "gbp-flatpak-manifest.h"

#define DISCOVERY_MAX_DEPTH 3

struct _GbpFlatpakConfigurationProvider
{
  IdeObject  parent_instance;
  GPtrArray *configs;
};

static void manifest_save_tick    (GTask                           *task);
static void manifest_needs_reload (GbpFlatpakConfigurationProvider *self,
                                   GbpFlatpakManifest              *manifest);

static void
gbp_flatpak_configuration_provider_save_cb (GObject      *object,
                                            GAsyncResult *result,
                                            gpointer      user_data)
{
  GbpFlatpakManifest *manifest = (GbpFlatpakManifest *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (GBP_IS_FLATPAK_MANIFEST (manifest));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!gbp_flatpak_manifest_save_finish (manifest, result, &error))
    g_warning ("Failed to save manifest: %s", error->message);

  manifest_save_tick (task);
}

static void
manifest_save_tick (GTask *task)
{
  g_autoptr(GbpFlatpakManifest) manifest = NULL;
  GPtrArray *manifests;

  g_assert (G_IS_TASK (task));

  manifests = g_task_get_task_data (task);
  g_assert (manifests != NULL);

  if (manifests->len == 0)
    {
      g_task_return_boolean (task, TRUE);
      return;
    }

  manifest = g_object_ref (g_ptr_array_index (manifests, manifests->len - 1));
  g_assert (GBP_IS_FLATPAK_MANIFEST (manifest));
  g_ptr_array_remove_index (manifests, manifests->len - 1);

  gbp_flatpak_manifest_save_async (manifest,
                                   g_task_get_cancellable (task),
                                   gbp_flatpak_configuration_provider_save_cb,
                                   g_object_ref (task));
}

static void
gbp_flatpak_configuration_provider_save_async (IdeConfigurationProvider *provider,
                                               GCancellable             *cancellable,
                                               GAsyncReadyCallback       callback,
                                               gpointer                  user_data)
{
  GbpFlatpakConfigurationProvider *self = (GbpFlatpakConfigurationProvider *)provider;
  g_autoptr(GTask) task = NULL;
  g_autoptr(GPtrArray) ar = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_CONFIGURATION_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gbp_flatpak_configuration_provider_save_async);
  g_task_set_priority (task, G_PRIORITY_LOW);

  if (self->configs == NULL || self->configs->len == 0)
    {
      g_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  g_assert (self->configs != NULL);
  g_assert (self->configs->len > 0);

  ar = g_ptr_array_new_with_free_func (g_object_unref);

  for (guint i = 0; i < self->configs->len; i++)
    {
      GbpFlatpakManifest *manifest = g_ptr_array_index (self->configs, i);

      if (ide_configuration_get_dirty (IDE_CONFIGURATION (manifest)))
        g_ptr_array_add (ar, g_object_ref (manifest));
    }

  g_task_set_task_data (task, g_steal_pointer (&ar), (GDestroyNotify)g_ptr_array_unref);

  manifest_save_tick (task);

  IDE_EXIT;
}

static gboolean
gbp_flatpak_configuration_provider_save_finish (IdeConfigurationProvider  *provider,
                                                GAsyncResult              *result,
                                                GError                   **error)
{
  g_assert (GBP_IS_FLATPAK_CONFIGURATION_PROVIDER (provider));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
load_manifest_worker (GTask        *task,
                      gpointer      source_object,
                      gpointer      task_data,
                      GCancellable *cancellable)
{
  GbpFlatpakConfigurationProvider *self = source_object;
  g_autoptr(GbpFlatpakManifest) manifest = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *name = NULL;
  IdeContext *context;
  GFile *file = task_data;

  g_assert (G_IS_TASK (task));
  g_assert (GBP_IS_FLATPAK_CONFIGURATION_PROVIDER (self));
  g_assert (G_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  context = ide_object_get_context (IDE_OBJECT (self));
  name = g_file_get_basename (file);
  manifest = gbp_flatpak_manifest_new (context, file, name);

  if (!g_initable_init (G_INITABLE (manifest), cancellable, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_signal_connect_object (manifest,
                           "needs-reload",
                           G_CALLBACK (manifest_needs_reload),
                           self,
                           G_CONNECT_SWAPPED);

  g_task_return_pointer (task, g_steal_pointer (&manifest), g_object_unref);
}

static void
load_manifest_async (GbpFlatpakConfigurationProvider *self,
                     GFile                           *file,
                     GCancellable                    *cancellable,
                     GAsyncReadyCallback              callback,
                     gpointer                         user_data)
{
  g_autoptr(GTask) task = NULL;

  g_assert (GBP_IS_FLATPAK_CONFIGURATION_PROVIDER (self));
  g_assert (G_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, load_manifest_async);
  g_task_set_priority (task, G_PRIORITY_LOW);
  g_task_set_task_data (task, g_object_ref (file), g_object_unref);
  g_task_run_in_thread (task, load_manifest_worker);
}

static GbpFlatpakManifest *
load_manifest_finish (GbpFlatpakConfigurationProvider  *self,
                      GAsyncResult                     *result,
                      GError                          **error)
{
  g_assert (GBP_IS_FLATPAK_CONFIGURATION_PROVIDER (self));
  g_assert (G_IS_TASK (result));
  g_assert (g_task_is_valid (G_TASK (result), self));

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
reload_manifest_cb (GObject      *object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  GbpFlatpakConfigurationProvider *self = (GbpFlatpakConfigurationProvider *)object;
  g_autoptr(GbpFlatpakManifest) old_manifest = user_data;
  g_autoptr(GbpFlatpakManifest) new_manifest = NULL;
  g_autoptr(GError) error = NULL;
  IdeConfigurationManager *manager;
  IdeConfiguration *current;
  IdeContext *context;
  gboolean is_active;

  g_assert (GBP_IS_FLATPAK_CONFIGURATION_PROVIDER (self));
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
  manager = ide_context_get_configuration_manager (context);
  current = ide_configuration_manager_get_current (manager);

  is_active = current == IDE_CONFIGURATION (old_manifest);

  ide_configuration_provider_emit_added (IDE_CONFIGURATION_PROVIDER (self),
                                         IDE_CONFIGURATION (new_manifest));

  if (is_active)
    ide_configuration_manager_set_current (manager,
                                           IDE_CONFIGURATION (new_manifest));

  ide_configuration_provider_emit_removed (IDE_CONFIGURATION_PROVIDER (self),
                                           IDE_CONFIGURATION (old_manifest));
}

static void
manifest_needs_reload (GbpFlatpakConfigurationProvider *self,
                       GbpFlatpakManifest              *manifest)
{
  g_autoptr(GTask) task = NULL;
  GFile *file;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_CONFIGURATION_PROVIDER (self));
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

static void
gbp_flatpak_configuration_provider_load_worker (GTask        *task,
                                                gpointer      source_object,
                                                gpointer      task_data,
                                                GCancellable *cancellable)
{
  GbpFlatpakConfigurationProvider *self = source_object;
  g_autoptr(GPtrArray) manifests = NULL;
  IdeContext *context;
  GPtrArray *files = task_data;

  g_assert (G_IS_TASK (task));
  g_assert (GBP_IS_FLATPAK_CONFIGURATION_PROVIDER (self));
  g_assert (files != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  context = ide_object_get_context (IDE_OBJECT (self));
  manifests = g_ptr_array_new_with_free_func (g_object_unref);

  for (guint i = 0; i < files->len; i++)
    {
      GFile *file = g_ptr_array_index (files, i);
      g_autoptr(GbpFlatpakManifest) manifest = NULL;
      g_autoptr(GError) error = NULL;
      g_autofree gchar *name = NULL;

      g_assert (G_IS_FILE (file));

      name = g_file_get_basename (file);
      manifest = gbp_flatpak_manifest_new (context, file, name);

      if (!g_initable_init (G_INITABLE (manifest), cancellable, &error))
        {
          g_message ("%s is not a flatpak manifest, skipping: %s",
                     name, error->message);
          continue;
        }

      g_signal_connect_object (manifest,
                               "needs-reload",
                               G_CALLBACK (manifest_needs_reload),
                               self,
                               G_CONNECT_SWAPPED);

      g_ptr_array_add (manifests, g_steal_pointer (&manifest));
    }

  g_task_return_pointer (task,
                         g_steal_pointer (&manifests),
                         (GDestroyNotify)g_ptr_array_unref);
}

static void
load_find_files_cb (GObject      *object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  GFile *file = (GFile *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GPtrArray) ret = NULL;

  g_assert (G_IS_FILE (file));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  ret = ide_g_file_find_finish (file, result, &error);
  IDE_PTR_ARRAY_SET_FREE_FUNC (ret, g_object_unref);

  if (ret == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_task_set_task_data (task, g_steal_pointer (&ret), (GDestroyNotify)g_ptr_array_unref);
  g_task_run_in_thread (task, gbp_flatpak_configuration_provider_load_worker);
}

static gboolean
contains_file (GbpFlatpakConfigurationProvider *self,
               GFile                           *file)
{
  g_autofree gchar *path = NULL;

  g_assert (GBP_IS_FLATPAK_CONFIGURATION_PROVIDER (self));
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
gbp_flatpak_configuration_provider_monitor_changed (GbpFlatpakConfigurationProvider *self,
                                                    GFile                           *file,
                                                    GFile                           *other_file,
                                                    GFileMonitorEvent                event,
                                                    IdeVcsMonitor                   *monitor)
{
  g_assert (GBP_IS_FLATPAK_CONFIGURATION_PROVIDER (self));
  g_assert (G_IS_FILE (file));
  g_assert (!other_file || G_IS_FILE (other_file));
  g_assert (IDE_IS_VCS_MONITOR (monitor));

  if (event == G_FILE_MONITOR_EVENT_CREATED)
    {
      g_autofree gchar *name = g_file_get_basename (file);

      if (name != NULL &&
          g_str_has_suffix (name, ".json") &&
          !contains_file (self, file))
        {
          g_autoptr(GbpFlatpakManifest) manifest = NULL;
          g_autoptr(GError) error = NULL;
          IdeContext *context;

          context = ide_object_get_context (IDE_OBJECT (self));
          manifest = gbp_flatpak_manifest_new (context, file, name);

          if (!g_initable_init (G_INITABLE (manifest), NULL, &error))
            {
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

          ide_configuration_provider_emit_added (IDE_CONFIGURATION_PROVIDER (self),
                                                 IDE_CONFIGURATION (manifest));
        }
    }
}

static void
gbp_flatpak_configuration_provider_load_async (IdeConfigurationProvider *provider,
                                               GCancellable             *cancellable,
                                               GAsyncReadyCallback       callback,
                                               gpointer                  user_data)
{
  GbpFlatpakConfigurationProvider *self = (GbpFlatpakConfigurationProvider *)provider;
  g_autoptr(GTask) task = NULL;
  IdeVcsMonitor *monitor;
  IdeContext *context;
  IdeVcs *vcs;
  GFile *workdir;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_FLATPAK_CONFIGURATION_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  context = ide_object_get_context (IDE_OBJECT (self));
  vcs = ide_context_get_vcs (context);
  workdir = ide_vcs_get_working_directory (vcs);
  monitor = ide_context_get_monitor (context);

  task = g_task_new (provider, cancellable, callback, user_data);
  g_task_set_source_tag (task, gbp_flatpak_configuration_provider_load_async);
  g_task_set_priority (task, G_PRIORITY_LOW);

  g_signal_connect_object (monitor,
                           "changed",
                           G_CALLBACK (gbp_flatpak_configuration_provider_monitor_changed),
                           self,
                           G_CONNECT_SWAPPED);

  ide_g_file_find_async (workdir,
                         "*.json",
                         cancellable,
                         load_find_files_cb,
                         g_steal_pointer (&task));

  IDE_EXIT;
}

static IdeConfiguration *
guess_best_config (GPtrArray *ar)
{
  g_assert (ar != NULL);
  g_assert (ar->len > 0);

  for (guint i = 0; i < ar->len; i++)
    {
      GbpFlatpakManifest *config = g_ptr_array_index (ar, i);
      g_autofree gchar *path = gbp_flatpak_manifest_get_path (config);

      if (strstr (path, "-unstable.json") != NULL)
        return IDE_CONFIGURATION (config);
    }

  for (guint i = 0; i < ar->len; i++)
    {
      GbpFlatpakManifest *config = g_ptr_array_index (ar, i);
      g_autofree gchar *path = gbp_flatpak_manifest_get_path (config);
      g_autofree gchar *base = g_path_get_basename (path);
      const gchar *app_id = ide_configuration_get_app_id (IDE_CONFIGURATION (config));
      g_autofree gchar *app_id_json = g_strdup_printf ("%s.json", app_id);

      /* If appid.json is the same as the filename, that is the
       * best match (after unstable) we can have. Use it.
       */
      if (dzl_str_equal0 (app_id_json, base))
        return IDE_CONFIGURATION (config);
    }

  return g_ptr_array_index (ar, 0);
}

static gboolean
gbp_flatpak_configuration_provider_load_finish (IdeConfigurationProvider  *provider,
                                                GAsyncResult              *result,
                                                GError                   **error)
{
  GbpFlatpakConfigurationProvider *self = (GbpFlatpakConfigurationProvider *)provider;
  g_autoptr(GPtrArray) configs = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_FLATPAK_CONFIGURATION_PROVIDER (self));
  g_assert (G_IS_TASK (result));
  g_assert (g_task_is_valid (G_TASK (result), provider));

  configs = g_task_propagate_pointer (G_TASK (result), error);

  if (configs == NULL)
    return FALSE;

  g_clear_pointer (&self->configs, g_ptr_array_unref);
  self->configs = g_ptr_array_ref (configs);

  for (guint i = 0; i < configs->len; i++)
    {
      IdeConfiguration *config = g_ptr_array_index (configs, i);

      g_assert (IDE_IS_CONFIGURATION (config));

      ide_configuration_provider_emit_added (provider, config);
    }

  if (configs->len > 0)
    {
      IdeConfiguration *config = guess_best_config (configs);
      IdeContext *context = ide_object_get_context (IDE_OBJECT (self));
      IdeConfigurationManager *manager = ide_context_get_configuration_manager (context);

      g_assert (IDE_IS_CONFIGURATION (config));

      /* TODO: We should have a GSetting for this, in config-manager */
      ide_configuration_manager_set_current (manager, config);
    }

  return TRUE;
}

static void
gbp_flatpak_configuration_provider_unload (IdeConfigurationProvider *provider)
{
  GbpFlatpakConfigurationProvider *self = (GbpFlatpakConfigurationProvider *)provider;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_CONFIGURATION_PROVIDER (self));

  if (self->configs != NULL)
    {
      for (guint i = self->configs->len; i > 0; i--)
        {
          g_autoptr(IdeConfiguration) config = NULL;

          config = g_object_ref (g_ptr_array_index (self->configs, i - 1));
          g_signal_handlers_disconnect_by_func (config,
                                                G_CALLBACK (manifest_needs_reload),
                                                self);
          g_ptr_array_remove_index (self->configs, i - 1);

          ide_configuration_provider_emit_removed (provider, config);
        }

      g_clear_pointer (&self->configs, g_ptr_array_unref);
    }

  IDE_EXIT;
}

static void
gbp_flatpak_configuration_provider_duplicate (IdeConfigurationProvider *provider,
                                              IdeConfiguration         *configuration)
{
  GbpFlatpakManifest *manifest = (GbpFlatpakManifest *)configuration;
  g_autofree gchar *path = NULL;
  g_autofree gchar *base = NULL;
  g_autoptr(GFile) parent = NULL;
  gchar *dot;
  GFile *file;

  g_assert (GBP_IS_FLATPAK_CONFIGURATION_PROVIDER (provider));
  g_assert (GBP_IS_FLATPAK_MANIFEST (manifest));

  file = gbp_flatpak_manifest_get_file (manifest);
  path = g_file_get_path (file);
  base = g_file_get_basename (file);
  parent = g_file_get_parent (file);

  if ((dot = strrchr (base, '.')))
    *dot = '\0';

  for (guint i = 2; i <= 10; i++)
    {
      g_autofree gchar *name = g_strdup_printf ("%s-%u.json", base, i);
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
gbp_flatpak_configuration_provider_delete (IdeConfigurationProvider *provider,
                                           IdeConfiguration         *configuration)
{
  GbpFlatpakConfigurationProvider *self = (GbpFlatpakConfigurationProvider *)provider;
  GbpFlatpakManifest *manifest = (GbpFlatpakManifest *)configuration;
  g_autoptr(IdeConfiguration) hold = NULL;
  g_autoptr(GFile) file = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *name = NULL;

  g_assert (GBP_IS_FLATPAK_CONFIGURATION_PROVIDER (self));
  g_assert (GBP_IS_FLATPAK_MANIFEST (manifest));

  hold = g_object_ref (configuration);
  file = g_object_ref (gbp_flatpak_manifest_get_file (manifest));
  name = g_file_get_basename (file);

  if (g_ptr_array_remove (self->configs, hold))
    {
      ide_configuration_provider_emit_removed (provider, hold);
      if (!g_file_delete (file, NULL, &error))
        ide_object_warning (provider, _("Failed to remove flatpak manifest: %s"), name);
    }
}

static void
configuration_provider_iface_init (IdeConfigurationProviderInterface *iface)
{
  iface->load_async = gbp_flatpak_configuration_provider_load_async;
  iface->load_finish = gbp_flatpak_configuration_provider_load_finish;
  iface->unload = gbp_flatpak_configuration_provider_unload;
  iface->save_async = gbp_flatpak_configuration_provider_save_async;
  iface->save_finish = gbp_flatpak_configuration_provider_save_finish;
  iface->duplicate = gbp_flatpak_configuration_provider_duplicate;
  iface->delete = gbp_flatpak_configuration_provider_delete;
}

G_DEFINE_TYPE_WITH_CODE (GbpFlatpakConfigurationProvider,
                         gbp_flatpak_configuration_provider,
                         IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_CONFIGURATION_PROVIDER,
                                                configuration_provider_iface_init))

static void
gbp_flatpak_configuration_provider_finalize (GObject *object)
{
  GbpFlatpakConfigurationProvider *self = (GbpFlatpakConfigurationProvider *)object;

  g_clear_pointer (&self->configs, g_ptr_array_unref);

  G_OBJECT_CLASS (gbp_flatpak_configuration_provider_parent_class)->finalize (object);
}

static void
gbp_flatpak_configuration_provider_class_init (GbpFlatpakConfigurationProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_flatpak_configuration_provider_finalize;
}

static void
gbp_flatpak_configuration_provider_init (GbpFlatpakConfigurationProvider *self)
{
  self->configs = g_ptr_array_new_with_free_func (g_object_unref);
}
