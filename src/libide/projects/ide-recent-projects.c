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

#include "ide-project-info.h"
#include "ide-project-info-private.h"
#include "ide-recent-projects.h"

#define INVALIDATE_DELAY_SECONDS 5

struct _IdeRecentProjects
{
  GObject       parent_instance;

  GSequence    *projects;
  GHashTable   *recent_uris;
  char         *file_uri;

  guint         reloading : 1;
};

#define MAX_PROJECT_INFOS 100

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeRecentProjects, ide_recent_projects, G_TYPE_OBJECT,
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

  file = _ide_project_info_get_real_file (project_info);
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
      GDateTime *last_modified_at = NULL;
      g_autoptr(GFile) project_file = NULL;
      g_autoptr(GFile) directory = NULL;
      g_autoptr(GPtrArray) languages = NULL;
      g_autoptr(IdeProjectInfo) project_info = NULL;
      g_autofree gchar *name = NULL;
      g_autofree gchar *description = NULL;
      const gchar *build_system_hint = NULL;
      const gchar *build_system_name = NULL;
      const gchar *uri = uris[z];
      const gchar *diruri = NULL;
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
      last_modified_at = g_bookmark_file_get_modified_date_time (projects_file, uri, NULL);

      for (gsize i = 0; i < len; i++)
        {
          if (g_str_has_prefix (groups [i], IDE_RECENT_PROJECTS_DIRECTORY))
            diruri = groups [i] + strlen (IDE_RECENT_PROJECTS_DIRECTORY);
        }

      if (diruri == NULL)
        {
          /* If the old project was a plain-ol'-directory, then we don't want
           * it's parent (which might be ~/Projects), instead reuse the project
           * file as the directory too.
           */
          if (g_file_query_file_type (project_file, 0, NULL) == G_FILE_TYPE_DIRECTORY)
            directory = g_file_dup (project_file);
          else
            directory = g_file_get_parent (project_file);
        }
      else
        directory = g_file_new_for_uri (diruri);

      languages = g_ptr_array_new_with_free_func (g_free);
      for (gsize i = 0; i < len; i++)
        {
          if (g_str_has_prefix (groups [i], IDE_RECENT_PROJECTS_LANGUAGE_GROUP_PREFIX))
            g_ptr_array_add (languages, g_strdup (groups [i] + strlen (IDE_RECENT_PROJECTS_LANGUAGE_GROUP_PREFIX)));
          else if (g_str_has_prefix (groups [i], IDE_RECENT_PROJECTS_BUILD_SYSTEM_GROUP_PREFIX))
            build_system_name = groups [i] + strlen (IDE_RECENT_PROJECTS_BUILD_SYSTEM_GROUP_PREFIX);
          else if (g_str_has_prefix (groups [i], IDE_RECENT_PROJECTS_BUILD_SYSTEM_HINT_GROUP_PREFIX))
            build_system_hint = groups [i] + strlen (IDE_RECENT_PROJECTS_BUILD_SYSTEM_HINT_GROUP_PREFIX);
        }

      /* Cleanup any extra space */
      for (guint i = 0; i < languages->len; i++)
        g_strstrip ((gchar *)g_ptr_array_index (languages, i));
      g_ptr_array_add (languages, NULL);

      project_info = g_object_new (IDE_TYPE_PROJECT_INFO,
                                   "build-system-hint", build_system_hint,
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
ide_recent_projects_dispose (GObject *object)
{
  IdeRecentProjects *self = (IdeRecentProjects *)object;

  g_clear_pointer (&self->projects, g_sequence_free);
  g_clear_pointer (&self->recent_uris, g_hash_table_unref);
  g_clear_pointer (&self->file_uri, g_free);

  G_OBJECT_CLASS (ide_recent_projects_parent_class)->dispose (object);
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
  object_class->dispose = ide_recent_projects_dispose;
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
      guint position;

      g_assert (IDE_IS_PROJECT_INFO (project_info));
      g_assert (self->projects != NULL);

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

      file = _ide_project_info_get_real_file (project_info);
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

      position = g_sequence_iter_get_position (iter);
      g_sequence_remove (iter);
      g_list_model_items_changed (G_LIST_MODEL (self), position, 1, 0);
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

static gboolean
ide_recent_projects_reload_in_idle_cb (gpointer user_data)
{
  IdeRecentProjects *self = user_data;
  g_autoptr(GSequence) sequence = NULL;
  g_autoptr(GHashTable) hashtable = NULL;
  guint n_items;

  g_assert (IDE_IS_RECENT_PROJECTS (self));

  sequence = g_steal_pointer (&self->projects);
  self->projects = g_sequence_new (g_object_unref);

  hashtable = g_steal_pointer (&self->recent_uris);
  self->recent_uris = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  /* Notify of removals, so we can re-add new items next */
  n_items = g_sequence_get_length (sequence);
  g_list_model_items_changed (G_LIST_MODEL (self), 0, n_items, 0);

  ide_recent_projects_load_recent (self);

  self->reloading = FALSE;

  return G_SOURCE_REMOVE;
}

void
ide_recent_projects_invalidate (IdeRecentProjects *self)
{

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_RECENT_PROJECTS (self));

  if (self->reloading)
    return;

  /* Reload from timeout so higher priority operations can finish
   * before yielding to main loop (since this is all currently
   * done synchronously). In practice the file will still be hot
   * in the page cache, so relatively fast regardless.
   *
   * The main reason for timeout over idle is to increase the chance
   * that we are not going to mess with the GtkListView of projects
   * by causing items-changed to occur.
   */
  self->reloading = TRUE;
  g_timeout_add_seconds_full (G_PRIORITY_LOW + 1000,
                              INVALIDATE_DELAY_SECONDS,
                              ide_recent_projects_reload_in_idle_cb,
                              g_object_ref (self),
                              g_object_unref);
}
