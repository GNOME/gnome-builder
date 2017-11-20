/* ide-build-target.c
 *
 * Copyright © 2016 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-build-target"

#include "buildsystem/ide-build-target.h"

G_DEFINE_INTERFACE (IdeBuildTarget, ide_build_target, IDE_TYPE_OBJECT)

static void
ide_build_target_default_init (IdeBuildTargetInterface *iface)
{
}

/**
 * ide_build_target_get_install_directory:
 *
 * Returns: (nullable) (transfer full): a #GFile or %NULL.
 */
GFile *
ide_build_target_get_install_directory (IdeBuildTarget *self)
{
  g_return_val_if_fail (IDE_IS_BUILD_TARGET (self), NULL);

  if (IDE_BUILD_TARGET_GET_IFACE (self)->get_install_directory)
    return IDE_BUILD_TARGET_GET_IFACE (self)->get_install_directory (self);

  return NULL;
}

/**
 * ide_build_target_get_name:
 *
 * Returns: (nullable) (transfer full): A filename or %NULL.
 */
gchar *
ide_build_target_get_name (IdeBuildTarget *self)
{
  g_return_val_if_fail (IDE_IS_BUILD_TARGET (self), NULL);

  if (IDE_BUILD_TARGET_GET_IFACE (self)->get_name)
    return IDE_BUILD_TARGET_GET_IFACE (self)->get_name (self);

  return NULL;
}

/**
 * ide_build_target_get_priority:
 * @self: an #IdeBuildTarget
 *
 * Gets the priority of the build target. This is used to sort build targets by
 * their importance. The lowest value (negative values are allowed) will be run
 * as the default run target by Builder.
 *
 * Returns: the priority of the build target
 *
 * Since: 3.28
 */
gint
ide_build_target_get_priority (IdeBuildTarget *self)
{
  g_return_val_if_fail (IDE_IS_BUILD_TARGET (self), 0);

  if (IDE_BUILD_TARGET_GET_IFACE (self)->get_priority)
    return IDE_BUILD_TARGET_GET_IFACE (self)->get_priority (self);
  return 0;
}

gint
ide_build_target_compare (const IdeBuildTarget *left,
                          const IdeBuildTarget *right)
{
  return ide_build_target_get_priority ((IdeBuildTarget *)left) -
         ide_build_target_get_priority ((IdeBuildTarget *)right);
}
