/* gbp-flatpak-configuration.c
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

#define G_LOG_DOMAIN "gbp-flatpak-configuration"

#include "ide-debug.h"

#include "gbp-flatpak-configuration.h"
#include "buildsystem/ide-configuration.h"

struct _GbpFlatpakConfiguration
{
  IdeConfiguration parent_instance;

  GFile *manifest;
  gchar *primary_module;
};

G_DEFINE_TYPE (GbpFlatpakConfiguration, gbp_flatpak_configuration, IDE_TYPE_CONFIGURATION)

enum {
  PROP_0,
  PROP_MANIFEST,
  PROP_PRIMARY_MODULE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

GFile *gbp_flatpak_configuration_get_manifest (GbpFlatpakConfiguration *self)
{
  g_return_val_if_fail (GBP_IS_FLATPAK_CONFIGURATION (self), NULL);

  return self->manifest;
}

const gchar *
gbp_flatpak_configuration_get_primary_module (GbpFlatpakConfiguration *self)
{
  g_return_val_if_fail (GBP_IS_FLATPAK_CONFIGURATION (self), NULL);

  return self->primary_module;
}

void
gbp_flatpak_configuration_set_primary_module (GbpFlatpakConfiguration *self,
                                              const gchar *primary_module)
{
  g_return_if_fail (GBP_IS_FLATPAK_CONFIGURATION (self));

  g_free (self->primary_module);
  self->primary_module = g_strdup (primary_module);
}

static void
gbp_flatpak_configuration_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  GbpFlatpakConfiguration *self = GBP_FLATPAK_CONFIGURATION (object);

  switch (prop_id)
    {
    case PROP_MANIFEST:
      g_value_set_object (value, gbp_flatpak_configuration_get_manifest (self));
      break;

    case PROP_PRIMARY_MODULE:
      g_value_set_string (value, gbp_flatpak_configuration_get_primary_module (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
gbp_flatpak_configuration_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  GbpFlatpakConfiguration *self = GBP_FLATPAK_CONFIGURATION (object);

  switch (prop_id)
    {
    case PROP_MANIFEST:
      self->manifest = g_value_dup_object (value);
      break;

    case PROP_PRIMARY_MODULE:
      gbp_flatpak_configuration_set_primary_module (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
gbp_flatpak_configuration_finalize (GObject *object)
{
  GbpFlatpakConfiguration *self = (GbpFlatpakConfiguration *)object;

  g_clear_object (&self->manifest);
  g_clear_pointer (&self->primary_module, g_free);

  G_OBJECT_CLASS (gbp_flatpak_configuration_parent_class)->finalize (object);
}

static void
gbp_flatpak_configuration_class_init (GbpFlatpakConfigurationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_flatpak_configuration_finalize;
  object_class->get_property = gbp_flatpak_configuration_get_property;
  object_class->set_property = gbp_flatpak_configuration_set_property;

  properties [PROP_MANIFEST] =
    g_param_spec_object ("manifest",
                         "Manifest",
                         "Manifest file",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_PRIMARY_MODULE] =
    g_param_spec_string ("primary-module",
                         "Primary module",
                         "Primary module",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_flatpak_configuration_init (GbpFlatpakConfiguration *self)
{
}
