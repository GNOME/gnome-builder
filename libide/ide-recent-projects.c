/* ide-recent-projects.c
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

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "ide-project-miner.h"
#include "ide-recent-projects.h"

struct _IdeRecentProjects
{
  GObject       parent_instance;

  GCancellable *cancellable;
  GPtrArray    *miners;
  GPtrArray    *projects;
  GHashTable   *recent_uris;

  gint          active;

  guint         discovered : 1;
};

G_DEFINE_TYPE (IdeRecentProjects, ide_recent_projects, G_TYPE_OBJECT)

enum {
  ADDED,
  LAST_SIGNAL
};

static guint gSignals [LAST_SIGNAL];

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
      g_ptr_array_add (self->projects, g_object_ref (project_info));
      g_signal_emit (self, gSignals [ADDED], 0, project_info);
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

static void
ide_recent_projects_load_recent (IdeRecentProjects *self,
                                 GtkRecentManager  *recent_manager)
{
  GList *iter;
  GList *list;

  g_assert (IDE_IS_RECENT_PROJECTS (self));
  g_assert (GTK_IS_RECENT_MANAGER (recent_manager));

  list = gtk_recent_manager_get_items (recent_manager);

  for (iter = list; iter; iter = iter->next)
    {
      g_autoptr(GDateTime) last_modified_at = NULL;
      g_autoptr(GFile) project_file = NULL;
      g_autoptr(GFile) directory = NULL;
      g_autoptr(IdeProjectInfo) project_info = NULL;
      GtkRecentInfo *recent_info = iter->data;
      const gchar *uri;
      const gchar *name;
      time_t modified;
      gchar **groups;
      gsize len;
      gsize i;

      groups = gtk_recent_info_get_groups (recent_info, &len);

      for (i = 0; i < len; i++)
        {
          if (g_str_equal (groups [i], "X-GNOME-Builder-Project"))
            goto is_project;
        }

      continue;

    is_project:
      name = gtk_recent_info_get_display_name (recent_info);
      modified = gtk_recent_info_get_modified (recent_info);
      last_modified_at = g_date_time_new_from_unix_local (modified);
      uri = gtk_recent_info_get_uri (recent_info);
      project_file = g_file_new_for_uri (uri);
      directory = g_file_get_parent (project_file);

      project_info = g_object_new (IDE_TYPE_PROJECT_INFO,
                                   "directory", directory,
                                   "file", project_file,
                                   "last-modified-at", last_modified_at,
                                   "name", name,
                                   NULL);

      ide_recent_projects_added (self, project_info);

      g_hash_table_insert (self->recent_uris, g_strdup (uri), NULL);
    }

  g_list_free_full (list, (GDestroyNotify)gtk_recent_info_unref);
}

static void
ide_recent_projects_finalize (GObject *object)
{
  IdeRecentProjects *self = (IdeRecentProjects *)object;

  g_clear_pointer (&self->miners, g_ptr_array_unref);
  g_clear_pointer (&self->projects, g_ptr_array_unref);
  g_clear_pointer (&self->recent_uris, g_hash_table_unref);
  g_clear_object (&self->cancellable);

  G_OBJECT_CLASS (ide_recent_projects_parent_class)->finalize (object);
}

static void
ide_recent_projects_class_init (IdeRecentProjectsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_recent_projects_finalize;

  /**
   * IdeRecentProjects::added:
   * @self: An #IdeRecentProjects
   * @project_info: An #IdeProjectInfo.
   *
   * The "added" signal is emitted when a new #IdeProjectInfo has been discovered.
   */
  gSignals [ADDED] =
    g_signal_new ("added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1,
                  IDE_TYPE_PROJECT_INFO);
}

static void
ide_recent_projects_init (IdeRecentProjects *self)
{
  GIOExtensionPoint *extension_point;
  GList *extensions;

  self->projects = g_ptr_array_new_with_free_func (g_object_unref);
  self->miners = g_ptr_array_new_with_free_func (g_object_unref);
  self->cancellable = g_cancellable_new ();
  self->recent_uris = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  extension_point = g_io_extension_point_lookup (IDE_PROJECT_MINER_EXTENSION_POINT);
  extensions = g_io_extension_point_get_extensions (extension_point);

  for (; extensions; extensions = extensions->next)
    {
      IdeProjectMiner *miner;
      GIOExtension *extension = extensions->data;
      GType type_id;

      type_id = g_io_extension_get_type (extension);

      if (!g_type_is_a (type_id, IDE_TYPE_PROJECT_MINER))
        {
          g_warning ("%s is not an IdeProjectMiner", g_type_name (type_id));
          continue;
        }

      miner = g_object_new (type_id, NULL);
      ide_recent_projects_add_miner (self, miner);
      g_object_unref (miner);
    }
}

/**
 * ide_recent_projects_get_projects:
 *
 * Gets a #GPtrArray containing the #IdeProjectInfo that have been discovered.
 *
 * Returns: (transfer container) (element-type IdeProjectInfo*): A #GPtrArray of #IdeProjectInfo.
 */
GPtrArray *
ide_recent_projects_get_projects (IdeRecentProjects *self)
{
  GPtrArray *ret;
  gsize i;

  g_return_val_if_fail (IDE_IS_RECENT_PROJECTS (self), NULL);

  ret = g_ptr_array_new_with_free_func (g_object_unref);

  for (i = 0; i < self->projects->len; i++)
    {
      IdeProjectInfo *project_info;

      project_info = g_ptr_array_index (self->projects, i);
      g_ptr_array_add (ret, g_object_ref (project_info));
    }

  return ret;
}

void
ide_recent_projects_discover_async (IdeRecentProjects   *self,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  GtkRecentManager *recent_manager;
  g_autoptr(GTask) task = NULL;
  gsize i;

  g_return_if_fail (IDE_IS_RECENT_PROJECTS (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

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

  recent_manager = gtk_recent_manager_get_default ();
  ide_recent_projects_load_recent (self, recent_manager);

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
