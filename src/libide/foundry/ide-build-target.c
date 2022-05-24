/* ide-build-target.c
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-build-target"

#include "config.h"

#include "ide-build-target.h"

G_DEFINE_INTERFACE (IdeBuildTarget, ide_build_target, IDE_TYPE_OBJECT)

static gchar*
ide_build_target_real_get_cwd (IdeBuildTarget *self)
{
  return NULL;
}

static gchar*
ide_build_target_real_get_language (IdeBuildTarget *self)
{
  return g_strdup ("asm");
}


static void
ide_build_target_default_init (IdeBuildTargetInterface *iface)
{
  iface->get_cwd = ide_build_target_real_get_cwd;
  iface->get_language = ide_build_target_real_get_language;
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
 * ide_build_target_get_install:
 * @self: an #IdeBuildTarget
 *
 * Checks if the #IdeBuildTarget gets installed.
 *
 * Returns: %TRUE if the build target is installed
 */
gboolean
ide_build_target_get_install (IdeBuildTarget *self)
{
  g_autoptr(GFile) dir = NULL;

  g_return_val_if_fail (IDE_IS_BUILD_TARGET (self), FALSE);

  if ((dir = ide_build_target_get_install_directory (self)))
    return TRUE;

  return FALSE;
}

/**
 * ide_build_target_get_display_name:
 *
 * Returns: (nullable) (transfer full): A display name for the build
 *   target to be displayed in UI. May contain pango markup.
 */
gchar *
ide_build_target_get_display_name (IdeBuildTarget *self)
{
  g_return_val_if_fail (IDE_IS_BUILD_TARGET (self), NULL);

  if (IDE_BUILD_TARGET_GET_IFACE (self)->get_display_name)
    return IDE_BUILD_TARGET_GET_IFACE (self)->get_display_name (self);
  else
    return ide_build_target_get_name (self);
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
 */
gint
ide_build_target_get_priority (IdeBuildTarget *self)
{
  g_return_val_if_fail (IDE_IS_BUILD_TARGET (self), 0);

  if (IDE_BUILD_TARGET_GET_IFACE (self)->get_priority)
    return IDE_BUILD_TARGET_GET_IFACE (self)->get_priority (self);
  return 0;
}

/**
 * ide_build_target_get_kind:
 * @self: a #IdeBuildTarget
 *
 * Gets the kind of artifact.
 *
 * Returns: an #IdeArtifactKind
 */
IdeArtifactKind
ide_build_target_get_kind (IdeBuildTarget *self)
{
  g_return_val_if_fail (IDE_IS_BUILD_TARGET (self), 0);

  if (IDE_BUILD_TARGET_GET_IFACE (self)->get_kind)
    return IDE_BUILD_TARGET_GET_IFACE (self)->get_kind (self);

  return IDE_ARTIFACT_KIND_NONE;
}

gint
ide_build_target_compare (const IdeBuildTarget *left,
                          const IdeBuildTarget *right)
{
  return ide_build_target_get_priority ((IdeBuildTarget *)left) -
         ide_build_target_get_priority ((IdeBuildTarget *)right);
}

/**
 * ide_build_target_get_argv:
 * @self: a #IdeBuildTarget
 *
 * Gets the arguments used to run the target.
 *
 * Returns: (transfer full): A #GStrv containing the arguments to
 *   run the target.
 */
gchar **
ide_build_target_get_argv (IdeBuildTarget *self)
{
  g_auto(GStrv) argv = NULL;

  g_return_val_if_fail (IDE_IS_BUILD_TARGET (self), NULL);

  if (IDE_BUILD_TARGET_GET_IFACE (self)->get_argv)
    argv = IDE_BUILD_TARGET_GET_IFACE (self)->get_argv (self);

  if (argv == NULL || *argv == NULL)
    {
      g_autofree gchar *name = ide_build_target_get_name (self);
      g_autoptr(GFile) dir = ide_build_target_get_install_directory (self);

      g_clear_pointer (&argv, g_strfreev);

      if (!g_path_is_absolute (name) && dir != NULL && g_file_is_native (dir))
        {
          g_autofree gchar *tmp = g_steal_pointer (&name);
          g_autoptr(GFile) child = g_file_get_child (dir, tmp);

          name = g_file_get_path (child);
        }

      argv = g_new (gchar *, 2);
      argv[0] = g_steal_pointer (&name);
      argv[1] = NULL;
    }

  return g_steal_pointer (&argv);
}

/**
 * ide_build_target_get_cwd:
 * @self: a #IdeBuildTarget
 *
 * For build systems and build target providers that insist to be run in
 * a specific place, this method gets the correct working directory.
 *
 * If this method returns %NULL, the runtime will pick a default working
 * directory for the spawned process (usually, the user home directory
 * in the host system, or the flatpak sandbox home under flatpak).
 *
 * Returns: (nullable) (transfer full): the working directory to use for this target
 */
gchar *
ide_build_target_get_cwd (IdeBuildTarget *self)
{
  g_return_val_if_fail (IDE_IS_BUILD_TARGET (self), NULL);

  return IDE_BUILD_TARGET_GET_IFACE (self)->get_cwd (self);
}

/**
 * ide_build_target_get_language:
 * @self: a #IdeBuildTarget
 *
 * Return the main programming language that was used to
 * write this build target.
 *
 * This method is primarily used to choose an appropriate
 * debugger. Therefore, if a build target is composed of
 * components in multiple language (eg. a GJS app with
 * GObject Introspection libraries, or a Java app with JNI
 * libraries), this should return the language that is
 * most likely to be appropriate for debugging.
 *
 * The default implementation returns "asm", which indicates
 * an unspecified language that compiles to native code.
 *
 * Returns: (transfer full): the programming language of this target
 */
gchar *
ide_build_target_get_language (IdeBuildTarget *self)
{
  g_return_val_if_fail (IDE_IS_BUILD_TARGET (self), NULL);

  return IDE_BUILD_TARGET_GET_IFACE (self)->get_language (self);
}
