/* gbp-meson-toolchain.c
 *
 * Copyright 2018 Collabora Ltd.
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
 * Authors: Corentin Noël <corentin.noel@collabora.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "gbp-meson-toolchain"

#include <glib/gi18n.h>

#include "gbp-meson-toolchain.h"
#include "gbp-meson-utils.h"

struct _GbpMesonToolchain
{
  IdeSimpleToolchain      parent_instance;
  gchar                  *file_path;
};

G_DEFINE_FINAL_TYPE (GbpMesonToolchain, gbp_meson_toolchain, IDE_TYPE_SIMPLE_TOOLCHAIN)

enum {
  PROP_0,
  PROP_FILE_PATH,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

GbpMesonToolchain *
gbp_meson_toolchain_new (IdeContext *context)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  return g_object_new (GBP_TYPE_MESON_TOOLCHAIN, NULL);
}

gboolean
gbp_meson_toolchain_load (GbpMesonToolchain  *self,
                          GFile              *file,
                          GError            **error)
{
  g_autofree gchar *path = g_file_get_path (file);
  g_autofree gchar *id = g_strconcat ("meson:", path, NULL);
  g_autofree gchar *display_name = g_strdup_printf (_("%s (Meson)"), path);
  g_autofree gchar *arch = NULL;
  g_autofree gchar *system = NULL;
  g_autoptr(GKeyFile) keyfile = g_key_file_new ();
  g_autoptr(IdeTriplet) triplet = NULL;
  g_autoptr(GError) list_error = NULL;
  g_auto(GStrv) binaries = NULL;

  if (!g_key_file_load_from_file (keyfile, path, G_KEY_FILE_NONE, error))
    return FALSE;

  arch = gbp_meson_key_file_get_string_quoted (keyfile, "host_machine", "cpu_family", error);
  if (arch == NULL)
    return FALSE;

  system = gbp_meson_key_file_get_string_quoted (keyfile, "host_machine", "system", error);
  if (system == NULL)
    return FALSE;

  triplet = ide_triplet_new_with_triplet (arch, system, NULL);

  g_clear_pointer (&self->file_path, g_free);
  self->file_path = g_steal_pointer (&path);

  ide_toolchain_set_id (IDE_TOOLCHAIN(self), id);
  ide_toolchain_set_display_name (IDE_TOOLCHAIN(self), display_name);
  ide_toolchain_set_host_triplet (IDE_TOOLCHAIN(self), triplet);

  binaries = g_key_file_get_keys (keyfile, "binaries", NULL, &list_error);
  if (binaries == NULL)
    return TRUE;

  for (int i = 0; binaries[i] != NULL; i++)
    {
      const gchar *lang = binaries[i];
      const gchar *tool_id = gbp_meson_get_tool_id_from_binary (lang);
      g_autoptr(GError) key_error = NULL;
      g_autofree gchar *exec_path = gbp_meson_key_file_get_string_quoted (keyfile, "binaries", lang, &key_error);

      if (g_strcmp0 (tool_id, IDE_TOOLCHAIN_TOOL_CC) == 0)
        ide_simple_toolchain_set_tool_for_language (IDE_SIMPLE_TOOLCHAIN(self),
                                                    gbp_meson_get_toolchain_language (lang),
                                                    IDE_TOOLCHAIN_TOOL_CC,
                                                    exec_path);
      else
        ide_simple_toolchain_set_tool_for_language (IDE_SIMPLE_TOOLCHAIN(self),
                                                    IDE_TOOLCHAIN_LANGUAGE_ANY,
                                                    tool_id,
                                                    exec_path);
    }

  return TRUE;
}

/**
 * gbp_meson_toolchain_get_file_path:
 * @self: an #GbpMesonToolchain
 *
 * Gets the path to the Meson cross-file
 *
 * Returns: (transfer none): the path to the Meson cross-file.
 */
const gchar *
gbp_meson_toolchain_get_file_path (GbpMesonToolchain  *self)
{
  g_return_val_if_fail (GBP_IS_MESON_TOOLCHAIN (self), NULL);

  return self->file_path;
}

static void
gbp_meson_toolchain_finalize (GObject *object)
{
  GbpMesonToolchain *self = (GbpMesonToolchain *)object;

  g_clear_pointer (&self->file_path, g_free);

  G_OBJECT_CLASS (gbp_meson_toolchain_parent_class)->finalize (object);
}

static void
gbp_meson_toolchain_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GbpMesonToolchain *self = GBP_MESON_TOOLCHAIN (object);

  switch (prop_id)
    {
    case PROP_FILE_PATH:
      g_value_set_string (value, gbp_meson_toolchain_get_file_path (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_meson_toolchain_class_init (GbpMesonToolchainClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_meson_toolchain_finalize;
  object_class->get_property = gbp_meson_toolchain_get_property;

  properties [PROP_FILE_PATH] =
    g_param_spec_string ("file-path",
                         "File path",
                         "The path of the cross-file",
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_meson_toolchain_init (GbpMesonToolchain *self)
{
}
