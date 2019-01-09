/* ide-buildconfig-configuration.c
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

#define G_LOG_DOMAIN "ide-buildconfig-configuration"

#include "config.h"

#include "ide-buildconfig-configuration.h"

struct _IdeBuildconfigConfiguration
{
  IdeConfiguration   parent_instance;

  gchar            **prebuild;
  gchar            **postbuild;
};

enum {
  PROP_0,
  PROP_PREBUILD,
  PROP_POSTBUILD,
  N_PROPS
};

G_DEFINE_TYPE (IdeBuildconfigConfiguration, ide_buildconfig_configuration, IDE_TYPE_CONFIGURATION)

static GParamSpec *properties [N_PROPS];

static void
ide_buildconfig_configuration_finalize (GObject *object)
{
  IdeBuildconfigConfiguration *self = (IdeBuildconfigConfiguration *)object;

  g_clear_pointer (&self->prebuild, g_strfreev);
  g_clear_pointer (&self->postbuild, g_strfreev);

  G_OBJECT_CLASS (ide_buildconfig_configuration_parent_class)->finalize (object);
}

static void
ide_buildconfig_configuration_get_property (GObject    *object,
                                            guint       prop_id,
                                            GValue     *value,
                                            GParamSpec *pspec)
{
  IdeBuildconfigConfiguration *self = (IdeBuildconfigConfiguration *)object;

  switch (prop_id)
    {
    case PROP_PREBUILD:
      g_value_set_boxed (value, ide_buildconfig_configuration_get_prebuild (self));
      break;

    case PROP_POSTBUILD:
      g_value_set_boxed (value, ide_buildconfig_configuration_get_postbuild (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_buildconfig_configuration_set_property (GObject      *object,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec)
{
  IdeBuildconfigConfiguration *self = (IdeBuildconfigConfiguration *)object;

  switch (prop_id)
    {
    case PROP_PREBUILD:
      ide_buildconfig_configuration_set_prebuild (self, g_value_get_boxed (value));
      break;

    case PROP_POSTBUILD:
      ide_buildconfig_configuration_set_postbuild (self, g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_buildconfig_configuration_class_init (IdeBuildconfigConfigurationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_buildconfig_configuration_finalize;
  object_class->get_property = ide_buildconfig_configuration_get_property;
  object_class->set_property = ide_buildconfig_configuration_set_property;

  properties [PROP_PREBUILD] =
    g_param_spec_boxed ("prebuild", NULL, NULL,
                        G_TYPE_STRV,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties [PROP_POSTBUILD] =
    g_param_spec_boxed ("postbuild", NULL, NULL,
                        G_TYPE_STRV,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_buildconfig_configuration_init (IdeBuildconfigConfiguration *self)
{
}

const gchar * const *
ide_buildconfig_configuration_get_prebuild (IdeBuildconfigConfiguration *self)
{
  g_return_val_if_fail (IDE_IS_BUILDCONFIG_CONFIGURATION (self), NULL);

  return (const gchar * const *)self->prebuild;
}

const gchar * const *
ide_buildconfig_configuration_get_postbuild (IdeBuildconfigConfiguration *self)
{
  g_return_val_if_fail (IDE_IS_BUILDCONFIG_CONFIGURATION (self), NULL);

  return (const gchar * const *)self->postbuild;
}

void
ide_buildconfig_configuration_set_prebuild (IdeBuildconfigConfiguration *self,
                                            const gchar * const         *prebuild)
{
  g_return_if_fail (IDE_IS_BUILDCONFIG_CONFIGURATION (self));

  if (self->prebuild != (gchar **)prebuild)
    {
      g_strfreev (self->prebuild);
      self->prebuild = g_strdupv ((gchar **)prebuild);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PREBUILD]);
    }
}

void
ide_buildconfig_configuration_set_postbuild (IdeBuildconfigConfiguration *self,
                                             const gchar * const         *postbuild)
{
  g_return_if_fail (IDE_IS_BUILDCONFIG_CONFIGURATION (self));

  if (self->postbuild != (gchar **)postbuild)
    {
      g_strfreev (self->postbuild);
      self->postbuild = g_strdupv ((gchar **)postbuild);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_POSTBUILD]);
    }
}
