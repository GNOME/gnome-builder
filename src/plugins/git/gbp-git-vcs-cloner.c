/* gbp-git-vcs-cloner.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-git-vcs-cloner"

#include "config.h"

#include <dazzle.h>
#include <glib/gi18n.h>
#include <libide-threading.h>

#include "gbp-git-remote-callbacks.h"
#include "gbp-git-vcs-cloner.h"

struct _GbpGitVcsCloner
{
  GObject parent_instance;
};

typedef struct
{
  IdeNotification *notif;
  IdeVcsUri       *uri;
  gchar           *branch;
  GFile           *location;
  GFile           *project_file;
  gchar           *author_name;
  gchar           *author_email;
} CloneRequest;

static void vcs_cloner_iface_init (IdeVcsClonerInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GbpGitVcsCloner, gbp_git_vcs_cloner, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_VCS_CLONER,
                                                vcs_cloner_iface_init))

static void
clone_request_free (gpointer data)
{
  CloneRequest *req = data;

  if (req != NULL)
    {
      g_clear_pointer (&req->uri, ide_vcs_uri_unref);
      g_clear_pointer (&req->branch, g_free);
      g_clear_object (&req->notif);
      g_clear_object (&req->location);
      g_clear_object (&req->project_file);
      g_slice_free (CloneRequest, req);
    }
}

static CloneRequest *
clone_request_new (IdeVcsUri       *uri,
                   const gchar     *branch,
                   GFile           *location,
                   IdeNotification *notif)
{
  CloneRequest *req;

  g_assert (uri);
  g_assert (location);
  g_assert (notif);

  req = g_slice_new0 (CloneRequest);
  req->uri = ide_vcs_uri_ref (uri);
  req->branch = g_strdup (branch);
  req->location = g_object_ref (location);
  req->project_file = NULL;
  req->notif = g_object_ref (notif);

  return req;
}

static void
gbp_git_vcs_cloner_class_init (GbpGitVcsClonerClass *klass)
{
}

static void
gbp_git_vcs_cloner_init (GbpGitVcsCloner *self)
{
}

static gchar *
gbp_git_vcs_cloner_get_title (IdeVcsCloner *cloner)
{
  return g_strdup ("Git");
}

static gboolean
gbp_git_vcs_cloner_validate_uri (IdeVcsCloner  *cloner,
                                 const gchar   *uri,
                                 gchar        **errmsg)
{
  g_autoptr(IdeVcsUri) vcs_uri = NULL;

  g_assert (IDE_IS_VCS_CLONER (cloner));
  g_assert (uri != NULL);

  vcs_uri = ide_vcs_uri_new (uri);

  if (vcs_uri != NULL)
    {
      const gchar *scheme = ide_vcs_uri_get_scheme (vcs_uri);
      const gchar *path = ide_vcs_uri_get_path (vcs_uri);

      if (ide_str_equal0 (scheme, "file"))
        {
          g_autoptr(GFile) file = g_file_new_for_path (path);

          if (!g_file_query_exists (file, NULL))
            {
              if (errmsg != NULL)
                *errmsg = g_strdup_printf ("A resository could not be found at “%s”.", path);
              return FALSE;
            }

          return TRUE;
        }

      /* We can only support certain schemes */
      if (ide_str_equal0 (scheme, "http") ||
          ide_str_equal0 (scheme, "https") ||
          ide_str_equal0 (scheme, "git") ||
          ide_str_equal0 (scheme, "rsync") ||
          ide_str_equal0 (scheme, "ssh"))
        return TRUE;

      if (errmsg != NULL)
        *errmsg = g_strdup_printf (_("The protocol “%s” is not supported."), scheme);
    }

  return FALSE;
}

static void
gbp_git_vcs_cloner_worker (IdeTask      *task,
                           gpointer      source_object,
                           gpointer      task_data,
                           GCancellable *cancellable)
{
  GbpGitVcsCloner *self = source_object;
  g_autoptr(GgitConfig) config = NULL;
  g_autoptr(GFile) config_file = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *uristr = NULL;
  GgitRepository *repository;
  GgitCloneOptions *clone_options;
  GgitFetchOptions *fetch_options;
  GgitRemoteCallbacks *callbacks;
  CloneRequest *req = task_data;

  g_assert (IDE_IS_TASK (task));
  g_assert (GBP_IS_GIT_VCS_CLONER (self));
  g_assert (req != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  callbacks = gbp_git_remote_callbacks_new (req->notif);

  g_signal_connect_object (cancellable,
                           "cancelled",
                           G_CALLBACK (gbp_git_remote_callbacks_cancel),
                           callbacks,
                           G_CONNECT_SWAPPED);

  fetch_options = ggit_fetch_options_new ();
  ggit_fetch_options_set_remote_callbacks (fetch_options, callbacks);

  clone_options = ggit_clone_options_new ();
  ggit_clone_options_set_is_bare (clone_options, FALSE);
  ggit_clone_options_set_checkout_branch (clone_options, req->branch);
  ggit_clone_options_set_fetch_options (clone_options, fetch_options);
  g_clear_pointer (&fetch_options, ggit_fetch_options_free);

  uristr = ide_vcs_uri_to_string (req->uri);

  repository = ggit_repository_clone (uristr, req->location, clone_options, &error);

  g_clear_object (&callbacks);
  g_clear_object (&clone_options);

  if (repository == NULL)
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  if (ide_task_return_error_if_cancelled (task))
    return;

  config_file = g_file_get_child (req->location, ".git/config");

  if ((config = ggit_config_new_from_file (config_file, &error)))
    {
      if (req->author_name)
        ggit_config_set_string (config, "user.name", req->author_name, &error);
      if (req->author_email)
        ggit_config_set_string (config, "user.email", req->author_email, &error);
    }

  req->project_file = ggit_repository_get_workdir (repository);

  ide_task_return_boolean (task, TRUE);

  g_clear_object (&repository);
}

static void
gbp_git_vcs_cloner_clone_async (IdeVcsCloner        *cloner,
                                const gchar         *uri,
                                const gchar         *destination,
                                GVariantDict        *options,
                                GCancellable        *cancellable,
                                IdeNotification    **notif,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  GbpGitVcsCloner *self = (GbpGitVcsCloner *)cloner;
  g_autoptr(IdeNotification) notif_local = NULL;
  g_autoptr(IdeVcsUri) vcs_uri = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GFile) location = NULL;
  g_autofree gchar *uristr = NULL;
  CloneRequest *req;
  const gchar *branch;

  g_assert (GBP_IS_GIT_VCS_CLONER (cloner));
  g_assert (uri != NULL);
  g_assert (destination != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_git_vcs_cloner_clone_async);

  notif_local = ide_notification_new ();
  if (notif != NULL)
    *notif = g_object_ref (notif_local);

  if (!g_variant_dict_lookup (options, "branch", "&s", &branch))
    branch = "master";

  /*
   * ggit_repository_clone() will block and we don't have a good way to
   * cancel it. So we need to return immediately (even though the clone
   * will continue in the background for now).
   *
   * FIXME: Find Ggit API to cancel clone. We might need access to the
   *    GgitRemote so we can ggit_remote_disconnect().
   */
  ide_task_set_return_on_cancel (task, TRUE);

  uristr = g_strstrip (g_strdup (uri));
  location = g_file_new_for_path (destination);

  vcs_uri = ide_vcs_uri_new (uristr);

  if (vcs_uri == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVAL,
                                 _("A valid Git URL is required"));
      return;
    }

  if (g_strcmp0 ("ssh", ide_vcs_uri_get_scheme (vcs_uri)) == 0)
    {
      if (ide_vcs_uri_get_user (vcs_uri) == NULL)
        ide_vcs_uri_set_user (vcs_uri, g_get_user_name ());
    }

  req = clone_request_new (vcs_uri, branch, location, notif_local);

  g_variant_dict_lookup (options, "author-name", "s", &req->author_name);
  g_variant_dict_lookup (options, "author-email", "s", &req->author_email);

  ide_task_set_task_data (task, req, clone_request_free);
  ide_task_run_in_thread (task, gbp_git_vcs_cloner_worker);
}

static gboolean
gbp_git_vcs_cloner_clone_finish (IdeVcsCloner  *cloner,
                                 GAsyncResult  *result,
                                 GError       **error)
{
  g_assert (GBP_IS_GIT_VCS_CLONER (cloner));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
vcs_cloner_iface_init (IdeVcsClonerInterface *iface)
{
  iface->get_title = gbp_git_vcs_cloner_get_title;
  iface->validate_uri = gbp_git_vcs_cloner_validate_uri;
  iface->clone_async = gbp_git_vcs_cloner_clone_async;
  iface->clone_finish = gbp_git_vcs_cloner_clone_finish;
}
