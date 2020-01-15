/* gbp-git-vcs.c
 *
 * Copyright 2014-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-git-vcs"

#include "config.h"

#include <glib/gi18n.h>

#include "daemon/ipc-git-types.h"

#include "gbp-git-branch.h"
#include "gbp-git-progress.h"
#include "gbp-git-tag.h"
#include "gbp-git-vcs.h"
#include "gbp-git-vcs-config.h"

struct _GbpGitVcs
{
  IdeObject         parent;

  /* read-only, thread-safe access */
  IpcGitRepository *repository;
  GFile            *workdir;
};

enum {
  PROP_0,
  PROP_BRANCH_NAME,
  PROP_WORKDIR,
  N_PROPS
};

static GFile *
gbp_git_vcs_get_workdir (IdeVcs *vcs)
{
  return GBP_GIT_VCS (vcs)->workdir;
}

static gboolean
gbp_git_vcs_is_ignored (IdeVcs  *vcs,
                        GFile   *file,
                        GError **error)
{
  GbpGitVcs *self = (GbpGitVcs *)vcs;
  g_autofree gchar *relative_path = NULL;
  gboolean is_ignored = FALSE;

  g_assert (GBP_IS_GIT_VCS (self));
  g_assert (G_IS_FILE (file));

  if (g_file_equal (self->workdir, file))
    return FALSE;

  /*
   * This may be called from threads.
   *
   * However, we do not change our GbpGitVcs.repository field after the
   * creation of the GbpGitVcs. Also, the GDBusProxy (IpcGitRepository)
   * is thread-safe in terms of calling operations on the remote object
   * from multiple threads.
   *
   * Also, GbpGitVcs.workdir is not changed after creation, so we can
   * use that to for determining the relative path.
   */

  if (!g_file_has_prefix (file, self->workdir))
    return TRUE;

  relative_path = g_file_get_relative_path (self->workdir, file);

  if (!ipc_git_repository_call_path_is_ignored_sync (self->repository,
                                                     relative_path,
                                                     &is_ignored,
                                                     NULL,
                                                     error))
    return FALSE;

  return is_ignored;
}

static IdeVcsConfig *
gbp_git_vcs_get_config (IdeVcs *vcs)
{
  IdeVcsConfig *config;

  g_assert (GBP_IS_GIT_VCS (vcs));

  config = g_object_new (GBP_TYPE_GIT_VCS_CONFIG,
                         "parent", vcs,
                         NULL);
  gbp_git_vcs_config_set_global (GBP_GIT_VCS_CONFIG (config), FALSE);

  return g_steal_pointer (&config);
}

static gchar *
gbp_git_vcs_get_branch_name (IdeVcs *vcs)
{
  return ipc_git_repository_dup_branch (GBP_GIT_VCS (vcs)->repository);
}

static void
gbp_git_vcs_switch_branch_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  IpcGitRepository *repository = (IpcGitRepository *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IPC_IS_GIT_REPOSITORY (repository));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ipc_git_repository_call_switch_branch_finish (repository, result, &error))
    {
      g_dbus_error_strip_remote_error (error);
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  ide_task_return_boolean (task, TRUE);
}

static void
gbp_git_vcs_switch_branch_async (IdeVcs              *vcs,
                                 IdeVcsBranch        *branch,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  GbpGitVcs *self = (GbpGitVcs *)vcs;
  g_autoptr(IdeTask) task = NULL;
  g_autofree gchar *branch_id = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GIT_VCS (self));
  g_assert (GBP_IS_GIT_BRANCH (branch));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_git_vcs_switch_branch_async);

  branch_id = ide_vcs_branch_get_id (branch);

  ipc_git_repository_call_switch_branch (self->repository,
                                         branch_id,
                                         cancellable,
                                         gbp_git_vcs_switch_branch_cb,
                                         g_steal_pointer (&task));
}

static gboolean
gbp_git_vcs_switch_branch_finish (IdeVcs        *vcs,
                                  GAsyncResult  *result,
                                  GError       **error)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GIT_VCS (vcs));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
gbp_git_vcs_push_branch_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  IpcGitRepository *repository = (IpcGitRepository *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IPC_IS_GIT_REPOSITORY (repository));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ipc_git_repository_call_push_finish (repository, result, &error))
    {
      g_print ("error: %p\n", error);
      g_dbus_error_strip_remote_error (error);
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  ide_object_message (ide_task_get_source_object (task), "%s", _("Pushed."));

  ide_task_return_boolean (task, TRUE);
}

static void
gbp_git_vcs_push_branch_async (IdeVcs              *vcs,
                               IdeVcsBranch        *branch,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  GbpGitVcs *self = (GbpGitVcs *)vcs;
  g_autofree gchar *branch_id = NULL;
  g_autofree gchar *name = NULL;
  g_autofree gchar *title = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(IdeNotification) notif = NULL;
  g_autoptr(IpcGitProgress) progress = NULL;
  g_autoptr(GPtrArray) ref_specs = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GIT_VCS (self));
  g_assert (GBP_IS_GIT_BRANCH (branch));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_git_vcs_push_branch_async);

  branch_id = ide_vcs_branch_get_id (branch);
  name = ide_vcs_branch_get_name (branch);

  ref_specs = g_ptr_array_new_with_free_func (g_free);
  g_ptr_array_add (ref_specs, g_strdup_printf ("%s:%s", branch_id, branch_id));
  g_ptr_array_add (ref_specs, NULL);

  notif = ide_notification_new ();
  title = g_strdup_printf (_("Pushing ref “%s”"), name);
  ide_notification_set_title (notif, title);
  ide_notification_set_has_progress (notif, TRUE);
  ide_notification_attach (notif, IDE_OBJECT (vcs));

  progress = gbp_git_progress_new (g_dbus_proxy_get_connection (G_DBUS_PROXY (self->repository)),
                                   notif,
                                   cancellable,
                                   &error);
  gbp_git_progress_set_withdraw (GBP_GIT_PROGRESS (progress), TRUE);
  ide_task_set_task_data (task, g_object_ref (progress), g_object_unref);

  if (error != NULL)
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ipc_git_repository_call_push (self->repository,
                                  "origin",
                                  (const gchar * const *)ref_specs->pdata,
                                  IPC_GIT_PUSH_FLAGS_NONE,
                                  g_dbus_interface_skeleton_get_object_path (G_DBUS_INTERFACE_SKELETON (progress)),
                                  cancellable,
                                  gbp_git_vcs_push_branch_cb,
                                  g_steal_pointer (&task));
}

static gboolean
gbp_git_vcs_push_branch_finish (IdeVcs        *vcs,
                                GAsyncResult  *result,
                                GError       **error)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GIT_VCS (vcs));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static GPtrArray *
create_branches (gchar **refs)
{
  GPtrArray *ret = g_ptr_array_new_with_free_func (g_object_unref);

  if (refs != NULL)
    {
      for (guint i = 0; refs[i]; i++)
        g_ptr_array_add (ret, gbp_git_branch_new (refs[i]));
    }

  return g_steal_pointer (&ret);
}

static void
gbp_git_vcs_list_branches_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  IpcGitRepository *repository = (IpcGitRepository *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_auto(GStrv) refs = NULL;

  g_assert (IPC_IS_GIT_REPOSITORY (repository));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ipc_git_repository_call_list_refs_by_kind_finish (repository, &refs, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_pointer (task, create_branches (refs), g_ptr_array_unref);
}

static void
gbp_git_vcs_list_branches_async (IdeVcs              *vcs,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  GbpGitVcs *self = (GbpGitVcs *)vcs;
  g_autoptr(IdeTask) task = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GIT_VCS (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_git_vcs_list_branches_async);

  ipc_git_repository_call_list_refs_by_kind (self->repository,
                                             IPC_GIT_REF_BRANCH,
                                             cancellable,
                                             gbp_git_vcs_list_branches_cb,
                                             g_steal_pointer (&task));
}

static GPtrArray *
gbp_git_vcs_list_branches_finish (IdeVcs        *vcs,
                                  GAsyncResult  *result,
                                  GError       **error)
{
  GPtrArray *ret;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GIT_VCS (vcs));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  return IDE_PTR_ARRAY_STEAL_FULL (&ret);
}

static GPtrArray *
create_tags (gchar **refs)
{
  GPtrArray *ret = g_ptr_array_new_with_free_func (g_object_unref);

  if (refs != NULL)
    {
      for (guint i = 0; refs[i]; i++)
        g_ptr_array_add (ret, gbp_git_tag_new (refs[i]));
    }

  return g_steal_pointer (&ret);
}

static void
gbp_git_vcs_list_tags_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  IpcGitRepository *repository = (IpcGitRepository *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_auto(GStrv) refs = NULL;

  g_assert (IPC_IS_GIT_REPOSITORY (repository));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ipc_git_repository_call_list_refs_by_kind_finish (repository, &refs, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_pointer (task, create_tags (refs), g_ptr_array_unref);
}

static void
gbp_git_vcs_list_tags_async (IdeVcs              *vcs,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  GbpGitVcs *self = (GbpGitVcs *)vcs;
  g_autoptr(IdeTask) task = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GIT_VCS (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_git_vcs_list_tags_async);

  ipc_git_repository_call_list_refs_by_kind (self->repository,
                                             IPC_GIT_REF_TAG,
                                             cancellable,
                                             gbp_git_vcs_list_tags_cb,
                                             g_steal_pointer (&task));
}

static GPtrArray *
gbp_git_vcs_list_tags_finish (IdeVcs        *vcs,
                              GAsyncResult  *result,
                              GError       **error)
{
  GPtrArray *ret;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GIT_VCS (vcs));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  return IDE_PTR_ARRAY_STEAL_FULL (&ret);
}

static GListModel *
create_status_model (GbpGitVcs *self,
                     GVariant  *files)
{
  g_autoptr(GListStore) store = NULL;
  GVariantIter iter;
  const gchar *path = NULL;
  guint flags = 0;

  g_assert (GBP_IS_GIT_VCS (self));
  g_assert (files != NULL);

  store = g_list_store_new (IDE_TYPE_VCS_FILE_INFO);

  g_variant_iter_init (&iter, files);

  while (g_variant_iter_next (&iter, "(&su)", &path, &flags))
    {
      g_autoptr(GFile) file = g_file_get_child (self->workdir, path);

      g_list_store_append (store,
                           g_object_new (IDE_TYPE_VCS_FILE_INFO,
                                         "file", file,
                                         "status", flags,
                                         NULL));
    }

  return G_LIST_MODEL (g_steal_pointer (&store));
}

static void
gbp_git_vcs_list_status_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  IpcGitRepository *repository = (IpcGitRepository *)object;
  g_autoptr(GVariant) files = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeTask) task = user_data;
  GbpGitVcs *self;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IPC_IS_GIT_REPOSITORY (repository));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);

  if (!ipc_git_repository_call_list_status_finish (repository, &files, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_object (task, create_status_model (self, files));
}

static void
gbp_git_vcs_list_status_async (IdeVcs              *vcs,
                               GFile               *directory_or_file,
                               gboolean             include_descendants,
                               gint                 io_priority,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  GbpGitVcs *self = (GbpGitVcs *)vcs;
  g_autoptr(IdeTask) task = NULL;
  g_autofree gchar *relative_path = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GIT_VCS (self));
  g_assert (G_IS_FILE (directory_or_file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_git_vcs_list_status_async);

  if (!g_file_has_prefix (directory_or_file, self->workdir) &&
      !g_file_equal (directory_or_file, self->workdir))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 _("Directory is not within repository"));
      return;
    }

  relative_path = g_file_get_relative_path (self->workdir, directory_or_file);

  ipc_git_repository_call_list_status (self->repository,
                                       relative_path ?: "",
                                       cancellable,
                                       gbp_git_vcs_list_status_cb,
                                       g_steal_pointer (&task));
}

static GListModel *
gbp_git_vcs_list_status_finish (IdeVcs        *vcs,
                                GAsyncResult  *result,
                                GError       **error)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GIT_VCS (vcs));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_object (IDE_TASK (result), error);
}

static void
vcs_iface_init (IdeVcsInterface *iface)
{
  iface->get_workdir = gbp_git_vcs_get_workdir;
  iface->is_ignored = gbp_git_vcs_is_ignored;
  iface->get_config = gbp_git_vcs_get_config;
  iface->get_branch_name = gbp_git_vcs_get_branch_name;
  iface->switch_branch_async = gbp_git_vcs_switch_branch_async;
  iface->switch_branch_finish = gbp_git_vcs_switch_branch_finish;
  iface->push_branch_async = gbp_git_vcs_push_branch_async;
  iface->push_branch_finish = gbp_git_vcs_push_branch_finish;
  iface->list_branches_async = gbp_git_vcs_list_branches_async;
  iface->list_branches_finish = gbp_git_vcs_list_branches_finish;
  iface->list_tags_async = gbp_git_vcs_list_tags_async;
  iface->list_tags_finish = gbp_git_vcs_list_tags_finish;
  iface->list_status_async = gbp_git_vcs_list_status_async;
  iface->list_status_finish = gbp_git_vcs_list_status_finish;
}

G_DEFINE_TYPE_WITH_CODE (GbpGitVcs, gbp_git_vcs, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_VCS, vcs_iface_init))

static GParamSpec *properties [N_PROPS];

static void
gbp_git_vcs_finalize (GObject *object)
{
  GbpGitVcs *self = (GbpGitVcs *)object;

  g_clear_object (&self->repository);

  G_OBJECT_CLASS (gbp_git_vcs_parent_class)->finalize (object);
}

static void
gbp_git_vcs_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  GbpGitVcs *self = GBP_GIT_VCS (object);

  switch (prop_id)
    {
    case PROP_BRANCH_NAME:
      g_value_take_string (value, gbp_git_vcs_get_branch_name (IDE_VCS (self)));
      break;

    case PROP_WORKDIR:
      g_value_take_object (value, g_file_dup (self->workdir));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_git_vcs_class_init (GbpGitVcsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_git_vcs_finalize;
  object_class->get_property = gbp_git_vcs_get_property;

  properties [PROP_BRANCH_NAME] =
    g_param_spec_string ("branch-name",
                         "Branch Name",
                         "The name of the current branch",
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_WORKDIR] =
    g_param_spec_object ("workdir",
                         "Workdir",
                         "The workdir of the vcs",
                         G_TYPE_FILE,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_git_vcs_init (GbpGitVcs *self)
{
}

static void
gbp_git_vcs_notify_branch_cb (GbpGitVcs        *self,
                              GParamSpec       *pspec,
                              IpcGitRepository *repository)
{
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BRANCH_NAME]);
}

static void
gbp_git_vcs_changed_cb (GbpGitVcs        *self,
                        IpcGitRepository *repository)
{
  ide_vcs_emit_changed (IDE_VCS (self));
}

GbpGitVcs *
gbp_git_vcs_new (IpcGitRepository *repository)
{
  const gchar *workdir;
  GbpGitVcs *ret;

  g_return_val_if_fail (IPC_IS_GIT_REPOSITORY (repository), NULL);

  workdir = ipc_git_repository_get_workdir (repository);

  ret = g_object_new (GBP_TYPE_GIT_VCS, NULL);
  ret->repository = g_object_ref (repository);
  ret->workdir = g_file_new_for_path (workdir);

  g_signal_connect_object (repository,
                           "notify::branch",
                           G_CALLBACK (gbp_git_vcs_notify_branch_cb),
                           ret,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (repository,
                           "changed",
                           G_CALLBACK (gbp_git_vcs_changed_cb),
                           ret,
                           G_CONNECT_SWAPPED);

  return g_steal_pointer (&ret);
}

IpcGitRepository *
gbp_git_vcs_get_repository (GbpGitVcs *self)
{
  g_return_val_if_fail (GBP_IS_GIT_VCS (self), NULL);

  return self->repository;
}
