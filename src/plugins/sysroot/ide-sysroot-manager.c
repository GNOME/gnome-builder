/* ide-sysroot-manager.c
 *
 * Copyright (C) 2018 Corentin Noël <corentin.noel@collabora.com>
 * Copyright (C) 2018 Collabora Ltd.
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

#include "ide-sysroot-manager.h"

struct _IdeSysrootManager
{
  GObject parent_instance;
  GKeyFile *key_file;
};

G_DEFINE_TYPE (IdeSysrootManager, ide_sysroot_manager, G_TYPE_OBJECT)

enum {
  TARGET_MODIFIED,
  TARGET_NAME_CHANGED,
  LAST_SIGNAL
};

static guint signals [LAST_SIGNAL];

static IdeSysrootManager *instance;

gchar *
sysroot_manager_get_path (void)
{
  gchar *directory_path = NULL;
  gchar *conf_file = NULL;

  directory_path = g_build_filename (g_get_user_config_dir (),
                                     ide_get_program_name (),
                                     "sysroot",
                                     NULL);

  g_mkdir_with_parents (directory_path, 0750);
  conf_file = g_build_filename (directory_path, "general.conf", NULL);
  g_free (directory_path);
  return conf_file;
}

static void
sysroot_manager_save (IdeSysrootManager *self)
{
  gchar *conf_file = NULL;
  GError *error = NULL;
  conf_file = sysroot_manager_get_path ();

  if (!g_key_file_save_to_file (self->key_file, conf_file, &error))
    {
      g_critical ("Error loading the sysroot configuration: %s", error->message);
      g_error_free (error);
    }

  g_free (conf_file);
}

IdeSysrootManager *
ide_sysroot_manager_get_default (void)
{
  if (instance == NULL)
    {
      instance = g_object_new (IDE_TYPE_SYSROOT_MANAGER, NULL);
    }

  return instance;
}

gchar *
ide_sysroot_manager_create_target (IdeSysrootManager *self)
{
  for (guint i = 0; i < UINT_MAX; i++)
    {
      gchar * result;
      GString *sysroot_name = g_string_new (NULL);
      g_string_printf (sysroot_name, "Sysroot %u", i);
      result = g_string_free (sysroot_name, FALSE);
      if (!g_key_file_has_group (self->key_file, result))
        {
          g_key_file_set_string (self->key_file, result, "Name", result);
          g_key_file_set_string (self->key_file, result, "Path", "/");
          sysroot_manager_save (self);
          g_signal_emit (self, signals[TARGET_MODIFIED], 0, result, IDE_SYSROOT_MANAGER_TARGET_CREATED);
          return result;
        }
    }

  return NULL;
}

void
ide_sysroot_manager_remove_target (IdeSysrootManager *self, const char *target)
{
  GError *error = NULL;
  g_key_file_remove_group (self->key_file, target, &error);
  if (error)
    {
      g_critical ("Error removing target \"%s\": %s", target, error->message);
      g_error_free (error);
    }

  g_signal_emit (self, signals[TARGET_MODIFIED], 0, target, IDE_SYSROOT_MANAGER_TARGET_REMOVED);
  sysroot_manager_save (self);
}

void
ide_sysroot_manager_set_target_name (IdeSysrootManager *self, const char *target, const char *name)
{
  g_key_file_set_string (self->key_file, target, "Name", name);
  g_signal_emit (self, signals[TARGET_MODIFIED], 0, target, IDE_SYSROOT_MANAGER_TARGET_CHANGED);
  g_signal_emit (self, signals[TARGET_NAME_CHANGED], 0, target, name);
  sysroot_manager_save (self);
}

gchar *
ide_sysroot_manager_get_target_name (IdeSysrootManager *self, const char *target)
{
  return g_key_file_get_string (self->key_file, target, "Name", NULL);
}

void
ide_sysroot_manager_set_target_path (IdeSysrootManager *self, const char *target, const char *path)
{
  g_key_file_set_string (self->key_file, target, "Path", path);
  g_signal_emit (self, signals[TARGET_MODIFIED], 0, target, IDE_SYSROOT_MANAGER_TARGET_CHANGED);
  sysroot_manager_save (self);
}

gchar *
ide_sysroot_manager_get_target_path (IdeSysrootManager *self, const char *target)
{
  return g_key_file_get_string (self->key_file, target, "Path", NULL);
}

void
ide_sysroot_manager_set_target_pkg_config_path (IdeSysrootManager *self, const char *target, const char *path)
{
  g_key_file_set_string (self->key_file, target, "PkgConfigPath", path);
  g_signal_emit (self, signals[TARGET_MODIFIED], 0, target, IDE_SYSROOT_MANAGER_TARGET_CHANGED);
  sysroot_manager_save (self);
}

gchar *
ide_sysroot_manager_get_target_pkg_config_path (IdeSysrootManager *self, const char *target)
{
  return g_key_file_get_string (self->key_file, target, "PkgConfigPath", NULL);
}

GArray *
ide_sysroot_manager_list (IdeSysrootManager *self)
{
  GArray *list = NULL;
  gchar **groups = NULL;
  gsize groups_length = 0;

  list = g_array_new (FALSE, FALSE, sizeof (char*));
  groups = g_key_file_get_groups (self->key_file, &groups_length);
  g_array_append_vals (list, groups, groups_length);

  return list;
}

void
ide_sysroot_manager_finalize (GObject *object)
{
  IdeSysrootManager *self = IDE_SYSROOT_MANAGER(object);

  g_clear_pointer (&self->key_file, g_key_file_free);

  G_OBJECT_CLASS (ide_sysroot_manager_parent_class)->finalize (object);
}

void
ide_sysroot_manager_class_init (IdeSysrootManagerClass *klass)
{
  G_OBJECT_CLASS (klass)->finalize = ide_sysroot_manager_finalize;

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
}

static void
ide_sysroot_manager_init (IdeSysrootManager *self)
{
  gchar *conf_file = NULL;
  GError *error = NULL;

  conf_file = sysroot_manager_get_path ();
  self->key_file = g_key_file_new ();
  g_key_file_load_from_file (self->key_file, conf_file, G_KEY_FILE_KEEP_COMMENTS, &error);
  if (error)
    {
      if (error->code != G_FILE_ERROR_NOENT)
        {
          g_critical ("Error loading the sysroot configuration: %s", error->message);
        }

      g_error_free (error);
    }
}
