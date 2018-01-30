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



static GRegex *filename_regex;

static void
gbp_flatpak_configuration_provider_save_worker (GTask        *task,
                                                gpointer      source_object,
                                                gpointer      task_data,
                                                GCancellable *cancellable)
{
  GbpFlatpakConfigurationProvider *self = source_object;

  IDE_ENTRY;

  g_assert (G_IS_TASK (task));
  g_assert (GBP_IS_FLATPAK_CONFIGURATION_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  g_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
gbp_flatpak_configuration_provider_save_async (IdeConfigurationProvider *provider,
                                               GCancellable             *cancellable,
                                               GAsyncReadyCallback       callback,
                                               gpointer                  user_data)
{
  GbpFlatpakConfigurationProvider *self = (GbpFlatpakConfigurationProvider *)provider;
  g_autoptr(GTask) task = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_CONFIGURATION_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gbp_flatpak_configuration_provider_save_async);
  g_task_set_priority (task, G_PRIORITY_LOW);
  g_task_run_in_thread (task, gbp_flatpak_configuration_provider_save_worker);

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

  if (ret == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_task_set_task_data (task, g_steal_pointer (&ret), (GDestroyNotify)g_ptr_array_unref);
  g_task_run_in_thread (task, gbp_flatpak_configuration_provider_load_worker);
}

static void
gbp_flatpak_configuration_provider_load_async (IdeConfigurationProvider *provider,
                                               GCancellable             *cancellable,
                                               GAsyncReadyCallback       callback,
                                               gpointer                  user_data)
{
  GbpFlatpakConfigurationProvider *self = (GbpFlatpakConfigurationProvider *)provider;
  g_autoptr(GTask) task = NULL;
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

  task = g_task_new (provider, cancellable, callback, user_data);
  g_task_set_source_tag (task, gbp_flatpak_configuration_provider_load_async);
  g_task_set_priority (task, G_PRIORITY_LOW);

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
          g_ptr_array_remove_index (self->configs, i);

          ide_configuration_provider_emit_removed (provider, config);
        }
    }

  g_clear_pointer (&self->configs, g_ptr_array_unref);

  IDE_EXIT;
}


static void
configuration_provider_iface_init (IdeConfigurationProviderInterface *iface)
{
  iface->load_async = gbp_flatpak_configuration_provider_load_async;
  iface->load_finish = gbp_flatpak_configuration_provider_load_finish;
  iface->unload = gbp_flatpak_configuration_provider_unload;
  iface->save_async = gbp_flatpak_configuration_provider_save_async;
  iface->save_finish = gbp_flatpak_configuration_provider_save_finish;
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

  /* This regex is based on https://wiki.gnome.org/HowDoI/ChooseApplicationID */
  filename_regex = g_regex_new ("^[[:alnum:]-_]+\\.[[:alnum:]-_]+(\\.[[:alnum:]-_]+)*\\.json$",
                                G_REGEX_OPTIMIZE, 0, NULL);
}

static void
gbp_flatpak_configuration_provider_init (GbpFlatpakConfigurationProvider *self)
{
  self->configs = g_ptr_array_new_with_free_func (g_object_unref);
}
