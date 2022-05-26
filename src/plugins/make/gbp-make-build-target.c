/* gbp-make-build-target.c
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

#define G_LOG_DOMAIN "gbp-make-build-target"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-foundry.h>

#include "gbp-make-build-target.h"

struct _GbpMakeBuildTarget
{
  IdeObject parent_instance;
  char *name;
};

static GFile *
gbp_make_build_target_get_install_directory (IdeBuildTarget *build_target)
{
  return NULL;
}

static char **
gbp_make_build_target_get_argv (IdeBuildTarget *build_target)
{
  return NULL;
}

static char *
gbp_make_build_target_get_display_name (IdeBuildTarget *build_target)
{
  GbpMakeBuildTarget *self = (GbpMakeBuildTarget *)build_target;

  if (ide_str_empty0 (self->name))
    return g_strdup (_("Default Make Target"));

  return g_strdup (self->name);
}

static char *
gbp_make_build_target_get_name (IdeBuildTarget *build_target)
{
  GbpMakeBuildTarget *self = (GbpMakeBuildTarget *)build_target;

  return g_strdup_printf ("make:%s", self->name ? self->name : "");
}

static IdeArtifactKind
gbp_make_build_target_get_kind (IdeBuildTarget *build_target)
{
  return IDE_ARTIFACT_KIND_NONE;
}

static int
gbp_make_build_target_get_priority (IdeBuildTarget *build_target)
{
  return 0;
}

static void
build_target_iface_init (IdeBuildTargetInterface *iface)
{
  iface->get_argv = gbp_make_build_target_get_argv;
  iface->get_display_name = gbp_make_build_target_get_display_name;
  iface->get_install_directory = gbp_make_build_target_get_install_directory;
  iface->get_kind = gbp_make_build_target_get_kind;
  iface->get_name = gbp_make_build_target_get_name;
  iface->get_priority = gbp_make_build_target_get_priority;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpMakeBuildTarget, gbp_make_build_target, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_BUILD_TARGET, build_target_iface_init))

static void
gbp_make_build_target_finalize (GObject *object)
{
  GbpMakeBuildTarget *self = (GbpMakeBuildTarget *)object;

  g_clear_pointer (&self->name, g_free);

  G_OBJECT_CLASS (gbp_make_build_target_parent_class)->finalize (object);
}

static void
gbp_make_build_target_class_init (GbpMakeBuildTargetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_make_build_target_finalize;
}

static void
gbp_make_build_target_init (GbpMakeBuildTarget *self)
{
}

GbpMakeBuildTarget *
gbp_make_build_target_new (const char *name)
{
  GbpMakeBuildTarget *self;

  if (ide_str_empty0 (name))
    name = NULL;

  self = g_object_new (GBP_TYPE_MAKE_BUILD_TARGET, NULL);
  self->name = g_strdup (name);

  return self;
}

const char *
gbp_make_build_target_get_make_target (GbpMakeBuildTarget *self)
{
  g_return_val_if_fail (GBP_IS_MAKE_BUILD_TARGET (self), NULL);

  return self->name;
}
