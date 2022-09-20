/* gbp-recent-workbench-addin.c
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

#define G_LOG_DOMAIN "gbp-recent-workbench-addin"

#include "config.h"

#include <libide-projects.h>
#include <libide-gui.h>

#include "ide-project-info-private.h"

#include "gbp-recent-workbench-addin.h"

struct _GbpRecentWorkbenchAddin
{
  GObject       parent_instance;
  IdeWorkbench *workbench;
};

static gboolean
directory_is_ignored (GFile *file)
{
  g_autofree gchar *relative_path = NULL;
  g_autoptr(GFile) downloads_dir = NULL;
  g_autoptr(GFile) home_dir = NULL;
  g_autoptr(GFile) projects_dir = NULL;
  GFileType file_type;

  g_assert (G_IS_FILE (file));

  projects_dir = g_file_new_for_path (ide_get_projects_dir ());
  home_dir = g_file_new_for_path (g_get_home_dir ());
  relative_path = g_file_get_relative_path (home_dir, file);
  file_type = g_file_query_file_type (file, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL);

  if (!g_file_has_prefix (file, home_dir) &&
      !g_file_has_prefix (file, projects_dir))
    return TRUE;

  /* First check downloads directory as we never want that */
  downloads_dir = g_file_new_for_path (g_get_user_special_dir (G_USER_DIRECTORY_DOWNLOAD));
  if (downloads_dir != NULL &&
      (g_file_equal (file, downloads_dir) ||
       g_file_has_prefix (file, downloads_dir)))
    return TRUE;

  /* If the directory is in the projects dir (and the projects dir is
   * not $HOME, then short-circuit as not ignored.
   */
  if (!g_file_equal (home_dir, projects_dir) &&
      g_file_has_prefix (file, projects_dir))
    return FALSE;

  /* Not in home or projects directory, ignore */
  if (relative_path == NULL)
    return TRUE;

  /*
   * Ignore dot directories, except .local.
   * We've had too many bug reports with people creating things
   * like gnome-shell extensions in their .local directory.
   */
  if (relative_path[0] == '.' &&
      !g_str_has_prefix (relative_path, ".local"G_DIR_SEPARATOR_S))
    return TRUE;

  if (file_type != G_FILE_TYPE_DIRECTORY)
    {
      g_autoptr(GFile) parent = g_file_get_parent (file);

      if (g_file_equal (home_dir, parent))
        return TRUE;
    }

  return FALSE;
}

static void
gbp_recent_workbench_addin_add_recent (GbpRecentWorkbenchAddin *self,
                                       IdeProjectInfo          *project_info)
{
  g_autofree gchar *recent_projects_path = NULL;
  g_autoptr(GBookmarkFile) projects_file = NULL;
  g_autoptr(GPtrArray) groups = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *uri = NULL;
  g_autofree gchar *app_exec = NULL;
  g_autofree gchar *dir = NULL;
  IdeBuildSystem *build_system;
  IdeDoap *doap;
  GFile *file;
  GFile *directory;

  IDE_ENTRY;

  g_assert (GBP_IS_RECENT_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (self->workbench));
  g_assert (IDE_IS_PROJECT_INFO (project_info));

  if (!(file = _ide_project_info_get_real_file (project_info)) ||
      directory_is_ignored (file))
    IDE_EXIT;

  recent_projects_path = g_build_filename (g_get_user_data_dir (),
                                           ide_get_program_name (),
                                           IDE_RECENT_PROJECTS_BOOKMARK_FILENAME,
                                           NULL);

  projects_file = g_bookmark_file_new ();

  if (!g_bookmark_file_load_from_file (projects_file, recent_projects_path, &error))
    {
      /*
       * If there was an error loading the file and the error is not "File does not
       * exist" then stop saving operation
       */
      if (error != NULL &&
          !g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
        {
          g_warning ("Unable to open recent projects \"%s\" file: %s",
                     recent_projects_path, error->message);
          IDE_EXIT;
        }
    }

  uri = g_file_get_uri (file);
  app_exec = g_strdup_printf ("%s -p %%p", ide_get_program_name ());

  g_bookmark_file_set_title (projects_file, uri, ide_project_info_get_name (project_info));
  g_bookmark_file_set_mime_type (projects_file, uri, "application/x-builder-project");
  g_bookmark_file_add_application (projects_file, uri, ide_get_program_name (), app_exec);
  g_bookmark_file_set_is_private (projects_file, uri, FALSE);

  doap = ide_project_info_get_doap (project_info);

  /* attach project description to recent info */
  if (doap != NULL)
    g_bookmark_file_set_description (projects_file, uri, ide_doap_get_shortdesc (doap));

  /* attach discovered languages to recent info */
  groups = g_ptr_array_new_with_free_func (g_free);
  g_ptr_array_add (groups, g_strdup (IDE_RECENT_PROJECTS_GROUP));
  if (doap != NULL)
    {
      gchar **languages;

      if ((languages = ide_doap_get_languages (doap)))
        {
          for (guint i = 0; languages[i]; i++)
            g_ptr_array_add (groups,
                             g_strdup_printf ("%s%s",
                                              IDE_RECENT_PROJECTS_LANGUAGE_GROUP_PREFIX,
                                              languages[i]));
        }
    }

  g_bookmark_file_set_groups (projects_file, uri, (const gchar **)groups->pdata, groups->len);

  build_system = ide_workbench_get_build_system (self->workbench);

  if (build_system != NULL)
    {
      g_autofree gchar *build_system_name = NULL;
      g_autofree gchar *build_system_group = NULL;

      build_system_name = ide_build_system_get_display_name (build_system);
      build_system_group = g_strdup_printf ("%s%s", IDE_RECENT_PROJECTS_BUILD_SYSTEM_GROUP_PREFIX, build_system_name);
      g_bookmark_file_add_group (projects_file, uri, build_system_group);
    }

  if ((directory = _ide_project_info_get_real_directory (project_info)))
    {
      g_autofree gchar *dir_group = NULL;
      g_autofree gchar *diruri = g_file_get_uri (directory);

      dir_group = g_strdup_printf ("%s%s", IDE_RECENT_PROJECTS_DIRECTORY, diruri);
      g_bookmark_file_add_group (projects_file, uri, dir_group);
    }

  IDE_TRACE_MSG ("Registering %s as recent project.", uri);

  /* ensure the containing directory exists */
  dir = g_path_get_dirname (recent_projects_path);
  g_mkdir_with_parents (dir, 0750);

  if (!g_bookmark_file_to_file (projects_file, recent_projects_path, &error))
    {
      g_warning ("Unable to save recent projects %s file: %s",
                 recent_projects_path, error->message);
      g_clear_error (&error);
    }

  /* Request that the recent projects be reloaded */
  ide_recent_projects_invalidate (ide_recent_projects_get_default ());

  IDE_EXIT;
}


static void
gbp_recent_workbench_addin_project_loaded (IdeWorkbenchAddin *addin,
                                           IdeProjectInfo    *project_info)
{
  GbpRecentWorkbenchAddin *self = (GbpRecentWorkbenchAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_RECENT_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_PROJECT_INFO (project_info));

  gbp_recent_workbench_addin_add_recent (self, project_info);
}

static void
gbp_recent_workbench_addin_load (IdeWorkbenchAddin *addin,
                                 IdeWorkbench      *workbench)
{
  GBP_RECENT_WORKBENCH_ADDIN (addin)->workbench = workbench;
}

static void
gbp_recent_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                   IdeWorkbench      *workbench)
{
  GBP_RECENT_WORKBENCH_ADDIN (addin)->workbench = NULL;
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = gbp_recent_workbench_addin_load;
  iface->unload = gbp_recent_workbench_addin_unload;
  iface->project_loaded = gbp_recent_workbench_addin_project_loaded;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpRecentWorkbenchAddin, gbp_recent_workbench_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN, workbench_addin_iface_init))

static void
gbp_recent_workbench_addin_class_init (GbpRecentWorkbenchAddinClass *klass)
{
}

static void
gbp_recent_workbench_addin_init (GbpRecentWorkbenchAddin *self)
{
}
