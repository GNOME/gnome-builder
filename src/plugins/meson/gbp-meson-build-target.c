/* gbp-meson-build-target.c
 *
 * Copyright © 2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-meson-build-target"

#include "gbp-meson-build-target.h"

struct _GbpMesonBuildTarget
{
  IdeObject  parent_instance;

  GFile     *install_directory;
  gchar     *name;
};

enum {
  PROP_0,
  PROP_INSTALL_DIRECTORY,
  PROP_NAME,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static GFile *
gbp_meson_build_target_get_install_directory (IdeBuildTarget *build_target)
{
  GbpMesonBuildTarget *self = (GbpMesonBuildTarget *)build_target;

  g_assert (GBP_IS_MESON_BUILD_TARGET (self));

  return self->install_directory ? g_object_ref (self->install_directory) : NULL;
}

static gchar *
gbp_meson_build_target_get_name (IdeBuildTarget *build_target)
{
  GbpMesonBuildTarget *self = (GbpMesonBuildTarget *)build_target;

  g_assert (GBP_IS_MESON_BUILD_TARGET (self));

  return self->name ? g_strdup (self->name) : NULL;
}

static void
build_target_iface_init (IdeBuildTargetInterface *iface)
{
  iface->get_install_directory = gbp_meson_build_target_get_install_directory;
  iface->get_name = gbp_meson_build_target_get_name;
}

G_DEFINE_TYPE_WITH_CODE (GbpMesonBuildTarget, gbp_meson_build_target, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_BUILD_TARGET, build_target_iface_init))

static void
gbp_meson_build_target_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  GbpMesonBuildTarget *self = GBP_MESON_BUILD_TARGET (object);

  switch (prop_id)
    {
    case PROP_INSTALL_DIRECTORY:
      g_value_set_object (value, self->install_directory);
      break;

    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_meson_build_target_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  GbpMesonBuildTarget *self = GBP_MESON_BUILD_TARGET (object);

  switch (prop_id)
    {
    case PROP_INSTALL_DIRECTORY:
      self->install_directory = g_value_dup_object (value);
      break;

    case PROP_NAME:
      self->name = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_meson_build_target_finalize (GObject *object)
{
  GbpMesonBuildTarget *self = (GbpMesonBuildTarget *)object;

  g_clear_object (&self->install_directory);
  g_clear_pointer (&self->name, g_free);

  G_OBJECT_CLASS (gbp_meson_build_target_parent_class)->finalize (object);
}

static void
gbp_meson_build_target_class_init (GbpMesonBuildTargetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_meson_build_target_finalize;
  object_class->get_property = gbp_meson_build_target_get_property;
  object_class->set_property = gbp_meson_build_target_set_property;

  properties [PROP_INSTALL_DIRECTORY] =
    g_param_spec_object ("install-directory",
                         NULL,
                         NULL,
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_NAME] =
    g_param_spec_string ("name",
                         NULL,
                         NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_meson_build_target_init (GbpMesonBuildTarget *self)
{
}

IdeBuildTarget *
gbp_meson_build_target_new (IdeContext *context,
                            GFile      *install_directory,
                            gchar      *name)
{
  GbpMesonBuildTarget *self;

  g_return_val_if_fail (!context || IDE_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (G_IS_FILE (install_directory), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  self = g_object_new (GBP_TYPE_MESON_BUILD_TARGET,
                       "context", context,
                       NULL);
  g_set_object (&self->install_directory, install_directory);
  self->name = g_strdup (name);

  return IDE_BUILD_TARGET (self);
}
