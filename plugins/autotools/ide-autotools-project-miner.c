/* ide-autotools-project-miner.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-autotools-project-miner"

#include <glib/gi18n.h>
#include <ide.h>

#include "ide-autotools-project-miner.h"

#define MAX_MINE_DEPTH 2

struct _IdeAutotoolsProjectMiner
{
  GObject  parent_instance;
  GFile   *root_directory;
};

static void project_miner_iface_init (IdeProjectMinerInterface *iface);

static GPtrArray *ignored_directories;

G_DEFINE_TYPE_EXTENDED (IdeAutotoolsProjectMiner, ide_autotools_project_miner, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_PROJECT_MINER, project_miner_iface_init))

enum {
  PROP_0,
  PROP_ROOT_DIRECTORY,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static IdeDoap *
ide_autotools_project_miner_find_doap (IdeAutotoolsProjectMiner *self,
                                       GCancellable             *cancellable,
                                       GFile                    *directory)
{
  g_autoptr(GFileEnumerator) enumerator = NULL;
  GFileInfo *file_info = NULL;

  g_assert (IDE_IS_AUTOTOOLS_PROJECT_MINER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (G_IS_FILE (directory));

  enumerator = g_file_enumerate_children (directory,
                                          G_FILE_ATTRIBUTE_STANDARD_NAME,
                                          G_FILE_QUERY_INFO_NONE,
                                          cancellable,
                                          NULL);
  if (!enumerator)
    return NULL;

  while ((file_info = g_file_enumerator_next_file (enumerator, cancellable, NULL)))
    {
      g_autofree gchar *name = NULL;

      name = g_strdup (g_file_info_get_name (file_info));

      g_clear_object (&file_info);

      if (name != NULL && g_str_has_suffix (name, ".doap"))
        {
          g_autoptr(GFile) doap_file = g_file_get_child (directory, name);
          IdeDoap *doap = ide_doap_new ();

          if (!ide_doap_load_from_file (doap, doap_file, cancellable, NULL))
            {
              g_object_unref (doap);
              continue;
            }

          return doap;
        }
    }

  return NULL;
}

static void
ide_autotools_project_miner_discovered (IdeAutotoolsProjectMiner *self,
                                        GCancellable             *cancellable,
                                        GFile                    *directory,
                                        GFileInfo                *file_info)
{
  g_autofree gchar *uri = NULL;
  g_autofree gchar *name = NULL;
  g_autoptr(GFile) file = NULL;
  g_autoptr(GFile) index_file = NULL;
  g_autoptr(GFileInfo) index_info = NULL;
  g_autoptr(IdeProjectInfo) project_info = NULL;
  g_autoptr(GDateTime) last_modified_at = NULL;
  g_autoptr(IdeDoap) doap = NULL;
  const gchar *filename;
  const gchar *shortdesc = NULL;
  gchar **languages = NULL;
  guint64 mtime;

  IDE_ENTRY;

  g_assert (IDE_IS_AUTOTOOLS_PROJECT_MINER (self));
  g_assert (G_IS_FILE (directory));
  g_assert (G_IS_FILE_INFO (file_info));

  uri = g_file_get_uri (directory);
  g_debug ("Discovered autotools project at %s", uri);

  mtime = g_file_info_get_attribute_uint64 (file_info, G_FILE_ATTRIBUTE_TIME_MODIFIED);

  doap = ide_autotools_project_miner_find_doap (self, cancellable, directory);

  /*
   * If there is a git repo, trust the .git/index file for time info,
   * it is more reliable than our directory mtime.
   */
  index_file = g_file_get_child (directory, ".git/index");
  index_info = g_file_query_info (index_file,
                                  G_FILE_ATTRIBUTE_TIME_MODIFIED,
                                  G_FILE_QUERY_INFO_NONE,
                                  cancellable,
                                  NULL);
  if (index_info != NULL)
    mtime = g_file_info_get_attribute_uint64 (index_info, G_FILE_ATTRIBUTE_TIME_MODIFIED);

  last_modified_at = g_date_time_new_from_unix_local (mtime);

  filename = g_file_info_get_attribute_byte_string (file_info, G_FILE_ATTRIBUTE_STANDARD_NAME);
  file = g_file_get_child (directory, filename);
  name = g_file_get_basename (directory);

  if (doap != NULL)
    {
      const gchar *doap_name = ide_doap_get_name (doap);

      if (!ide_str_empty0 (doap_name))
        {
          g_free (name);
          name = g_strdup (doap_name);
        }

      shortdesc = ide_doap_get_shortdesc (doap);
      languages = ide_doap_get_languages (doap);
    }

  project_info = g_object_new (IDE_TYPE_PROJECT_INFO,
                               "description", shortdesc,
                               "directory", directory,
                               "doap", doap,
                               "file", file,
                               "last-modified-at", last_modified_at,
                               "languages", languages,
                               "name", name,
                               "priority", 100,
                               NULL);

  ide_project_miner_emit_discovered (IDE_PROJECT_MINER (self), project_info);

  IDE_EXIT;
}

static gboolean
directory_is_ignored (GFile *directory)
{
  g_assert (G_IS_FILE (directory));
  g_assert (ignored_directories != NULL);

  for (guint i = 0; i < ignored_directories->len; i++)
    {
      GFile *ignored_directory = g_ptr_array_index (ignored_directories, i);

      if (g_file_equal (directory, ignored_directory))
        return TRUE;
    }

  if (!g_file_is_native (directory))
    return TRUE;

  return FALSE;
}

static void
ide_autotools_project_miner_mine_directory (IdeAutotoolsProjectMiner *self,
                                            GFile                    *directory,
                                            guint                     depth,
                                            GCancellable             *cancellable)
{
  g_autoptr(GFileEnumerator) file_enum = NULL;
  g_autoptr(GPtrArray) directories = NULL;
  gpointer file_info_ptr;

  g_assert (IDE_IS_AUTOTOOLS_PROJECT_MINER (self));
  g_assert (G_IS_FILE (directory));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (depth == MAX_MINE_DEPTH)
    return;

  if (directory_is_ignored (directory))
    return;

#ifdef IDE_ENABLE_TRACE
  {
    g_autofree gchar *uri = NULL;

    uri = g_file_get_uri (directory);
    IDE_TRACE_MSG ("Mining directory %s", uri);
  }
#endif

  file_enum = g_file_enumerate_children (directory,
                                         G_FILE_ATTRIBUTE_STANDARD_NAME","
                                         G_FILE_ATTRIBUTE_STANDARD_TYPE","
                                         G_FILE_ATTRIBUTE_TIME_MODIFIED,
                                         G_FILE_QUERY_INFO_NONE,
                                         cancellable,
                                         NULL);

  if (file_enum == NULL)
    return;

  while ((file_info_ptr = g_file_enumerator_next_file (file_enum, cancellable, NULL)))
    {
      g_autoptr(GFileInfo) file_info = file_info_ptr;
      const gchar *filename;
      GFileType file_type;
      GFile *child;

      file_type = g_file_info_get_attribute_uint32 (file_info, G_FILE_ATTRIBUTE_STANDARD_TYPE);
      filename = g_file_info_get_attribute_byte_string (file_info, G_FILE_ATTRIBUTE_STANDARD_NAME);

      if (filename && filename [0] == '.')
        continue;

      switch (file_type)
        {
        case G_FILE_TYPE_DIRECTORY:
          if (directories == NULL)
            directories = g_ptr_array_new_with_free_func (g_object_unref);
          child = g_file_get_child (directory, filename);
          g_ptr_array_add (directories, child);
          break;

        case G_FILE_TYPE_REGULAR:
          if ((0 == g_strcmp0 (filename, "configure.ac")) ||
              (0 == g_strcmp0 (filename, "configure.in")))
            {
              ide_autotools_project_miner_discovered (self, cancellable, directory, file_info);
              g_clear_object (&file_info);
              return;
            }
          break;

        case G_FILE_TYPE_UNKNOWN:
        case G_FILE_TYPE_SYMBOLIC_LINK:
        case G_FILE_TYPE_SPECIAL:
        case G_FILE_TYPE_SHORTCUT:
        case G_FILE_TYPE_MOUNTABLE:
        default:
          break;
        }
    }

  if (directories != NULL)
    {
      gsize i;

      for (i = 0; i < directories->len; i++)
        {
          GFile *child = g_ptr_array_index (directories, i);
          ide_autotools_project_miner_mine_directory (self, child, depth + 1, cancellable);
        }
    }
}

static void
ide_autotools_project_miner_worker (GTask        *task,
                                    gpointer      source_object,
                                    gpointer      task_data,
                                    GCancellable *cancellable)
{
  IdeAutotoolsProjectMiner *self = source_object;
  GFile *directory = task_data;

  IDE_ENTRY;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_AUTOTOOLS_PROJECT_MINER (self));
  g_assert (G_IS_FILE (directory));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  ide_autotools_project_miner_mine_directory (self, directory, 0, cancellable);

  g_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
ide_autotools_project_miner_mine_async (IdeProjectMiner     *miner,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  IdeAutotoolsProjectMiner *self = (IdeAutotoolsProjectMiner *)miner;
  g_autoptr(GTask) task = NULL;
  g_autoptr(GFile) directory = NULL;
  g_autoptr(GSettings) settings = NULL;
  g_autofree gchar *projects_dir = NULL;
  g_autofree gchar *path = NULL;

  g_assert (IDE_IS_AUTOTOOLS_PROJECT_MINER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (miner, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_autotools_project_miner_mine_async);
  g_task_set_priority (task, G_PRIORITY_LOW);

  /*
   * Get the projects directory from GSettings.
   * We use this to avoid checking the entire home directory.
   * This defaults to "~/Projects" but the user can override.
   */
  settings = g_settings_new ("org.gnome.builder");
  projects_dir = g_settings_get_string (settings, "projects-directory");
  path = ide_path_expand (projects_dir);
  directory = g_file_new_for_path (path);

  if (self->root_directory)
    g_task_set_task_data (task, g_object_ref (self->root_directory), g_object_unref);
  else
    g_task_set_task_data (task, g_object_ref (directory), g_object_unref);

  g_task_run_in_thread (task, ide_autotools_project_miner_worker);
}

static gboolean
ide_autotools_project_miner_mine_finish (IdeProjectMiner  *miner,
                                         GAsyncResult     *result,
                                         GError          **error)
{
  GTask *task = (GTask *)result;

  g_assert (IDE_IS_AUTOTOOLS_PROJECT_MINER (miner));
  g_assert (G_IS_TASK (task));

  return g_task_propagate_boolean (task, error);
}

static void
ide_autotools_project_miner_finalize (GObject *object)
{
  IdeAutotoolsProjectMiner *self = (IdeAutotoolsProjectMiner *)object;

  g_clear_object (&self->root_directory);

  G_OBJECT_CLASS (ide_autotools_project_miner_parent_class)->finalize (object);
}

static void
ide_autotools_project_miner_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  IdeAutotoolsProjectMiner *self = IDE_AUTOTOOLS_PROJECT_MINER (object);

  switch (prop_id)
    {
    case PROP_ROOT_DIRECTORY:
      g_value_set_object (value, ide_autotools_project_miner_get_root_directory (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_autotools_project_miner_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  IdeAutotoolsProjectMiner *self = IDE_AUTOTOOLS_PROJECT_MINER (object);

  switch (prop_id)
    {
    case PROP_ROOT_DIRECTORY:
      ide_autotools_project_miner_set_root_directory (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
project_miner_iface_init (IdeProjectMinerInterface *iface)
{
  iface->mine_async = ide_autotools_project_miner_mine_async;
  iface->mine_finish = ide_autotools_project_miner_mine_finish;
}

static void
ide_autotools_project_miner_class_init (IdeAutotoolsProjectMinerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  g_autoptr(GFile) home = NULL;

  object_class->finalize = ide_autotools_project_miner_finalize;
  object_class->get_property = ide_autotools_project_miner_get_property;
  object_class->set_property = ide_autotools_project_miner_set_property;

  properties [PROP_ROOT_DIRECTORY] =
    g_param_spec_object ("root-directory",
                         "Root Directory",
                         "The root directory to scan from.",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  ignored_directories = g_ptr_array_new ();
  home = g_file_new_for_path (g_get_home_dir ());

  for (guint i = 0; i < G_USER_N_DIRECTORIES; i++)
    {
      g_autoptr(GFile) dir = NULL;
      const gchar *path;

      path = g_get_user_special_dir (i);
      if (path == NULL)
        continue;

      dir = g_file_new_for_path (path);
      if (dir == NULL)
        continue;

      if (!g_file_equal (dir, home))
        g_ptr_array_add (ignored_directories, g_steal_pointer (&dir));
    }
}

static void
ide_autotools_project_miner_init (IdeAutotoolsProjectMiner *self)
{
}

/**
 * ide_autotools_project_miner_get_root_directory:
 *
 * Gets the IdeAutotoolsProjectMiner:root-directory property.
 * Scans will start from this directory.
 *
 * Returns: (transfer none) (nullable): A #GFile or %NULL.
 */
GFile *
ide_autotools_project_miner_get_root_directory (IdeAutotoolsProjectMiner *self)
{
  g_return_val_if_fail (IDE_IS_AUTOTOOLS_PROJECT_MINER (self), NULL);

  return self->root_directory;
}

void
ide_autotools_project_miner_set_root_directory (IdeAutotoolsProjectMiner *self,
                                                GFile                    *root_directory)
{
  g_return_if_fail (IDE_IS_AUTOTOOLS_PROJECT_MINER (self));
  g_return_if_fail (!root_directory || G_IS_FILE (root_directory));

  if (g_set_object (&self->root_directory, root_directory))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ROOT_DIRECTORY]);
}
