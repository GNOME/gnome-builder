/* ipc-git-repository-impl.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#define G_LOG_DOMAIN "ipc-git-repository-impl"

#include <libgit2-glib/ggit.h>
#include <git2.h>
#include <glib/gi18n.h>
#include <stdlib.h>

#include "ipc-git-blame-impl.h"
#include "ipc-git-change-monitor-impl.h"
#include "ipc-git-config-impl.h"
#include "ipc-git-index-monitor.h"
#include "ipc-git-progress.h"
#include "ipc-git-remote-callbacks.h"
#include "ipc-git-repository-impl.h"
#include "ipc-git-types.h"
#include "ipc-git-util.h"

#if LIBGIT2_VER_MAJOR > 0 || (LIBGIT2_VER_MAJOR == 0 && LIBGIT2_VER_MINOR >= 28)
G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC (git_buf, git_buf_dispose)
#else
G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC (git_buf, git_buf_free)
#endif

struct _IpcGitRepositoryImpl
{
  IpcGitRepositorySkeleton  parent;
  GgitRepository           *repository;
  GHashTable               *blamers;
  GHashTable               *change_monitors;
  GHashTable               *configs;
  IpcGitIndexMonitor       *monitor;
};

static void
ipc_git_repository_impl_monitor_changed_cb (IpcGitRepositoryImpl *self,
                                            IpcGitIndexMonitor   *monitor)
{
  g_autoptr(GgitRef) head_ref = NULL;
  const char *shortname = NULL;
  GHashTableIter iter;
  gpointer key;

  g_assert (IPC_IS_GIT_REPOSITORY_IMPL (self));
  g_assert (IPC_IS_GIT_INDEX_MONITOR (monitor));

  g_hash_table_iter_init (&iter, self->change_monitors);
  while (g_hash_table_iter_next (&iter, (gpointer *)&key, NULL))
    {
      IpcGitChangeMonitorImpl *change_monitor = key;
      g_assert (IPC_IS_GIT_CHANGE_MONITOR_IMPL (change_monitor));
      ipc_git_change_monitor_impl_reset (change_monitor);
    }

  if ((head_ref = ggit_repository_get_head (self->repository, NULL)))
    {
      g_assert (GGIT_IS_REF (head_ref));
      shortname = ggit_ref_get_shorthand (head_ref);
    }

  if (shortname == NULL)
    shortname = "main";

  ipc_git_repository_set_branch ((IpcGitRepository *)self, shortname);
  ipc_git_repository_emit_changed (IPC_GIT_REPOSITORY (self));
}

static gchar *
get_signing_key (IpcGitRepositoryImpl *self)
{
  g_autoptr(GgitConfig) config = NULL;
  const gchar *ret = NULL;

  g_assert (IPC_IS_GIT_REPOSITORY_IMPL (self));

  if ((config = ggit_repository_get_config (self->repository, NULL)))
    {
      g_autoptr(GgitConfig) snapshot = ggit_config_snapshot (config, NULL);

      if (snapshot != NULL)
        ret = ggit_config_get_string (snapshot, "user.signingkey", NULL);
    }

  return g_strdup (ret);
}

static gint
ipc_git_repository_impl_handle_list_status_cb (const gchar     *path,
                                               GgitStatusFlags  flags,
                                               gpointer         user_data)
{
  GVariantBuilder *builder = user_data;

  g_assert (path != NULL);
  g_assert (builder != NULL);

  g_variant_builder_add (builder, "(^ayu)", path, flags);

  return GIT_OK;
}

static gboolean
ipc_git_repository_impl_handle_list_status (IpcGitRepository      *repository,
                                            GDBusMethodInvocation *invocation,
                                            const gchar           *path)
{
  IpcGitRepositoryImpl *self = (IpcGitRepositoryImpl *)repository;
  g_autoptr(GgitRepository) repo  = NULL;
  g_autoptr(GgitStatusOptions) options = NULL;
  g_autoptr(GFile) location = NULL;
  g_autoptr(GError) error = NULL;
  const gchar *paths[] = { NULL, NULL };
  GVariantBuilder builder;

  g_assert (IPC_IS_GIT_REPOSITORY_IMPL (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (path != NULL);

  paths[0] = path[0] ? path : NULL;

  location = ggit_repository_get_location (self->repository);

  if (!(repo = ggit_repository_open (location, &error)))
    return complete_wrapped_error (invocation, error);

  options = ggit_status_options_new (GGIT_STATUS_OPTION_DEFAULT,
                                     GGIT_STATUS_SHOW_INDEX_AND_WORKDIR,
                                     paths);

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(ayu)"));

  if (!ggit_repository_file_status_foreach (repo,
                                            options,
                                            ipc_git_repository_impl_handle_list_status_cb,
                                            &builder,
                                            &error))
    complete_wrapped_error (invocation, error);
  else
    ipc_git_repository_complete_list_status (repository,
                                             invocation,
                                             g_variant_builder_end (&builder));

  return TRUE;
}

static gboolean
ipc_git_repository_impl_handle_switch_branch (IpcGitRepository      *repository,
                                              GDBusMethodInvocation *invocation,
                                              const gchar           *branch)
{
  IpcGitRepositoryImpl *self = (IpcGitRepositoryImpl *)repository;
  g_autoptr(GgitCheckoutOptions) checkout_options = NULL;
  g_autoptr(GgitObject) obj = NULL;
  g_autoptr(GgitRef) ref = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) workdir = NULL;
  const gchar *shortname;

  g_assert (IPC_IS_GIT_REPOSITORY_IMPL (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (branch != NULL);

  checkout_options = ggit_checkout_options_new ();
  ggit_checkout_options_set_strategy (checkout_options, GGIT_CHECKOUT_SAFE);

  if (!(ref = ggit_repository_lookup_reference (self->repository, branch, &error)) ||
      !(obj = ggit_ref_lookup (ref, &error)) ||
      !ggit_repository_checkout_tree (self->repository, obj, checkout_options, &error) ||
      !ggit_repository_set_head (self->repository, branch, &error))
    return complete_wrapped_error (invocation, error);

  if (!(shortname = ggit_ref_get_shorthand (ref)))
    shortname = "main";

  workdir = ggit_repository_get_workdir (self->repository);

  ipc_git_repository_set_branch (repository, shortname);
  ipc_git_repository_set_workdir (repository, g_file_peek_path (workdir));
  ipc_git_repository_complete_switch_branch (repository, invocation);

  ipc_git_repository_emit_changed (repository);

  return TRUE;
}

static gboolean
ipc_git_repository_impl_handle_path_is_ignored (IpcGitRepository      *repository,
                                                GDBusMethodInvocation *invocation,
                                                const gchar           *path)
{
  IpcGitRepositoryImpl *self = (IpcGitRepositoryImpl *)repository;
  g_autoptr(GError) error = NULL;
  gboolean ret;

  g_assert (IPC_IS_GIT_REPOSITORY_IMPL (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (path != NULL);

  ret = ggit_repository_path_is_ignored (self->repository, path, &error);

  if (error != NULL)
    complete_wrapped_error (invocation, error);
  else
    ipc_git_repository_complete_path_is_ignored (repository, invocation, ret);

  return TRUE;
}

static gint
compare_refs (gconstpointer a,
              gconstpointer b)
{
  return g_utf8_collate (*(const gchar **)a, *(const gchar **)b);
}

static gboolean
ipc_git_repository_impl_handle_list_refs_by_kind (IpcGitRepository      *repository,
                                                  GDBusMethodInvocation *invocation,
                                                  guint                  kind)
{
  IpcGitRepositoryImpl *self = (IpcGitRepositoryImpl *)repository;
  g_autoptr(GPtrArray) ret = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IPC_IS_GIT_REPOSITORY_IMPL (repository));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));

  if (kind != IPC_GIT_REF_BRANCH && kind != IPC_GIT_REF_TAG)
    {
      g_dbus_method_invocation_return_dbus_error (invocation,
                                                  "org.freedesktop.DBus.Error.InvalidArgs",
                                                  "kind must be a tag or a branch");
      return TRUE;
    }

  ret = g_ptr_array_new_with_free_func (g_free);

  if (kind == IPC_GIT_REF_BRANCH)
    {
      g_autoptr(GgitBranchEnumerator) enumerator = NULL;

      if (!(enumerator = ggit_repository_enumerate_branches (self->repository, GGIT_BRANCH_LOCAL, &error)))
        return complete_wrapped_error (invocation, error);

      while (ggit_branch_enumerator_next (enumerator))
        {
          g_autoptr(GgitRef) ref = ggit_branch_enumerator_get (enumerator);
          const gchar *name = ggit_ref_get_name (ref);
          g_ptr_array_add (ret, g_strdup (name));
        }
    }
  else if (kind == IPC_GIT_REF_TAG)
    {
      g_autofree gchar **names = NULL;

      if (!(names = ggit_repository_list_tags (self->repository, &error)))
        return complete_wrapped_error (invocation, error);

      for (guint i = 0; names[i] != NULL; i++)
        g_ptr_array_add (ret, g_steal_pointer (&names[i]));
    }
  else
    g_assert_not_reached ();

  qsort (ret->pdata, ret->len, sizeof (gchar *), compare_refs);
  g_ptr_array_add (ret, NULL);

  ipc_git_repository_complete_list_refs_by_kind (repository,
                                                 invocation,
                                                 (const gchar * const *)ret->pdata);

  return TRUE;
}

static gboolean
ipc_git_repository_impl_handle_close (IpcGitRepository      *repository,
                                      GDBusMethodInvocation *invocation)
{
  g_assert (IPC_IS_GIT_REPOSITORY_IMPL (repository));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));

  /* Service will drop it's reference from the hashtable */
  ipc_git_repository_emit_closed (IPC_GIT_REPOSITORY (repository));

  g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (repository));
  ipc_git_repository_complete_close (repository, invocation);

  return TRUE;
}

static void
on_monitor_closed_cb (IpcGitRepositoryImpl *self,
                      IpcGitChangeMonitor  *monitor)
{
  g_assert (IPC_IS_GIT_REPOSITORY_IMPL (self));
  g_assert (IPC_IS_GIT_CHANGE_MONITOR (monitor));

  if (self->change_monitors != NULL)
    g_hash_table_remove (self->change_monitors, monitor);
}

static void
on_blame_closed_cb (IpcGitRepositoryImpl *self,
                    IpcGitBlame  *blame)
{
  g_assert (IPC_IS_GIT_REPOSITORY_IMPL (self));
  g_assert (IPC_IS_GIT_BLAME (blame));

  if (self->blamers != NULL)
    g_hash_table_remove (self->blamers, blame);
}

static gboolean
ipc_git_repository_impl_handle_create_change_monitor (IpcGitRepository      *repository,
                                                      GDBusMethodInvocation *invocation,
                                                      const gchar           *path)
{
  IpcGitRepositoryImpl *self = (IpcGitRepositoryImpl *)repository;
  g_autoptr(IpcGitChangeMonitor) monitor = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *guid = NULL;
  g_autofree gchar *obj_path = NULL;
  GDBusConnection *conn;

  g_assert (IPC_IS_GIT_REPOSITORY_IMPL (repository));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (path != NULL);

  conn = g_dbus_method_invocation_get_connection (invocation);
  guid = g_dbus_generate_guid ();
  obj_path = g_strdup_printf ("/org/gnome/Builder/Git/ChangeMonitor/%s", guid);
  monitor = ipc_git_change_monitor_impl_new (self->repository, path);

  if (!g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (monitor), conn, obj_path, &error))
    return complete_wrapped_error (invocation, error);

  g_signal_connect_object (monitor,
                           "closed",
                           G_CALLBACK (on_monitor_closed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_hash_table_insert (self->change_monitors,
                       g_steal_pointer (&monitor),
                       g_strdup (obj_path));

  ipc_git_repository_complete_create_change_monitor (repository, invocation, obj_path);

  return TRUE;
}

static gboolean
ipc_git_repository_impl_handle_blame (IpcGitRepository      *repository,
                                      GDBusMethodInvocation *invocation,
                                      const gchar           *path)
{
  IpcGitRepositoryImpl *self = (IpcGitRepositoryImpl *)repository;
  g_autoptr(IpcGitBlame) blame = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *guid = NULL;
  g_autofree gchar *obj_path = NULL;
  GDBusConnection *conn;

  g_assert (IPC_IS_GIT_REPOSITORY_IMPL (repository));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (path != NULL);

  conn = g_dbus_method_invocation_get_connection (invocation);
  guid = g_dbus_generate_guid ();
  obj_path = g_strdup_printf ("/org/gnome/Builder/Git/Blame/%s", guid);
  blame = ipc_git_blame_impl_new (self->repository, path);

  if (!g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (blame), conn, obj_path, &error))
    return complete_wrapped_error (invocation, error);

  g_signal_connect_object (blame,
                           "closed",
                           G_CALLBACK (on_blame_closed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_hash_table_insert (self->blamers,
                       g_steal_pointer (&blame),
                       g_strdup (obj_path));

  ipc_git_repository_complete_blame (repository, invocation, obj_path);

  return TRUE;
}

static gboolean
ipc_git_repository_impl_handle_stage_file (IpcGitRepository      *repository,
                                           GDBusMethodInvocation *invocation,
                                           const gchar           *path)
{
  IpcGitRepositoryImpl *self = (IpcGitRepositoryImpl *)repository;
  g_autoptr(GgitIndex) index = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IPC_IS_GIT_REPOSITORY_IMPL (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (path != NULL);

  if (!(index = ggit_repository_get_index (self->repository, &error)) ||
      !ggit_index_add_path (index, path, &error) ||
      !ggit_index_write (index, &error))
    return complete_wrapped_error (invocation, error);
  else
    ipc_git_repository_complete_stage_file (repository, invocation);

  ipc_git_repository_emit_changed (repository);

  return TRUE;
}

static GgitDiff *
ipc_git_repository_impl_get_index_diff (IpcGitRepositoryImpl  *self,
                                        GgitTree             **out_tree,
                                        GError               **error)
{
  g_autoptr(GgitDiffOptions) options = NULL;
  g_autoptr(GgitIndex) index = NULL;
  g_autoptr(GgitDiff) diff = NULL;
  g_autoptr(GgitTree) tree = NULL;

  g_assert (IPC_IS_GIT_REPOSITORY_IMPL (self));

  if (out_tree != NULL)
    *out_tree = NULL;

  options = ggit_diff_options_new ();
  ggit_diff_options_set_flags (options,
                               GGIT_DIFF_INCLUDE_UNTRACKED |
                               GGIT_DIFF_DISABLE_PATHSPEC_MATCH |
                               GGIT_DIFF_RECURSE_UNTRACKED_DIRS);
  ggit_diff_options_set_n_context_lines (options, 3);
  ggit_diff_options_set_n_interhunk_lines (options, 3);

  if (!ggit_repository_is_empty (self->repository, error))
    {
      g_autoptr(GgitRef) head = NULL;
      g_autoptr(GgitObject) obj = NULL;

      if (!(head = ggit_repository_get_head (self->repository, error)) ||
          !(obj = ggit_ref_lookup (head, error)))
        return NULL;

      tree = ggit_commit_get_tree (GGIT_COMMIT (obj));
    }

  if (!(index = ggit_repository_get_index (self->repository, error)) ||
      !(diff = ggit_diff_new_tree_to_index (self->repository, tree, index, options, error)))
    return NULL;

  if (out_tree != NULL)
    *out_tree = g_steal_pointer (&tree);

  return g_steal_pointer (&diff);
}

static gboolean
get_signed_data (const gchar  *data,
                 const gchar  *key,
                 gchar       **signed_data,
                 GError      **error)
{
  g_autoptr(GSubprocessLauncher) launcher = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autofree gchar *stdout_buf = NULL;

  g_assert (data);
  g_assert (signed_data);

  launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDIN_PIPE |
                                        G_SUBPROCESS_FLAGS_STDOUT_PIPE);
  subprocess = g_subprocess_launcher_spawn (launcher,
                                            error,
                                            "gpg",
                                            "--clear-sign",
                                            "--default-key",
                                            key,
                                            "-",
                                            NULL);
  if (subprocess == NULL)
    return FALSE;

  if (!g_subprocess_communicate_utf8 (subprocess, data, NULL, &stdout_buf, NULL, error))
    return FALSE;

  *signed_data = g_steal_pointer (&stdout_buf);

  return TRUE;
}

static GgitOId *
commit_create_with_signature (GgitRepository  *repository,
                              const gchar     *update_ref,
                              GgitSignature   *author,
                              GgitSignature   *committer,
                              const gchar     *message_encoding,
                              const gchar     *message,
                              GgitTree        *tree,
                              GgitCommit     **parents,
                              gint             parent_count,
                              const gchar     *gpg_key_id,
                              GError         **error)
{
  g_autofree git_commit **parents_native = NULL;
  g_autofree gchar *contents = NULL;
  g_autoptr(GgitOId) wrapped = NULL;
  g_auto(git_buf) buf = {0};
  g_autoptr(GgitRef) head = NULL;
  g_autoptr(GgitRef) new_head = NULL;
  git_oid oid;
  gint ret;

  g_assert (GGIT_IS_REPOSITORY (repository));
  g_assert (update_ref != NULL);
  g_assert (GGIT_IS_SIGNATURE (author));
  g_assert (GGIT_IS_SIGNATURE (committer));
  g_assert (GGIT_IS_TREE (tree));
  g_assert (parents != NULL);
  g_assert (parent_count > 0);
  g_assert (GGIT_IS_COMMIT (parents[0]));

  parents_native = g_new0 (git_commit *, parent_count);

  for (gint i = 0; i < parent_count; ++i)
    parents_native[i] = _ggit_native_get (parents[i]);

  ret = git_commit_create_buffer (&buf,
                                  _ggit_native_get (repository),
                                  _ggit_native_get (author),
                                  _ggit_native_get (committer),
                                  message_encoding,
                                  message,
                                  _ggit_native_get (tree),
                                  parent_count,
                                  /* just cast to (void*) to avoid the differences
                                   * between various libgit2 versions. See #2183.
                                   */
                                  (gpointer)parents_native);

  if (ret != GIT_OK)
    {
      _ggit_error_set (error, ret);
      return NULL;
    }

  if (!get_signed_data (buf.ptr, gpg_key_id, &contents, error))
    return NULL;

  ret = git_commit_create_with_signature (&oid,
                                          _ggit_native_get (repository),
                                          buf.ptr,
                                          contents,
                                          NULL);

  if (ret != GIT_OK)
    {
      _ggit_error_set (error, ret);
      return NULL;
    }

  wrapped = _ggit_oid_wrap (&oid);

  if (!(head = ggit_repository_get_head (repository, error)))
    return NULL;

  if (!(new_head = ggit_ref_set_target (head, wrapped, "commit", error)))
    return NULL;

  return g_steal_pointer (&wrapped);
}

static gboolean
ipc_git_repository_impl_commit (IpcGitRepositoryImpl  *self,
                                GgitSignature         *author,
                                GgitSignature         *committer,
                                GgitDiff              *diff,
                                const gchar           *message,
                                IpcGitCommitFlags      flags,
                                const gchar           *gpg_key_id,
                                GError               **error)
{
  g_autofree gchar *stripped = NULL;
  g_autofree gchar *signoff_message = NULL;
  g_autoptr(GgitTree) tree = NULL;
  g_autoptr(GgitIndex) index = NULL;
  g_autoptr(GgitRef) head = NULL;
  g_autoptr(GgitOId) written = NULL;
  g_autoptr(GgitOId) oid = NULL;
  g_autoptr(GgitOId) target = NULL;
  g_autoptr(GgitCommit) commit = NULL;

  g_assert (IPC_IS_GIT_REPOSITORY_IMPL (self));
  g_assert (GGIT_IS_REPOSITORY (self->repository));
  g_assert (GGIT_IS_SIGNATURE (author));
  g_assert (GGIT_IS_SIGNATURE (committer));
  g_assert (GGIT_IS_DIFF (diff));

  if ((flags & IPC_GIT_COMMIT_FLAGS_AMEND) &&
      (flags & IPC_GIT_COMMIT_FLAGS_GPG_SIGN))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   _("Cannot set AMEND and GPG_SIGN flags"));
      return FALSE;
    }

  if ((flags & IPC_GIT_COMMIT_FLAGS_GPG_SIGN) && !gpg_key_id)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   _("Cannot sign commit without GPG_KEY_ID"));
      return FALSE;
    }

  /* Remove extra whitespace */
  message = stripped = g_strstrip (g_strdup (message));

  /* Maybe add sign-off message */
  if (flags & IPC_GIT_COMMIT_FLAGS_SIGNOFF)
    {
      signoff_message = g_strdup_printf ("%s\n%sSigned-off-by: %s <%s>\n",
                                         message,
                                         /* Extra \n if first sign-off */
                                         strstr (message, "\nSigned-off-by: ") ? "" : "\n",
                                         ggit_signature_get_name (committer),
                                         ggit_signature_get_email (committer));
      message = signoff_message;
    }

  if (!(index = ggit_repository_get_index (self->repository, error)) ||
      !(written = ggit_index_write_tree (index, error)) ||
      !(tree = ggit_repository_lookup_tree (self->repository, written, error)) ||
      !(head = ggit_repository_get_head (self->repository, error)) ||
      !(target = ggit_ref_get_target (head)) ||
      !(commit = ggit_repository_lookup_commit (self->repository, target, error)))
    return FALSE;

  /* TODO: Hooks */

  if (flags & IPC_GIT_COMMIT_FLAGS_AMEND)
    oid = ggit_commit_amend (commit,
                             "HEAD",
                             author,
                             committer,
                             NULL, /* UTF-8 */
                             message,
                             tree,
                             error);
  else if (flags & IPC_GIT_COMMIT_FLAGS_GPG_SIGN)
    oid = commit_create_with_signature (self->repository,
                                        "HEAD",
                                        author,
                                        committer,
                                        NULL, /* UTF-8 */
                                        message,
                                        tree,
                                        &commit,
                                        1,
                                        gpg_key_id,
                                        error);
  else
    oid = ggit_repository_create_commit (self->repository,
                                         "HEAD",
                                         author,
                                         committer,
                                         NULL, /* UTF-8 */
                                         message,
                                         tree,
                                         &commit,
                                         1,
                                         error);

  return oid != NULL;
}

static gboolean
ipc_git_repository_impl_handle_commit (IpcGitRepository      *repository,
                                       GDBusMethodInvocation *invocation,
                                       GVariant              *details,
                                       guint                  flags)
{
  IpcGitRepositoryImpl *self = (IpcGitRepositoryImpl *)repository;
  g_autoptr(GgitTree) tree = NULL;
  g_autoptr(GgitDiff) diff = NULL;
  g_autoptr(GgitSignature) author_sig = NULL;
  g_autoptr(GgitSignature) committer_sig = NULL;
  g_autoptr(GError) error = NULL;
  const gchar *author_name;
  const gchar *author_email;
  const gchar *committer_name;
  const gchar *committer_email;
  const gchar *commit_msg;
  const gchar *gpg_key_id;

  g_assert (IPC_IS_GIT_REPOSITORY_IMPL (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (details != NULL);

  if (!g_variant_lookup (details, "GPG_KEY_ID", "&s", &gpg_key_id))
    gpg_key_id = get_signing_key (self);

  if (!g_variant_lookup (details, "AUTHOR_NAME", "&s", &author_name) ||
      !g_variant_lookup (details, "AUTHOR_EMAIL", "&s", &author_email) ||
      !g_variant_lookup (details, "COMMITTER_NAME", "&s", &committer_name) ||
      !g_variant_lookup (details, "COMMITTER_EMAIL", "&s", &committer_email) ||
      !g_variant_lookup (details, "COMMIT_MSG", "&s", &commit_msg))
    {
      g_dbus_method_invocation_return_dbus_error (invocation,
                                                  "org.freedesktop.DBus.Error.InvalidArgs",
                                                  "Invalid details for commit");
      return TRUE;
    }

  if (!(author_sig = ggit_signature_new_now (author_name, author_email, &error)) ||
      !(committer_sig = ggit_signature_new_now (committer_name, committer_email, &error)) ||
      !(diff = ipc_git_repository_impl_get_index_diff (self, &tree, &error)) ||
      !ipc_git_repository_impl_commit (self, author_sig, committer_sig, diff, commit_msg, flags, gpg_key_id, &error))
    return complete_wrapped_error (invocation, error);

  ipc_git_repository_complete_commit (repository, invocation);
  ipc_git_repository_emit_changed (repository);

  return TRUE;
}

typedef struct
{
  GgitRemote           *remote;
  GgitPushOptions      *push_options;
  IpcGitProgress       *progress;
  GgitRemoteCallbacks  *callbacks;
  GgitProxyOptions     *proxy_options;
  gchar               **ref_names;
} Push;

static void
push_free (Push *push)
{
  g_clear_object (&push->callbacks);
  g_clear_object (&push->progress);
  g_clear_object (&push->proxy_options);
  g_clear_object (&push->push_options);
  g_clear_object (&push->remote);
  g_clear_pointer (&push->ref_names, g_strfreev);
  g_slice_free (Push, push);
}

static void
push_worker (GTask        *task,
             gpointer      source_object,
             gpointer      task_data,
             GCancellable *cancellable)
{
  g_autoptr(GError) error = NULL;
  Push *push = task_data;

  g_assert (G_IS_TASK (task));
  g_assert (IPC_IS_GIT_REPOSITORY_IMPL (source_object));
  g_assert (push != NULL);
  g_assert (GGIT_IS_REMOTE (push->remote));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  ggit_remote_push (push->remote,
                    (const gchar * const *)push->ref_names,
                    push->push_options,
                    &error);

  if (error != NULL)
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_boolean (task, TRUE);
}

static void
ipc_git_repository_impl_handle_push_cb (GObject      *object,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
  IpcGitRepository *repository = (IpcGitRepository *)object;
  g_autoptr(GDBusMethodInvocation) invocation = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IPC_IS_GIT_REPOSITORY_IMPL (repository));
  g_assert (G_IS_TASK (result));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));

  if (g_task_propagate_boolean (G_TASK (result), &error))
    {
      ipc_git_repository_complete_push (repository, invocation);
      ipc_git_repository_emit_changed (repository);
    }
  else
    {
      complete_wrapped_error (invocation, error);
    }
}

static gboolean
ipc_git_repository_impl_handle_push (IpcGitRepository      *repository,
                                     GDBusMethodInvocation *invocation,
                                     const gchar           *url,
                                     const gchar * const   *ref_names,
                                     guint                  flags,
                                     const gchar           *progress_path)
{
  IpcGitRepositoryImpl *self = (IpcGitRepositoryImpl *)repository;
  g_autoptr(IpcGitProgress) progress = NULL;
  g_autoptr(GgitRemoteCallbacks) callbacks = NULL;
  g_autoptr(GgitProxyOptions) proxy_options = NULL;
  g_autoptr(GgitPushOptions) push_options = NULL;
  g_autoptr(GgitRemote) remote = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = NULL;
  Push *push;

  g_assert (IPC_IS_GIT_REPOSITORY_IMPL (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (GGIT_IS_REPOSITORY (self->repository));
  g_assert (url != NULL);
  g_assert (ref_names != NULL);

  if (flags & IPC_GIT_PUSH_FLAGS_ATOMIC)
    {
      g_dbus_method_invocation_return_dbus_error (invocation,
                                                  "org.freedesktop.DBus.Error.InvalidArgs",
                                                  "atomic is not currently supported");
      return TRUE;
    }

  /* Try to lookup the remote, or create a new anonyous URL for it */
  if (!(remote = ggit_repository_lookup_remote (self->repository, url, &error)))
    {
      g_autoptr(GError) anon_error = NULL;

      if (!(remote = ggit_remote_new_anonymous (self->repository, url, &anon_error)))
        return complete_wrapped_error (invocation, error);

      g_clear_error (&error);
    }

  progress = ipc_git_progress_proxy_new_sync (g_dbus_method_invocation_get_connection (invocation),
                                              G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                                              NULL,
                                              progress_path,
                                              NULL,
                                              &error);
  if (progress == NULL)
    return complete_wrapped_error (invocation, error);

  callbacks = ipc_git_remote_callbacks_new (progress, -1);

  push_options = ggit_push_options_new ();
  ggit_push_options_set_remote_callbacks (push_options, callbacks);

  push = g_slice_new0 (Push);
  push->ref_names = g_strdupv ((gchar **)ref_names);
  push->remote = g_steal_pointer (&remote);
  push->progress = g_steal_pointer (&progress);
  push->callbacks = g_steal_pointer (&callbacks);
  push->proxy_options = g_steal_pointer (&proxy_options);
  push->push_options = g_steal_pointer (&push_options);

  task = g_task_new (self, NULL, ipc_git_repository_impl_handle_push_cb, g_object_ref (invocation));
  g_task_set_source_tag (task, ipc_git_repository_impl_handle_push);
  g_task_set_task_data (task, push, (GDestroyNotify)push_free);
  g_task_run_in_thread (task, push_worker);

  return TRUE;
}

static void
ipc_git_repository_impl_config_closed_cb (IpcGitRepositoryImpl *self,
                                          IpcGitConfigImpl     *config)
{
  g_assert (IPC_IS_GIT_REPOSITORY_IMPL (self));
  g_assert (IPC_IS_GIT_CONFIG_IMPL (config));

  g_hash_table_remove (self->configs, config);
}

static gboolean
ipc_git_repository_impl_handle_load_config (IpcGitRepository      *repository,
                                            GDBusMethodInvocation *invocation)
{
  IpcGitRepositoryImpl *self = (IpcGitRepositoryImpl *)repository;
  g_autoptr(IpcGitConfig) config = NULL;
  g_autoptr(GgitConfig) gconfig = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *uuid = NULL;
  g_autofree gchar *obj_path = NULL;
  GDBusConnection *conn;

  g_assert (IPC_IS_GIT_REPOSITORY_IMPL (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));

  if (!(gconfig = ggit_repository_get_config (self->repository, &error)))
    return complete_wrapped_error (invocation, error);

  config = ipc_git_config_impl_new (gconfig);

  conn = g_dbus_method_invocation_get_connection (invocation);
  uuid = g_dbus_generate_guid ();
  obj_path = g_strdup_printf ("/org/gnome/Builder/Config/%s", uuid);
  g_hash_table_insert (self->configs, g_object_ref (config), g_strdup (uuid));

  g_signal_connect_object (config,
                           "closed",
                           G_CALLBACK (ipc_git_repository_impl_config_closed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  if (g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (config), conn, obj_path, &error))
    ipc_git_repository_complete_load_config (repository, invocation, obj_path);
  else
    complete_wrapped_error (invocation, error);

  return TRUE;
}

typedef struct
{
  GFile                      *location;
  GgitSubmoduleUpdateOptions *options;
  GError                     *error;
  guint                       init : 1;
} UpdateSubmodules;

static void
update_submodules_free (UpdateSubmodules *state)
{
  g_clear_object (&state->location);
  g_clear_object (&state->options);
  g_clear_error (&state->error);
  g_slice_free (UpdateSubmodules, state);
}

static gint
ipc_git_repository_impl_submodule_foreach_cb (GgitSubmodule *submodule,
                                              const gchar   *name,
                                              gpointer       user_data)
{
  UpdateSubmodules *state = user_data;

  g_assert (submodule != NULL);
  g_assert (name != NULL);

  if (state->error == NULL)
    ggit_submodule_update (submodule, state->init, state->options, &state->error);

  return GIT_OK;
}

static void
update_submodules_worker (GTask        *task,
                          gpointer      source_object,
                          gpointer      task_data,
                          GCancellable *cancellable)
{
  UpdateSubmodules *state = task_data;
  g_autoptr(GgitRepository) repository = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_TASK (task));
  g_assert (IPC_IS_GIT_REPOSITORY (source_object));
  g_assert (state != NULL);
  g_assert (GGIT_IS_SUBMODULE_UPDATE_OPTIONS (state->options));

  if (!(repository = ggit_repository_open (state->location, &error)))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  if (!ggit_repository_submodule_foreach (repository,
                                          ipc_git_repository_impl_submodule_foreach_cb,
                                          state,
                                          &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  if (state->error != NULL)
    g_task_return_error (task, g_steal_pointer (&state->error));
  else
    g_task_return_boolean (task, TRUE);
}

static void
complete_update_submodules (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  IpcGitRepository *self = (IpcGitRepository *)object;
  g_autoptr(GDBusMethodInvocation) invocation = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IPC_IS_GIT_REPOSITORY (self));
  g_assert (G_IS_TASK (result));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));

  if (!g_task_propagate_boolean (G_TASK (result), &error))
    {
      complete_wrapped_error (invocation, error);
      return;
    }
  else
    {
      ipc_git_repository_complete_update_submodules (self, invocation);
      ipc_git_repository_emit_changed (self);
    }
}

static gboolean
ipc_git_repository_impl_handle_update_submodules (IpcGitRepository      *repository,
                                                  GDBusMethodInvocation *invocation,
                                                  gboolean               init,
                                                  const gchar           *progress_path)
{
  IpcGitRepositoryImpl *self = (IpcGitRepositoryImpl *)repository;
  g_autoptr(GgitRemoteCallbacks) callbacks = NULL;
  g_autoptr(GgitSubmoduleUpdateOptions) update_options = NULL;
  g_autoptr(IpcGitProgress) progress  = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = NULL;
  UpdateSubmodules *state;
  GgitFetchOptions *fetch_options = NULL;
  GDBusConnection *conn = NULL;

  g_assert (IPC_IS_GIT_REPOSITORY (repository));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (progress_path != NULL);

  conn = g_dbus_method_invocation_get_connection (invocation);
  progress = ipc_git_progress_proxy_new_sync (conn,
                                              G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                                              NULL, progress_path, NULL, &error);
  callbacks = ipc_git_remote_callbacks_new (progress, -1);

  update_options = ggit_submodule_update_options_new ();
  ggit_submodule_update_options_set_fetch_options (update_options, fetch_options);

  state = g_slice_new0 (UpdateSubmodules);
  state->location = ggit_repository_get_location (self->repository);
  state->options = g_steal_pointer (&update_options);
  state->init = !!init;
  state->error = NULL;

  task = g_task_new (self, NULL, complete_update_submodules, g_object_ref (invocation));
  g_task_set_source_tag (task, ipc_git_repository_impl_handle_update_submodules);
  g_task_set_task_data (task, state, (GDestroyNotify)update_submodules_free);
  g_task_run_in_thread (task, update_submodules_worker);

  g_clear_pointer (&fetch_options, ggit_fetch_options_free);

  return TRUE;
}

static gboolean
ipc_git_repository_impl_handle_get_remote_url (IpcGitRepository      *repository,
                                               GDBusMethodInvocation *invocation,
                                               const gchar           *remote_name)
{
  IpcGitRepositoryImpl *self = (IpcGitRepositoryImpl *)repository;
  g_autoptr(GgitRemote) remote = NULL;
  g_autoptr(GError) error = NULL;
  const char *url = NULL;

  g_assert (IPC_IS_GIT_REPOSITORY_IMPL (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (remote_name != NULL);

  if ((remote = ggit_repository_lookup_remote (self->repository, remote_name, &error)))
    url = ggit_remote_get_url (remote);

  if (url == NULL)
    url = "";

  ipc_git_repository_complete_get_remote_url (repository, g_steal_pointer (&invocation), url);

  return TRUE;
}

static void
git_repository_iface_init (IpcGitRepositoryIface *iface)
{
  iface->handle_close = ipc_git_repository_impl_handle_close;
  iface->handle_commit = ipc_git_repository_impl_handle_commit;
  iface->handle_create_change_monitor = ipc_git_repository_impl_handle_create_change_monitor;
  iface->handle_blame = ipc_git_repository_impl_handle_blame;
  iface->handle_list_refs_by_kind = ipc_git_repository_impl_handle_list_refs_by_kind;
  iface->handle_list_status = ipc_git_repository_impl_handle_list_status;
  iface->handle_load_config = ipc_git_repository_impl_handle_load_config;
  iface->handle_path_is_ignored = ipc_git_repository_impl_handle_path_is_ignored;
  iface->handle_push = ipc_git_repository_impl_handle_push;
  iface->handle_stage_file = ipc_git_repository_impl_handle_stage_file;
  iface->handle_switch_branch = ipc_git_repository_impl_handle_switch_branch;
  iface->handle_update_submodules = ipc_git_repository_impl_handle_update_submodules;
  iface->handle_get_remote_url = ipc_git_repository_impl_handle_get_remote_url;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (IpcGitRepositoryImpl, ipc_git_repository_impl, IPC_TYPE_GIT_REPOSITORY_SKELETON,
                               G_IMPLEMENT_INTERFACE (IPC_TYPE_GIT_REPOSITORY, git_repository_iface_init))

static void
ipc_git_repository_impl_finalize (GObject *object)
{
  IpcGitRepositoryImpl *self = (IpcGitRepositoryImpl *)object;

  g_clear_object (&self->monitor);
  g_clear_pointer (&self->change_monitors, g_hash_table_unref);
  g_clear_pointer (&self->blamers, g_hash_table_unref);
  g_clear_pointer (&self->configs, g_hash_table_unref);
  g_clear_object (&self->repository);

  G_OBJECT_CLASS (ipc_git_repository_impl_parent_class)->finalize (object);
}

static void
ipc_git_repository_impl_class_init (IpcGitRepositoryImplClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ipc_git_repository_impl_finalize;
}

static void
ipc_git_repository_impl_init (IpcGitRepositoryImpl *self)
{
  self->configs = g_hash_table_new_full (NULL, NULL, g_object_unref, g_free);
  self->change_monitors = g_hash_table_new_full (NULL, NULL, g_object_unref, g_free);
  self->blamers = g_hash_table_new_full (NULL, NULL, g_object_unref, g_free);
}

IpcGitRepository *
ipc_git_repository_impl_open (GFile   *location,
                              GError **error)
{
  g_autoptr(GgitRepository) repository = NULL;
  g_autoptr(GFile) workdir = NULL;
  g_autoptr(GFile) gitdir = NULL;
  g_autofree gchar *branch = NULL;
  IpcGitRepositoryImpl *ret;

  g_return_val_if_fail (G_IS_FILE (location), NULL);

  /* If @location is a regular file, we might have a git-worktree link */
  if (g_file_query_file_type (location, 0, NULL) == G_FILE_TYPE_REGULAR)
    {
      g_autofree gchar *contents = NULL;
      gsize len;

      if (g_file_load_contents (location, NULL, &contents, &len, NULL, NULL))
        {
          g_auto(GStrv) lines = g_strsplit (contents, "\n", 0);

          for (gsize i = 0; lines[i] != NULL; i++)
            {
              gchar *line = lines[i];

              if (g_str_has_prefix (line, "gitdir: "))
                {
                  g_autoptr(GFile) location_parent = g_file_get_parent (location);
                  const gchar *path = line + strlen ("gitdir: ");
                  const gchar *sep;

                  g_clear_object (&location);

                  if (g_path_is_absolute (path))
                    location = gitdir = g_file_new_for_path (path);
                  else
                    location = gitdir = g_file_resolve_relative_path (location_parent, path);

                  /*
                   * Worktrees only have a single branch, and it is the name
                   * of the suffix of .git/worktrees/<name>
                   */
                  if ((sep = strrchr (line, G_DIR_SEPARATOR)))
                    branch = g_strdup (sep + 1);

                  break;
                }
            }
        }
    }

  if (!(repository = ggit_repository_open (location, error)))
    return NULL;

  if (branch == NULL)
    {
      g_autoptr(GgitRef) ref = NULL;

      if ((ref = ggit_repository_get_head (repository, NULL)))
        branch = g_strdup (ggit_ref_get_shorthand (ref));

      if (branch == NULL)
        branch = g_strdup ("main");
    }

  workdir = ggit_repository_get_workdir (repository);

  ret = g_object_new (IPC_TYPE_GIT_REPOSITORY_IMPL,
                      "branch", branch,
                      "location", g_file_peek_path (location),
                      "workdir", g_file_peek_path (workdir),
                      NULL);
  ret->repository = g_steal_pointer (&repository);
  ret->monitor = ipc_git_index_monitor_new (location);

  g_signal_connect_object (ret->monitor,
                           "changed",
                           G_CALLBACK (ipc_git_repository_impl_monitor_changed_cb),
                           ret,
                           G_CONNECT_SWAPPED);

  return IPC_GIT_REPOSITORY (g_steal_pointer (&ret));
}
