/* ide-git-vcs.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>
#include <libgit2-glib/ggit.h>

#include "ide-context.h"
#include "ide-git-vcs.h"
#include "ide-project.h"
#include "ide-project-file.h"
#include "ide-project-files.h"

typedef struct
{
  GgitRepository *repository;
} IdeGitVcsPrivate;

static void g_async_initable_init_interface (GAsyncInitableIface *iface);

G_DEFINE_TYPE_EXTENDED (IdeGitVcs, ide_git_vcs, IDE_TYPE_VCS, 0,
                        G_ADD_PRIVATE (IdeGitVcs)
                        G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE,
                                               g_async_initable_init_interface))

enum {
  PROP_0,
  PROP_REPOSITORY,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

GgitRepository *
ide_git_vcs_get_repository (IdeGitVcs *vcs)
{
  IdeGitVcsPrivate *priv = ide_git_vcs_get_instance_private (vcs);

  g_return_val_if_fail (IDE_IS_GIT_VCS (vcs), NULL);

  return priv->repository;
}

static void
ide_git_vcs_finalize (GObject *object)
{
  IdeGitVcs *self = (IdeGitVcs *)object;
  IdeGitVcsPrivate *priv = ide_git_vcs_get_instance_private (self);

  g_clear_object (&priv->repository);

  G_OBJECT_CLASS (ide_git_vcs_parent_class)->finalize (object);
}

static void
ide_git_vcs_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  IdeGitVcs *self = IDE_GIT_VCS (object);

  switch (prop_id)
    {
    case PROP_REPOSITORY:
      g_value_set_object (value, ide_git_vcs_get_repository (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_git_vcs_class_init (IdeGitVcsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_git_vcs_finalize;
  object_class->get_property = ide_git_vcs_get_property;

  gParamSpecs [PROP_REPOSITORY] =
    g_param_spec_object ("repository",
                         _("Repository"),
                         _("The git repository for the project."),
                         GGIT_TYPE_REPOSITORY,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_REPOSITORY,
                                   gParamSpecs [PROP_REPOSITORY]);
}

static void
ide_git_vcs_init (IdeGitVcs *self)
{
}

static void
ide_git_vcs_reload_index_add_path (IdeGitVcs   *self,
                                   GHashTable  *cache,
                                   const gchar *path,
                                   gboolean     is_directory)
{
  IdeProjectItem *parent;
  IdeProjectItem *item;
  IdeContext *context;
  GFileInfo *file_info = NULL;
  gchar *dir;
  gchar *name;

  g_return_if_fail (IDE_IS_GIT_VCS (self));
  g_return_if_fail (cache);
  g_return_if_fail (path);

  context = ide_object_get_context (IDE_OBJECT (self));

  dir = g_path_get_dirname (path);
  name = g_path_get_basename (path);

  parent = g_hash_table_lookup (cache, dir);

  if (!parent)
    {
      ide_git_vcs_reload_index_add_path (self, cache, dir, TRUE);
      parent = g_hash_table_lookup (cache, dir);
    }

  g_assert (IDE_IS_PROJECT_ITEM (parent));

  file_info = g_file_info_new ();
  g_file_info_set_name (file_info, name);
  g_file_info_set_display_name (file_info, name);

  /*
   * TODO: We can probably extract some additional information from the
   *       index such as symbolic link, etc.
   */
  if (is_directory)
    {
      g_file_info_set_file_type (file_info, G_FILE_TYPE_DIRECTORY);
    }

  item = g_object_new (IDE_TYPE_PROJECT_FILE,
                       "context", context,
                       "parent", parent,
                       "file-info", file_info,
                       NULL);
  ide_project_item_append (parent, item);

  g_hash_table_insert (cache, g_strdup (path), g_object_ref (item));

  g_clear_object (&file_info);
  g_clear_object (&item);
  g_clear_pointer (&dir, g_free);
  g_clear_pointer (&name, g_free);
}

static gboolean
ide_git_vcs_reload_index (IdeGitVcs  *self,
                          GError    **error)
{
  IdeGitVcsPrivate *priv = ide_git_vcs_get_instance_private (self);
  GgitIndexEntries *entries = NULL;
  IdeProjectItem *root;
  IdeProjectItem *files = NULL;
  IdeContext *context;
  IdeProject *project;
  GgitIndex *index = NULL;
  GHashTable *cache = NULL;
  gboolean ret = FALSE;
  guint count;
  guint i;

  g_return_val_if_fail (IDE_IS_GIT_VCS (self), FALSE);

  index = ggit_repository_get_index (priv->repository, error);
  if (!index)
    goto cleanup;

  entries = ggit_index_get_entries (index);
  if (!entries)
    goto cleanup;

  count = ggit_index_entries_size (entries);
  cache = g_hash_table_new_full (g_str_hash, g_str_equal,
                                 g_free, g_object_unref);

  context = ide_object_get_context (IDE_OBJECT (self));
  project = ide_context_get_project (context);
  root = ide_project_get_root (project);
  files = g_object_new (IDE_TYPE_PROJECT_FILES,
                        "context", context,
                        "parent", root,
                        NULL);
  ide_project_item_append (root, files);

  g_hash_table_insert (cache, g_strdup ("."), g_object_ref (files));

  for (i = 0; i < count; i++)
    {
      GgitIndexEntry *entry;
      const gchar *path;

      entry = ggit_index_entries_get_by_index (entries, i);
      path = ggit_index_entry_get_path (entry);

      ide_git_vcs_reload_index_add_path (self, cache, path, FALSE);

      ggit_index_entry_unref (entry);
    }

  ret = TRUE;

cleanup:
  g_clear_pointer (&cache, g_hash_table_unref);
  g_clear_pointer (&entries, ggit_index_entries_unref);
  g_clear_object (&files);
  g_clear_object (&index);

  return ret;
}

static void
ide_git_vcs_init_worker (GTask        *task,
                         gpointer      source_object,
                         gpointer      task_data,
                         GCancellable *cancellable)
{
  IdeGitVcsPrivate *priv;
  GgitRepository *repository = NULL;
  IdeGitVcs *self = source_object;
  GError *error = NULL;
  GFile *directory = task_data;
  GFile *location = NULL;

  g_return_if_fail (G_IS_TASK (task));
  g_return_if_fail (IDE_IS_GIT_VCS (self));
  g_return_if_fail (G_IS_FILE (directory));

  priv = ide_git_vcs_get_instance_private (self);

  location = ggit_repository_discover (directory, &error);

  if (!location)
    {
      g_task_return_error (task, error);
      goto cleanup;
    }

  repository = ggit_repository_open (location, &error);

  if (!repository)
    {
      g_task_return_error (task, error);
      goto cleanup;
    }

  priv->repository = g_object_ref (repository);

  if (!ide_git_vcs_reload_index (self, &error))
    {
      g_task_return_error (task, error);
      goto cleanup;
    }

  g_task_return_boolean (task, TRUE);

cleanup:
  g_clear_object (&location);
  g_clear_object (&repository);
}

static void
ide_git_vcs_init_async (GAsyncInitable      *initable,
                        int                  io_priority,
                        GCancellable        *cancellable,
                        GAsyncReadyCallback  callback,
                        gpointer             user_data)
{
  IdeContext *context;
  IdeGitVcs *self = (IdeGitVcs *)initable;
  GFile *project_file;
  GTask *task;

  g_return_if_fail (IDE_IS_GIT_VCS (self));

  context = ide_object_get_context (IDE_OBJECT (initable));
  project_file = ide_context_get_project_file (context);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_task_data (task, g_object_ref (project_file), g_object_unref);
  g_task_run_in_thread (task, ide_git_vcs_init_worker);
  g_object_unref (task);
}

static gboolean
ide_git_vcs_init_finish (GAsyncInitable  *initable,
                         GAsyncResult    *result,
                         GError         **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (G_IS_TASK (task), FALSE);

  return g_task_propagate_boolean (task, error);
}

static void
g_async_initable_init_interface (GAsyncInitableIface *iface)
{
  iface->init_async = ide_git_vcs_init_async;
  iface->init_finish = ide_git_vcs_init_finish;
}
