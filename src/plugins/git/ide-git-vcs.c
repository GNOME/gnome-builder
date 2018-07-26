/* ide-git-vcs.c
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-git-vcs"

#include <git2.h>
#include <glib/gi18n.h>
#include <libgit2-glib/ggit.h>

#include "ide-git-buffer-change-monitor.h"
#include "ide-git-vcs.h"
#include "ide-git-vcs-config.h"

#define DEFAULT_CHANGED_TIMEOUT_SECS 1

struct _IdeGitVcs
{
  IdeObject       parent_instance;

  /*
   * The repository_mutex is used to ensure that we only access the
   * self->repository instance from a single thread at a time. We
   * need to maintain this invariant for the thread-safety on
   * ide_vcs_is_ignored() to hold true.
   */
  GMutex          repository_mutex;
  GgitRepository *repository;

  /*
   * The change monitors all share a copy of the repository so that
   * they can avoid coordinating with our self->repository. Instead,
   * they have a single worker thread to maintain their thread-safety.
   */
  GgitRepository *change_monitor_repository;

  GFile          *working_directory;
  GFileMonitor   *monitor;

  /*
   * If we're in a worktree, we might have a permanent branch name
   * as we can't change branches.
   */
  gchar          *worktree_branch;

  guint           changed_timeout;

  guint           reloading : 1;
  guint           loaded_files : 1;
};

typedef struct
{
  GFile      *repository_location;
  GFile      *directory_or_file;
  GFile      *workdir;
  GListStore *store;
  guint       recursive : 1;
} ListStatus;

static void     g_async_initable_init_interface (GAsyncInitableIface  *iface);
static void     ide_git_vcs_init_iface          (IdeVcsInterface      *iface);
static void     ide_git_vcs_reload_async        (IdeGitVcs            *self,
                                                 GCancellable         *cancellable,
                                                 GAsyncReadyCallback   callback,
                                                 gpointer              user_data);
static gboolean ide_git_vcs_reload_finish       (IdeGitVcs            *self,
                                                 GAsyncResult         *result,
                                                 GError              **error);

G_DEFINE_TYPE_EXTENDED (IdeGitVcs, ide_git_vcs, IDE_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_VCS, ide_git_vcs_init_iface)
                        G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE,
                                               g_async_initable_init_interface))

enum {
  PROP_0,
  LAST_PROP,

  /* Override properties */
  PROP_BRANCH_NAME,
  PROP_WORKING_DIRECTORY,
};

enum {
  RELOADED,
  LAST_SIGNAL
};

static guint signals [LAST_SIGNAL];

static void
list_status_free (gpointer data)
{
  ListStatus *ls = data;

  g_clear_object (&ls->repository_location);
  g_clear_object (&ls->directory_or_file);
  g_clear_object (&ls->workdir);
  g_clear_object (&ls->store);
  g_slice_free (ListStatus, ls);
}

static GFile *
ide_git_vcs_get_working_directory (IdeVcs *vcs)
{
  IdeGitVcs *self = (IdeGitVcs *)vcs;

  g_return_val_if_fail (IDE_IS_GIT_VCS (self), NULL);

  /* Note: This function is expected to be thread-safe for
   *       those holding a reference to @vcs. So
   *       @working_directory cannot be changed after creation
   *       and must be valid for the lifetime of @vcs.
   */

  return self->working_directory;
}

static IdeVcsConfig *
ide_git_vcs_get_config (IdeVcs *vcs)
{
  g_return_val_if_fail (IDE_IS_GIT_VCS (vcs), NULL);

  return (IdeVcsConfig *)ide_git_vcs_config_new ();
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

/*
 * This is like ggit_repository_discover() but seems to work inside
 * of the flatpak runtime. Something is preventing git_repository_discover()
 * in libgit2 from working correctly, possibly the mount setup. Either
 * way, until it's fixed we can use this.
 */
static GFile *
ide_git_vcs_discover (IdeGitVcs  *self,
                      GFile      *file,
                      GError    **error)
{
  g_autofree gchar *name = NULL;
  g_autoptr(GFile) parent = NULL;
  g_autoptr(GFile) child = NULL;

  g_assert (IDE_IS_GIT_VCS (self));
  g_assert (G_IS_FILE (file));

  /*
   * TODO: Switch to using the new discover_full().
   */

  if (!g_file_is_native (file))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_SUPPORTED,
                   "Only native file systems are supported for git.");
      return NULL;
    }

  name = g_file_get_basename (file);

  if (g_strcmp0 (name, ".git") == 0)
    return g_object_ref (file);

  /*
   * Work around for in-tree tests which we do not
   * want to use the git backend.
   *
   * TODO: Allow options during context creation.
   */
  child = g_file_get_child (file, ".you-dont-git-me");

  if (g_file_query_exists (child, NULL))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_SUPPORTED,
                   "The project has blocked use of the git plugin");
      return NULL;
    }

  g_clear_object (&child);
  child = g_file_get_child (file, ".git");

  if (g_file_query_exists (child, NULL))
    return g_object_ref (child);

  parent = g_file_get_parent (file);

  if (parent == NULL || g_file_equal (parent, file))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_FOUND,
                   "Failed to discover git directory");
      return NULL;
    }

  return ide_git_vcs_discover (self, parent, error);
}

static GgitRepository *
ide_git_vcs_load (IdeGitVcs  *self,
                  GError    **error)
{
  g_autofree gchar *uri = NULL;
  g_autoptr(GFile) location = NULL;
  GgitRepository *repository = NULL;
  IdeContext *context;
  GFile *project_file;

  g_assert (IDE_IS_GIT_VCS (self));
  g_assert (error != NULL);

  context = ide_object_get_context (IDE_OBJECT (self));
  project_file = ide_context_get_project_file (context);

  if (!(location = ide_git_vcs_discover (self, project_file, error)))
    {
      if (g_error_matches (*error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED))
        return NULL;

      g_clear_error (error);

      /* Fallback to libgit2(-glib) discovery */
      if (!(location = ggit_repository_discover (project_file, error)))
        return NULL;
    }

  /* If @location is a regular file, we might have a git-worktree link */
  if (g_file_query_file_type (location, 0, NULL) == G_FILE_TYPE_REGULAR)
    {
      g_autofree gchar *contents = NULL;
      gsize len;

      if (g_file_load_contents (location, NULL, &contents, &len, NULL, NULL))
        {
          IdeLineReader reader;
          gchar *line;
          gsize line_len;

          ide_line_reader_init (&reader, contents, len);

          while ((line = ide_line_reader_next (&reader, &line_len)))
            {
              line[line_len] = 0;

              if (g_str_has_prefix (line, "gitdir: "))
                {
                  const gchar *branch;

                  g_clear_object (&location);
                  location = g_file_new_for_path (line + strlen ("gitdir: "));

                  /*
                   * Worktrees only have a single branch, and it is the name
                   * of the suffix of .git/worktrees/<name>
                   */
                  if (self->worktree_branch == NULL &&
                      (branch = strrchr (line, G_DIR_SEPARATOR)))
                    self->worktree_branch = g_strdup (branch + 1);

                  break;
                }
            }
        }
    }

  uri = g_file_get_uri (location);
  g_debug ("Discovered .git location at “%s”", uri);

  if (!(repository = ggit_repository_open (location, error)))
    return NULL;

  /* Note: Only set this once on load, otherwise not thread-safe. */
  if (self->working_directory == NULL)
    self->working_directory = ggit_repository_get_workdir (repository);

  return repository;
}

static void
handle_reload_from_changed_timeout (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  IdeGitVcs *self = (IdeGitVcs *)object;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_GIT_VCS (self));
  g_assert (G_IS_ASYNC_RESULT (result));

  /* Call finish() so that "changed" is emitted */
  if (!ide_git_vcs_reload_finish (self, result, &error))
    ide_object_warning (self, "git: %s", error->message);
}

static gboolean
ide_git_vcs__changed_timeout_cb (gpointer user_data)
{
  IdeGitVcs *self = user_data;

  IDE_ENTRY;

  g_assert (IDE_IS_GIT_VCS (self));

  self->changed_timeout = 0;

  ide_git_vcs_reload_async (self,
                            NULL,
                            handle_reload_from_changed_timeout,
                            NULL);

  IDE_RETURN (G_SOURCE_REMOVE);
}

static void
ide_git_vcs__monitor_changed_cb (IdeGitVcs         *self,
                                 GFile             *file,
                                 GFile             *other_file,
                                 GFileMonitorEvent  event_type,
                                 gpointer           user_data)
{
  g_autofree gchar *name = NULL;
  g_autofree gchar *other_name = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_GIT_VCS (self));
  g_assert (G_IS_FILE (file));
  g_assert (!other_file || G_IS_FILE (other_file));

  name = g_file_get_basename (file);

  if (other_file != NULL)
    other_name = g_file_get_basename (other_file);

  if (dzl_str_equal0 (name, "index") || dzl_str_equal0 (other_name, "index") ||
      dzl_str_equal0 (name, "HEAD") || dzl_str_equal0 (other_name, "HEAD"))
    {
      dzl_clear_source (&self->changed_timeout);
      self->changed_timeout = g_timeout_add_seconds (DEFAULT_CHANGED_TIMEOUT_SECS,
                                                     ide_git_vcs__changed_timeout_cb,
                                                     self);
    }

  IDE_EXIT;
}

static gboolean
ide_git_vcs_load_monitor_locked (IdeGitVcs  *self,
                                 GError    **error)
{
  gboolean ret = TRUE;

  g_assert (IDE_IS_GIT_VCS (self));

  if (self->monitor == NULL)
    {
      g_autoptr(GFile) location = NULL;
      g_autoptr(GFileMonitor) monitor = NULL;
      GError *local_error = NULL;

      location = ggit_repository_get_location (self->repository);

      monitor = g_file_monitor_directory (location,
                                          G_FILE_MONITOR_NONE,
                                          NULL,
                                          &local_error);

      if (monitor == NULL)
        {
          ide_object_warning (self,
                              /* translators: %s is replaced with the error message */
                              _("Failed to establish git file monitor: %s"),
                              local_error->message);
          g_propagate_error (error, local_error);
          ret = FALSE;
        }
      else
        {
          IDE_TRACE_MSG ("Git index monitor registered.");
          g_signal_connect_object (monitor,
                                   "changed",
                                   G_CALLBACK (ide_git_vcs__monitor_changed_cb),
                                   self,
                                   G_CONNECT_SWAPPED);
          self->monitor = g_steal_pointer (&monitor);
        }
    }

  return ret;
}

static void
ide_git_vcs_reload_worker (IdeTask      *task,
                           gpointer      source_object,
                           gpointer      task_data,
                           GCancellable *cancellable)
{
  IdeGitVcs *self = source_object;
  g_autoptr(GgitRepository) repository1 = NULL;
  g_autoptr(GgitRepository) repository2 = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_TASK (task));
  g_assert (IDE_IS_GIT_VCS (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (!(repository1 = ide_git_vcs_load (self, &error)) ||
      !(repository2 = ide_git_vcs_load (self, &error)))
    {
      g_debug ("%s", error->message);
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  g_mutex_lock (&self->repository_mutex);

  g_set_object (&self->repository, repository1);
  g_set_object (&self->change_monitor_repository, repository2);

  if (!ide_git_vcs_load_monitor_locked (self, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  g_mutex_unlock (&self->repository_mutex);

  IDE_EXIT;
}

static void
ide_git_vcs_reload_async (IdeGitVcs           *self,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_GIT_VCS (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_run_in_thread (task, ide_git_vcs_reload_worker);

  IDE_EXIT;
}

static gboolean
ide_git_vcs_reload_finish (IdeGitVcs     *self,
                           GAsyncResult  *result,
                           GError       **error)
{
  IdeTask *task = (IdeTask *)result;
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_GIT_VCS (self), FALSE);

  self->reloading = FALSE;

  ret = ide_task_propagate_boolean (task, error);

  if (ret)
    {
      g_signal_emit (self, signals [RELOADED], 0, self->change_monitor_repository);
      ide_vcs_emit_changed (IDE_VCS (self));
    }

  IDE_RETURN (ret);
}

static gboolean
ide_git_vcs_is_ignored (IdeVcs  *vcs,
                        GFile   *file,
                        GError **error)
{
  g_autofree gchar *name = NULL;
  IdeGitVcs *self = (IdeGitVcs *)vcs;
  gboolean ret = FALSE;

  g_assert (IDE_IS_GIT_VCS (self));
  g_assert (G_IS_FILE (file));

  /* Note: this function is required to be thread-safe so that workers
   *       can check if files are ignored from a thread without
   *       round-tripping to the main thread.
   */

  /* self->working_directory is not changed after creation, so safe
   * to access it from a thread.
   */
  name = g_file_get_relative_path (self->working_directory, file);
  if (g_strcmp0 (name, ".git") == 0)
    return TRUE;

  /*
   * If we have a valid name to work with, we want to query the
   * repository. But this could be called from a thread, so ensure
   * we are the only thread accessing self->repository right now.
   */
  if (name != NULL)
    {
      g_mutex_lock (&self->repository_mutex);
      ret = ggit_repository_path_is_ignored (self->repository, name, error);
      g_mutex_unlock (&self->repository_mutex);
    }

  return ret;
}

static gchar *
ide_git_vcs_get_branch_name (IdeVcs *vcs)
{
  IdeGitVcs *self = (IdeGitVcs *)vcs;
  GgitRef *ref;
  gchar *ret = NULL;

  g_assert (IDE_IS_GIT_VCS (self));

  g_mutex_lock (&self->repository_mutex);
  ret = g_strdup (self->worktree_branch);
  ref = ggit_repository_get_head (self->repository, NULL);
  g_mutex_unlock (&self->repository_mutex);

  if (ret != NULL)
    return ret;

  if (ref != NULL)
    {
      ret = g_strdup (ggit_ref_get_shorthand (ref));
      g_object_unref (ref);
    }
  else
    {
      /* initial commit, no branch name yet */
      ret = g_strdup ("master");
    }

  return ret;
}

static void
ide_git_vcs_real_reloaded (IdeGitVcs      *self,
                           GgitRepository *repository)
{
  g_assert (IDE_IS_GIT_VCS (self));
  g_assert (GGIT_IS_REPOSITORY (repository));

  g_object_notify (G_OBJECT (self), "branch-name");
}

static gint
ide_git_vcs_list_status_cb (const gchar     *path,
                            GgitStatusFlags  flags,
                            gpointer         user_data)
{
  ListStatus *state = user_data;
  g_autoptr(GFile) file = NULL;
  g_autoptr(IdeVcsFileInfo) info = NULL;
  IdeVcsFileStatus status = 0;

  g_assert (path != NULL);
  g_assert (state != NULL);
  g_assert (G_IS_LIST_STORE (state->store));
  g_assert (G_IS_FILE (state->workdir));

  file = g_file_get_child (state->workdir, path);

  switch (flags)
    {
    case GGIT_STATUS_INDEX_DELETED:
    case GGIT_STATUS_WORKING_TREE_DELETED:
      status = IDE_VCS_FILE_STATUS_DELETED;
      break;

    case GGIT_STATUS_INDEX_RENAMED:
      status = IDE_VCS_FILE_STATUS_RENAMED;
      break;

    case GGIT_STATUS_INDEX_NEW:
    case GGIT_STATUS_WORKING_TREE_NEW:
      status = IDE_VCS_FILE_STATUS_ADDED;
      break;

    case GGIT_STATUS_INDEX_MODIFIED:
    case GGIT_STATUS_INDEX_TYPECHANGE:
    case GGIT_STATUS_WORKING_TREE_MODIFIED:
    case GGIT_STATUS_WORKING_TREE_TYPECHANGE:
      status = IDE_VCS_FILE_STATUS_CHANGED;
      break;

    case GGIT_STATUS_IGNORED:
      status = IDE_VCS_FILE_STATUS_IGNORED;
      break;

    case GGIT_STATUS_CURRENT:
      status = IDE_VCS_FILE_STATUS_UNCHANGED;
      break;

    default:
      status = IDE_VCS_FILE_STATUS_UNTRACKED;
      break;
    }

  info = g_object_new (IDE_TYPE_VCS_FILE_INFO,
                       "file", file,
                       "status", status,
                       NULL);

  g_list_store_append (state->store, info);

  return 0;
}

static void
ide_git_vcs_list_status_worker (IdeTask      *task,
                                gpointer      source_object,
                                gpointer      task_data,
                                GCancellable *cancellable)
{
  ListStatus *state = task_data;
  g_autoptr(GListStore) store = NULL;
  g_autoptr(GFile) workdir = NULL;
  g_autoptr(GgitRepository) repository = NULL;
  g_autoptr(GgitStatusOptions) options = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *relative = NULL;
  gchar *strv[] = { NULL, NULL };

  g_assert (IDE_IS_TASK (task));
  g_assert (IDE_IS_GIT_VCS (source_object));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (state != NULL);
  g_assert (G_IS_FILE (state->repository_location));

  if (!(repository = ggit_repository_open (state->repository_location, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  if (!(workdir = ggit_repository_get_workdir (repository)))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "Failed to locate working directory");
      return;
    }

  g_set_object (&state->workdir, workdir);

  if (state->directory_or_file != NULL)
    relative = g_file_get_relative_path (workdir, state->directory_or_file);

  strv[0] = relative;
  options = ggit_status_options_new (GGIT_STATUS_OPTION_DEFAULT,
                                     GGIT_STATUS_SHOW_INDEX_AND_WORKDIR,
                                     (const gchar **)strv);

  store = g_list_store_new (IDE_TYPE_VCS_FILE_INFO);
  g_set_object (&state->store, store);

  if (!ggit_repository_file_status_foreach (repository,
                                            options,
                                            ide_git_vcs_list_status_cb,
                                            state,
                                            &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_pointer (task, g_steal_pointer (&store), g_object_unref);
}

static void
ide_git_vcs_list_status_async (IdeVcs              *vcs,
                               GFile               *directory_or_file,
                               gboolean             include_descendants,
                               gint                 io_priority,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  IdeGitVcs *self = (IdeGitVcs *)vcs;
  g_autoptr(IdeTask) task = NULL;
  ListStatus *state;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_GIT_VCS (self));
  g_return_if_fail (!directory_or_file || G_IS_FILE (directory_or_file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  g_mutex_lock (&self->repository_mutex);
  state = g_slice_new0 (ListStatus);
  state->directory_or_file = g_object_ref (directory_or_file);
  state->repository_location = ggit_repository_get_location (self->repository);
  state->recursive = !!include_descendants;
  g_mutex_unlock (&self->repository_mutex);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_git_vcs_list_status_async);
  ide_task_set_priority (task, io_priority);
  ide_task_set_return_on_cancel (task, TRUE);
  ide_task_set_task_data (task, state, list_status_free);

  if (state->repository_location == NULL)
    ide_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "No repository loaded");
  else
    ide_task_run_in_thread (task, ide_git_vcs_list_status_worker);

  IDE_EXIT;
}

static GListModel *
ide_git_vcs_list_status_finish (IdeVcs        *vcs,
                                GAsyncResult  *result,
                                GError       **error)
{
  g_return_val_if_fail (IDE_IS_GIT_VCS (vcs), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static void
ide_git_vcs_dispose (GObject *object)
{
  IdeGitVcs *self = (IdeGitVcs *)object;

  IDE_ENTRY;

  dzl_clear_source (&self->changed_timeout);

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

  IDE_EXIT;
}

static void
ide_git_vcs_finalize (GObject *object)
{
  IdeGitVcs *self = (IdeGitVcs *)object;

  IDE_ENTRY;

  g_mutex_clear (&self->repository_mutex);
  g_clear_pointer (&self->worktree_branch, g_free);

  G_OBJECT_CLASS (ide_git_vcs_parent_class)->finalize (object);

  IDE_EXIT;
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
    case PROP_BRANCH_NAME:
      g_value_take_string (value, ide_git_vcs_get_branch_name (IDE_VCS (self)));
      break;

    case PROP_WORKING_DIRECTORY:
      g_value_set_object (value, ide_git_vcs_get_working_directory (IDE_VCS (self)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_git_vcs_init_iface (IdeVcsInterface *iface)
{
  iface->get_working_directory = ide_git_vcs_get_working_directory;
  iface->get_buffer_change_monitor = ide_git_vcs_get_buffer_change_monitor;
  iface->is_ignored = ide_git_vcs_is_ignored;
  iface->get_config = ide_git_vcs_get_config;
  iface->get_branch_name = ide_git_vcs_get_branch_name;
  iface->list_status_async = ide_git_vcs_list_status_async;
  iface->list_status_finish = ide_git_vcs_list_status_finish;
}

static void
ide_git_vcs_class_init (IdeGitVcsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_git_vcs_finalize;
  object_class->dispose = ide_git_vcs_dispose;
  object_class->get_property = ide_git_vcs_get_property;

  g_object_class_override_property (object_class, PROP_BRANCH_NAME, "branch-name");
  g_object_class_override_property (object_class, PROP_WORKING_DIRECTORY, "working-directory");

  /**
   * IdeGitVcs::reloaded:
   * @self: An #IdeGitVfs
   * @repository: a #GgitRepository
   *
   * This signal is emitted when the git index has been reloaded. Various consumers may want to
   * reload their git objects upon this notification. Such an example would be the line diffs
   * that are rendered in the source view gutter.
   *
   * The @repository instance is to aide consumers in locating the repository and should not
   * be used directly except in very specific situations. The gutter change renderer uses this
   * instance in a threaded manner.
   */
  signals [RELOADED] =
    g_signal_new_class_handler ("reloaded",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (ide_git_vcs_real_reloaded),
                                NULL, NULL, NULL,
                                G_TYPE_NONE, 1, GGIT_TYPE_REPOSITORY);
}

static void
ide_git_vcs_init (IdeGitVcs *self)
{
  g_mutex_init (&self->repository_mutex);
}

static void
ide_git_vcs_init_async__reload_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  IdeGitVcs *self = (IdeGitVcs *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_TASK (task));
  g_assert (IDE_IS_GIT_VCS (self));

  if (!ide_git_vcs_reload_finish (self, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);
}

static void
ide_git_vcs_init_async (GAsyncInitable      *initable,
                        int                  io_priority,
                        GCancellable        *cancellable,
                        GAsyncReadyCallback  callback,
                        gpointer             user_data)
{
  IdeGitVcs *self = (IdeGitVcs *)initable;
  g_autoptr(IdeTask) task = NULL;

  g_return_if_fail (IDE_IS_GIT_VCS (self));

  task = ide_task_new (self, cancellable, callback, user_data);
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
  IdeTask *task = (IdeTask *)result;

  g_return_val_if_fail (IDE_IS_TASK (task), FALSE);

  return ide_task_propagate_boolean (task, error);
}

static void
g_async_initable_init_interface (GAsyncInitableIface *iface)
{
  iface->init_async = ide_git_vcs_init_async;
  iface->init_finish = ide_git_vcs_init_finish;
}
