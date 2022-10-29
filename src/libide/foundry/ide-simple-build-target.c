/* ide-simple-build-target.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-simple-build-target"

#include "config.h"

#include "ide-build-target.h"
#include "ide-simple-build-target.h"

typedef struct
{
  GFile *install_directory;
  gchar *name;
  gchar **argv;
  gchar *cwd;
  gchar *language;
  gint priority;
} IdeSimpleBuildTargetPrivate;

static void build_target_iface_init (IdeBuildTargetInterface *iface);

G_DEFINE_TYPE_WITH_CODE (IdeSimpleBuildTarget, ide_simple_build_target, IDE_TYPE_OBJECT,
                         G_ADD_PRIVATE (IdeSimpleBuildTarget)
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_BUILD_TARGET,
                                                build_target_iface_init))

static void
ide_simple_build_target_finalize (GObject *object)
{
  IdeSimpleBuildTarget *self = (IdeSimpleBuildTarget *)object;
  IdeSimpleBuildTargetPrivate *priv = ide_simple_build_target_get_instance_private (self);

  g_clear_object (&priv->install_directory);
  g_clear_pointer (&priv->name, g_free);
  g_clear_pointer (&priv->argv, g_strfreev);
  g_clear_pointer (&priv->cwd, g_free);
  g_clear_pointer (&priv->language, g_free);

  G_OBJECT_CLASS (ide_simple_build_target_parent_class)->finalize (object);
}

static void
ide_simple_build_target_class_init (IdeSimpleBuildTargetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_simple_build_target_finalize;
}

static void
ide_simple_build_target_init (IdeSimpleBuildTarget *self)
{
}

IdeSimpleBuildTarget *
ide_simple_build_target_new (IdeContext *context)
{
  g_return_val_if_fail (!context || IDE_IS_CONTEXT (context), NULL);

  return g_object_new (IDE_TYPE_SIMPLE_BUILD_TARGET,
                       "parent", context,
                       NULL);
}

void
ide_simple_build_target_set_install_directory (IdeSimpleBuildTarget *self,
                                               GFile                *install_directory)
{
  IdeSimpleBuildTargetPrivate *priv = ide_simple_build_target_get_instance_private (self);

  g_return_if_fail (IDE_IS_SIMPLE_BUILD_TARGET (self));
  g_return_if_fail (!install_directory || G_IS_FILE (install_directory));

  g_set_object (&priv->install_directory, install_directory);
}

void
ide_simple_build_target_set_name (IdeSimpleBuildTarget *self,
                                  const gchar          *name)
{
  IdeSimpleBuildTargetPrivate *priv = ide_simple_build_target_get_instance_private (self);

  g_return_if_fail (IDE_IS_SIMPLE_BUILD_TARGET (self));

  g_set_str (&priv->name, name);
}

void
ide_simple_build_target_set_priority (IdeSimpleBuildTarget *self,
                                      gint                  priority)
{
  IdeSimpleBuildTargetPrivate *priv = ide_simple_build_target_get_instance_private (self);

  g_return_if_fail (IDE_IS_SIMPLE_BUILD_TARGET (self));

  priv->priority = priority;
}

void
ide_simple_build_target_set_argv (IdeSimpleBuildTarget *self,
                                  const gchar * const  *argv)
{
  IdeSimpleBuildTargetPrivate *priv = ide_simple_build_target_get_instance_private (self);

  g_return_if_fail (IDE_IS_SIMPLE_BUILD_TARGET (self));

  if (priv->argv != (gchar **)argv)
    {
      g_strfreev (priv->argv);
      priv->argv = g_strdupv ((gchar **)argv);
    }
}

void
ide_simple_build_target_set_cwd (IdeSimpleBuildTarget *self,
                                 const gchar          *cwd)
{
  IdeSimpleBuildTargetPrivate *priv = ide_simple_build_target_get_instance_private (self);

  g_return_if_fail (IDE_IS_SIMPLE_BUILD_TARGET (self));

  g_set_str (&priv->cwd, cwd);
}

void
ide_simple_build_target_set_language (IdeSimpleBuildTarget *self,
                                      const gchar          *language)
{
  IdeSimpleBuildTargetPrivate *priv = ide_simple_build_target_get_instance_private (self);

  g_return_if_fail (IDE_IS_SIMPLE_BUILD_TARGET (self));

  g_set_str (&priv->language, language);
}

static GFile *
get_install_directory (IdeBuildTarget *target)
{
  IdeSimpleBuildTarget *self = IDE_SIMPLE_BUILD_TARGET (target);
  IdeSimpleBuildTargetPrivate *priv = ide_simple_build_target_get_instance_private (self);
  return priv->install_directory ? g_object_ref (priv->install_directory) : NULL;
}

static gchar *
get_name (IdeBuildTarget *target)
{
  IdeSimpleBuildTarget *self = IDE_SIMPLE_BUILD_TARGET (target);
  IdeSimpleBuildTargetPrivate *priv = ide_simple_build_target_get_instance_private (self);
  return g_strdup (priv->name);
}

static gchar **
get_argv (IdeBuildTarget *target)
{
  IdeSimpleBuildTarget *self = IDE_SIMPLE_BUILD_TARGET (target);
  IdeSimpleBuildTargetPrivate *priv = ide_simple_build_target_get_instance_private (self);
  return g_strdupv (priv->argv);
}

static gchar *
get_cwd (IdeBuildTarget *target)
{
  IdeSimpleBuildTarget *self = IDE_SIMPLE_BUILD_TARGET (target);
  IdeSimpleBuildTargetPrivate *priv = ide_simple_build_target_get_instance_private (self);
  return g_strdup (priv->cwd);
}

static gchar *
get_language (IdeBuildTarget *target)
{
  IdeSimpleBuildTarget *self = IDE_SIMPLE_BUILD_TARGET (target);
  IdeSimpleBuildTargetPrivate *priv = ide_simple_build_target_get_instance_private (self);
  return g_strdup (priv->language);
}

static gint
get_priority (IdeBuildTarget *target)
{
  IdeSimpleBuildTarget *self = IDE_SIMPLE_BUILD_TARGET (target);
  IdeSimpleBuildTargetPrivate *priv = ide_simple_build_target_get_instance_private (self);
  return priv->priority;
}

static void
build_target_iface_init (IdeBuildTargetInterface *iface)
{
  iface->get_install_directory = get_install_directory;
  iface->get_name = get_name;
  iface->get_priority = get_priority;
  iface->get_argv = get_argv;
  iface->get_cwd = get_cwd;
  iface->get_language = get_language;
}
