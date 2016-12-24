/* gbp-flatpak-configuration-provider.c
 *
 * Copyright (C) 2016 Matthew Leeds <mleeds@redhat.com>
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

#include <string.h>
#include <flatpak.h>
#include <json-glib/json-glib.h>

#include "util/ide-posix.h"

#include "buildsystem/ide-environment.h"
#include "buildsystem/ide-configuration.h"
#include "buildsystem/ide-configuration-provider.h"
#include "gbp-flatpak-configuration-provider.h"
#include "gbp-flatpak-configuration.h"

struct _GbpFlatpakConfigurationProvider
{
  GObject                  parent_instance;
  IdeConfigurationManager *manager;
  GCancellable            *cancellable;
  GPtrArray               *configurations;
};

typedef struct
{
  gchar          *app_id;
  gchar          *config_opts;
  gchar          *prefix;
  gchar          *primary_module;
  gchar          *runtime_id;
  GFile          *file;
  IdeEnvironment *environment;
} FlatpakManifest;

static void configuration_provider_iface_init (IdeConfigurationProviderInterface *);

G_DEFINE_TYPE_EXTENDED (GbpFlatpakConfigurationProvider, gbp_flatpak_configuration_provider, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_CONFIGURATION_PROVIDER,
                                               configuration_provider_iface_init))

static void gbp_flatpak_configuration_provider_load (IdeConfigurationProvider *provider, IdeConfigurationManager *manager);
static void gbp_flatpak_configuration_provider_unload (IdeConfigurationProvider *provider, IdeConfigurationManager *manager);

static gboolean
contains_id (GPtrArray   *ar,
             const gchar *id)
{
  g_assert (ar != NULL);
  g_assert (id != NULL);

  for (guint i = 0; i < ar->len; i++)
    {
      IdeConfiguration *configuration = g_ptr_array_index (ar, i);

      g_assert (IDE_IS_CONFIGURATION (configuration));

      if (ide_str_equal0 (id, ide_configuration_get_id (configuration)))
        return TRUE;
    }

  return FALSE;
}

static void
flatpak_manifest_free (void *data)
{
  FlatpakManifest *manifest = data;

  g_free (manifest->app_id);
  g_free (manifest->config_opts);
  g_free (manifest->prefix);
  g_free (manifest->primary_module);
  g_free (manifest->runtime_id);
  g_clear_object (&manifest->environment);
  g_clear_object (&manifest->file);
  g_slice_free (FlatpakManifest, manifest);
}

JsonNode *
guess_primary_module (JsonNode *modules_node,
                      GFile    *directory)
{
  JsonArray *modules;
  JsonNode *module;
  g_autofree gchar *dir_name;

  g_assert (G_IS_FILE (directory));

  dir_name = g_file_get_basename (directory);
  g_assert (!ide_str_empty0 (dir_name));
  g_return_val_if_fail (JSON_NODE_HOLDS_ARRAY (modules_node), NULL);

  /* TODO: Support module strings that refer to other files? */
  modules = json_node_get_array (modules_node);
  if (json_array_get_length (modules) == 1)
    {
      module = json_array_get_element (modules, 0);
      if (JSON_NODE_HOLDS_OBJECT (module))
        return module;
    }
  else
    {
      for (guint i = 0; i < json_array_get_length (modules); i++)
        {
          module = json_array_get_element (modules, i);
          if (JSON_NODE_HOLDS_OBJECT (module))
            {
              const gchar *module_name;
              module_name = json_object_get_string_member (json_node_get_object (module), "name");
              if (g_strcmp0 (module_name, dir_name) == 0)
                return module;
              if (json_object_has_member (json_node_get_object (module), "modules"))
                {
                  JsonNode *nested_modules_node;
                  JsonNode *nested_primary_module;
                  nested_modules_node = json_object_get_member (json_node_get_object (module), "modules");
                  nested_primary_module = guess_primary_module (nested_modules_node, directory);
                  if (nested_primary_module != NULL)
                    return nested_primary_module;
                }
            }
        }
    }

  return NULL;
}

static gboolean
check_dir_for_manifests (GFile         *directory,
                         GPtrArray     *manifests,
                         GCancellable  *cancellable,
                         GError       **error)
{
  g_autoptr(GFileEnumerator) enumerator = NULL;
  GFileInfo *file_info = NULL;

  enumerator = g_file_enumerate_children (directory,
                                          G_FILE_ATTRIBUTE_STANDARD_NAME,
                                          G_FILE_QUERY_INFO_NONE,
                                          cancellable,
                                          error);
  if (!enumerator)
    return FALSE;

  while ((file_info = g_file_enumerator_next_file (enumerator, cancellable, NULL)))
    {
      GFileType file_type;
      g_autofree gchar *name = NULL;
      g_autofree gchar *path = NULL;
      const gchar *platform;
      const gchar *branch;
      const gchar *arch;
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
      JsonNode *primary_module_node = NULL;
      JsonObject *root_object = NULL;
      g_autoptr(GError) local_error = NULL;
      g_autoptr(GFile) file = NULL;
      FlatpakManifest *manifest;

      file_type = g_file_info_get_file_type (file_info);
      name = g_strdup (g_file_info_get_name (file_info));
      g_clear_object (&file_info);

      if (name == NULL)
        continue;

      file = g_file_get_child (directory, name);

      if (file_type == G_FILE_TYPE_DIRECTORY)
        {
          if (!check_dir_for_manifests (file, manifests, cancellable, error))
            return FALSE;
          continue;
        }

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

      /**
       * TODO: Currently we just support the build-options object that's global to the
       * manifest, but modules can have their own build-options as well that override
       * global ones, so we should consider supporting that. The main difficulty would
       * be keeping track of each so they can be written back to the file properly when
       * the user makes changes in the Builder interface.
       */
      if (json_object_has_member (root_object, "build-options") &&
          JSON_NODE_HOLDS_OBJECT (json_object_get_member (root_object, "build-options")))
        {
          JsonObject *build_options = NULL;
          IdeEnvironment *environment;

          build_options = json_object_get_object_member (root_object, "build-options");

          if (json_object_has_member (build_options, "prefix"))
            {
              const gchar *prefix;
              prefix = json_object_get_string_member (build_options, "prefix");
              if (prefix != NULL)
                manifest->prefix = g_strdup (prefix);
            }

          environment = ide_environment_new ();
          if (json_object_has_member (build_options, "cflags"))
            {
              const gchar *cflags;
              cflags = json_object_get_string_member (build_options, "cflags");
              if (cflags != NULL)
                ide_environment_setenv (environment, "CFLAGS", cflags);
            }
          if (json_object_has_member (build_options, "cxxflags"))
            {
              const gchar *cxxflags;
              cxxflags = json_object_get_string_member (build_options, "cxxflags");
              if (cxxflags != NULL)
                ide_environment_setenv (environment, "CXXFLAGS", cxxflags);
            }
          if (json_object_has_member (build_options, "env"))
            {
              JsonObject *env_vars;
              env_vars = json_object_get_object_member (build_options, "env");
              if (env_vars != NULL)
                {
                  g_autoptr(GList) env_list = NULL;
                  GList *l;
                  env_list = json_object_get_members (env_vars);
                  for (l = env_list; l != NULL; l = l->next)
                    {
                      const gchar *env_name = (gchar *)l->data;
                      const gchar *env_value = json_object_get_string_member (env_vars, env_name);
                      if (!ide_str_empty0 (env_name) && !ide_str_empty0 (env_value))
                        ide_environment_setenv (environment, env_name, env_value);
                    }
                }
            }
          manifest->environment = environment;
        }

      platform = json_node_get_string (runtime_node);
      if (!JSON_NODE_HOLDS_VALUE (runtime_version_node) || ide_str_empty0 (json_node_get_string (runtime_version_node)))
        branch = "master";
      else
        branch = json_node_get_string (runtime_version_node);
      arch = flatpak_get_default_arch ();
      manifest->runtime_id = g_strdup_printf ("flatpak:%s/%s/%s", platform, branch, arch);

      if (JSON_NODE_HOLDS_VALUE (app_id_node))
        manifest->app_id = json_node_dup_string (app_id_node);
      else
        manifest->app_id = json_node_dup_string (id_node);

      primary_module_node = guess_primary_module (modules_node, directory);
      if (primary_module_node != NULL && JSON_NODE_HOLDS_OBJECT (primary_module_node))
        {
          JsonObject *primary_module_object = json_node_get_object (primary_module_node);
          manifest->primary_module = g_strdup (json_object_get_string_member (primary_module_object, "name"));
          if (json_object_has_member (primary_module_object, "config-opts"))
            {
              JsonArray *config_opts_array;
              config_opts_array = json_object_get_array_member (primary_module_object, "config-opts");
              if (config_opts_array != NULL)
                {
                  GPtrArray *config_opts_strv;
                  config_opts_strv = g_ptr_array_new_with_free_func (g_free);
                  for (guint i = 0; i < json_array_get_length (config_opts_array); i++)
                    {
                      const gchar *next_option;
                      next_option = json_array_get_string_element (config_opts_array, i);
                      g_ptr_array_add (config_opts_strv, g_strdup (next_option));
                    }
                  g_ptr_array_add (config_opts_strv, NULL);
                  manifest->config_opts = g_strjoinv (" ", (gchar **)config_opts_strv->pdata);
                  g_ptr_array_free (config_opts_strv, TRUE);
                }
            }
        }

      g_ptr_array_add (manifests, manifest);
    }

  return TRUE;
}

static GPtrArray *
gbp_flatpak_configuration_provider_find_flatpak_manifests (GbpFlatpakConfigurationProvider *self,
                                                           GCancellable                    *cancellable,
                                                           GFile                           *directory,
                                                           GError                         **error)
{
  GPtrArray *ar;

  g_assert (GBP_IS_FLATPAK_CONFIGURATION_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (G_IS_FILE (directory));

  ar = g_ptr_array_new ();
  g_ptr_array_set_free_func (ar, flatpak_manifest_free);

  if (!check_dir_for_manifests (directory, ar, cancellable, error))
    {
      g_ptr_array_free (ar, TRUE);
      return NULL;
    }

  return ar;
}

static gboolean
gbp_flatpak_configuration_provider_load_manifests (GbpFlatpakConfigurationProvider  *self,
                                                   GPtrArray                        *configurations,
                                                   GCancellable                     *cancellable,
                                                   GError                          **error)
{
  g_autoptr(GPtrArray) ar = NULL;
  g_autoptr(GFileInfo) file_info = NULL;
  IdeContext *context;
  GFile *project_file;
  g_autoptr(GFile) project_dir = NULL;

  g_assert (GBP_IS_FLATPAK_CONFIGURATION_PROVIDER (self));

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

  ar = gbp_flatpak_configuration_provider_find_flatpak_manifests (self, cancellable, project_dir, error);

  if (ar == NULL)
    return FALSE;

  IDE_TRACE_MSG ("Found %u flatpak manifests", ar->len);

  for (guint i = 0; i < ar->len; i++)
    {
      GbpFlatpakConfiguration *configuration;
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

      if (contains_id (configurations, id))
        continue;

      /**
       * TODO: There are a few more fields in the manifests that Builder needs, but they
       * are read when needed by the runtime. If we set up a file monitor to reload the
       * configuration when it changes on disk, it might make more sense for those fields
       * to be read and processed here so we're only parsing the manifest in one place.
       */
      configuration = g_object_new (GBP_TYPE_FLATPAK_CONFIGURATION,
                                    "app-id", manifest->app_id,
                                    "context", context,
                                    "display-name", filename,
                                    "device-id", "local",
                                    "id", id,
                                    "manifest", manifest->file,
                                    "prefix", (manifest->prefix != NULL ? manifest->prefix : "/app"),
                                    "runtime-id", manifest->runtime_id,
                                    NULL);
      if (manifest->primary_module != NULL)
        gbp_flatpak_configuration_set_primary_module (configuration, manifest->primary_module);
      if (manifest->environment != NULL)
        ide_configuration_set_environment (IDE_CONFIGURATION (configuration), manifest->environment);
      if (manifest->config_opts != NULL)
        ide_configuration_set_config_opts (IDE_CONFIGURATION (configuration), manifest->config_opts);

      g_ptr_array_add (configurations, configuration);
    }

  return TRUE;
}

static void
gbp_flatpak_configuration_provider_load_worker (GTask        *task,
                                                gpointer      source_object,
                                                gpointer      task_data,
                                                GCancellable *cancellable)
{
  GbpFlatpakConfigurationProvider *self = source_object;
  g_autoptr(GPtrArray) ret = NULL;
  g_autoptr(GFile) file = NULL;
  g_autofree gchar *path = NULL;
  GError *error = NULL;

  IDE_ENTRY;

  g_assert (G_IS_TASK (task));
  g_assert (GBP_IS_FLATPAK_CONFIGURATION_PROVIDER (self));
  g_assert (IDE_IS_CONFIGURATION_MANAGER (self->manager));

  ret = g_ptr_array_new_with_free_func (g_object_unref);

  /* Load flatpak manifests in the repo */
  if (!gbp_flatpak_configuration_provider_load_manifests (self, ret, cancellable, &error))
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
    }

  g_task_return_pointer (task, g_steal_pointer (&ret), (GDestroyNotify)g_ptr_array_unref);

  IDE_EXIT;
}

static void
gbp_flatpak_configuration_provider_load_cb (GObject      *object,
                                            GAsyncResult *result,
                                            gpointer      user_data)
{
  GbpFlatpakConfigurationProvider *self = (GbpFlatpakConfigurationProvider *)object;
  GPtrArray *ret;
  GError *error = NULL;
  guint i;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_CONFIGURATION_PROVIDER (self));
  g_assert (G_IS_TASK (result));

  if (!(ret = g_task_propagate_pointer (G_TASK (result), &error)))
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
      IDE_EXIT;
    }

  for (i = 0; i < ret->len; i++)
    {
      IdeConfiguration *configuration = g_ptr_array_index (ret, i);

      ide_configuration_manager_add (self->manager, configuration);
    }

  self->configurations = ret;

  IDE_EXIT;
}

static void
gbp_flatpak_configuration_provider_load (IdeConfigurationProvider *provider,
                                         IdeConfigurationManager  *manager)
{
  GbpFlatpakConfigurationProvider *self = (GbpFlatpakConfigurationProvider *)provider;
  g_autoptr(GTask) task = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_CONFIGURATION_PROVIDER (self));
  g_assert (IDE_IS_CONFIGURATION_MANAGER (manager));

  ide_set_weak_pointer (&self->manager, manager);

  self->cancellable = g_cancellable_new ();

  task = g_task_new (self, self->cancellable, gbp_flatpak_configuration_provider_load_cb, NULL);
  g_task_run_in_thread (task, gbp_flatpak_configuration_provider_load_worker);

  IDE_EXIT;
}

static void
gbp_flatpak_configuration_provider_unload (IdeConfigurationProvider *provider,
                                           IdeConfigurationManager  *manager)
{
  GbpFlatpakConfigurationProvider *self = (GbpFlatpakConfigurationProvider *)provider;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_CONFIGURATION_PROVIDER (self));
  g_assert (IDE_IS_CONFIGURATION_MANAGER (manager));

  if (self->configurations != NULL)
    {
      for (guint i= 0; i < self->configurations->len; i++)
        {
          IdeConfiguration *configuration = g_ptr_array_index (self->configurations, i);

          ide_configuration_manager_remove (manager, configuration);
        }
    }

  g_clear_pointer (&self->configurations, g_ptr_array_unref);

  if (self->cancellable != NULL)
    g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  ide_clear_weak_pointer (&self->manager);

  IDE_EXIT;
}

static void
gbp_flatpak_configuration_provider_class_init (GbpFlatpakConfigurationProviderClass *klass)
{
}

static void
gbp_flatpak_configuration_provider_init (GbpFlatpakConfigurationProvider *self)
{
}

static void
configuration_provider_iface_init (IdeConfigurationProviderInterface *iface)
{
  iface->load = gbp_flatpak_configuration_provider_load;
  iface->unload = gbp_flatpak_configuration_provider_unload;
}
