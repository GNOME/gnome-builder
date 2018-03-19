/* gbp-meson-toolchain.c
 *
 * Copyright 2018 Corentin Noël <corentin.noel@collabora.com>
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
 */

#define G_LOG_DOMAIN "gbp-meson-toolchain"

#include "gbp-meson-toolchain.h"

struct _GbpMesonToolchain
{
  IdeToolchain            parent_instance;
  gchar                  *file_path;
};

G_DEFINE_TYPE (GbpMesonToolchain, gbp_meson_toolchain, IDE_TYPE_TOOLCHAIN)

enum {
  PROP_0,
  PROP_FILE_PATH,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

GbpMesonToolchain *
gbp_meson_toolchain_new (IdeContext   *context,
                         GFile        *file)
{
  g_autofree gchar *path = g_file_get_path (file);
  g_autofree gchar *id = g_strconcat ("meson:", path, NULL);
  g_autofree gchar *arch = NULL;
  g_autoptr(GKeyFile) keyfile = g_key_file_new ();
  g_autoptr(GError) error = NULL;

  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  if (g_key_file_load_from_file (keyfile, path, G_KEY_FILE_NONE, &error))
    {
      g_autoptr(GError) read_error = NULL;
      g_autofree gchar *read_result = NULL;

      arch = g_key_file_get_string (keyfile, "host_machine", "cpu_family", &read_error);
      /* We need to remove leading and trailing aportrophe */
      if (arch != NULL)
        arch = g_utf8_substring (arch, 1, g_utf8_strlen (arch, -1) - 1);
    }
  else
    return NULL;

  return g_object_new (GBP_TYPE_MESON_TOOLCHAIN,
                       "context", context,
                       "file-path", path,
                       "id", id,
                       "host-architecture", arch,
                       NULL);
}

/**
 * ide_toolchain_get_id:
 * @self: an #IdeToolchain
 *
 * Gets the internal identifier of the toolchain
 *
 * Returns: (transfer none): the unique identifier.
 *
 * Since: 3.30
 */
const gchar *
gbp_meson_toolchain_get_file_path (GbpMesonToolchain  *self)
{
  g_return_val_if_fail (GBP_IS_MESON_TOOLCHAIN (self), NULL);

  return self->file_path;
}

void
gbp_meson_toolchain_set_file_path (GbpMesonToolchain  *self,
                                   const gchar        *file_path)
{
  g_return_if_fail (GBP_IS_MESON_TOOLCHAIN (self));
  g_return_if_fail (file_path != NULL);

  if (g_strcmp0 (file_path, self->file_path) != 0)
    {
      g_free (self->file_path);
      self->file_path = g_strdup (file_path);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_FILE_PATH]);
    }
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
gbp_meson_toolchain_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GbpMesonToolchain *self = GBP_MESON_TOOLCHAIN (object);

  switch (prop_id)
    {
    case PROP_FILE_PATH:
      gbp_meson_toolchain_set_file_path (self, g_value_get_string (value));
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
  object_class->set_property = gbp_meson_toolchain_set_property;

  properties [PROP_FILE_PATH] =
    g_param_spec_string ("file-path",
                         "File path",
                         "The path of the cross-file",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_meson_toolchain_init (GbpMesonToolchain *self)
{
  
}
