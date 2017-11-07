/* ide-recent-projects.c
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libpeas/peas.h>

#include "ide-global.h"
#include "projects/ide-project-miner.h"
#include "projects/ide-recent-projects.h"

struct _IdeRecentProjects
{
  GObject       parent_instance;

  GCancellable *cancellable;
  GPtrArray    *miners;
  GSequence    *projects;
  GHashTable   *recent_uris;
  gchar        *file_uri;

  gint          active;

  guint         discovered : 1;
};

#define MAX_PROJECT_INFOS 100

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (IdeRecentProjects, ide_recent_projects, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL,
                                                list_model_iface_init))

IdeRecentProjects *
ide_recent_projects_new (void)
{
  return g_object_new (IDE_TYPE_RECENT_PROJECTS, NULL);
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

static void
ide_recent_projects__miner_discovered (IdeRecentProjects *self,
                                       IdeProjectInfo    *project_info,
                                       IdeProjectMiner   *miner)
{
  g_assert (IDE_IS_PROJECT_MINER (miner));
  g_assert (IDE_IS_RECENT_PROJECTS (self));
  g_assert (IDE_IS_PROJECT_INFO (project_info));

  ide_recent_projects_added (self, project_info);
}

static void
ide_recent_projects__miner_mine_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  IdeRecentProjects *self;
  g_autoptr(GTask) task = user_data;
  IdeProjectMiner *miner = (IdeProjectMiner *)object;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_PROJECT_MINER (miner));
  self = g_task_get_source_object (task);
  g_assert (IDE_IS_RECENT_PROJECTS (self));

  ide_project_miner_mine_finish (miner, result, NULL);

  if (--self->active == 0)
    g_task_return_boolean (task, TRUE);
}

static void
ide_recent_projects_add_miner (IdeRecentProjects *self,
                               IdeProjectMiner   *miner)
{
  g_assert (IDE_IS_RECENT_PROJECTS (self));
  g_assert (IDE_IS_PROJECT_MINER (miner));

  g_signal_connect_object (miner,
                           "discovered",
                           G_CALLBACK (ide_recent_projects__miner_discovered),
                           self,
                           G_CONNECT_SWAPPED);

  g_ptr_array_add (self->miners, g_object_ref (miner));
}

static GBookmarkFile *
ide_recent_projects_get_bookmarks (IdeRecentProjects  *self,
                                   GError            **error)
{
  GBookmarkFile *bookmarks;

  g_assert (IDE_IS_RECENT_PROJECTS (self));
  g_assert (error != NULL);

  bookmarks = g_bookmark_file_new ();

  if (!g_bookmark_file_load_from_file (bookmarks, self->file_uri, error))
    {
      if (!g_error_matches (*error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
        {
          g_object_unref (bookmarks);
          return NULL;
        }
    }

  return bookmarks;
}

static void
ide_recent_projects_load_recent (IdeRecentProjects *self)
{
  g_autoptr(GBookmarkFile) projects_file = NULL;
  g_autoptr(GError) error = NULL;
  gboolean needs_sync = FALSE;
  gchar **uris;
  gssize z;

  g_assert (IDE_IS_RECENT_PROJECTS (self));

  projects_file = ide_recent_projects_get_bookmarks (self, &error);

  if (projects_file == NULL)
    {
      g_warning ("Unable to open recent projects file: %s", error->message);
      return;
    }

  uris = g_bookmark_file_get_uris (projects_file, NULL);

  for (z = 0; uris[z]; z++)
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
      time_t modified;
      g_auto(GStrv) groups = NULL;
      gsize len;
      gsize i;

      groups = g_bookmark_file_get_groups (projects_file, uri, &len, NULL);

      for (i = 0; i < len; i++)
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
      directory = g_file_get_parent (project_file);

      languages = g_ptr_array_new ();
      for (i = 0; i < len; i++)
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
  IdeRecentProjects *self = (IdeRecentProjects *)model;

  g_assert (IDE_IS_RECENT_PROJECTS (self));

  return g_sequence_get_length (self->projects);
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
foreach_miner_func (PeasExtensionSet *set,
                    PeasPluginInfo   *plugin_info,
                    PeasExtension    *exten,
                    gpointer          user_data)
{
  IdeRecentProjects *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_PROJECT_MINER (exten));
  g_assert (IDE_IS_RECENT_PROJECTS (self));

  ide_recent_projects_add_miner (self, IDE_PROJECT_MINER (exten));
}

static void
ide_recent_projects_finalize (GObject *object)
{
  IdeRecentProjects *self = (IdeRecentProjects *)object;

  g_clear_pointer (&self->miners, g_ptr_array_unref);
  g_clear_pointer (&self->projects, g_sequence_free);
  g_clear_pointer (&self->recent_uris, g_hash_table_unref);
  g_clear_object (&self->cancellable);
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

  object_class->finalize = ide_recent_projects_finalize;
}

static void
ide_recent_projects_init (IdeRecentProjects *self)
{
  PeasExtensionSet *set;
  PeasEngine *engine;

  self->projects = g_sequence_new (g_object_unref);
  self->miners = g_ptr_array_new_with_free_func (g_object_unref);
  self->cancellable = g_cancellable_new ();
  self->recent_uris = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  self->file_uri = g_build_filename (g_get_user_data_dir (),
                                     ide_get_program_name (),
                                     IDE_RECENT_PROJECTS_BOOKMARK_FILENAME,
                                     NULL);

  engine = peas_engine_get_default ();
  set = peas_extension_set_new (engine, IDE_TYPE_PROJECT_MINER, NULL);
  peas_extension_set_foreach (set, foreach_miner_func, self);
  g_clear_object (&set);
}

/**
 * ide_recent_projects_get_projects:
 *
 * Gets a #GPtrArray containing the #IdeProjectInfo that have been discovered.
 *
 * Returns: (transfer container) (element-type IdeProjectInfo*): a #GPtrArray of #IdeProjectInfo.
 */
GPtrArray *
ide_recent_projects_get_projects (IdeRecentProjects *self)
{
  GSequenceIter *iter;
  GPtrArray *ret;

  g_return_val_if_fail (IDE_IS_RECENT_PROJECTS (self), NULL);

  ret = g_ptr_array_new_with_free_func (g_object_unref);

  for (iter = g_sequence_get_begin_iter (self->projects);
       !g_sequence_iter_is_end (iter);
       g_sequence_iter_next (iter))
    {
      IdeProjectInfo *project_info;

      project_info = g_sequence_get (iter);
      g_ptr_array_add (ret, g_object_ref (project_info));
    }

  return ret;
}

void
ide_recent_projects_discover_async (IdeRecentProjects   *self,
                                    gboolean             recent_only,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  gsize i;

  g_return_if_fail (IDE_IS_RECENT_PROJECTS (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_recent_projects_discover_async);
  g_task_set_priority (task, G_PRIORITY_LOW);

  if (self->discovered)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               _("%s() may only be executed once"),
                               G_STRFUNC);
      return;
    }

  self->discovered = TRUE;

  ide_recent_projects_load_recent (self);

  if (recent_only || g_getenv ("IDE_DO_NOT_SCAN_PROJECTS") != NULL)
    {
      g_task_return_boolean (task, TRUE);
      return;
    }

  self->active = self->miners->len;

  if (self->active == 0)
    {
      g_task_return_boolean (task, TRUE);
      return;
    }

  for (i = 0; i < self->miners->len; i++)
    {
      IdeProjectMiner *miner;

      miner = g_ptr_array_index (self->miners, i);
      ide_project_miner_mine_async (miner,
                                    self->cancellable,
                                    ide_recent_projects__miner_mine_cb,
                                    g_object_ref (task));
    }
}

gboolean
ide_recent_projects_discover_finish (IdeRecentProjects  *self,
                                     GAsyncResult       *result,
                                     GError            **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (IDE_IS_RECENT_PROJECTS (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (task), FALSE);

  return g_task_propagate_boolean (task, error);
}

/**
 * ide_recent_projects_remove:
 * @self: An #IdeRecentProjects
 * @project_infos: (transfer none) (element-type IdeProjectInfo): a #GList of #IdeProjectInfo.
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

  projects_file = ide_recent_projects_get_bookmarks (self, &error);

  if (projects_file == NULL)
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
