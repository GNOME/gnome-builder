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

#include "gbp-flatpak-configuration.h"
#include "gbp-flatpak-runtime.h"

struct _GbpFlatpakConfiguration
{
  IdeConfiguration parent_instance;

  gchar  *branch;
  gchar  *command;
  gchar **finish_args;
  GFile  *manifest;
  gchar  *platform;
  gchar  *primary_module;
  gchar  *sdk;
};

G_DEFINE_TYPE (GbpFlatpakConfiguration, gbp_flatpak_configuration, IDE_TYPE_CONFIGURATION)

enum {
  PROP_0,
  PROP_BRANCH,
  PROP_COMMAND,
  PROP_FINISH_ARGS,
  PROP_MANIFEST,
  PROP_PLATFORM,
  PROP_PRIMARY_MODULE,
  PROP_SDK,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

const gchar *
gbp_flatpak_configuration_get_branch (GbpFlatpakConfiguration *self)
{
  g_return_val_if_fail (GBP_IS_FLATPAK_CONFIGURATION (self), NULL);

  return self->branch;
}

void
gbp_flatpak_configuration_set_branch (GbpFlatpakConfiguration *self,
                                      const gchar             *branch)
{
  g_return_if_fail (GBP_IS_FLATPAK_CONFIGURATION (self));

  g_free (self->branch);
  self->branch = g_strdup (branch);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BRANCH]);
}

const gchar *
gbp_flatpak_configuration_get_command (GbpFlatpakConfiguration *self)
{
  g_return_val_if_fail (GBP_IS_FLATPAK_CONFIGURATION (self), NULL);

  return self->command;
}

void
gbp_flatpak_configuration_set_command (GbpFlatpakConfiguration *self,
                                       const gchar             *command)
{
  g_return_if_fail (GBP_IS_FLATPAK_CONFIGURATION (self));

  g_free (self->command);
  self->command = g_strdup (command);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_COMMAND]);
}

const gchar * const *
gbp_flatpak_configuration_get_finish_args (GbpFlatpakConfiguration *self)
{
  g_return_val_if_fail (GBP_IS_FLATPAK_CONFIGURATION (self), NULL);

  return (const gchar * const *)self->finish_args;
}

void
gbp_flatpak_configuration_set_finish_args (GbpFlatpakConfiguration *self,
                                           const gchar * const     *finish_args)
{
  g_return_if_fail (GBP_IS_FLATPAK_CONFIGURATION (self));

  if (self->finish_args != (gchar **)finish_args)
    {
      g_strfreev (self->finish_args);
      self->finish_args = g_strdupv ((gchar **)finish_args);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_FINISH_ARGS]);
    }
}

gchar *
gbp_flatpak_configuration_get_manifest_path (GbpFlatpakConfiguration *self)
{
  g_return_val_if_fail (GBP_IS_FLATPAK_CONFIGURATION (self), NULL);

  if (self->manifest != NULL)
    return g_file_get_path (self->manifest);

  return NULL;
}

GFile *
gbp_flatpak_configuration_get_manifest (GbpFlatpakConfiguration *self)
{
  g_return_val_if_fail (GBP_IS_FLATPAK_CONFIGURATION (self), NULL);

  return self->manifest;
}

const gchar *
gbp_flatpak_configuration_get_platform (GbpFlatpakConfiguration *self)
{
  g_return_val_if_fail (GBP_IS_FLATPAK_CONFIGURATION (self), NULL);

  return self->platform;
}

void
gbp_flatpak_configuration_set_platform (GbpFlatpakConfiguration *self,
                                        const gchar             *platform)
{
  g_return_if_fail (GBP_IS_FLATPAK_CONFIGURATION (self));

  g_free (self->platform);
  self->platform = g_strdup (platform);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PLATFORM]);
}

const gchar *
gbp_flatpak_configuration_get_primary_module (GbpFlatpakConfiguration *self)
{
  g_return_val_if_fail (GBP_IS_FLATPAK_CONFIGURATION (self), NULL);

  return self->primary_module;
}

void
gbp_flatpak_configuration_set_primary_module (GbpFlatpakConfiguration *self,
                                              const gchar             *primary_module)
{
  g_return_if_fail (GBP_IS_FLATPAK_CONFIGURATION (self));

  g_free (self->primary_module);
  self->primary_module = g_strdup (primary_module);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PRIMARY_MODULE]);
}

const gchar *
gbp_flatpak_configuration_get_sdk (GbpFlatpakConfiguration *self)
{
  g_return_val_if_fail (GBP_IS_FLATPAK_CONFIGURATION (self), NULL);

  return self->sdk;
}

void
gbp_flatpak_configuration_set_sdk (GbpFlatpakConfiguration *self,
                                   const gchar             *sdk)
{
  g_return_if_fail (GBP_IS_FLATPAK_CONFIGURATION (self));

  if (g_strcmp0 (self->sdk, sdk) != 0)
    {
      g_free (self->sdk);
      self->sdk = g_strdup (sdk);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SDK]);
    }
}

static gboolean
gbp_flatpak_configuration_supports_runtime (IdeConfiguration *configuration,
                                            IdeRuntime       *runtime)
{
  g_assert (GBP_IS_FLATPAK_CONFIGURATION (configuration));
  g_assert (IDE_IS_RUNTIME (runtime));

  return GBP_IS_FLATPAK_RUNTIME (runtime);
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
    case PROP_BRANCH:
      g_value_set_string (value, gbp_flatpak_configuration_get_branch (self));
      break;

    case PROP_COMMAND:
      g_value_set_string (value, gbp_flatpak_configuration_get_command (self));
      break;

    case PROP_FINISH_ARGS:
      g_value_set_boxed (value, gbp_flatpak_configuration_get_finish_args (self));
      break;

    case PROP_MANIFEST:
      g_value_set_object (value, gbp_flatpak_configuration_get_manifest (self));
      break;

    case PROP_PLATFORM:
      g_value_set_string (value, gbp_flatpak_configuration_get_platform (self));
      break;

    case PROP_PRIMARY_MODULE:
      g_value_set_string (value, gbp_flatpak_configuration_get_primary_module (self));
      break;

    case PROP_SDK:
      g_value_set_string (value, gbp_flatpak_configuration_get_sdk (self));
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
    case PROP_BRANCH:
      gbp_flatpak_configuration_set_branch (self, g_value_get_string (value));
      break;

    case PROP_COMMAND:
      gbp_flatpak_configuration_set_command (self, g_value_get_string (value));
      break;

    case PROP_FINISH_ARGS:
      gbp_flatpak_configuration_set_finish_args (self, g_value_get_boxed (value));
      break;

    case PROP_MANIFEST:
      self->manifest = g_value_dup_object (value);
      break;

    case PROP_PLATFORM:
      gbp_flatpak_configuration_set_platform (self, g_value_get_string (value));
      break;

    case PROP_PRIMARY_MODULE:
      gbp_flatpak_configuration_set_primary_module (self, g_value_get_string (value));
      break;

    case PROP_SDK:
      gbp_flatpak_configuration_set_sdk (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
gbp_flatpak_configuration_finalize (GObject *object)
{
  GbpFlatpakConfiguration *self = (GbpFlatpakConfiguration *)object;

  g_clear_pointer (&self->branch, g_free);
  g_clear_pointer (&self->command, g_free);
  g_clear_pointer (&self->finish_args, g_strfreev);
  g_clear_object (&self->manifest);
  g_clear_pointer (&self->platform, g_free);
  g_clear_pointer (&self->primary_module, g_free);
  g_clear_pointer (&self->sdk, g_free);

  G_OBJECT_CLASS (gbp_flatpak_configuration_parent_class)->finalize (object);
}

static void
gbp_flatpak_configuration_class_init (GbpFlatpakConfigurationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeConfigurationClass *config_class = IDE_CONFIGURATION_CLASS (klass);

  object_class->finalize = gbp_flatpak_configuration_finalize;
  object_class->get_property = gbp_flatpak_configuration_get_property;
  object_class->set_property = gbp_flatpak_configuration_set_property;

  config_class->supports_runtime = gbp_flatpak_configuration_supports_runtime;

  properties [PROP_BRANCH] =
    g_param_spec_string ("branch",
                         "Branch",
                         "Branch",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_COMMAND] =
    g_param_spec_string ("command",
                         "Command",
                         "Command",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_FINISH_ARGS] =
    g_param_spec_boxed ("finish-args",
                        "Finish args",
                        "Finish args",
                        G_TYPE_STRV,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT |
                         G_PARAM_STATIC_STRINGS));

  properties [PROP_MANIFEST] =
    g_param_spec_object ("manifest",
                         "Manifest",
                         "Manifest file",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_PLATFORM] =
    g_param_spec_string ("platform",
                         "Platform",
                         "Platform",
                         NULL,
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

  properties [PROP_SDK] =
    g_param_spec_string ("sdk",
                         "Sdk",
                         "Sdk",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_flatpak_configuration_init (GbpFlatpakConfiguration *self)
{
  ide_configuration_set_prefix (IDE_CONFIGURATION (self), "/app");
}
