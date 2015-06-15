/* ide-git-vcs.c
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

#define G_LOG_DOMAIN "ide-git-vcs"

#include <git2.h>
#include <glib/gi18n.h>
#include <libgit2-glib/ggit.h>

#include "ide-async-helper.h"
#include "ide-context.h"
#include "ide-debug.h"
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

enum {
  RELOADED,
  LAST_SIGNAL
};

static GParamSpec *gParamSpecs [LAST_PROP];
static guint gSignals [LAST_SIGNAL];

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

static GgitRepository *
ide_git_vcs_load (IdeGitVcs  *self,
                  GError    **error)
{
  g_autoptr(GFile) location = NULL;
  GgitRepository *repository = NULL;
  IdeContext *context;
  GFile *project_file;

  g_assert (IDE_IS_GIT_VCS (self));

  context = ide_object_get_context (IDE_OBJECT (self));
  project_file = ide_context_get_project_file (context);

  if (!(location = ggit_repository_discover (project_file, error)))
    return NULL;

  if (!(repository = ggit_repository_open (location, error)))
    return NULL;

  /* Only set once, on load. Not thread-safe otherwise. */
  if (self->working_directory == NULL)
    self->working_directory = ggit_repository_get_workdir (repository);

  return repository;
}

static gboolean
ide_git_vcs__changed_timeout_cb (gpointer user_data)
{
  IdeGitVcs *self = user_data;

  IDE_ENTRY;

  g_assert (IDE_IS_GIT_VCS (self));

  self->changed_timeout = 0;
  ide_git_vcs_reload_async (self, NULL, NULL, NULL);

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
ide_git_vcs_load_monitor (IdeGitVcs  *self,
                          GError    **error)
{
  gboolean ret = TRUE;

  g_assert (IDE_IS_GIT_VCS (self));

  if (self->monitor == NULL)
    {
      g_autoptr(GFile) location = NULL;
      g_autoptr(GFileMonitor) monitor = NULL;
      g_autoptr(GFile) heads_dir = NULL;
      GFileMonitorFlags flags = G_FILE_MONITOR_WATCH_MOUNTS;

      location = ggit_repository_get_location (self->repository);
      heads_dir = g_file_get_child (location, "refs/heads");
      monitor = g_file_monitor (heads_dir, flags, NULL, error);

      ret = !!monitor;

      if (monitor)
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
  GError *error = NULL;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_GIT_VCS (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (!(repository1 = ide_git_vcs_load (self, &error)) ||
      !(repository2 = ide_git_vcs_load (self, &error)))
    {
      g_task_return_error (task, error);
      return;
    }

  g_set_object (&self->repository, repository1);
  g_set_object (&self->change_monitor_repository, repository2);

  if (!ide_git_vcs_load_monitor (self, &error))
    {
      g_task_return_error (task, error);
      return;
    }

  g_task_return_boolean (task, TRUE);
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
  g_signal_emit (self, gSignals [RELOADED], 0, self->change_monitor_repository);
  ret = g_task_propagate_boolean (task, error);

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

  name = g_file_get_relative_path (self->working_directory, file);
  if (g_strcmp0 (name, ".git") == 0)
    return TRUE;

  if (name != NULL)
    return ggit_repository_path_is_ignored (self->repository, name, error);

  return ret;
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
  vcs_class->is_ignored = ide_git_vcs_is_ignored;

  /**
   * IdeGitVcs:repository:
   *
   * This property contains the underlying #GgitRepository that can be used to lookup git
   * information. Consumers should be careful about using this directly. It is not thread-safe
   * to use this object, nor is it safe to perform many blocking calls from the main thread.
   *
   * You might want to get the #GgitRepository:location property and create your own instance
   * of the repository for threaded operations.
   */
  gParamSpecs [PROP_REPOSITORY] =
    g_param_spec_object ("repository",
                         _("Repository"),
                         _("The git repository for the project."),
                         GGIT_TYPE_REPOSITORY,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);

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
  gSignals [RELOADED] = g_signal_new ("reloaded",
                                      G_TYPE_FROM_CLASS (klass),
                                      G_SIGNAL_RUN_LAST,
                                      0,
                                      NULL, NULL, NULL,
                                      G_TYPE_NONE,
                                      1,
                                      GGIT_TYPE_REPOSITORY);
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
