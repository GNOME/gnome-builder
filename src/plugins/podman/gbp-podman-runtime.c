/* gbp-podman-runtime.c
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

#define G_LOG_DOMAIN "gbp-podman-runtime"

#include "config.h"

#include <glib/gi18n.h>
#include <json-glib/json-glib.h>

#include "gbp-podman-runtime.h"
#include "gbp-podman-runtime-private.h"
#include "gbp-podman-subprocess-launcher.h"

struct _GbpPodmanRuntime
{
  IdeRuntime  parent_instance;
  JsonObject *object;
  gchar      *id;
  GMutex      mutex;
  guint       has_started : 1;
  GList      *layers;
};

G_DEFINE_FINAL_TYPE (GbpPodmanRuntime, gbp_podman_runtime, IDE_TYPE_RUNTIME)

static void
maybe_start (GbpPodmanRuntime *self)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;

  g_assert (GBP_IS_PODMAN_RUNTIME (self));
  g_assert (self->id != NULL);

  if (self->has_started)
    return;

  g_mutex_lock (&self->mutex);

  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDERR_SILENCE |
                                          G_SUBPROCESS_FLAGS_STDOUT_SILENCE);
  ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
  ide_subprocess_launcher_push_argv (launcher, "podman");
  ide_subprocess_launcher_push_argv (launcher, "start");
  ide_subprocess_launcher_push_argv (launcher, self->id);

  if ((subprocess = ide_subprocess_launcher_spawn (launcher, NULL, NULL)))
    {
      ide_subprocess_wait_async (subprocess, NULL, NULL, NULL);
      self->has_started = TRUE;
    }

  g_mutex_unlock (&self->mutex);
}

static IdeSubprocessLauncher *
gbp_podman_runtime_create_launcher (IdeRuntime  *runtime,
                                    GError     **error)
{
  GbpPodmanRuntime *self = (GbpPodmanRuntime *)runtime;
  IdeSubprocessLauncher *launcher;

  g_assert (GBP_IS_PODMAN_RUNTIME (self));
  g_assert (self->id != NULL);

  maybe_start (self);

  launcher = g_object_new (GBP_TYPE_PODMAN_SUBPROCESS_LAUNCHER,
                           "id", self->id,
                           NULL);

  return launcher;
}

char *
_gbp_podman_runtime_parse_toml_line (const char *line)
{
  g_auto(GStrv) elements = NULL;

  g_return_val_if_fail (line != NULL, NULL);

  elements = g_strsplit (line, "=", 0);
  if (g_strv_length (elements) != 2)
    return NULL;
  g_strstrip (g_strdelimit (elements[1], "\"", ' '));
  return ide_path_expand (elements[1]);
}

char *
_gbp_podman_runtime_parse_storage_configuration (const char  *storage_conf,
                                                 StorageType  type)
{
  g_autoptr(GFile) storage_conf_file = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFileInputStream) fis = NULL;
  g_autoptr(GDataInputStream) dis = NULL;
  char *line = NULL;

  storage_conf_file = g_file_new_for_path (storage_conf);
  if (!(fis = g_file_read (storage_conf_file, NULL, &error)))
    return NULL;

  dis = g_data_input_stream_new (G_INPUT_STREAM (fis));

  while ((line = g_data_input_stream_read_line (dis, NULL, NULL, &error)) != NULL)
    {
      g_autofree char *local_line = line;
      if (type == LOCAL_STORAGE_CONFIGURATION &&
          g_str_has_prefix (local_line, "graphroot"))
        {
          return _gbp_podman_runtime_parse_toml_line (line);
        }
      else if (type == GLOBAL_STORAGE_CONFIGURATION &&
               g_str_has_prefix (local_line, "rootless_storage_path"))
        {
          return _gbp_podman_runtime_parse_toml_line (line);
        }
    }

  return NULL;
}

/* see man 5 containers-storage.json */
static char *
get_storage_directory (void)
{
  g_autofree char *user_local_storage_conf = NULL;
  g_autofree char *global_storage_conf = NULL;

  /* first search for user local storage configuration */
  user_local_storage_conf = g_build_filename (g_get_home_dir (),
                                              ".config",
                                              "containers",
                                              "storage.conf",
                                              NULL);

  if (g_file_test (user_local_storage_conf, G_FILE_TEST_EXISTS))
    {
      return _gbp_podman_runtime_parse_storage_configuration (user_local_storage_conf,
                                                              LOCAL_STORAGE_CONFIGURATION);
    }

  /* second search for a global storage configuration */
  global_storage_conf = g_build_filename ("etc", "containers", "storage.conf", NULL);

  if (g_file_test (global_storage_conf, G_FILE_TEST_EXISTS))
    {
      return _gbp_podman_runtime_parse_storage_configuration (global_storage_conf,
                                                              GLOBAL_STORAGE_CONFIGURATION);
    }

  return NULL;
}

static char *
get_layer_dir (const char *storage_directory,
               const char *layer)
{
  /* We don't use XDG data dir because this might be in a container
   * or flatpak environment that doesn't match. And generally, it's
   * always .local.
   */
  return g_build_filename (storage_directory,
                           "overlay",
                           layer,
                           "diff",
                           NULL);
}

static char *
find_parent_layer (GbpPodmanRuntime *runtime,
                   JsonParser       *parser,
                   const char       *layer)
{
  JsonNode *root;
  JsonArray *ar;
  guint n_items;

  g_assert (JSON_IS_PARSER (parser));
  g_assert (layer != NULL);

  if (!(root = json_parser_get_root (parser)) ||
      !JSON_NODE_HOLDS_ARRAY (root) ||
      !(ar = json_node_get_array (root)))
    return NULL;

  n_items = json_array_get_length (ar);

  for (guint i = 0; i < n_items; i++)
    {
      JsonObject *item = json_array_get_object_element (ar, i);
      const char *parent;
      const char *id;

      if (item == NULL ||
          !json_object_has_member (item, "id") ||
          !json_object_has_member (item, "parent") ||
          !(id = json_object_get_string_member (item, "id")) ||
          strcmp (id, layer) != 0 ||
          !(parent = json_object_get_string_member (item, "parent")))
        continue;

      return (char *)parent;
    }

  return NULL;
}

static char *
find_image_layer (JsonParser *parser,
                  const char *image)
{
  JsonNode *root;
  JsonArray *ar;
  guint n_items;

  g_assert (JSON_IS_PARSER (parser));
  g_assert (image != NULL);

  if (!(root = json_parser_get_root (parser)) ||
      !JSON_NODE_HOLDS_ARRAY (root) ||
      !(ar = json_node_get_array (root)))
    return NULL;

  n_items = json_array_get_length (ar);

  for (guint i = 0; i < n_items; i++)
    {
      JsonObject *item = json_array_get_object_element (ar, i);
      const char *id;
      const char *layer;

      if (item == NULL ||
          !json_object_has_member (item, "id") ||
          !json_object_has_member (item, "layer") ||
          !(id = json_object_get_string_member (item, "id")) ||
          strcmp (id, image) != 0 ||
          !(layer = json_object_get_string_member (item, "layer")))
        continue;

      return (char *)layer;
    }

  return NULL;
}

static void
resolve_overlay (GbpPodmanRuntime *runtime)
{
  gchar *podman_id;
  g_autofree gchar *container_json = NULL;
  g_autofree gchar *layer_json = NULL;
  g_autofree gchar *image_json = NULL;
  g_autofree char *storage_directory = NULL;
  g_autoptr(JsonParser) parser;
  g_autoptr(JsonParser) image_parser;
  g_autoptr(GFile) overlay = NULL;
  g_autoptr(GFileInfo) overlay_info = NULL;
  g_autoptr(GError) error = NULL;
  const gchar *image_id = NULL;
  JsonNode *root;
  JsonArray *containers_arr;
  gchar *layer = NULL;

  g_assert (GBP_IS_PODMAN_RUNTIME (runtime));

  podman_id = runtime->id;
  parser = json_parser_new ();
  image_parser = json_parser_new ();

  /* find storage location first */
  if ((storage_directory = get_storage_directory ()) == NULL)
    {
      /* assume default */
      storage_directory = g_build_filename (g_get_home_dir (),
                                            ".local",
                                            "share",
                                            "containers",
                                            "storage",
                                            NULL);
    }

  /* test first if overlay has the correct ownership see: https://github.com/containers/storage/issues/1068
   * so in order for this to work this has to be fixed
   */
  overlay = g_file_new_build_filename (storage_directory,
                                       "overlay",
                                       NULL);
  overlay_info = g_file_query_info (overlay, G_FILE_ATTRIBUTE_ACCESS_CAN_READ, G_FILE_QUERY_INFO_NONE, NULL, &error);
  if (error)
    {
      ide_object_warning (ide_object_get_context (IDE_OBJECT (runtime)), "Cannot read overlay folder: %s", error->message);
      return;
    }

  if (!g_file_info_get_attribute_boolean (overlay_info, G_FILE_ATTRIBUTE_ACCESS_CAN_READ))
    {
      ide_object_warning (ide_object_get_context (IDE_OBJECT (runtime)), "Cannot read overlay folder: podman file translation won't work");
      return;
    }

  container_json = g_build_filename (storage_directory,
                                     "overlay-containers",
                                     "containers.json",
                                     NULL);

  layer_json = g_build_filename (storage_directory,
                                 "overlay-layers",
                                 "layers.json",
                                 NULL);

  image_json = g_build_filename (storage_directory,
                                 "overlay-images",
                                 "images.json",
                                 NULL);

  json_parser_load_from_file (parser, container_json, NULL);
  root = json_parser_get_root (parser);
  containers_arr = json_node_get_array (root);
  for (guint i = 0; i < json_array_get_length (containers_arr); i++)
    {
      JsonObject *cont = json_array_get_object_element (containers_arr, i);
      const gchar *cid = json_object_get_string_member (cont, "id");
      if (ide_str_equal0 (cid, podman_id))
        {
          const gchar *layer_id = json_object_get_string_member (cont, "layer");
          layer = get_layer_dir (storage_directory, layer_id);
          image_id = json_object_get_string_member (cont, "image");
        }
    }

  json_parser_load_from_file (parser, layer_json, &error);
  if (error)
    return;

  if (layer != NULL)
    {
      /* apply all parent layers */
      do {
        runtime->layers = g_list_append (runtime->layers, layer);
      } while ((layer = find_parent_layer (runtime, parser, layer)));
    }

  /* apply image layer */
  json_parser_load_from_file (image_parser, image_json, &error);
  if (error)
    return;

  if ((layer = find_image_layer (image_parser, image_id)))
    {
      do
        runtime->layers = g_list_append (runtime->layers, layer);
      while ((layer = find_parent_layer (runtime, parser, layer)));
    }
}

/*
 * Translation here is important as all our machinery relies on the correct files. In case of
 * containers it is important to search for the correct files in their respective storage.
 */
static GFile *
gbp_podman_runtime_translate_file (IdeRuntime *runtime,
                                   GFile      *file)
{
  GbpPodmanRuntime *self = (GbpPodmanRuntime *)runtime;
  g_autofree gchar *path = NULL;

  g_assert (IDE_IS_RUNTIME (runtime));
  g_assert (GBP_IS_PODMAN_RUNTIME (runtime));

  path = g_file_get_path (file);

  if (g_str_has_prefix (path, "/usr/") || g_str_has_prefix (path, "/etc/"))
    {
      /* find the correct layer */
      for (GList *cur = self->layers; cur; cur = g_list_next (cur))
        {
          gchar *layer = cur->data;
          g_autofree gchar *translated_file = g_build_filename (layer, path, NULL);
          if (g_file_test (translated_file, G_FILE_TEST_EXISTS))
            return g_file_new_build_filename (translated_file, NULL);
        }
    }
  return NULL;
}

static void
gbp_podman_runtime_destroy (IdeObject *object)
{
  GbpPodmanRuntime *self = (GbpPodmanRuntime *)object;

  g_clear_pointer (&self->object, json_object_unref);
  g_clear_pointer (&self->id, g_free);

  IDE_OBJECT_CLASS (gbp_podman_runtime_parent_class)->destroy (object);
}

static void
gbp_podman_runtime_finalize (GObject *object)
{
  GbpPodmanRuntime *self = (GbpPodmanRuntime *)object;

  g_clear_pointer (&self->id, g_free);
  g_mutex_clear (&self->mutex);
  g_clear_list (&self->layers, g_free);

  G_OBJECT_CLASS (gbp_podman_runtime_parent_class)->finalize (object);
}

static void
gbp_podman_runtime_class_init (GbpPodmanRuntimeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);
  IdeRuntimeClass *runtime_class = IDE_RUNTIME_CLASS (klass);

  object_class->finalize = gbp_podman_runtime_finalize;

  i_object_class->destroy = gbp_podman_runtime_destroy;

  runtime_class->create_launcher = gbp_podman_runtime_create_launcher;
  runtime_class->translate_file = gbp_podman_runtime_translate_file;
}

static void
gbp_podman_runtime_init (GbpPodmanRuntime *self)
{
  g_mutex_init (&self->mutex);
}

GbpPodmanRuntime *
gbp_podman_runtime_new (JsonObject *object)
{
  g_autofree gchar *full_id = NULL;
  g_autofree gchar *name = NULL;
  GbpPodmanRuntime *self;
  const gchar *id;
  const gchar *names;
  JsonArray *names_arr;
  JsonNode *names_node;
  JsonNode *labels_node;
  gboolean is_toolbox = FALSE;
  const gchar *category;

  g_return_val_if_fail (object != NULL, NULL);

  if (json_object_has_member (object, "ID"))
    id = json_object_get_string_member (object, "ID");
  else
    id = json_object_get_string_member (object, "Id");

  names_node = json_object_get_member (object, "Names");
  if (JSON_NODE_HOLDS_ARRAY (names_node))
    {
      names_arr = json_node_get_array (names_node);
      names = json_array_get_string_element (names_arr, 0);
    }
  else
    {
      names = json_node_get_string (names_node);
    }

  if (json_object_has_member (object, "Labels") &&
      (labels_node = json_object_get_member (object, "Labels")) &&
      JSON_NODE_HOLDS_OBJECT (labels_node))
    {
      JsonObject *labels = json_node_get_object (labels_node);

      /* Check if this is a toolbox container */
      if (json_object_has_member (labels, "com.github.debarshiray.toolbox") ||
          json_object_has_member (labels, "com.github.containers.toolbox"))
        is_toolbox = TRUE;
    }

  full_id = g_strdup_printf ("podman:%s", id);

  if (is_toolbox)
    {
      name = g_strdup_printf ("Toolbox %s", names);
      /* translators: this is a path to browse to the runtime, likely only "containers" should be translated */
      category = _("Containers/Toolbox");
    }
  else
    {
      name = g_strdup_printf ("Podman %s", names);
      /* translators: this is a path to browse to the runtime, likely only "containers" should be translated */
      category = _("Containers/Podman");
    }

  g_return_val_if_fail (id != NULL, NULL);
  g_return_val_if_fail (names != NULL, NULL);

  self = g_object_new (GBP_TYPE_PODMAN_RUNTIME,
                       "id", full_id,
                       "category", category,
                       "display-name", names,
                       NULL);
  self->object = json_object_ref (object);
  self->id = g_strdup (id);

  resolve_overlay (self);

  return g_steal_pointer (&self);
}
