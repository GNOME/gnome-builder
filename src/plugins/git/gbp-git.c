/* gbp-git.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#define G_LOG_DOMAIN "gbp-git"

#include "config.h"

#include <libgit2-glib/ggit.h>

#include "gbp-git.h"
#include "line-cache.h"

struct _GbpGit
{
  GObject         parent_instance;
  GMutex          mutex;
  GFile          *workdir;
  GgitRepository *repository;
};

G_DEFINE_TYPE (GbpGit, gbp_git, G_TYPE_OBJECT)

static void
gbp_git_finalize (GObject *object)
{
  GbpGit *self = (GbpGit *)object;

  g_clear_object (&self->workdir);
  g_clear_object (&self->repository);
  g_mutex_clear (&self->mutex);

  G_OBJECT_CLASS (gbp_git_parent_class)->finalize (object);
}

static void
gbp_git_class_init (GbpGitClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_git_finalize;
}

static void
gbp_git_init (GbpGit *self)
{
  g_mutex_init (&self->mutex);
}

GbpGit *
gbp_git_new (void)
{
  return g_object_new (GBP_TYPE_GIT, NULL);
}

void
gbp_git_set_workdir (GbpGit *self,
                     GFile  *workdir)
{
  g_return_if_fail (GBP_IS_GIT (self));
  g_return_if_fail (G_IS_FILE (workdir));

  if (g_set_object (&self->workdir, workdir))
    {
      g_clear_object (&self->repository);
    }
}

static void
gbp_git_is_ignored (GTask        *task,
                    gpointer      source_object,
                    gpointer      task_data,
                    GCancellable *cancellable)
{
  g_assert (G_IS_TASK (task));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  g_task_return_boolean (task, FALSE);
}

void
gbp_git_is_ignored_async (GbpGit              *self,
                          const gchar         *path,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_assert (GBP_IS_GIT (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gbp_git_is_ignored_async);
  g_task_set_priority (task, G_PRIORITY_HIGH);
  g_task_run_in_thread (task, gbp_git_is_ignored);
}

gboolean
gbp_git_is_ignored_finish (GbpGit        *self,
                           GAsyncResult  *result,
                           GError       **error)
{
  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
gbp_git_switch_branch (GTask        *task,
                       gpointer      source_object,
                       gpointer      task_data,
                       GCancellable *cancellable)
{
  g_assert (G_IS_TASK (task));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  g_task_return_boolean (task, FALSE);
}

void
gbp_git_switch_branch_async (GbpGit              *self,
                             const gchar         *branch_name,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_assert (GBP_IS_GIT (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gbp_git_switch_branch_async);
  g_task_set_priority (task, G_PRIORITY_LOW + 1000);
  g_task_run_in_thread (task, gbp_git_switch_branch);
}

gboolean
gbp_git_switch_branch_finish (GbpGit        *self,
                              GAsyncResult  *result,
                              gchar        **switch_to_directory,
                              GError       **error)
{
  g_autoptr(GError) local_error = NULL;
  g_autofree gchar *other_path = NULL;

  g_assert (GBP_IS_GIT (self));
  g_assert (G_IS_TASK (result));

  other_path = g_task_propagate_pointer (G_TASK (result), &local_error);

  if (local_error != NULL)
    {
      g_propagate_error (error, g_steal_pointer (&local_error));
      return FALSE;
    }

  if (switch_to_directory != NULL)
    *switch_to_directory = g_steal_pointer (&other_path);

  return TRUE;
}

static void
gbp_git_list_refs_by_kind (GTask        *task,
                           gpointer      source_object,
                           gpointer      task_data,
                           GCancellable *cancellable)
{
  g_assert (G_IS_TASK (task));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  g_task_return_pointer (task,
                         g_ptr_array_new (),
                         (GDestroyNotify)g_ptr_array_unref);
}

void
gbp_git_list_refs_by_kind_async (GbpGit              *self,
                                 GbpGitRefKind        kind,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_assert (GBP_IS_GIT (self));
  g_assert (kind > 0);
  g_assert (kind <= 3);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gbp_git_list_refs_by_kind_async);
  g_task_run_in_thread (task, gbp_git_list_refs_by_kind);
}

GPtrArray *
gbp_git_list_refs_by_kind_finish (GbpGit        *self,
                                  GAsyncResult  *result,
                                  GError       **error)
{
  g_assert (GBP_IS_GIT (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
gbp_git_list_status (GTask        *task,
                     gpointer      source_object,
                     gpointer      task_data,
                     GCancellable *cancellable)
{
  g_assert (G_IS_TASK (task));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  g_task_return_pointer (task,
                         g_ptr_array_new (),
                         (GDestroyNotify)g_ptr_array_unref);
}

void
gbp_git_list_status_async (GbpGit              *self,
                           const gchar         *directory_or_file,
                           gboolean             include_descendants,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_assert (GBP_IS_GIT (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gbp_git_list_status_async);
  g_task_run_in_thread (task, gbp_git_list_status);
}

GPtrArray *
gbp_git_list_status_finish (GbpGit        *self,
                            GAsyncResult  *result,
                            GError       **error)
{
  g_assert (GBP_IS_GIT (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_pointer (G_TASK (result), error);
}

typedef struct
{
  gchar            *url;
  GFile            *destination;
  GgitCloneOptions *options;
} Clone;

static void
clone_free (Clone *c)
{
  g_clear_pointer (&c->url, g_free);
  g_clear_object (&c->destination);
  g_clear_object (&c->options);
  g_slice_free (Clone, c);
}

static void
gbp_git_clone_url_worker (GTask        *task,
                          gpointer      source_object,
                          gpointer      task_data,
                          GCancellable *cancellable)
{
  Clone *state = task_data;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_TASK (task));
  g_assert (GBP_IS_GIT (source_object));
  g_assert (state != NULL);
  g_assert (state->url != NULL);
  g_assert (state->destination != NULL);

  ggit_repository_clone (state->url,
                         state->destination,
                         state->options,
                         &error);

  if (error != NULL)
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_boolean (task, TRUE);
}

void
gbp_git_clone_url_async (GbpGit                *self,
                         const gchar           *url,
                         GFile                 *destination,
                         GgitCloneOptions      *options,
                         GCancellable          *cancellable,
                         GAsyncReadyCallback    callback,
                         gpointer               user_data)
{
  g_autoptr(GTask) task = NULL;
  Clone *c;

  g_assert (GBP_IS_GIT (self));
  g_assert (url != NULL);
  g_assert (G_IS_FILE (destination));
  g_assert (!options || GGIT_IS_CLONE_OPTIONS (options));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  c = g_slice_new0 (Clone);
  c->url = g_strdup (url);
  c->destination = g_object_ref (destination);
  c->options = options ? g_object_ref (options) : NULL;

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gbp_git_clone_url_async);
  g_task_set_priority (task, G_PRIORITY_LOW);
  g_task_set_task_data (task, c, (GDestroyNotify)clone_free);
  g_task_set_return_on_cancel (task, TRUE);
  g_task_run_in_thread (task, gbp_git_clone_url_worker);
}

gboolean
gbp_git_clone_url_finish (GbpGit        *self,
                          GAsyncResult  *result,
                          GError       **error)
{
  g_assert (GBP_IS_GIT (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_boolean (G_TASK (result), error);
}

typedef struct
{
  GFile                      *workdir;
  GgitSubmoduleUpdateOptions *options;
  GError                     *error;
} UpdateSubmodules;

static void
update_submodules_free (UpdateSubmodules *state)
{
  g_clear_object (&state->workdir);
  g_clear_object (&state->options);
  g_clear_error (&state->error);
  g_slice_free (UpdateSubmodules, state);
}

static gint
gbp_git_update_submodules_foreach_submodule_cb (GgitSubmodule *submodule,
                                                const gchar   *name,
                                                gpointer       user_data)
{
  UpdateSubmodules *state = user_data;

  g_assert (submodule != NULL);
  g_assert (name != NULL);
  g_assert (state != NULL);

  if (state->error != NULL)
    ggit_submodule_update (submodule,
                           FALSE,
                           state->options,
                           &state->error);

  return GIT_OK;
}

static void
gbp_git_update_submodules_worker (GTask        *task,
                                  gpointer      source_object,
                                  gpointer      task_data,
                                  GCancellable *cancellable)
{
  UpdateSubmodules *state = task_data;
  g_autoptr(GgitRepository) repository = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_TASK (task));
  g_assert (GBP_IS_GIT (source_object));
  g_assert (state != NULL);
  g_assert (G_IS_FILE (state->workdir));
  g_assert (GGIT_IS_SUBMODULE_UPDATE_OPTIONS (state->options));

  if (!(repository = ggit_repository_open (state->workdir, &error)))
    goto handle_error;

  if (!ggit_repository_submodule_foreach (repository,
                                          gbp_git_update_submodules_foreach_submodule_cb,
                                          state,
                                          &error))
    goto handle_error;

  error = g_steal_pointer (&state->error);

handle_error:

  if (error != NULL)
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_boolean (task, TRUE);
}

void
gbp_git_update_submodules_async (GbpGit                     *self,
                                 GgitSubmoduleUpdateOptions *options,
                                 GCancellable               *cancellable,
                                 GAsyncReadyCallback         callback,
                                 gpointer                    user_data)
{
  g_autoptr(GTask) task = NULL;
  UpdateSubmodules *state;

  g_assert (GBP_IS_GIT (self));
  g_assert (GGIT_IS_SUBMODULE_UPDATE_OPTIONS (options));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gbp_git_clone_url_async);

  if (self->workdir == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_INITIALIZED,
                               "No workdir has been set for the project");
      return;
    }

  state = g_slice_new0 (UpdateSubmodules);
  state->options = g_object_ref (options);
  state->workdir = g_object_ref (self->workdir);

  g_task_set_priority (task, G_PRIORITY_LOW);
  g_task_set_task_data (task, state, (GDestroyNotify)update_submodules_free);
  g_task_set_return_on_cancel (task, TRUE);
  g_task_run_in_thread (task, gbp_git_update_submodules_worker);
}

gboolean
gbp_git_update_submodules_finish (GbpGit        *self,
                                  GAsyncResult  *result,
                                  GError       **error)
{
  g_assert (GBP_IS_GIT (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_boolean (G_TASK (result), error);
}

typedef struct
{
  GFile    *workdir;
  gchar    *key;
  GVariant *value;
  guint     global : 1;
} UpdateConfig;

static void
update_config_free (UpdateConfig *state)
{
  g_clear_object (&state->workdir);
  g_clear_pointer (&state->key, g_free);
  g_clear_pointer (&state->value, g_variant_unref);
  g_slice_free (UpdateConfig, state);
}

static void
gbp_git_update_config_worker (GTask        *task,
                              gpointer      source_object,
                              gpointer      task_data,
                              GCancellable *cancellable)
{
  UpdateConfig *state = task_data;
  g_autoptr(GgitRepository) repository = NULL;
  g_autoptr(GgitConfig) config = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_TASK (task));
  g_assert (GBP_IS_GIT (source_object));
  g_assert (state != NULL);
  g_assert (state->key != NULL);
  g_assert (G_IS_FILE (state->workdir) || state->global);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (!state->global)
    {
      if (!(repository = ggit_repository_open (state->workdir, &error)) ||
          !(config = ggit_repository_get_config (repository, &error)))
        {
          g_task_return_error (task, g_steal_pointer (&error));
          return;
        }
    }
  else
    {
      if (!(config = ggit_config_new_default (&error)))
        {
          g_task_return_error (task, g_steal_pointer (&error));
          return;
        }
    }

  g_assert (config != NULL);
  g_assert (GGIT_IS_CONFIG (config));

  if (state->value == NULL)
    {
      if (!ggit_config_delete_entry (config, state->key, &error))
        g_task_return_error (task, g_steal_pointer (&error));
      else
        g_task_return_boolean (task, TRUE);
    }
  else
    {
      if (g_variant_is_of_type (state->value, G_VARIANT_TYPE_STRING))
        ggit_config_set_string (config,
                                state->key,
                                g_variant_get_string (state->value, NULL),
                                &error);
      else if (g_variant_is_of_type (state->value, G_VARIANT_TYPE_BOOLEAN))
        ggit_config_set_bool (config,
                              state->key,
                              g_variant_get_boolean (state->value),
                              &error);
      else if (g_variant_is_of_type (state->value, G_VARIANT_TYPE_INT32))
        ggit_config_set_int32 (config,
                               state->key,
                               g_variant_get_int32 (state->value),
                               &error);
      else if (g_variant_is_of_type (state->value, G_VARIANT_TYPE_INT64))
        ggit_config_set_int64 (config,
                               state->key,
                               g_variant_get_int64 (state->value),
                               &error);
      else
        error = g_error_new (G_IO_ERROR,
                             G_IO_ERROR_NOT_SUPPORTED,
                             "Unsupported data type: %s",
                             g_variant_get_type_string (state->value));

      if (error != NULL)
        g_task_return_error (task, g_steal_pointer (&error));
      else
        g_task_return_boolean (task, TRUE);
    }
}

void
gbp_git_update_config_async (GbpGit              *self,
                             gboolean             global,
                             const gchar         *key,
                             GVariant            *value,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  UpdateConfig *state;

  g_assert (GBP_IS_GIT (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  state = g_slice_new0 (UpdateConfig);
  state->workdir = self->workdir ? g_file_dup (self->workdir) : NULL;
  state->key = g_strdup (key);
  state->value = value ? g_variant_ref (value) : NULL;
  state->global = !!global;

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gbp_git_update_config_async);
  g_task_set_task_data (task, state, (GDestroyNotify)update_config_free);

  if (!global && self->workdir == NULL)
    g_task_return_new_error (task,
                             G_IO_ERROR,
                             G_IO_ERROR_NOT_INITIALIZED,
                             "Repository not initialized");
  else
    g_task_run_in_thread (task, gbp_git_update_config_worker);
}

gboolean
gbp_git_update_config_finish (GbpGit        *self,
                              GAsyncResult  *result,
                              GError       **error)
{
  g_assert (GBP_IS_GIT (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_boolean (G_TASK (result), error);
}

typedef struct
{
  GFile *workdir;
  gchar *key;
} ReadConfig;

static void
read_config_free (ReadConfig *state)
{
  g_clear_object (&state->workdir);
  g_clear_pointer (&state->key, g_free);
  g_slice_free (ReadConfig, state);
}

static void
gbp_git_read_config_worker (GTask        *task,
                            gpointer      source_object,
                            gpointer      task_data,
                            GCancellable *cancellable)
{
  ReadConfig *state = task_data;
  g_autoptr(GgitRepository) repository = NULL;
  g_autoptr(GgitConfig) config = NULL;
  g_autoptr(GgitConfig) snapshot = NULL;
  g_autoptr(GgitConfigEntry) entry = NULL;
  g_autoptr(GError) error = NULL;
  const gchar *str;

  g_assert (G_IS_TASK (task));
  g_assert (GBP_IS_GIT (source_object));
  g_assert (state != NULL);
  g_assert (state->key != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (state->workdir != NULL)
    {
      if (!(repository = ggit_repository_open (state->workdir, &error)) ||
          !(config = ggit_repository_get_config (repository, &error)))
        {
          g_task_return_error (task, g_steal_pointer (&error));
          return;
        }
    }
  else
    {
      if (!(config = ggit_config_new_default (&error)))
        {
          g_task_return_error (task, g_steal_pointer (&error));
          return;
        }
    }

  if (!(snapshot = ggit_config_snapshot (config, &error)) ||
      !(entry = ggit_config_get_entry (snapshot, state->key, &error)))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  if (!(str = ggit_config_entry_get_value (entry)))
    str = "";

  g_task_return_pointer (task,
                         g_variant_take_ref (g_variant_new_string (str)),
                         (GDestroyNotify)g_variant_unref);
}

void
gbp_git_read_config_async (GbpGit              *self,
                           const gchar         *key,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  ReadConfig *state;

  g_assert (GBP_IS_GIT (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  state = g_slice_new0 (ReadConfig);
  state->workdir = self->workdir ? g_file_dup (self->workdir) : NULL;
  state->key = g_strdup (key);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gbp_git_read_config_async);
  g_task_set_task_data (task, state, (GDestroyNotify)read_config_free);
  g_task_run_in_thread (task, gbp_git_read_config_worker);
}

GVariant *
gbp_git_read_config_finish (GbpGit        *self,
                            GAsyncResult  *result,
                            GError       **error)
{
  g_assert (GBP_IS_GIT (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_pointer (G_TASK (result), error);
}

typedef struct
{
  GFile *in_directory;
  guint  bare : 1;
} CreateRepo;

static void
create_repo_free (CreateRepo *state)
{
  g_clear_object (&state->in_directory);
  g_slice_free (CreateRepo, state);
}

static void
gbp_git_create_repo_worker (GTask        *task,
                            gpointer      source_object,
                            gpointer      task_data,
                            GCancellable *cancellable)
{
  CreateRepo *state = task_data;
  g_autoptr(GgitRepository) repository = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_TASK (task));
  g_assert (GBP_IS_GIT (source_object));
  g_assert (state != NULL);
  g_assert (G_IS_FILE (state->in_directory));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (!(repository = ggit_repository_init_repository (state->in_directory, state->bare, &error)))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_boolean (task, TRUE);
}

void
gbp_git_create_repo_async (GbpGit              *self,
                           GFile               *in_directory,
                           gboolean             bare,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  CreateRepo *state;

  g_assert (GBP_IS_GIT (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  state = g_slice_new0 (CreateRepo);
  state->in_directory = g_file_dup (in_directory);
  state->bare = !!bare;

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gbp_git_create_repo_async);
  g_task_set_task_data (task, state, (GDestroyNotify)create_repo_free);
  g_task_run_in_thread (task, gbp_git_create_repo_worker);
}

gboolean
gbp_git_create_repo_finish (GbpGit        *self,
                            GAsyncResult  *result,
                            GError       **error)
{
  g_assert (GBP_IS_GIT (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_boolean (G_TASK (result), error);
}

typedef struct
{
  GFile *directory;
  GFile *workdir;
  GFile *dot_git;
  gchar *branch;
  guint  is_worktree : 1;
} Discover;

static void
discover_free (Discover *state)
{
  g_clear_object (&state->directory);
  g_clear_object (&state->workdir);
  g_clear_object (&state->dot_git);
  g_clear_pointer (&state->branch, g_free);
  g_slice_free (Discover, state);
}

static void
gbp_git_discover_worker (GTask        *task,
                         gpointer      source_object,
                         gpointer      task_data,
                         GCancellable *cancellable)
{
  Discover *state = task_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) location = NULL;
  g_autoptr(GFile) workdir = NULL;
  g_autoptr(GgitRepository) repository = NULL;
  g_autofree gchar *worktree_branch = NULL;
  gboolean is_worktree = FALSE;

  g_assert (G_IS_TASK (task));
  g_assert (GBP_IS_GIT (source_object));
  g_assert (state != NULL);
  g_assert (G_IS_FILE (state->directory));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  /* Short-circuit if we don't .git */
  if (!(location = ggit_repository_discover_full (state->directory, TRUE, NULL, &error)))
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               "Failed to locate git repository location");
      return;
    }

  /* If @location is a regular file, we might have a git-worktree link */
  if (g_file_query_file_type (location, 0, NULL) == G_FILE_TYPE_REGULAR)
    {
      g_autofree gchar *contents = NULL;
      gsize len;

      if (g_file_load_contents (location, NULL, &contents, &len, NULL, NULL))
        {
          /* g_file_load_contents() provides a suffix \0 */
          g_auto(GStrv) lines = g_strsplit (contents, "\n", 0);

          for (guint i = 0; lines[i] != NULL; i++)
            {
              gchar *line = lines[i];

              if (g_str_has_prefix (line, "gitdir: "))
                {
                  g_autoptr(GFile) location_parent = g_file_get_parent (location);
                  const gchar *path = line + strlen ("gitdir: ");
                  const gchar *branch;

                  is_worktree = TRUE;

                  g_clear_object (&location);

                  if (g_path_is_absolute (path))
                    location = g_file_new_for_path (path);
                  else
                    location = g_file_resolve_relative_path (location_parent, path);

                  /*
                   * Worktrees only have a single branch, and it is the name
                   * of the suffix of .git/worktrees/<name>
                   */
                  if ((branch = strrchr (line, G_DIR_SEPARATOR)))
                    worktree_branch = g_strdup (branch + 1);

                  break;
                }
            }
        }
    }

  if (!(repository = ggit_repository_open (location, &error)))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  workdir = ggit_repository_get_workdir (repository);

  g_assert (G_IS_FILE (location));
  g_assert (G_IS_FILE (workdir));
  g_assert (GGIT_IS_REPOSITORY (repository));

  if (worktree_branch == NULL)
    {
      g_autoptr(GgitRef) ref = NULL;

      if ((ref = ggit_repository_get_head (repository, NULL)))
        worktree_branch = g_strdup (ggit_ref_get_shorthand (ref));

      if (worktree_branch == NULL)
        worktree_branch = g_strdup ("master");
    }

  state->workdir = g_file_dup (workdir);
  state->dot_git = g_file_dup (location);
  state->branch = g_steal_pointer (&worktree_branch);
  state->is_worktree = !!is_worktree;

  g_task_return_boolean (task, TRUE);
}

void
gbp_git_discover_async (GbpGit                      *self,
                        GFile                       *directory,
                        GCancellable                *cancellable,
                        GAsyncReadyCallback          callback,
                        gpointer                     user_data)
{
  g_autoptr(GTask) task = NULL;
  Discover *state;

  g_return_if_fail (GBP_IS_GIT (self));
  g_return_if_fail (G_IS_FILE (directory));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  state = g_slice_new0 (Discover);
  state->directory = g_file_dup (directory);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gbp_git_discover_async);
  g_task_set_task_data (task, state, (GDestroyNotify)discover_free);
  g_task_run_in_thread (task, gbp_git_discover_worker);
}

gboolean
gbp_git_discover_finish (GbpGit        *self,
                         GAsyncResult  *result,
                         GFile        **workdir,
                         GFile        **dot_git,
                         gchar        **branch,
                         gboolean      *is_worktree,
                         GError       **error)
{
  g_return_val_if_fail (GBP_IS_GIT (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);
  g_return_val_if_fail (workdir != NULL, FALSE);
  g_return_val_if_fail (dot_git != NULL, FALSE);
  g_return_val_if_fail (branch != NULL, FALSE);
  g_return_val_if_fail (is_worktree != NULL, FALSE);

  if (g_task_propagate_boolean (G_TASK (result), error))
    {
      Discover *state = g_task_get_task_data (G_TASK (result));

      *workdir = g_steal_pointer (&state->workdir);
      *dot_git = g_steal_pointer (&state->dot_git);
      *branch = g_steal_pointer (&state->branch);
      *is_worktree = state->is_worktree;

      return TRUE;
    }

  return FALSE;
}

typedef struct
{
  gchar  *path;
  GBytes *bytes;
} GetChanges;

typedef struct
{
  gint old_start;
  gint old_lines;
  gint new_start;
  gint new_lines;
} Range;

static void
get_changes_free (GetChanges *state)
{
  g_clear_pointer (&state->path, g_free);
  g_clear_pointer (&state->bytes, g_bytes_unref);
  g_slice_free (GetChanges, state);
}

static gint
diff_hunk_cb (GgitDiffDelta *delta,
              GgitDiffHunk  *hunk,
              gpointer       user_data)
{
  GArray *ranges = user_data;
  Range range;

  g_assert (delta != NULL);
  g_assert (hunk != NULL);
  g_assert (ranges != NULL);

  range.old_start = ggit_diff_hunk_get_old_start (hunk);
  range.old_lines = ggit_diff_hunk_get_old_lines (hunk);
  range.new_start = ggit_diff_hunk_get_new_start (hunk);
  range.new_lines = ggit_diff_hunk_get_new_lines (hunk);

  g_array_append_val (ranges, range);

  return 0;
}

static void
gbp_git_get_changes_worker (GTask        *task,
                            gpointer      source_object,
                            gpointer      task_data,
                            GCancellable *cancellable)
{
  GbpGit *self = source_object;
  GetChanges *state = task_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GMutexLocker) locker = NULL;
  g_autoptr(GgitDiffOptions) options = NULL;
  g_autoptr(GVariant) ret = NULL;
  g_autoptr(GArray) ranges = NULL;
  GgitTreeEntry *entry = NULL;
  const gchar *contents;
  GgitObject *commit = NULL;
  GgitObject *blob = NULL;
  LineCache *cache = NULL;
  GgitTree *tree = NULL;
  GgitOId *entry_oid = NULL;
  GgitOId *oid = NULL;
  GgitRef *head = NULL;
  gsize len;

  g_assert (G_IS_TASK (task));
  g_assert (GBP_IS_GIT (self));
  g_assert (state != NULL);
  g_assert (state->path != NULL);
  g_assert (state->bytes != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  locker = g_mutex_locker_new (&self->mutex);

  if (self->repository == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_INITIALIZED,
                               "No repository has been loaded");
      return;
    }

  if (!(head = ggit_repository_get_head (self->repository, &error)) ||
      !(oid = ggit_ref_get_target (head)) ||
      !(commit = ggit_repository_lookup (self->repository, oid, GGIT_TYPE_COMMIT, &error)) ||
      !(tree = ggit_commit_get_tree (GGIT_COMMIT (commit))) ||
      !(entry = ggit_tree_get_by_path (tree, state->path, &error)) ||
      !(entry_oid = ggit_tree_entry_get_id (entry)) ||
      !(blob = ggit_repository_lookup (self->repository, entry_oid, GGIT_TYPE_BLOB, &error)))
    goto cleanup;

  contents = g_bytes_get_data (state->bytes, &len);

  g_assert (GGIT_IS_BLOB (blob));
  g_assert (contents != NULL);

  ranges = g_array_new (FALSE, FALSE, sizeof (Range));
  options = ggit_diff_options_new ();
  ggit_diff_options_set_n_context_lines (options, 0);
  ggit_diff_blob_to_buffer (GGIT_BLOB (blob),
                            state->path,
                            (const guint8 *)contents,
                            len,
                            state->path,
                            options,
                            NULL,         /* File Callback */
                            NULL,         /* Binary Callback */
                            diff_hunk_cb, /* Hunk Callback */
                            NULL,
                            ranges,
                            &error);

  if (error != NULL)
    goto cleanup;

  cache = line_cache_new ();

  for (guint i = 0; i < ranges->len; i++)
    {
      const Range *range = &g_array_index (ranges, Range, i);
      gint start_line = range->new_start - 1;
      gint end_line = range->new_start + range->new_lines - 1;

      if (range->old_lines == 0 && range->new_lines > 0)
        line_cache_mark_range (cache, start_line, end_line, LINE_MARK_ADDED);
      else if (range->new_lines == 0 && range->old_lines > 0)
        {
          if (start_line < 0)
            line_cache_mark_range (cache, 0, 0, LINE_MARK_PREVIOUS_REMOVED);
          else
            line_cache_mark_range (cache, start_line + 1, start_line + 1, LINE_MARK_REMOVED);
        }
      else
        line_cache_mark_range (cache, start_line, end_line, LINE_MARK_CHANGED);
    }

  ret = line_cache_to_variant (cache);

cleanup:
  g_clear_object (&blob);
  g_clear_pointer (&entry_oid, ggit_oid_free);
  g_clear_pointer (&entry, ggit_tree_entry_unref);
  g_clear_object (&tree);
  g_clear_object (&commit);
  g_clear_pointer (&oid, ggit_oid_free);
  g_clear_object (&head);
  g_clear_pointer (&cache, line_cache_free);

  g_assert (ret != NULL || error != NULL);

  if (error != NULL)
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_pointer (task,
                           g_steal_pointer (&ret),
                           (GDestroyNotify)g_variant_unref);
}

void
gbp_git_get_changes_async (GbpGit              *self,
                           const gchar         *path,
                           GBytes              *bytes,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  GetChanges *state;

  g_assert (GBP_IS_GIT (self));
  g_assert (path != NULL);
  g_assert (bytes != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  state = g_slice_new0 (GetChanges);
  state->path = g_strdup (path);
  state->bytes = g_bytes_ref (bytes);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gbp_git_get_changes_async);
  g_task_set_task_data (task, state, (GDestroyNotify)get_changes_free);

  if (self->repository == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_INITIALIZED,
                               "No repository loaded");
      return;
    }

  g_task_run_in_thread (task, gbp_git_get_changes_worker);
}

GVariant *
gbp_git_get_changes_finish (GbpGit        *self,
                            GAsyncResult  *result,
                            GError       **error)
{
  g_assert (GBP_IS_GIT (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_pointer (G_TASK (result), error);
}
