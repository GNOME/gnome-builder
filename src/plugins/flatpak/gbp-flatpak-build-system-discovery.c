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

#define DISCOVERY_MAX_DEPTH 3

/*
 * TODO: It would be nice if this could share more code with GbpFlatpakConfigurationProvider.
 */

struct _GbpFlatpakBuildSystemDiscovery
{
  GObject parent_instance;
};

static GRegex *filename_regex;

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
      g_autoptr(GMatchInfo) match_info = NULL;
      g_autoptr(GFile) file = NULL;
      GFileType file_type;
      const gchar *name;

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

      g_regex_match (filename_regex, name, 0, &match_info);
      if (!g_match_info_matches (match_info))
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

static gchar *
gbp_flatpak_build_system_discovery_discover (IdeBuildSystemDiscovery  *discovery,
                                             GFile                    *project_file,
                                             GCancellable             *cancellable,
                                             gint                     *priority,
                                             GError                  **error)
{
  g_autoptr(GPtrArray) manifests = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_BUILD_SYSTEM_DISCOVERY (discovery));
  g_assert (G_IS_FILE (project_file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (priority != NULL);

  manifests = g_ptr_array_new_with_free_func (g_object_unref);
  gbp_flatpak_build_system_discovery_find_manifests (project_file, manifests, 0, cancellable);

  IDE_TRACE_MSG ("We found %u potential manifests", manifests->len);

  if (priority)
    *priority = 0;

  for (guint i = 0; i < manifests->len; i++)
    {
      GFile *file = g_ptr_array_index (manifests, i);
      g_autofree gchar *path = NULL;
      g_autofree gchar *base = NULL;
      const gchar *buildsystem;
      const gchar *app_id_str;
      g_autoptr(JsonParser) parser = NULL;
      JsonObject *root_object;
      JsonNode *root_node;
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
      parser = json_parser_new ();

      if (!json_parser_load_from_file (parser, path, NULL))
        continue;

      root_node = json_parser_get_root (parser);

      if (NULL != (root_object = json_node_get_object (root_node)) &&
          NULL != (app_id = json_object_get_member (root_object, "app-id")) &&
          JSON_NODE_HOLDS_VALUE (app_id) &&
          NULL != (app_id_str = json_node_get_string (app_id)) &&
          g_str_has_prefix (base, app_id_str) &&
          NULL != (modules_node = json_object_get_member (root_object, "modules")) &&
          JSON_NODE_HOLDS_ARRAY (modules_node) &&
          NULL != (modules_array = json_node_get_array (modules_node)) &&
          /* TODO: Discovery matching source element */
          (len = json_array_get_length (modules_array)) > 0 &&
          NULL != (source_node = json_array_get_element (modules_array, len - 1)) &&
          JSON_NODE_HOLDS_OBJECT (source_node) &&
          NULL != (source_object = json_node_get_object (source_node)) &&
          json_object_has_member (source_object, "buildsystem") &&
          NULL != (buildsystem_node = json_object_get_member (source_object, "buildsystem")) &&
          JSON_NODE_HOLDS_VALUE (buildsystem_node) &&
          NULL != (buildsystem = json_node_get_string (buildsystem_node)) &&
          *buildsystem != '\0')
        {
          gchar *ret;

          if (dzl_str_equal0 (buildsystem, "cmake-ninja"))
            buildsystem = "cmake";
          else if (dzl_str_equal0 (buildsystem, "simple"))
            buildsystem = "directory";

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

G_DEFINE_TYPE_WITH_CODE (GbpFlatpakBuildSystemDiscovery,
                         gbp_flatpak_build_system_discovery,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_BUILD_SYSTEM_DISCOVERY, build_system_discovery_iface_init))

static void
gbp_flatpak_build_system_discovery_class_init (GbpFlatpakBuildSystemDiscoveryClass *klass)
{
  /* This regex is based on https://wiki.gnome.org/HowDoI/ChooseApplicationID */
  filename_regex = g_regex_new ("^[[:alnum:]-_]+\\.[[:alnum:]-_]+(\\.[[:alnum:]-_]+)*\\.json$",
                                G_REGEX_OPTIMIZE, 0, NULL);
}

static void
gbp_flatpak_build_system_discovery_init (GbpFlatpakBuildSystemDiscovery *self)
{
}
