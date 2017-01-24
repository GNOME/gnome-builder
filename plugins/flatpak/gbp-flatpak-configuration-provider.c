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

#define WRITEBACK_TIMEOUT_SECS 2

struct _GbpFlatpakConfigurationProvider
{
  GObject                  parent_instance;
  IdeConfigurationManager *manager;
  GCancellable            *cancellable;
  GPtrArray               *configurations;

  gulong                   writeback_handler;
  guint                    change_count;
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

static void
gbp_flatpak_configuration_provider_save_worker (GTask        *task,
                                                gpointer      source_object,
                                                gpointer      task_data,
                                                GCancellable *cancellable)
{
  GbpFlatpakConfigurationProvider *self = source_object;
  GError *error = NULL;

  IDE_ENTRY;

  g_assert (G_IS_TASK (task));
  g_assert (GBP_IS_FLATPAK_CONFIGURATION_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  self->change_count = 0;

  if (self->configurations == NULL)
    IDE_EXIT;

  for (guint i = 0; i < self->configurations->len; i++)
    {
      g_autoptr(GFileInputStream) file_stream = NULL;
      g_autoptr(GDataInputStream) data_stream = NULL;
      g_autoptr(GRegex) runtime_regex = NULL;
      g_autoptr(GRegex) build_options_regex = NULL;
      g_autoptr(GRegex) config_opts_regex = NULL;
      g_autoptr(GRegex) primary_module_regex = NULL;
      g_autoptr(GPtrArray) new_lines = NULL;
      g_autoptr(GBytes) bytes = NULL;
      g_auto(GStrv) new_config_opts = NULL;
      g_auto(GStrv) new_runtime_parts = NULL;
      g_auto(GStrv) new_environ = NULL;
      g_autofree gchar *primary_module_regex_str = NULL;
      g_autofree gchar *primary_module_right_curly_brace = NULL;
      g_autofree gchar *right_curly_brace_line = NULL;
      g_autofree gchar *primary_module_indent = NULL;
      g_autofree gchar *build_options_indent = NULL;
      g_autofree gchar *config_opt_indent = NULL;
      g_autofree gchar *array_prefix = NULL;
      g_autofree gchar *new_config_opts_string = NULL;
      const gchar *primary_module;
      const gchar *new_runtime_id;
      const gchar *config_prefix;
      const gchar *new_prefix;
      gchar *json_string;
      gchar *new_runtime_name;
      GFile *manifest;
      gboolean in_config_opts_array;
      gboolean in_primary_module;
      gboolean in_build_options;
      gboolean config_opts_replaced;
      gboolean build_options_replaced;
      guint opts_per_line;
      guint nested_curly_braces;

      GbpFlatpakConfiguration *configuration = (GbpFlatpakConfiguration *)g_ptr_array_index (self->configurations, i);

      manifest = gbp_flatpak_configuration_get_manifest (configuration);
      if (manifest == NULL)
        continue;

      primary_module = gbp_flatpak_configuration_get_primary_module (configuration);
      if (primary_module == NULL)
        {
          g_warning ("Flatpak manifest configuration has no primary module set");
          continue;
        }

      file_stream = g_file_read (manifest, NULL, &error);
      if (file_stream == NULL)
        {
          g_task_return_error (task, error);
          IDE_EXIT;
        }

      data_stream = g_data_input_stream_new (G_INPUT_STREAM (file_stream));

      runtime_regex = g_regex_new ("^\\s*\"runtime\"\\s*:\\s*\"(?<id>.+)\",$", 0, 0, NULL);
      build_options_regex = g_regex_new ("^\\s*\"build-options\"\\s*:\\s*{$", 0, 0, NULL);
      config_opts_regex = g_regex_new ("^(\\s*\"config-opts\"\\s*:\\s*\\[\\s*).+$", 0, 0, NULL);
      primary_module_regex_str = g_strdup_printf ("^(\\s*)\"name\"\\s*:\\s*\"%s\",$", primary_module);
      primary_module_regex = g_regex_new (primary_module_regex_str, 0, 0, NULL);

      new_runtime_id = ide_configuration_get_runtime_id (IDE_CONFIGURATION (configuration));
      if (g_str_has_prefix (new_runtime_id, "flatpak:"))
        {
          new_runtime_parts = g_strsplit (new_runtime_id + 8, "/", 3);
          if (new_runtime_parts[0] != NULL)
            new_runtime_name = new_runtime_parts[0];
        }

      new_config_opts_string = g_strdup (ide_configuration_get_config_opts (IDE_CONFIGURATION (configuration)));
      if (!ide_str_empty0 (new_config_opts_string))
        {
          new_config_opts = g_strsplit (g_strstrip (new_config_opts_string), " ", 0);
        }

      new_environ = ide_configuration_get_environ (IDE_CONFIGURATION (configuration));

      config_prefix = ide_configuration_get_prefix (IDE_CONFIGURATION (configuration));
      new_prefix = (g_strcmp0 (config_prefix, "/app") != 0) ? config_prefix : "";

      /**
       * XXX: The following code, which parses parts of the manifest file and edits
       * it to match the options chosen by the user in Builder's interface, assumes
       * that the JSON is "pretty" (meaning it has lots of whitespace and newlines),
       * which is not technically a requirement for JSON but a de facto standard used
       * by developers.
       */
      new_lines = g_ptr_array_new_with_free_func (g_free);
      in_config_opts_array = FALSE;
      in_primary_module = FALSE;
      in_build_options = FALSE;
      config_opts_replaced = FALSE;
      build_options_replaced = FALSE;
      nested_curly_braces = 0;
      for (;;)
        {
          gchar *line;

          line = g_data_input_stream_read_line_utf8 (data_stream, NULL, NULL, &error);
          if (error != NULL)
            {
              g_task_return_error (task, error);
              IDE_EXIT;
            }
          if (line == NULL)
            break;

          /* Check if we've reached the primary module's section */
          if (!in_primary_module)
            {
              g_autoptr(GMatchInfo) match_info = NULL;
              g_regex_match (primary_module_regex, line, 0, &match_info);
              if (g_match_info_matches (match_info))
                {
                  gchar *previous_line;
                  g_auto(GStrv) previous_line_parts = NULL;

                  in_primary_module = TRUE;
                  primary_module_indent = g_match_info_fetch (match_info, 1);

                  /* Replace '}' with '{' in the last line to get the right indentation */
                  previous_line = (gchar *)g_ptr_array_index (new_lines, new_lines->len - 1);
                  previous_line_parts = g_strsplit (previous_line, "{", 0);
                  primary_module_right_curly_brace = g_strjoinv ("}", previous_line_parts);
                }
            }

          /* Replace the runtime with the user-chosen one */
          if (!ide_str_empty0 (new_runtime_name))
            {
              g_autoptr(GMatchInfo) match_info = NULL;
              g_regex_match (runtime_regex, line, 0, &match_info);
              if (g_match_info_matches (match_info))
                {
                  gchar *old_runtime_ptr;
                  gchar *new_line;
                  g_autofree gchar *id = NULL;
                  id = g_match_info_fetch_named (match_info, "id");
                  old_runtime_ptr = g_strstr_len (line, -1, id);
                  *old_runtime_ptr = '\0';
                  new_line = g_strdup_printf ("%s%s\",", line, new_runtime_name);
                  g_free (line);
                  line = new_line;
                }
            }

          /* Update the build-options object */
          if (!in_build_options && !build_options_replaced)
            {
              g_autoptr(GMatchInfo) match_info = NULL;
              g_regex_match (build_options_regex, line, 0, &match_info);
              if (g_match_info_matches (match_info))
                {
                  in_build_options = TRUE;
                }
            }
          else if (in_build_options)
            {
              if (g_strstr_len (line, -1, "{") != NULL)
                nested_curly_braces++;
              if (g_strstr_len (line, -1, "}") == NULL)
                {
                  if (build_options_indent == NULL)
                    {
                      g_autoptr(GRegex) build_options_internal_regex = NULL;
                      g_autoptr(GMatchInfo) match_info = NULL;
                      build_options_internal_regex = g_regex_new ("^(\\s*)\".+\"\\s*:.*$", 0, 0, NULL);
                      g_regex_match (build_options_internal_regex, line, 0, &match_info);
                      if (g_match_info_matches (match_info))
                        {
                          build_options_indent = g_match_info_fetch (match_info, 1);
                        }
                    }
                  /* Discard the line because it will be replaced with new info */
                  g_free (line);
                  continue;
                }
              else
                {
                  if (nested_curly_braces > 0)
                    {
                      nested_curly_braces--;
                      g_free (line);
                      continue;
                    }
                  else
                    {
                      /* We're at the closing curly brace for build-options */
                      guint num_env;
                      num_env = g_strv_length (new_environ);
                      if (num_env > 0 || !ide_str_empty0 (new_prefix))
                        {
                          g_autofree gchar *cflags_line = NULL;
                          g_autofree gchar *cxxflags_line = NULL;
                          g_autoptr(GPtrArray) env_lines = NULL;
                          if (build_options_indent == NULL)
                            build_options_indent = g_strdup ("        ");
                          for (guint j = 0; new_environ[j]; j++)
                            {
                              g_auto(GStrv) line_parts = NULL;
                              line_parts = g_strsplit (new_environ[j], "=", 2);
                              if (g_strcmp0 (line_parts[0], "CFLAGS") == 0)
                                cflags_line = g_strdup_printf ("%s\"cflags\": \"%s\"",
                                                               build_options_indent,
                                                               line_parts[1]);
                              else if (g_strcmp0 (line_parts[0], "CXXFLAGS") == 0)
                                cxxflags_line = g_strdup_printf ("%s\"cxxflags\": \"%s\"",
                                                                 build_options_indent,
                                                                 line_parts[1]);
                              else
                                {
                                  if (env_lines == NULL)
                                    {
                                      env_lines = g_ptr_array_new_with_free_func (g_free);
                                      g_ptr_array_add (env_lines, g_strdup_printf ("%s\"env\": {", build_options_indent));
                                    }
                                  g_ptr_array_add (env_lines, g_strdup_printf ("%s    \"%s\": \"%s\"",
                                                                               build_options_indent,
                                                                               line_parts[0],
                                                                               line_parts[1]));
                                }
                            }
                          if (cflags_line != NULL)
                            {
                              gchar *line_ending;
                              line_ending = (!ide_str_empty0 (new_prefix) || cxxflags_line != NULL || env_lines != NULL) ? "," : "";
                              g_ptr_array_add (new_lines, g_strdup_printf ("%s%s", cflags_line, line_ending));
                            }
                          if (cxxflags_line != NULL)
                            {
                              gchar *line_ending;
                              line_ending = (!ide_str_empty0 (new_prefix) || env_lines != NULL) ? "," : "";
                              g_ptr_array_add (new_lines, g_strdup_printf ("%s%s", cxxflags_line, line_ending));
                            }
                          if (!ide_str_empty0 (new_prefix))
                            {
                              gchar *line_ending;
                              line_ending = (env_lines != NULL) ? "," : "";
                              g_ptr_array_add (new_lines, g_strdup_printf ("%s\"prefix\": \"%s\"%s",
                                                                           build_options_indent,
                                                                           new_prefix,
                                                                           line_ending));
                            }
                          if (env_lines != NULL)
                            {
                              g_ptr_array_add (env_lines, g_strdup_printf ("%s}", build_options_indent));
                              for (guint j = 0; j < env_lines->len; j++)
                                {
                                  gchar *env_line;
                                  gchar *line_ending;
                                  line_ending = (j > 0 && j < env_lines->len - 2) ? "," : "";
                                  env_line = (gchar *)g_ptr_array_index (env_lines, j);
                                  g_ptr_array_add (new_lines, g_strdup_printf ("%s%s", env_line, line_ending));
                                }
                            }
                        }
                       in_build_options = FALSE;
                       build_options_replaced = TRUE;
                    }
                }
            }

          if (in_primary_module)
            {
              g_autoptr(GMatchInfo) match_info = NULL;

              /* Check if we're at the end of the module and haven't seen a config-opts property */
              if (g_str_has_prefix (line, primary_module_right_curly_brace))
                {
                  in_primary_module = FALSE;
                  if (!config_opts_replaced && new_config_opts != NULL)
                    {
                      gchar *previous_line;

                      previous_line = (gchar *)g_ptr_array_remove_index (new_lines, new_lines->len - 1);
                      g_ptr_array_add (new_lines, g_strdup_printf ("%s,", previous_line));
                      right_curly_brace_line = line;
                      line = g_strdup_printf ("%s\"config-opts\": []", primary_module_indent);
                    }
                }

              /* Update the list of configure options, or omit it entirely */
              g_regex_match (config_opts_regex, line, 0, &match_info);
              if (g_match_info_matches (match_info) || in_config_opts_array)
                {
                  gchar *right_bracket;

                  right_bracket = g_strstr_len (line, -1, "]");
                  if (g_match_info_matches (match_info))
                    {
                      array_prefix = g_match_info_fetch (match_info, 1);
                      if (right_bracket != NULL)
                        {
                          /*
                           * Ensure that all options will be on one line,
                           * even if there are more than before
                           */
                          if (new_config_opts == NULL)
                            opts_per_line = 1;
                          else
                            opts_per_line = g_strv_length (new_config_opts);
                        }
                      else
                        {
                          in_config_opts_array = TRUE;
                          if (new_config_opts == NULL)
                            opts_per_line = 1;
                          else
                            {
                              g_auto(GStrv) line_parts = NULL;
                              line_parts = g_strsplit (line, "\"", 0);
                              opts_per_line = (g_strv_length (line_parts) - 3) / 2;
                              opts_per_line = (opts_per_line > 0) ? opts_per_line : 1;
                            }
                          g_free (line);
                          continue;
                        }
                    }
                  if (right_bracket == NULL)
                    {
                      in_config_opts_array = TRUE;
                      config_opt_indent = g_strsplit (line, "\"", 0)[0];
                      g_free (line);
                      continue;
                    }

                  /* At this point it's either a single line or we're on the last line */
                  in_config_opts_array = FALSE;
                  config_opts_replaced = TRUE;
                  if (new_config_opts == NULL)
                    {
                      g_free (line);
                      line = g_strdup_printf ("%s],", array_prefix);
                    }
                  else
                    {
                      gchar *array_suffix;
                      array_suffix = *(right_bracket - 1) == ' ' ? " ]" : "]";
                      if (config_opt_indent == NULL)
                        {
                          g_auto(GStrv) line_parts = NULL;
                          line_parts = g_strsplit (line, "\"", 0);
                          config_opt_indent = g_strdup (line_parts[0]);
                        }
                      for (guint j = 0; g_strv_length (new_config_opts) > j; j += opts_per_line)
                        {
                          g_autoptr(GPtrArray) config_opts_subset = NULL;
                          g_autofree gchar *opts_this_line = NULL;
                          gchar *prefix;
                          gchar *suffix;
                          gchar *new_line;

                          prefix = (j == 0) ? array_prefix : config_opt_indent;
                          suffix = (g_strv_length (new_config_opts) <= j + opts_per_line) ? array_suffix : "";
                          config_opts_subset = g_ptr_array_new ();
                          for (guint k = 0; k < opts_per_line && new_config_opts[j+k]; ++k)
                            g_ptr_array_add (config_opts_subset, new_config_opts[j+k]);
                          g_ptr_array_add (config_opts_subset, NULL);
                          opts_this_line = g_strjoinv ("\", \"", (gchar **)config_opts_subset->pdata);

                          new_line = g_strdup_printf ("%s\"%s\"%s,", prefix, opts_this_line, suffix);
                          g_ptr_array_add (new_lines, new_line);
                        }
                      /* Discard the line that was just replaced with the new config-opts array */
                      g_free (line);
                      continue;
                    }
                }
            }

          g_ptr_array_add (new_lines, line);
          if (right_curly_brace_line != NULL)
            {
              g_ptr_array_add (new_lines, right_curly_brace_line);
              right_curly_brace_line = NULL;
            }
        }

      /* Ensure there's a newline at the end of the file */
      g_ptr_array_add (new_lines, g_strdup (""));
      g_ptr_array_add (new_lines, NULL);

      /* Write the updated lines to the disk */
      json_string = g_strjoinv ("\n", (gchar **)new_lines->pdata);
      bytes = g_bytes_new_take (json_string, strlen (json_string));
      if (!g_file_replace_contents (manifest,
                                    g_bytes_get_data (bytes, NULL),
                                    g_bytes_get_size (bytes),
                                    NULL,
                                    FALSE,
                                    G_FILE_CREATE_NONE,
                                    NULL,
                                    cancellable,
                                    &error))
        {
          g_task_return_error (task, error);
          IDE_EXIT;
        }
    }

  IDE_EXIT;
}

void
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

  if (self->change_count == 0)
    g_task_return_boolean (task, TRUE);
  else
    g_task_run_in_thread (task, gbp_flatpak_configuration_provider_save_worker);

  IDE_EXIT;
}

gboolean
gbp_flatpak_configuration_provider_save_finish (IdeConfigurationProvider  *provider,
                                                GAsyncResult              *result,
                                                GError                   **error)
{
  GbpFlatpakConfigurationProvider *self = (GbpFlatpakConfigurationProvider *)provider;

  g_return_val_if_fail (GBP_IS_FLATPAK_CONFIGURATION_PROVIDER (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static gboolean
gbp_flatpak_configuration_provider_do_writeback (gpointer data)
{
  GbpFlatpakConfigurationProvider *self = data;

  g_assert (GBP_IS_FLATPAK_CONFIGURATION_PROVIDER (self));

  self->writeback_handler = 0;

  gbp_flatpak_configuration_provider_save_async (IDE_CONFIGURATION_PROVIDER (self), NULL, NULL, NULL);

  return G_SOURCE_REMOVE;
}

static void
gbp_flatpak_configuration_provider_queue_writeback (GbpFlatpakConfigurationProvider *self)
{
  g_assert (GBP_IS_FLATPAK_CONFIGURATION_PROVIDER (self));

  IDE_ENTRY;

  if (self->writeback_handler != 0)
    g_source_remove (self->writeback_handler);

  self->writeback_handler = g_timeout_add_seconds (WRITEBACK_TIMEOUT_SECS,
                                                   gbp_flatpak_configuration_provider_do_writeback,
                                                   self);

  IDE_EXIT;
}

static void
gbp_flatpak_configuration_provider_changed (GbpFlatpakConfigurationProvider *self,
                                            IdeConfiguration *configuration)
{
  g_assert (GBP_IS_FLATPAK_CONFIGURATION_PROVIDER (self));
  g_assert (IDE_IS_CONFIGURATION (configuration));

  self->change_count++;

  gbp_flatpak_configuration_provider_queue_writeback (self);
}

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

      g_signal_connect_object (configuration,
                               "changed",
                               G_CALLBACK (gbp_flatpak_configuration_provider_changed),
                               self,
                               G_CONNECT_SWAPPED);

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

  ide_clear_source (&self->writeback_handler);

  if (self->configurations != NULL)
    {
      for (guint i = 0; i < self->configurations->len; i++)
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
  iface->save_async = gbp_flatpak_configuration_provider_save_async;
  iface->save_finish = gbp_flatpak_configuration_provider_save_finish;
}
