/* gbp-flatpak-build-system-discovery.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-flatpak-build-system-discovery"

#include <json-glib/json-glib.h>

#include "gbp-flatpak-build-system-discovery.h"
#include "gbp-flatpak-util.h"

#define DISCOVERY_MAX_DEPTH 3

/*
 * TODO: It would be nice if this could share more code with GbpFlatpakConfigurationProvider.
 */

struct _GbpFlatpakBuildSystemDiscovery
{
  GObject parent_instance;
};

/* Returns whether @filename seems to be a JSON/YAML file, naively detected and return its basename. */
static gchar *
maybe_is_json_or_yaml_file (const char *filename)
{
  size_t filename_len = strlen (filename);
  if (filename_len >= strlen (".json") && g_str_has_suffix (filename, ".json")) {
      return g_strndup (filename, filename_len - strlen (".json"));
  } else if (filename_len >= strlen (".yaml") && g_str_has_suffix (filename, ".yaml")) {
      return g_strndup (filename, filename_len - strlen (".yaml"));
  }  else if (filename_len >= strlen (".yml") && g_str_has_suffix (filename, ".yml")) {
      return g_strndup (filename, filename_len - strlen (".yml"));
  }

  return NULL;
}

static void
gbp_flatpak_build_system_discovery_find_manifests (GFile        *directory,
                                                   GPtrArray    *results,
                                                   gint          depth,
                                                   GCancellable *cancellable)
{
  g_autoptr(GFileEnumerator) enumerator = NULL;
  g_autoptr(GPtrArray) child_dirs = NULL;
  gpointer infoptr;

  g_assert (G_IS_FILE (directory));
  g_assert (results != NULL);
  g_assert (depth < DISCOVERY_MAX_DEPTH);

  if (g_cancellable_is_cancelled (cancellable))
    return;

  enumerator = g_file_enumerate_children (directory,
                                          G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK","
                                          G_FILE_ATTRIBUTE_STANDARD_NAME","
                                          G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                          G_FILE_QUERY_INFO_NONE,
                                          cancellable,
                                          NULL);

  if (enumerator == NULL)
    return;

  while (NULL != (infoptr = g_file_enumerator_next_file (enumerator, cancellable, NULL)))
    {
      g_autoptr(GFileInfo) info = infoptr;
      g_autoptr(GFile) file = NULL;
      GFileType file_type;
      const gchar *name;
      g_autofree gchar *app_id = NULL;

      if (g_file_info_get_is_symlink (info))
        continue;

      if (NULL == (name = g_file_info_get_name (info)))
        continue;

      file_type = g_file_info_get_file_type (info);
      file = g_file_get_child (directory, name);

      if (file_type == G_FILE_TYPE_DIRECTORY)
        {
          /* TODO: Use a global ignored-file filter from libide */
          if (g_strcmp0 (name, ".flatpak-builder") == 0 || g_strcmp0 (name, ".git") == 0)
            continue;

          if (depth < DISCOVERY_MAX_DEPTH - 1)
            {
              if (child_dirs == NULL)
                child_dirs = g_ptr_array_new_with_free_func (g_object_unref);
              g_ptr_array_add (child_dirs, g_steal_pointer (&file));
              continue;
            }
        }

      if (!(app_id = maybe_is_json_or_yaml_file (name)))
        continue;

      if (!g_application_id_is_valid (app_id))
        continue;

      g_ptr_array_add (results, g_steal_pointer (&file));
    }

  if (child_dirs != NULL)
    {
      for (guint i = 0; i < child_dirs->len; i++)
        {
          GFile *file = g_ptr_array_index (child_dirs, i);

          if (g_cancellable_is_cancelled (cancellable))
            return;

          gbp_flatpak_build_system_discovery_find_manifests (file, results, depth + 1, cancellable);
        }
    }
}

static int
sort_by_path (gconstpointer a,
              gconstpointer b)
{
  GFile *file_a = *(GFile * const *)a;
  GFile *file_b = *(GFile * const *)b;
  g_autofree char *collate_a = g_utf8_collate_key_for_filename (g_file_peek_path (file_a), -1);
  g_autofree char *collate_b = g_utf8_collate_key_for_filename (g_file_peek_path (file_b), -1);

  return g_strcmp0 (collate_a, collate_b);
}

static gchar *
gbp_flatpak_build_system_discovery_discover (IdeBuildSystemDiscovery  *discovery,
                                             GFile                    *project_file,
                                             GCancellable             *cancellable,
                                             gint                     *priority,
                                             GError                  **error)
{
  g_autoptr(GPtrArray) manifests = NULL;
  g_autoptr(GFile) project_dir = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_BUILD_SYSTEM_DISCOVERY (discovery));
  g_assert (G_IS_FILE (project_file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (priority != NULL);

  manifests = g_ptr_array_new_with_free_func (g_object_unref);
  gbp_flatpak_build_system_discovery_find_manifests (project_file, manifests, 0, cancellable);

  IDE_TRACE_MSG ("We found %u potential manifests", manifests->len);

  g_ptr_array_sort (manifests, sort_by_path);

#ifdef IDE_ENABLE_TRACE
  for (guint i = 0; i < manifests->len; i++)
    IDE_TRACE_MSG ("  Manifest[%u]: %s\n",
                   i,
                   g_file_peek_path (g_ptr_array_index (manifests, i)));
#endif


  if (g_file_query_file_type (project_file, 0, NULL) == G_FILE_TYPE_DIRECTORY)
    project_dir = g_object_ref (project_file);
  else
    project_dir = g_file_get_parent (project_file);

  if (priority)
    *priority = 0;

  for (guint i = 0; i < manifests->len; i++)
    {
      GFile *file = g_ptr_array_index (manifests, i);
      g_autoptr(JsonNode) root_node = NULL;
      g_autofree gchar *path = NULL;
      g_autofree gchar *base = NULL;
      const gchar *buildsystem;
      const gchar *app_id_str;
      JsonObject *root_object;
      JsonNode *app_id;
      JsonNode *modules_node;
      JsonArray *modules_array;
      JsonNode *source_node;
      JsonObject *source_object;
      JsonNode *buildsystem_node;
      guint len;

      if (NULL == (path = g_file_get_path (file)))
        continue;

      IDE_TRACE_MSG ("Checking potential manifest \"%s\"", path);

      base = g_file_get_basename (file);

      if (g_str_has_suffix (base, ".yaml") || g_str_has_suffix (base, ".yml"))
        {
          g_autofree gchar *contents = NULL;
          gsize contents_len = 0;

          if (!g_file_load_contents (file, cancellable, &contents, &contents_len, NULL, error))
            continue;

          root_node = gbp_flatpak_yaml_to_json (contents, contents_len, error);
          if (!root_node)
            continue;
        }
      else
        {
          g_autoptr(JsonParser) parser = json_parser_new ();

          if (!json_parser_load_from_file (parser, path, NULL))
            continue;

          root_node = json_parser_steal_root (parser);
        }


      if ((root_object = json_node_get_object (root_node)) &&
          ((app_id = json_object_get_member (root_object, "id")) ||
           (app_id = json_object_get_member (root_object, "app-id"))) &&
          JSON_NODE_HOLDS_VALUE (app_id) &&
          (app_id_str = json_node_get_string (app_id)) &&
          g_str_has_prefix (base, app_id_str) &&
          (modules_node = json_object_get_member (root_object, "modules")) &&
          JSON_NODE_HOLDS_ARRAY (modules_node) &&
          (modules_array = json_node_get_array (modules_node)) &&
          /* TODO: Discovery matching source element */
          (len = json_array_get_length (modules_array)) > 0 &&
          (source_node = json_array_get_element (modules_array, len - 1)) &&
          JSON_NODE_HOLDS_OBJECT (source_node) &&
          (source_object = json_node_get_object (source_node)) &&
          json_object_has_member (source_object, "buildsystem") &&
          (buildsystem_node = json_object_get_member (source_object, "buildsystem")) &&
          JSON_NODE_HOLDS_VALUE (buildsystem_node) &&
          (buildsystem = json_node_get_string (buildsystem_node)) &&
          *buildsystem != '\0')
        {
          gchar *ret;

          if (ide_str_equal0 (buildsystem, "cmake-ninja"))
            buildsystem = "cmake";
          else if (ide_str_equal0 (buildsystem, "simple"))
            {
              JsonNode *sdk_extensions;
              JsonArray *sdk_extensions_array;

              buildsystem = "directory";

              /* Check for a cargo project */
              if ((sdk_extensions = json_object_get_member (root_object, "sdk-extensions")) &&
                  JSON_NODE_HOLDS_ARRAY (sdk_extensions) &&
                  (sdk_extensions_array = json_node_get_array (sdk_extensions)))
                {
                  guint ar_len = json_array_get_length (sdk_extensions_array);

                  for (guint j = 0; j < ar_len; j++)
                    {
                      const char *extension = json_array_get_string_element (sdk_extensions_array, j);

                      if (ide_str_equal0 (extension, "org.freedesktop.Sdk.Extension.rust-stable") ||
                          ide_str_equal0 (extension, "org.freedesktop.Sdk.Extension.rust-nightly"))
                        {
                          g_autoptr(GFile) Cargo_toml = g_file_get_child (project_dir, "Cargo.toml");

                          if (g_file_query_exists (Cargo_toml, NULL))
                            buildsystem = "cargo";
                        }
                    }
                }
            }

          /* Set priority higher than normal discoveries */
          if (priority != NULL)
            *priority = -1000;

          ret = g_strdup (buildsystem);
          IDE_TRACE_MSG ("Discovered buildsystem of type \"%s\"", ret);
          IDE_RETURN (ret);
        }
    }

  IDE_RETURN (NULL);
}

static void
build_system_discovery_iface_init (IdeBuildSystemDiscoveryInterface *iface)
{
  iface->discover = gbp_flatpak_build_system_discovery_discover;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpFlatpakBuildSystemDiscovery,
                         gbp_flatpak_build_system_discovery,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_BUILD_SYSTEM_DISCOVERY, build_system_discovery_iface_init))

static void
gbp_flatpak_build_system_discovery_class_init (GbpFlatpakBuildSystemDiscoveryClass *klass)
{
}

static void
gbp_flatpak_build_system_discovery_init (GbpFlatpakBuildSystemDiscovery *self)
{
}
