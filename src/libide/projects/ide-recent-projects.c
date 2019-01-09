/* ide-recent-projects.c
 *
 * Copyright 2015-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-recent-projects"

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libide-core.h>

#include "ide-recent-projects.h"

struct _IdeRecentProjects
{
  GObject       parent_instance;

  GSequence    *projects;
  GHashTable   *recent_uris;
  gchar        *file_uri;

  guint         discovered : 1;
};

#define MAX_PROJECT_INFOS 100

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (IdeRecentProjects, ide_recent_projects, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

IdeRecentProjects *
ide_recent_projects_new (void)
{
  return g_object_new (IDE_TYPE_RECENT_PROJECTS, NULL);
}

/**
 * ide_recent_projects_get_default:
 *
 * Gets a shared #IdeRecentProjects instance.
 *
 * If this instance is unref'd, a new instance will be created on the next
 * request to get the default #IdeRecentProjects instance.
 *
 * Returns: (transfer none): an #IdeRecentProjects
 *
 * Since: 3.32
 */
IdeRecentProjects *
ide_recent_projects_get_default (void)
{
  static IdeRecentProjects *instance;

  if (instance == NULL)
    g_set_weak_pointer (&instance, ide_recent_projects_new ());

  return instance;
}

static void
ide_recent_projects_added (IdeRecentProjects *self,
                           IdeProjectInfo    *project_info)
{
  g_autofree gchar *uri = NULL;
  GFile *file;

  g_assert (IDE_IS_RECENT_PROJECTS (self));
  g_assert (IDE_IS_PROJECT_INFO (project_info));

  file = ide_project_info_get_file (project_info);
  uri = g_file_get_uri (file);

  if (!g_hash_table_contains (self->recent_uris, uri))
    {
      GSequenceIter *iter;
      gint position;

      iter = g_sequence_insert_sorted (self->projects,
                                       g_object_ref (project_info),
                                       (GCompareDataFunc)ide_project_info_compare,
                                       NULL);
      position = g_sequence_iter_get_position (iter);
      if (position > MAX_PROJECT_INFOS)
        g_sequence_remove (iter);
      else
        g_list_model_items_changed (G_LIST_MODEL (self), position, 0, 1);
    }
}

static GBookmarkFile *
ide_recent_projects_get_bookmarks (IdeRecentProjects  *self,
                                   GError            **error)
{
  g_autoptr(GBookmarkFile) bookmarks = NULL;
  g_autoptr(GError) local_error = NULL;

  g_assert (IDE_IS_RECENT_PROJECTS (self));

  bookmarks = g_bookmark_file_new ();

  if (!g_bookmark_file_load_from_file (bookmarks, self->file_uri, &local_error))
    {
      if (!g_error_matches (local_error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
        {
          g_propagate_error (error, g_steal_pointer (&local_error));
          return NULL;
        }
    }

  return g_steal_pointer (&bookmarks);
}

static void
ide_recent_projects_load_recent (IdeRecentProjects *self)
{
  g_autoptr(GBookmarkFile) projects_file = NULL;
  g_autoptr(GError) error = NULL;
  gboolean needs_sync = FALSE;
  gchar **uris;

  g_assert (IDE_IS_RECENT_PROJECTS (self));

  if (!(projects_file = ide_recent_projects_get_bookmarks (self, &error)))
    {
      g_warning ("Unable to open recent projects file: %s", error->message);
      return;
    }

  uris = g_bookmark_file_get_uris (projects_file, NULL);

  for (gsize z = 0; uris[z]; z++)
    {
      g_autoptr(GDateTime) last_modified_at = NULL;
      g_autoptr(GFile) project_file = NULL;
      g_autoptr(GFile) directory = NULL;
      g_autoptr(GPtrArray) languages = NULL;
      g_autoptr(IdeProjectInfo) project_info = NULL;
      g_autofree gchar *name = NULL;
      g_autofree gchar *description = NULL;
      const gchar *build_system_name = NULL;
      const gchar *uri = uris[z];
      const gchar *diruri = NULL;
      time_t modified;
      g_auto(GStrv) groups = NULL;
      gsize len;

      groups = g_bookmark_file_get_groups (projects_file, uri, &len, NULL);

      for (gsize i = 0; i < len; i++)
        {
          if (g_str_equal (groups [i], IDE_RECENT_PROJECTS_GROUP))
            goto is_project;
        }

      continue;

    is_project:
      project_file = g_file_new_for_uri (uri);

      if (g_file_is_native (project_file) && !g_file_query_exists (project_file, NULL))
        {
          g_bookmark_file_remove_item (projects_file, uri, NULL);
          needs_sync = TRUE;
          continue;
        }

      name = g_bookmark_file_get_title (projects_file, uri, NULL);
      description = g_bookmark_file_get_description (projects_file, uri, NULL);
      modified = g_bookmark_file_get_modified  (projects_file, uri, NULL);
      last_modified_at = g_date_time_new_from_unix_local (modified);

      for (gsize i = 0; i < len; i++)
        {
          if (g_str_has_prefix (groups [i], IDE_RECENT_PROJECTS_DIRECTORY))
            diruri = groups [i] + strlen (IDE_RECENT_PROJECTS_DIRECTORY);
        }

      if (diruri == NULL)
        directory = g_file_get_parent (project_file);
      else
        directory = g_file_new_for_uri (diruri);

      languages = g_ptr_array_new ();
      for (gsize i = 0; i < len; i++)
        {
          if (g_str_has_prefix (groups [i], IDE_RECENT_PROJECTS_LANGUAGE_GROUP_PREFIX))
            g_ptr_array_add (languages, groups [i] + strlen (IDE_RECENT_PROJECTS_LANGUAGE_GROUP_PREFIX));
          else if (g_str_has_prefix (groups [i], IDE_RECENT_PROJECTS_BUILD_SYSTEM_GROUP_PREFIX))
            build_system_name = groups [i] + strlen (IDE_RECENT_PROJECTS_BUILD_SYSTEM_GROUP_PREFIX);
        }
      g_ptr_array_add (languages, NULL);

      project_info = g_object_new (IDE_TYPE_PROJECT_INFO,
                                   "build-system-name", build_system_name,
                                   "description", description,
                                   "directory", directory,
                                   "file", project_file,
                                   "is-recent", TRUE,
                                   "languages", (gchar **)languages->pdata,
                                   "last-modified-at", last_modified_at,
                                   "name", name,
                                   NULL);

      ide_recent_projects_added (self, project_info);

      g_hash_table_insert (self->recent_uris, g_strdup (uri), NULL);
    }

  g_strfreev (uris);

  if (needs_sync)
    g_bookmark_file_to_file (projects_file, self->file_uri, NULL);
}

static GType
ide_recent_projects_get_item_type (GListModel *model)
{
  return IDE_TYPE_PROJECT_INFO;
}

static guint
ide_recent_projects_get_n_items (GListModel *model)
{
  g_assert (IDE_IS_RECENT_PROJECTS (model));

  return g_sequence_get_length (IDE_RECENT_PROJECTS (model)->projects);
}

static gpointer
ide_recent_projects_get_item (GListModel *model,
                              guint       position)
{
  IdeRecentProjects *self = (IdeRecentProjects *)model;
  GSequenceIter *iter;

  g_assert (IDE_IS_RECENT_PROJECTS (self));

  if ((iter = g_sequence_get_iter_at_pos (self->projects, position)))
    return g_object_ref (g_sequence_get (iter));

  return NULL;
}

static void
ide_recent_projects_constructed (GObject *object)
{
  IdeRecentProjects *self = IDE_RECENT_PROJECTS (object);

  G_OBJECT_CLASS (ide_recent_projects_parent_class)->constructed (object);

  ide_recent_projects_load_recent (self);
}

static void
ide_recent_projects_finalize (GObject *object)
{
  IdeRecentProjects *self = (IdeRecentProjects *)object;

  g_clear_pointer (&self->projects, g_sequence_free);
  g_clear_pointer (&self->recent_uris, g_hash_table_unref);
  g_clear_pointer (&self->file_uri, g_free);

  G_OBJECT_CLASS (ide_recent_projects_parent_class)->finalize (object);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = ide_recent_projects_get_item_type;
  iface->get_n_items = ide_recent_projects_get_n_items;
  iface->get_item = ide_recent_projects_get_item;
}

static void
ide_recent_projects_class_init (IdeRecentProjectsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ide_recent_projects_constructed;
  object_class->finalize = ide_recent_projects_finalize;
}

static void
ide_recent_projects_init (IdeRecentProjects *self)
{
  self->projects = g_sequence_new (g_object_unref);
  self->recent_uris = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  self->file_uri = g_build_filename (g_get_user_data_dir (),
                                     ide_get_program_name (),
                                     IDE_RECENT_PROJECTS_BOOKMARK_FILENAME,
                                     NULL);
}

/**
 * ide_recent_projects_remove:
 * @self: An #IdeRecentProjects
 * @project_infos: (transfer none) (element-type IdeProjectInfo): a #GList
 *   of #IdeProjectInfo.
 *
 * Removes the provided projects from the recent projects file.
 *
 * Since: 3.32
 */
void
ide_recent_projects_remove (IdeRecentProjects *self,
                            GList             *project_infos)
{
  g_autoptr(GBookmarkFile) projects_file = NULL;
  g_autoptr(GError) error = NULL;
  GList *liter;

  g_return_if_fail (IDE_IS_RECENT_PROJECTS (self));

  if (!(projects_file = ide_recent_projects_get_bookmarks (self, &error)))
    {
      g_warning ("Failed to load bookmarks file: %s", error->message);
      return;
    }

  for (liter = project_infos; liter; liter = liter->next)
    {
      IdeProjectInfo *project_info = liter->data;
      g_autofree gchar *file_uri = NULL;
      GSequenceIter *iter;
      GFile *file;

      g_assert (IDE_IS_PROJECT_INFO (liter->data));

      iter = g_sequence_lookup (self->projects,
                                project_info,
                                (GCompareDataFunc)ide_project_info_compare,
                                NULL);

      if (iter == NULL)
        {
          g_warning ("Project \"%s\" was not found, cannot remove.",
                     ide_project_info_get_name (project_info));
          g_clear_error (&error);
          continue;
        }

      file = ide_project_info_get_file (project_info);
      file_uri = g_file_get_uri (file);

      if (!g_bookmark_file_remove_item (projects_file, file_uri, &error))
        {
          g_autofree gchar *with_slash = g_strdup_printf ("%s/", file_uri);

          /* Sometimes we don't get a match because the directory is missing a
           * trailing slash. Annoying, I know. See the following for the
           * upstream bug filed in GLib.
           *
           * https://bugzilla.gnome.org/show_bug.cgi?id=765449
           */
          if (!g_bookmark_file_remove_item (projects_file, with_slash, NULL))
            {
              g_warning ("Failed to remove recent project: %s", error->message);
              g_clear_error (&error);
              continue;
            }

          g_clear_error (&error);
        }

      g_sequence_remove (iter);
    }

  if (!g_bookmark_file_to_file (projects_file, self->file_uri, &error))
    {
      g_warning ("Failed to save recent projects file: %s", error->message);
      return;
    }
}

gchar *
ide_recent_projects_find_by_directory (IdeRecentProjects *self,
                                       const gchar       *directory)
{
  g_autoptr(GBookmarkFile) bookmarks = NULL;
  g_auto(GStrv) uris = NULL;
  gsize len = 0;

  g_return_val_if_fail (IDE_IS_RECENT_PROJECTS (self), NULL);
  g_return_val_if_fail (directory, NULL);

  if (!g_file_test (directory, G_FILE_TEST_IS_DIR))
    return NULL;

  if (!(bookmarks = ide_recent_projects_get_bookmarks (self, NULL)))
    return NULL;

  uris = g_bookmark_file_get_uris (bookmarks, &len);

  for (gsize i = 0; i < len; i++)
    {
      /* TODO: Make a better compare that deals with trailing / */
      if (g_str_has_prefix (uris[i], directory))
        return g_strdup (uris[i]);
    }

  return NULL;
}
