/* gbp-flatpak-sdk.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-flatpak-sdk"

#include "config.h"

#include <json-glib/json-glib.h>

#include "gbp-flatpak-sdk.h"

struct _GbpFlatpakSdk
{
  IdeSdk parent_instance;

  GMutex mutex;

  char *id;
  char *name;
  char *arch;
  char *branch;
  char *sdk_name;
  char *sdk_branch;
  char *deploy_dir;
  char *metadata;
  char *mount_path;

  guint is_sdk_extension : 1;
  guint discovered_mount_path : 1;
};

G_DEFINE_FINAL_TYPE (GbpFlatpakSdk, gbp_flatpak_sdk, IDE_TYPE_SDK)

static void
gbp_flatpak_sdk_finalize (GObject *object)
{
  GbpFlatpakSdk *self = (GbpFlatpakSdk *)object;

  g_clear_pointer (&self->id, g_free);
  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->arch, g_free);
  g_clear_pointer (&self->branch, g_free);
  g_clear_pointer (&self->sdk_name, g_free);
  g_clear_pointer (&self->sdk_branch, g_free);
  g_clear_pointer (&self->deploy_dir, g_free);
  g_clear_pointer (&self->metadata, g_free);
  g_clear_pointer (&self->mount_path, g_free);

  g_mutex_clear (&self->mutex);

  G_OBJECT_CLASS (gbp_flatpak_sdk_parent_class)->finalize (object);
}

static void
gbp_flatpak_sdk_class_init (GbpFlatpakSdkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_flatpak_sdk_finalize;
}

static void
gbp_flatpak_sdk_init (GbpFlatpakSdk *self)
{
  g_mutex_init (&self->mutex);
}

GbpFlatpakSdk *
gbp_flatpak_sdk_new_from_variant (GVariant *variant)
{
  GbpFlatpakSdk *ret;
  g_autofree char *name = NULL;
  g_autofree char *arch = NULL;
  g_autofree char *branch = NULL;
  g_autofree char *sdk_name = NULL;
  g_autofree char *sdk_branch = NULL;
  g_autofree char *deploy_dir = NULL;
  g_autofree char *metadata = NULL;
  g_autofree char *title = NULL;
  gboolean is_sdk_extension = FALSE;

  g_return_val_if_fail (variant != NULL, NULL);
  g_return_val_if_fail (g_variant_is_of_type (variant, G_VARIANT_TYPE ("(sssssssb)")), NULL);

  g_variant_get (variant,
                 "(sssssssb)",
                 &name,
                 &arch,
                 &branch,
                 &sdk_name,
                 &sdk_branch,
                 &deploy_dir,
                 &metadata,
                 &is_sdk_extension);

  title = g_strdup_printf ("%s/%s/%s", name, arch, branch);

  ret = g_object_new (GBP_TYPE_FLATPAK_SDK,
                      "title", title,
                      "can-update", TRUE,
                      NULL);

  ret->id = g_strdup_printf ("runtime/%s/%s/%s", name, arch, branch);
  ret->name = g_steal_pointer (&name);
  ret->arch = g_steal_pointer (&arch);
  ret->branch = g_steal_pointer (&branch);
  ret->sdk_name = g_steal_pointer (&sdk_name);
  ret->sdk_branch = g_steal_pointer (&sdk_branch);
  ret->deploy_dir = g_steal_pointer (&deploy_dir);
  ret->metadata = g_steal_pointer (&metadata);
  ret->is_sdk_extension = !!is_sdk_extension;

  return ret;
}

const char *
gbp_flatpak_sdk_get_id (GbpFlatpakSdk *self)
{
  g_return_val_if_fail (GBP_IS_FLATPAK_SDK (self), NULL);

  return self->id;
}

static const char *
gbp_flatpak_sdk_get_mount_path (GbpFlatpakSdk *self)
{
  g_assert (GBP_IS_FLATPAK_SDK (self));

  if (!self->discovered_mount_path)
    {
      g_mutex_lock (&self->mutex);

      if (!self->discovered_mount_path)
        {
          g_autofree char *json_path = g_build_filename (self->deploy_dir, "files", "manifest.json", NULL);
          g_autoptr(JsonParser) parser = json_parser_new ();

          if (json_parser_load_from_mapped_file (parser, json_path, NULL))
            {
              JsonObject *obj;
              JsonNode *node;

              if ((node = json_parser_get_root (parser)) &&
                  JSON_NODE_HOLDS_OBJECT (node) &&
                  (obj = json_node_get_object (node)) &&
                  json_object_has_member (obj, "build-options") &&
                  (node = json_object_get_member (obj, "build-options")) &&
                  JSON_NODE_HOLDS_OBJECT (node) &&
                  (obj = json_node_get_object (node)) &&
                  json_object_has_member (obj, "prefix") &&
                  (node = json_object_get_member (obj, "prefix")) &&
                  JSON_NODE_HOLDS_VALUE (node))
                self->mount_path = json_node_dup_string (node);
            }

          /* If we're a .Debug, then assume we're at /usr/lib/debug */
          if (self->mount_path == NULL && g_str_has_suffix (self->name, ".Debug"))
            self->mount_path = g_strdup ("/usr/lib/debug");

          self->discovered_mount_path = TRUE;
        }

      g_mutex_unlock (&self->mutex);
    }

  return self->mount_path ? self->mount_path : "/usr";
}

GFile *
gbp_flatpak_sdk_translate_file (GbpFlatpakSdk *self,
                                GFile         *file)
{
  g_autofree char *deploy_dir_files = NULL;
  const char *path;
  const char *mount_path;

  g_assert (GBP_IS_FLATPAK_SDK (self));
  g_assert (G_IS_FILE (file));

  /* We only support native files as UNIX paths */
  if (!g_file_is_native (file) || !(path = g_file_peek_path (file)))
    return NULL;

  /* This should be handled by gbp_flatpak_manifest_translate_file() */
  if (g_str_equal (path, "/app") || g_str_has_prefix (path, "/app/"))
    return NULL;

  deploy_dir_files = g_build_filename (self->deploy_dir, "files", NULL);

  /* Get the mount path (default is /usr) but things like "id".Debug will
   * be mounted at /usr/lib/debug (unless specified by a manifest.json).
   */
  if ((mount_path = gbp_flatpak_sdk_get_mount_path (self)))
    {
      gsize len = strlen (mount_path);

      if (strncmp (path, mount_path, len) == 0)
        {
          if (path[len] == 0 || !(path = g_path_skip_root (path + len)) || path[0] == 0)
            path = NULL;

          return g_file_new_build_filename (deploy_dir_files, path, NULL);
        }
    }

  /* Sometimes we'll be trying to resovle a path to sources when debugging,
   * usually extracted with readelf/DWARF data from the likes of GDB. This
   * is generally only provided by the .Debug runtimes, so we can short-circuit
   * based on our id.
   */
  if (g_str_has_suffix (self->name, ".Debug") &&
      g_str_has_suffix (path, "/run/build-runtime/"))
    {
      g_autoptr(GFile) translated = g_file_new_build_filename (deploy_dir_files,
                                                               "sources",
                                                               path + strlen ("/run/build-runtime/"),
                                                               NULL);

      /* Just to be sure this is within our .Debug. While not currently in use,
       * I don't see any reason why we can't have multiple .Debug runtimes in
       * play providing sources access at different subdirectories.
       */
      if (g_file_query_exists (translated, NULL))
        return g_steal_pointer (&translated);
    }

  return NULL;
}
