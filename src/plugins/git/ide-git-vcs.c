/* ide-git-vcs.c
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

  guint           changed_timeout;

  guint           reloading : 1;
  guint           loaded_files : 1;
};

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
  g_autoptr(GFile) git = NULL;
  g_autoptr(GFile) child = NULL;
  GFile *ret;

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

  ret = ide_git_vcs_discover (self, parent, error);

  return ret;
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
    g_warning ("%s", error->message);
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
  IDE_ENTRY;

  g_assert (IDE_IS_GIT_VCS (self));

  if (self->changed_timeout != 0)
    g_source_remove (self->changed_timeout);

  self->changed_timeout = g_timeout_add_seconds (DEFAULT_CHANGED_TIMEOUT_SECS,
                                                 ide_git_vcs__changed_timeout_cb,
                                                 self);

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
      g_autoptr(GFile) index_file = NULL;
      GError *local_error = NULL;

      location = ggit_repository_get_location (self->repository);
      index_file = g_file_get_child (location, "index");
      monitor = g_file_monitor (index_file, 0, NULL, &local_error);

      if (monitor == NULL)
        {
          g_warning ("%s", local_error->message);
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
          self->monitor = g_object_ref (monitor);
        }
    }

  return ret;
}

static void
ide_git_vcs_reload_worker (GTask        *task,
                           gpointer      source_object,
                           gpointer      task_data,
                           GCancellable *cancellable)
{
  IdeGitVcs *self = source_object;
  g_autoptr(GgitRepository) repository1 = NULL;
  g_autoptr(GgitRepository) repository2 = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_GIT_VCS (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (!(repository1 = ide_git_vcs_load (self, &error)) ||
      !(repository2 = ide_git_vcs_load (self, &error)))
    {
      g_debug ("%s", error->message);
      g_task_return_error (task, error);
      IDE_EXIT;
    }

  g_mutex_lock (&self->repository_mutex);

  g_set_object (&self->repository, repository1);
  g_set_object (&self->change_monitor_repository, repository2);

  if (!ide_git_vcs_load_monitor_locked (self, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_boolean (task, TRUE);

  g_mutex_unlock (&self->repository_mutex);

  IDE_EXIT;
}

static void
ide_git_vcs_reload_async (IdeGitVcs           *self,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_GIT_VCS (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_run_in_thread (task, ide_git_vcs_reload_worker);

  IDE_EXIT;
}

static gboolean
ide_git_vcs_reload_finish (IdeGitVcs     *self,
                           GAsyncResult  *result,
                           GError       **error)
{
  GTask *task = (GTask *)result;
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_GIT_VCS (self), FALSE);

  self->reloading = FALSE;

  ret = g_task_propagate_boolean (task, error);

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
  ref = ggit_repository_get_head (self->repository, NULL);
  g_mutex_unlock (&self->repository_mutex);

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

static void
ide_git_vcs_dispose (GObject *object)
{
  IdeGitVcs *self = (IdeGitVcs *)object;

  IDE_ENTRY;

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

  IDE_EXIT;
}

static void
ide_git_vcs_finalize (GObject *object)
{
  IdeGitVcs *self = (IdeGitVcs *)object;

  IDE_ENTRY;

  g_mutex_clear (&self->repository_mutex);

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
   * @repository: A #GgitRepository
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
