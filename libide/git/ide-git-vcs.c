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

#include "ide-async-helper.h"
#include "ide-context.h"
#include "ide-git-buffer-change-monitor.h"
#include "ide-git-vcs.h"
#include "ide-project.h"
#include "ide-project-file.h"
#include "ide-project-files.h"

#define DEFAULT_CHANGED_TIMEOUT_SECS 1

struct _IdeGitVcs
{
  IdeVcs parent_instance;

  GgitRepository *repository;
  GgitRepository *change_monitor_repository;
  GFile          *working_directory;
  GFileMonitor   *monitor;

  guint           changed_timeout;

  guint           reloading : 1;
  guint           loaded_files : 1;
};

static void     g_async_initable_init_interface (GAsyncInitableIface  *iface);
static void     ide_git_vcs_reload_async        (IdeGitVcs            *self,
                                                 GCancellable         *cancellable,
                                                 GAsyncReadyCallback   callback,
                                                 gpointer              user_data);
static gboolean ide_git_vcs_reload_finish       (IdeGitVcs            *self,
                                                 GAsyncResult         *result,
                                                 GError              **error);

G_DEFINE_TYPE_EXTENDED (IdeGitVcs, ide_git_vcs, IDE_TYPE_VCS, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE,
                                               g_async_initable_init_interface))

enum {
  PROP_0,
  PROP_REPOSITORY,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

/**
 * ide_git_vcs_get_repository:
 *
 * Retrieves the underlying #GgitRepository used by @vcs.
 *
 * Returns: (transfer none): A #GgitRepository.
 */
GgitRepository *
ide_git_vcs_get_repository (IdeGitVcs *self)
{
  g_return_val_if_fail (IDE_IS_GIT_VCS (self), NULL);

  return self->repository;
}

static GFile *
ide_git_vcs_get_working_directory (IdeVcs *vcs)
{
  IdeGitVcs *self = (IdeGitVcs *)vcs;

  g_return_val_if_fail (IDE_IS_GIT_VCS (self), NULL);

  return self->working_directory;
}

static IdeBufferChangeMonitor *
ide_git_vcs_get_buffer_change_monitor (IdeVcs    *vcs,
                                       IdeBuffer *buffer)
{
  IdeGitVcs *self = (IdeGitVcs *)vcs;
  IdeContext *context;

  g_return_val_if_fail (IDE_IS_GIT_VCS (vcs), NULL);

  context = ide_object_get_context (IDE_OBJECT (vcs));

  return g_object_new (IDE_TYPE_GIT_BUFFER_CHANGE_MONITOR,
                       "buffer", buffer,
                       "context", context,
                       "repository", self->change_monitor_repository,
                       NULL);
}

static void
ide_git_vcs_load_repository_worker (GTask        *task,
                                    gpointer      source_object,
                                    gpointer      task_data,
                                    GCancellable *cancellable)
{
  GFile *project_file = task_data;
  g_autoptr(GFile) location = NULL;
  GgitRepository *repository = NULL;
  GError *error = NULL;

  g_assert (G_IS_TASK (task));
  g_assert (G_IS_FILE (project_file));

  location = ggit_repository_discover (project_file, &error);

  if (!location)
    {
      g_task_return_error (task, error);
      return;
    }

  repository = ggit_repository_open (location, &error);

  if (!repository)
    {
      g_task_return_error (task, error);
      return;
    }

  g_task_return_pointer (task, repository, g_object_unref);
}

static void
ide_git_vcs_load_repository_async (IdeGitVcs           *self,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  IdeContext *context;
  GFile *project_file;

  g_assert (IDE_IS_GIT_VCS (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  context = ide_object_get_context (IDE_OBJECT (self));
  project_file = ide_context_get_project_file (context);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_task_data (task, g_object_ref (project_file), g_object_unref);
  g_task_run_in_thread (task, ide_git_vcs_load_repository_worker);
}

static GgitRepository *
ide_git_vcs_load_repository_finish (IdeGitVcs     *self,
                                    GAsyncResult  *result,
                                    GError       **error)
{
  GTask *task = (GTask *)result;
  GgitRepository *ret;

  g_assert (IDE_IS_GIT_VCS (self));

  ret = g_task_propagate_pointer (task, error);

  if (ret)
    {
      GFile *working_directory;

      working_directory = ggit_repository_get_workdir (ret);
      g_set_object (&self->working_directory, working_directory);
    }

  return ret;
}

static void
ide_git_vcs_reload_index_add_path (IdeGitVcs   *self,
                                   GHashTable  *cache,
                                   const gchar *path,
                                   const gchar *workdir,
                                   gboolean     is_directory)
{
  IdeProjectItem *parent;
  IdeProjectItem *item;
  IdeContext *context;
  GFileInfo *file_info = NULL;
  GFile *file = NULL;
  g_autofree gchar *fullpath = NULL;
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
      ide_git_vcs_reload_index_add_path (self, cache, dir, workdir, TRUE);
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
    g_file_info_set_file_type (file_info, G_FILE_TYPE_DIRECTORY);

  fullpath = g_build_filename (workdir, path, NULL);
  file = g_file_new_for_path (fullpath);

  item = g_object_new (IDE_TYPE_PROJECT_FILE,
                       "context", context,
                       "file", file,
                       "file-info", file_info,
                       "parent", parent,
                       "path", path,
                       NULL);
  ide_project_item_append (parent, item);

  g_hash_table_insert (cache, g_strdup (path), g_object_ref (item));

  g_clear_object (&file);
  g_clear_object (&file_info);
  g_clear_object (&item);
  g_clear_pointer (&dir, g_free);
  g_clear_pointer (&name, g_free);
}

static void
ide_git_vcs_build_tree_worker (GTask        *task,
                               gpointer      source_object,
                               gpointer      task_data,
                               GCancellable *cancellable)
{
  IdeGitVcs *self = source_object;
  GgitIndexEntries *entries = NULL;
  GgitRepository *repository = task_data;
  IdeProjectItem *root;
  IdeProjectItem *files = NULL;
  IdeContext *context;
  IdeProject *project;
  GgitIndex *index = NULL;
  GHashTable *cache = NULL;
  g_autofree gchar *workdir = NULL;
  g_autoptr(GFile) workdir_file = NULL;
  GError *error = NULL;
  guint count;
  guint i;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_GIT_VCS (self));
  g_assert (GGIT_IS_REPOSITORY (repository));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  index = ggit_repository_get_index (repository, &error);
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

  ide_project_reader_lock (project);
  root = ide_project_get_root (project);
  files = g_object_new (IDE_TYPE_PROJECT_FILES,
                        "context", context,
                        "parent", root,
                        NULL);
  ide_project_reader_unlock (project);

  g_hash_table_insert (cache, g_strdup ("."), g_object_ref (files));

  workdir_file = ggit_repository_get_workdir (repository);
  workdir = g_file_get_path (workdir_file);

  for (i = 0; i < count; i++)
    {
      GgitIndexEntry *entry;
      const gchar *path;

      entry = ggit_index_entries_get_by_index (entries, i);
      path = ggit_index_entry_get_path (entry);
      ide_git_vcs_reload_index_add_path (self, cache, path, workdir, FALSE);
      ggit_index_entry_unref (entry);
    }

cleanup:
  if (error)
    g_task_return_error (task, error);
  else
    g_task_return_pointer (task, g_object_ref (files), g_object_unref);

  g_clear_pointer (&cache, g_hash_table_unref);
  g_clear_pointer (&entries, ggit_index_entries_unref);
  g_clear_object (&files);
  g_clear_object (&index);
}

static void
ide_git_vcs_build_tree_async (IdeGitVcs           *self,
                              GgitRepository      *repository,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_assert (IDE_IS_GIT_VCS (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_task_data (task, g_object_ref (repository), g_object_unref);
  g_task_run_in_thread (task, ide_git_vcs_build_tree_worker);
}

static IdeProjectFiles *
ide_git_vcs_build_tree_finish (IdeGitVcs     *self,
                               GAsyncResult  *result,
                               GError       **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (IDE_IS_GIT_VCS (self), NULL);

  return g_task_propagate_pointer (task, error);
}

static void
ide_git_vcs__reload_cb (GObject      *object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  IdeGitVcs *self = (IdeGitVcs *)object;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_GIT_VCS (self));

  if (!ide_git_vcs_reload_finish (self, result, &error))
    g_message ("%s", error->message);
}


static gboolean
ide_git_vcs__changed_timeout_cb (gpointer user_data)
{
  IdeGitVcs *self = user_data;

  g_assert (IDE_IS_GIT_VCS (self));

  self->changed_timeout = 0;

  ide_git_vcs_reload_async (self, NULL, ide_git_vcs__reload_cb, NULL);

  return G_SOURCE_REMOVE;
}

static void
ide_git_vcs__monitor_changed_cb (IdeGitVcs         *self,
                                 GFile             *file,
                                 GFile             *other_file,
                                 GFileMonitorEvent  event_type,
                                 gpointer           user_data)
{
  g_assert (IDE_IS_GIT_VCS (self));

  if (event_type == G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT)
    {
      if (self->changed_timeout)
        g_source_remove (self->changed_timeout);

      self->changed_timeout = g_timeout_add_seconds (DEFAULT_CHANGED_TIMEOUT_SECS,
                                                     ide_git_vcs__changed_timeout_cb,
                                                     self);
    }
}

static gboolean
ide_git_vcs_load_monitor (IdeGitVcs  *self,
                          GError    **error)
{
  gboolean ret = TRUE;

  g_assert (IDE_IS_GIT_VCS (self));

  if (!self->monitor)
    {
      g_autoptr(GFile) location = NULL;
      g_autoptr(GFileMonitor) monitor = NULL;
      g_autoptr(GFile) index_file = NULL;

      location = ggit_repository_get_location (self->repository);
      index_file = g_file_get_child (location, "index");
      monitor = g_file_monitor_file (index_file, G_FILE_MONITOR_NONE, NULL, error);

      ret = !!monitor;

      if (monitor)
        {
          g_signal_connect_object (monitor,
                                   "changed",
                                   G_CALLBACK (ide_git_vcs__monitor_changed_cb),
                                   self,
                                   G_CONNECT_SWAPPED);

          self->monitor = g_object_ref (monitor);
        }
    }

  return ret;
}

static void
ide_git_vcs_reload__load_repository_cb3 (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
  IdeGitVcs *self = (IdeGitVcs *)object;
  g_autoptr(GTask) task = user_data;
  GgitRepository *repository;
  GError *error = NULL;

  g_assert (IDE_IS_GIT_VCS (self));
  g_assert (G_IS_ASYNC_RESULT (result));

  repository = ide_git_vcs_load_repository_finish (self, result, &error);

  if (!repository)
    {
      g_task_return_error (task, error);
      return;
    }

  g_set_object (&self->change_monitor_repository, repository);

  /*
   * Now finally, load the change monitor so that we can detect future changes.
   */
  if (!ide_git_vcs_load_monitor (self, &error))
    {
      g_task_return_error (task, error);
      return;
    }

  g_task_return_boolean (task, TRUE);
}

static void
ide_git_vcs_reload__build_tree_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  IdeGitVcs *self = (IdeGitVcs *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(IdeProjectFiles) files = NULL;
  GError *error = NULL;

  g_assert (IDE_IS_GIT_VCS (self));
  g_assert (G_IS_TASK (task));

  files = ide_git_vcs_build_tree_finish (self, result, &error);

  if (!files)
    {
      g_task_return_error (task, error);
      return;
    }

  /*
   * XXX:
   *
   * This is a hack to only load the project files the first time. We need to do this for real
   * in the project tree to make appropriate events for tree changes.
   */
  if (!self->loaded_files)
    {
      IdeContext *context;
      IdeProject *project;
      IdeProjectItem *root;

      context = ide_object_get_context (IDE_OBJECT (self));
      project = ide_context_get_project (context);

      ide_project_writer_lock (project);
      root = ide_project_get_root (project);
      /* TODO: Replace existing item!!! */
      ide_project_item_append (root, IDE_PROJECT_ITEM (files));
      ide_project_writer_unlock (project);

      self->loaded_files = TRUE;
    }

  /*
   * Load the repository a third time for use by the threaded change monitors generating diffs.
   * I know it seems like a lot of loading, but it is a lot better than the N_FILES repositories
   * we had open previously.
   */
  ide_git_vcs_load_repository_async (self,
                                     g_task_get_cancellable (task),
                                     ide_git_vcs_reload__load_repository_cb3,
                                     g_object_ref (task));
}

static void
ide_git_vcs_reload__load_repository_cb2 (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
  IdeGitVcs *self = (IdeGitVcs *)object;
  g_autoptr(GTask) task = user_data;
  GgitRepository *repository;
  GError *error = NULL;

  g_assert (IDE_IS_GIT_VCS (self));
  g_assert (G_IS_ASYNC_RESULT (result));

  repository = ide_git_vcs_load_repository_finish (self, result, &error);

  if (!repository)
    {
      g_task_return_error (task, error);
      return;
    }

  /*
   * Now go load the files for the project tree.
   */
  ide_git_vcs_build_tree_async (self,
                                repository,
                                g_task_get_cancellable (task),
                                ide_git_vcs_reload__build_tree_cb,
                                g_object_ref (task));

  g_clear_object (&repository);
}

static void
ide_git_vcs_reload__load_repository_cb (GObject      *object,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
  IdeGitVcs *self = (IdeGitVcs *)object;
  g_autoptr(GTask) task = user_data;
  GgitRepository *repository;
  GError *error = NULL;

  g_assert (IDE_IS_GIT_VCS (self));
  g_assert (G_IS_ASYNC_RESULT (result));

  repository = ide_git_vcs_load_repository_finish (self, result, &error);

  if (!repository)
    {
      g_task_return_error (task, error);
      return;
    }

  g_set_object (&self->repository, repository);

  /*
   * Now load the repository again for use by the threaded index builder.
   */
  ide_git_vcs_load_repository_async (self,
                                     g_task_get_cancellable (task),
                                     ide_git_vcs_reload__load_repository_cb2,
                                     g_object_ref (task));
}

static void
ide_git_vcs_reload_async (IdeGitVcs           *self,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_assert (IDE_IS_GIT_VCS (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  if (self->reloading)
    {
      /*
       * Ignore if we are already reloading. We should probably set a bit here and attept to
       * reload again after the current process completes.
       */
      g_task_return_boolean (task, TRUE);
      return;
    }

  self->reloading = TRUE;

  ide_git_vcs_load_repository_async (self,
                                     NULL,
                                     ide_git_vcs_reload__load_repository_cb,
                                     g_object_ref (task));
}

static gboolean
ide_git_vcs_reload_finish (IdeGitVcs     *self,
                           GAsyncResult  *result,
                           GError       **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (IDE_IS_GIT_VCS (self), FALSE);

  self->reloading = FALSE;

  return g_task_propagate_boolean (task, error);
}

static void
ide_git_vcs_dispose (GObject *object)
{
  IdeGitVcs *self = (IdeGitVcs *)object;

  if (self->changed_timeout)
    {
      g_source_remove (self->changed_timeout);
      self->changed_timeout = 0;
    }

  if (self->monitor)
    {
      if (!g_file_monitor_is_cancelled (self->monitor))
        g_file_monitor_cancel (self->monitor);
      g_clear_object (&self->monitor);
    }

  g_clear_object (&self->change_monitor_repository);
  g_clear_object (&self->repository);
  g_clear_object (&self->working_directory);

  G_OBJECT_CLASS (ide_git_vcs_parent_class)->dispose (object);
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
  IdeVcsClass *vcs_class = IDE_VCS_CLASS (klass);

  object_class->dispose = ide_git_vcs_dispose;
  object_class->get_property = ide_git_vcs_get_property;

  vcs_class->get_working_directory = ide_git_vcs_get_working_directory;
  vcs_class->get_buffer_change_monitor = ide_git_vcs_get_buffer_change_monitor;

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
ide_git_vcs_init_async__reload_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  IdeGitVcs *self = (IdeGitVcs *)object;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_GIT_VCS (self));

  if (!ide_git_vcs_reload_finish (self, result, &error))
    g_task_return_error (task, error);
  else
    g_task_return_boolean (task, TRUE);
}

static void
ide_git_vcs_init_async (GAsyncInitable      *initable,
                        int                  io_priority,
                        GCancellable        *cancellable,
                        GAsyncReadyCallback  callback,
                        gpointer             user_data)
{
  IdeGitVcs *self = (IdeGitVcs *)initable;
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (IDE_IS_GIT_VCS (self));

  task = g_task_new (self, cancellable, callback, user_data);
  ide_git_vcs_reload_async (self,
                            cancellable,
                            ide_git_vcs_init_async__reload_cb,
                            g_object_ref (task));
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
