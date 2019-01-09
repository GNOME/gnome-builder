/* gbp-sysroot-manager.c
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "gbp-sysroot-manager"
#define BASIC_LIBDIRS "/usr/lib/pkgconfig:/usr/share/pkgconfig"

#include "gbp-sysroot-manager.h"

struct _GbpSysrootManager
{
  GObject parent_instance;
  GKeyFile *key_file;
};

G_DEFINE_TYPE (GbpSysrootManager, gbp_sysroot_manager, G_TYPE_OBJECT)

enum {
  TARGET_MODIFIED,
  TARGET_NAME_CHANGED,
  TARGET_ARCH_CHANGED,
  N_SIGNALS
};

static guint signals [N_SIGNALS];

static gchar *
sysroot_manager_get_path (void)
{
  g_autofree gchar *directory_path = NULL;
  g_autofree gchar *conf_file = NULL;

  directory_path = g_build_filename (g_get_user_config_dir (),
                                     ide_get_program_name (),
                                     "sysroot",
                                     NULL);

  g_mkdir_with_parents (directory_path, 0750);
  conf_file = g_build_filename (directory_path, "general.conf", NULL);
  return g_steal_pointer (&conf_file);
}

/**
 * sysroot_manager_find_additional_pkgconfig_paths:
 *
 * Returns a colon-separated list of additional pkgconfig paths
 *
 * Returns: (transfer full) (nullable): additional guessed paths
 */
static gchar *
sysroot_manager_find_additional_pkgconfig_paths (GbpSysrootManager *self,
                                                 const gchar       *target)
{
  g_autofree gchar *path = NULL;
  g_autofree gchar *lib64_path = NULL;
  g_autofree gchar *target_arch = NULL;
  g_autofree gchar *libmultiarch_path = NULL;
  g_autofree gchar *returned_paths = NULL;

  g_assert (GBP_IS_SYSROOT_MANAGER (self));
  g_assert (self->key_file != NULL);
  g_assert (target != NULL);

  path = gbp_sysroot_manager_get_target_path (self, target);
  lib64_path = g_build_filename (path, "usr", "lib64", "pkgconfig", NULL);
  target_arch = gbp_sysroot_manager_get_target_arch (self, target);
  libmultiarch_path = g_build_filename (path, "usr", "lib", target_arch, "pkgconfig", NULL);

  if (g_file_test (lib64_path, G_FILE_TEST_EXISTS))
    returned_paths = g_steal_pointer (&lib64_path);

  if (g_file_test (libmultiarch_path, G_FILE_TEST_EXISTS))
    {
      g_autofree gchar *previous_returned_path = g_steal_pointer (&returned_paths);
      returned_paths = g_strjoin (":", libmultiarch_path, previous_returned_path, NULL);
    }

  return g_strdup (returned_paths);
}

static void
sysroot_manager_save (GbpSysrootManager *self)
{
  g_autofree gchar *conf_file = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (GBP_IS_SYSROOT_MANAGER (self));
  g_assert (self->key_file != NULL);

  conf_file = sysroot_manager_get_path ();

  if (!g_key_file_save_to_file (self->key_file, conf_file, &error))
    g_warning ("Error saving the sysroot configuration: %s", error->message);
}

/**
 * gbp_sysroot_manager_get_default:
 *
 * Returns the default #GbpSysrootManager instance.
 *
 * Returns: (transfer none): the common sysroot manager
 */
GbpSysrootManager *
gbp_sysroot_manager_get_default (void)
{
  static GbpSysrootManager *instance;

  /* TODO: This needs to be attached to the IdeContext somehow, as this is
   *       not ideal when two contexts are loaded and sharing occurs.
   */

  if (instance == NULL)
    {
      instance = g_object_new (GBP_TYPE_SYSROOT_MANAGER, NULL);
      g_object_add_weak_pointer (G_OBJECT (instance), (gpointer *)&instance);
    }

  return instance;
}

/**
 * gbp_sysroot_manager_create_target:
 * @self: a #GbpSysrootManager
 *
 * This creates a new target and initializes its fields to the default parameters.
 *
 * Returns: (transfer full): the unique identifier of the new target
 */
gchar *
gbp_sysroot_manager_create_target (GbpSysrootManager *self)
{
  g_return_val_if_fail (GBP_IS_SYSROOT_MANAGER (self), NULL);
  g_return_val_if_fail (self->key_file != NULL, NULL);

  for (guint i = 0; i < UINT_MAX; i++)
    {
      gchar *result;
      g_autoptr(GString) sysroot_name = g_string_new (NULL);

      g_string_printf (sysroot_name, "Sysroot %u", i);
      result = sysroot_name->str;
      if (!g_key_file_has_group (self->key_file, result))
        {
          g_key_file_set_string (self->key_file, result, "Name", result);
          g_key_file_set_string (self->key_file, result, "Path", "/");
          sysroot_manager_save (self);
          g_signal_emit (self, signals[TARGET_MODIFIED], 0, result, GBP_SYSROOT_MANAGER_TARGET_CREATED);
          return g_string_free (g_steal_pointer (&sysroot_name), FALSE);
        }
    }

  return NULL;
}

void
gbp_sysroot_manager_remove_target (GbpSysrootManager *self,
                                   const gchar       *target)
{
  g_autoptr(GError) error = NULL;

  g_return_if_fail (GBP_IS_SYSROOT_MANAGER (self));
  g_return_if_fail (self->key_file != NULL);
  g_return_if_fail (target != NULL);

  if (!g_key_file_remove_group (self->key_file, target, &error))
    g_warning ("Error removing target \"%s\": %s", target, error->message);

  g_signal_emit (self, signals[TARGET_MODIFIED], 0, target, GBP_SYSROOT_MANAGER_TARGET_REMOVED);
  sysroot_manager_save (self);
}

/**
 * gbp_sysroot_manager_set_target_name:
 * @self: a #GbpSysrootManager
 * @target: the unique identifier of the target
 * @name: the displayable name of the target
 *
 * Sets the displayable name of the target.
 */
void
gbp_sysroot_manager_set_target_name (GbpSysrootManager *self,
                                     const gchar       *target,
                                     const gchar       *name)
{
  g_return_if_fail (GBP_IS_SYSROOT_MANAGER (self));
  g_return_if_fail (self->key_file != NULL);
  g_return_if_fail (target != NULL);

  g_key_file_set_string (self->key_file, target, "Name", name);
  g_signal_emit (self, signals[TARGET_MODIFIED], 0, target, GBP_SYSROOT_MANAGER_TARGET_CHANGED);
  g_signal_emit (self, signals[TARGET_NAME_CHANGED], 0, target, name);
  sysroot_manager_save (self);
}

/**
 * gbp_sysroot_manager_get_target_name:
 * @self: a #GbpSysrootManager
 * @target: the unique identifier of the target
 *
 * Gets the displayable name of the target.
 *
 * Returns: (transfer full): the name of the target to display.
 */
gchar *
gbp_sysroot_manager_get_target_name (GbpSysrootManager *self,
                                     const gchar       *target)
{
  g_return_val_if_fail (GBP_IS_SYSROOT_MANAGER (self), NULL);
  g_return_val_if_fail (self->key_file != NULL, NULL);
  g_return_val_if_fail (target != NULL, NULL);

  return g_key_file_get_string (self->key_file, target, "Name", NULL);
}

/**
 * gbp_sysroot_manager_set_target_arch:
 * @self: a #GbpSysrootManager
 * @target: the unique identifier of the target
 * @name: the architecture of the target
 *
 * Sets the architecture of the target.
 */
void
gbp_sysroot_manager_set_target_arch (GbpSysrootManager *self,
                                     const gchar       *target,
                                     const gchar       *arch)
{
  g_return_if_fail (GBP_IS_SYSROOT_MANAGER (self));
  g_return_if_fail (self->key_file != NULL);
  g_return_if_fail (target != NULL);

  g_key_file_set_string (self->key_file, target, "Arch", arch);
  g_signal_emit (self, signals[TARGET_MODIFIED], 0, target, GBP_SYSROOT_MANAGER_TARGET_CHANGED);
  g_signal_emit (self, signals[TARGET_ARCH_CHANGED], 0, target, arch);
  sysroot_manager_save (self);
}

/**
 * gbp_sysroot_manager_get_target_arch:
 * @self: a #GbpSysrootManager
 * @target: the unique identifier of the target
 *
 * Gets the architecture of the target.
 *
 * Returns: (transfer full): the architecture of the target.
 */
gchar *
gbp_sysroot_manager_get_target_arch (GbpSysrootManager *self,
                                     const gchar       *target)
{
  g_return_val_if_fail (GBP_IS_SYSROOT_MANAGER (self), NULL);
  g_return_val_if_fail (self->key_file != NULL, NULL);
  g_return_val_if_fail (target != NULL, NULL);

  return g_key_file_get_string (self->key_file, target, "Arch", NULL);
}

/**
 * gbp_sysroot_manager_set_target_path:
 * @self: a #GbpSysrootManager
 * @target: the unique identifier of the target
 * @path: the sysroot path of the target
 *
 * Sets the sysroot path of the target.
 */
void
gbp_sysroot_manager_set_target_path (GbpSysrootManager *self,
                                     const gchar       *target,
                                     const gchar       *path)
{
  g_autofree gchar *current_path = NULL;
  g_autofree gchar *current_pkgconfigs = NULL;

  g_return_if_fail (GBP_IS_SYSROOT_MANAGER (self));
  g_return_if_fail (self->key_file != NULL);
  g_return_if_fail (target != NULL);
  g_return_if_fail (path != NULL);

  current_path = gbp_sysroot_manager_get_target_path (self, target);
  g_key_file_set_string (self->key_file, target, "Path", path);
  g_signal_emit (self, signals[TARGET_MODIFIED], 0, target, GBP_SYSROOT_MANAGER_TARGET_CHANGED);
  sysroot_manager_save (self);

  current_pkgconfigs = gbp_sysroot_manager_get_target_pkg_config_path (self, target);
  if (current_pkgconfigs == NULL || g_strcmp0 (current_pkgconfigs, "") == 0)
    {
      g_auto(GStrv) path_parts = NULL;
      g_autofree gchar *additional_paths = NULL;
      g_autofree gchar *found_pkgconfigs = g_steal_pointer (&current_pkgconfigs);

      // Prepend the sysroot path to the BASIC_LIBDIRS values
      path_parts = g_strsplit (BASIC_LIBDIRS, ":", 0);
      for (gint i = g_strv_length (path_parts) - 1; i >= 0; i--)
        {
          g_autofree gchar *path_i = NULL;
          g_autofree gchar *previous_pkgconfigs = g_steal_pointer (&found_pkgconfigs);

          path_i = g_build_path (G_DIR_SEPARATOR_S, path, path_parts[i], NULL);
          found_pkgconfigs = g_strjoin (":", path_i, previous_pkgconfigs, NULL);
        }

      additional_paths = sysroot_manager_find_additional_pkgconfig_paths (self, target);
      current_pkgconfigs = g_strjoin (":", found_pkgconfigs, additional_paths, NULL);

      gbp_sysroot_manager_set_target_pkg_config_path (self, target, current_pkgconfigs);
    }
  else
    {
      g_autoptr(GError) regex_error = NULL;
      g_autoptr(GRegex) regex = NULL;
      g_autofree gchar *current_path_escaped = NULL;

      current_path_escaped = g_regex_escape_string (current_path, -1);
      regex = g_regex_new (current_path_escaped, 0, 0, &regex_error);
      if (regex_error == NULL)
        {
          g_autofree gchar *previous_pkgconfigs = g_steal_pointer (&current_pkgconfigs);

          current_pkgconfigs = g_regex_replace_literal (regex, previous_pkgconfigs, (gssize) -1, 0, path, 0, &regex_error);
          if (regex_error == NULL)
            gbp_sysroot_manager_set_target_pkg_config_path (self, target, current_pkgconfigs);
          else
            g_warning ("Regex error: %s", regex_error->message);
        }
      else
        g_warning ("Regex error: %s", regex_error->message);
    }
}

/**
 * gbp_sysroot_manager_get_target_path:
 * @self: a #GbpSysrootManager
 * @target: the unique identifier of the target
 *
 * Gets the sysroot path of the target.
 *
 * Returns: (transfer full): the sysroot path of the target.
 */
gchar *
gbp_sysroot_manager_get_target_path (GbpSysrootManager *self,
                                     const gchar       *target)
{
  g_return_val_if_fail (GBP_IS_SYSROOT_MANAGER (self), NULL);
  g_return_val_if_fail (self->key_file != NULL, NULL);
  g_return_val_if_fail (target != NULL, NULL);

  return g_key_file_get_string (self->key_file, target, "Path", NULL);
}


/**
 * gbp_sysroot_manager_set_target_pkg_config_path:
 * @self: a #GbpSysrootManager
 * @target: the unique identifier of the target
 * @path: (nullable): the additional Pkg-Config paths of the target or %NULL
 *
 * Sets the additional Pkg-Config paths of the target.
 * It is possible to use several paths by separating them with a colon character.
 */
void
gbp_sysroot_manager_set_target_pkg_config_path (GbpSysrootManager *self,
                                                const gchar       *target,
                                                const gchar       *path)
{
  g_return_if_fail (GBP_IS_SYSROOT_MANAGER (self));
  g_return_if_fail (self->key_file != NULL);
  g_return_if_fail (target != NULL);

  g_key_file_set_string (self->key_file, target, "PkgConfigPath", path);
  g_signal_emit (self, signals[TARGET_MODIFIED], 0, target, GBP_SYSROOT_MANAGER_TARGET_CHANGED);
  sysroot_manager_save (self);
}

/**
 * gbp_sysroot_manager_get_target_pkg_config_path:
 * @self: a #GbpSysrootManager
 * @target: the unique identifier of the target
 *
 * Gets the additional Pkg-Config paths of the target.
 *
 * This is often used when the target has its libraries in an architecture-specific folder.
 *
 * Returns: (transfer full) (nullable): the additional paths to pkg-config, using a colon separator.
 */
gchar *
gbp_sysroot_manager_get_target_pkg_config_path (GbpSysrootManager *self,
                                                const gchar       *target)
{
  g_return_val_if_fail (GBP_IS_SYSROOT_MANAGER (self), NULL);
  g_return_val_if_fail (self->key_file != NULL, NULL);
  g_return_val_if_fail (target != NULL, NULL);

  return g_key_file_get_string (self->key_file, target, "PkgConfigPath", NULL);
}

/**
 * gbp_sysroot_manager_list:
 * @self: a #GbpSysrootManager
 *
 * Retrieves the list of all the available sysroot unique identifiers.
 *
 * Returns: (transfer full) (nullable): the %NULL-terminated list of all the available sysroot
 * unique identifiers.
 */
gchar **
gbp_sysroot_manager_list (GbpSysrootManager *self)
{
  g_return_val_if_fail (GBP_IS_SYSROOT_MANAGER (self), NULL);
  g_return_val_if_fail (self->key_file != NULL, NULL);

  return g_key_file_get_groups (self->key_file, NULL);
}

static void
gbp_sysroot_manager_finalize (GObject *object)
{
  GbpSysrootManager *self = GBP_SYSROOT_MANAGER(object);

  g_clear_pointer (&self->key_file, g_key_file_free);

  G_OBJECT_CLASS (gbp_sysroot_manager_parent_class)->finalize (object);
}

void
gbp_sysroot_manager_class_init (GbpSysrootManagerClass *klass)
{
  G_OBJECT_CLASS (klass)->finalize = gbp_sysroot_manager_finalize;

  signals [TARGET_MODIFIED] =
    g_signal_new_class_handler ("target-changed",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_FIRST,
                                NULL, NULL, NULL, NULL,
                                G_TYPE_NONE,
                                2,
                                G_TYPE_STRING,
                                G_TYPE_INT);

  signals [TARGET_NAME_CHANGED] =
    g_signal_new_class_handler ("target-name-changed",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_FIRST,
                                NULL, NULL, NULL, NULL,
                                G_TYPE_NONE,
                                2,
                                G_TYPE_STRING,
                                G_TYPE_STRING);

  signals [TARGET_ARCH_CHANGED] =
    g_signal_new_class_handler ("target-arch-changed",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_FIRST,
                                NULL, NULL, NULL, NULL,
                                G_TYPE_NONE,
                                2,
                                G_TYPE_STRING,
                                G_TYPE_STRING);
}

static void
gbp_sysroot_manager_init (GbpSysrootManager *self)
{
  g_autofree gchar *conf_file = NULL;
  g_autoptr(GError) error = NULL;

  conf_file = sysroot_manager_get_path ();
  self->key_file = g_key_file_new ();

  if (!g_key_file_load_from_file (self->key_file, conf_file, G_KEY_FILE_KEEP_COMMENTS, &error) &&
      !g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
    g_warning ("Error loading the sysroot configuration: %s", error->message);
}
