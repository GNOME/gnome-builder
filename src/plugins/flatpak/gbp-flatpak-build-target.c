/* gbp-flatpak-build-target.c
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

#define G_LOG_DOMAIN "gbp-flatpak-build-target"

#include "gbp-flatpak-build-target.h"

struct _GbpFlatpakBuildTarget
{
  IdeObject parent;

  gchar *command;
};

enum {
  PROP_0,
  PROP_COMMAND,
  N_PROPS
};

static gchar *
gbp_flatpak_build_target_get_display_name (IdeBuildTarget *build_target)
{
  GbpFlatpakBuildTarget *self = GBP_FLATPAK_BUILD_TARGET (build_target);

  return g_strdup_printf ("%s <span fgalpha='32767' size='smaller'>(Flatpak)</span>", self->command);
}

static gchar *
gbp_flatpak_build_target_get_name (IdeBuildTarget *build_target)
{
  GbpFlatpakBuildTarget *self = GBP_FLATPAK_BUILD_TARGET (build_target);

  return g_strdup (self->command);
}

static gchar **
gbp_flatpak_build_target_get_argv (IdeBuildTarget *build_target)
{
  GbpFlatpakBuildTarget *self = GBP_FLATPAK_BUILD_TARGET (build_target);
  gchar *argv[] = { self->command, NULL };

  return g_strdupv (argv);
}

static GFile *
gbp_flatpak_build_target_get_install_directory (IdeBuildTarget *build_target)
{
  GbpFlatpakBuildTarget *self = (GbpFlatpakBuildTarget *)build_target;

  g_assert (GBP_IS_FLATPAK_BUILD_TARGET (self));

  if (!g_path_is_absolute (self->command))
    return g_file_new_for_path ("/app/bin");

  return NULL;
}

static gint
gbp_flatpak_build_target_get_priority (IdeBuildTarget *build_target)
{
  return -100;
}

static void
build_target_iface_init (IdeBuildTargetInterface *iface)
{
  iface->get_display_name = gbp_flatpak_build_target_get_display_name;
  iface->get_name = gbp_flatpak_build_target_get_name;
  iface->get_argv = gbp_flatpak_build_target_get_argv;
  iface->get_install_directory = gbp_flatpak_build_target_get_install_directory;
  iface->get_priority = gbp_flatpak_build_target_get_priority;
}

G_DEFINE_TYPE_WITH_CODE (GbpFlatpakBuildTarget, gbp_flatpak_build_target, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_BUILD_TARGET, build_target_iface_init))

static GParamSpec *properties [N_PROPS];

static void
gbp_flatpak_build_target_finalize (GObject *object)
{
  GbpFlatpakBuildTarget *self = (GbpFlatpakBuildTarget *)object;

  g_clear_pointer (&self->command, g_free);

  G_OBJECT_CLASS (gbp_flatpak_build_target_parent_class)->finalize (object);
}

static void
gbp_flatpak_build_target_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  GbpFlatpakBuildTarget *self = GBP_FLATPAK_BUILD_TARGET (object);

  switch (prop_id)
    {
    case PROP_COMMAND:
      g_value_set_string (value, self->command);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_flatpak_build_target_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  GbpFlatpakBuildTarget *self = GBP_FLATPAK_BUILD_TARGET (object);

  switch (prop_id)
    {
    case PROP_COMMAND:
      self->command = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_flatpak_build_target_class_init (GbpFlatpakBuildTargetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_flatpak_build_target_finalize;
  object_class->get_property = gbp_flatpak_build_target_get_property;
  object_class->set_property = gbp_flatpak_build_target_set_property;

  properties [PROP_COMMAND] =
    g_param_spec_string ("command", NULL, NULL, NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_flatpak_build_target_init (GbpFlatpakBuildTarget *self)
{
}
