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

#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <libide-threading.h>

#include "daemon/ipc-git-service.h"
#include "daemon/ipc-git-types.h"

#include "gbp-git-branch.h"
#include "gbp-git-client.h"
#include "gbp-git-progress.h"
#include "gbp-git-vcs-cloner.h"

struct _GbpGitVcsCloner
{
  IdeObject parent_instance;
  int       clone_pty;
};

typedef struct
{
  IpcGitProgress  *progress;
  IdeNotification *notif;
  IdeVcsUri       *uri;
  gchar           *branch;
  GFile           *location;
  GFile           *project_file;
} CloneRequest;

static void vcs_cloner_iface_init (IdeVcsClonerInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpGitVcsCloner, gbp_git_vcs_cloner, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_VCS_CLONER, vcs_cloner_iface_init))

static const char *
guess_ssh_user_from_host (const char *host)
{
  if (host != NULL)
    {
      /* TODO: check .ssh/config for User mappings */
      if (strstr (host, "gitlab.") != NULL ||
          strstr (host, "github.") != NULL)
        return "git";
    }

  return g_get_user_name ();
}

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
      g_clear_object (&req->progress);
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
gbp_git_vcs_cloner_destroy (IdeObject *object)
{
  GbpGitVcsCloner *self = (GbpGitVcsCloner *)object;

  g_clear_fd (&self->clone_pty, NULL);

  IDE_OBJECT_CLASS (gbp_git_vcs_cloner_parent_class)->destroy (object);
}

static void
gbp_git_vcs_cloner_class_init (GbpGitVcsClonerClass *klass)
{
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  i_object_class->destroy = gbp_git_vcs_cloner_destroy;
}

static void
gbp_git_vcs_cloner_init (GbpGitVcsCloner *self)
{
  self->clone_pty = -1;
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
                *errmsg = g_strdup_printf (_("A repository could not be found at “%s”."), path);
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
gbp_git_vcs_cloner_clone_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  IpcGitService *service = (IpcGitService *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *git_location = NULL;

  g_assert (IPC_IS_GIT_SERVICE (service));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ipc_git_service_call_clone_finish (service, &git_location, NULL, result, &error))
    {
      g_print ("Failed to clone: %s\n", error->message);
      g_dbus_error_strip_remote_error (error);
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  ide_task_return_boolean (task, TRUE);
}

static void
gbp_git_vcs_cloner_clone_async (IdeVcsCloner        *cloner,
                                const gchar         *uri,
                                const gchar         *destination,
                                GVariant            *options,
                                IdeNotification     *notif,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  GbpGitVcsCloner *self = (GbpGitVcsCloner *)cloner;
  g_autoptr(IdeNotification) notif_local = NULL;
  g_autoptr(IdeVcsUri) vcs_uri = NULL;
  g_autoptr(IpcGitService) service = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GFile) location = NULL;
  g_autoptr(GUnixFDList) fd_list = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *uristr = NULL;
  GDBusConnection *connection;
  GbpGitClient *client = NULL;
  CloneRequest *req;
  GVariantDict dict;
  const gchar *branch;
  IdeContext *context;
  int handle;

  g_assert (GBP_IS_GIT_VCS_CLONER (cloner));
  g_assert (uri != NULL);
  g_assert (destination != NULL);
  g_assert (!notif || IDE_IS_NOTIFICATION (notif));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  /* Get our client to communicate with the daemon */
  context = ide_object_get_context (IDE_OBJECT (self));
  client = gbp_git_client_from_context (context);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_git_vcs_cloner_clone_async);

  /* Ensure we always have a notification to work with */
  if (notif == NULL)
    {
      notif_local = ide_notification_new ();
      notif = notif_local;
    }

  ide_notification_set_title (notif, _("Cloning repository"));

  /* Extract branch, leave other options to pass-through */
  g_variant_dict_init (&dict, options);
  if (!g_variant_dict_lookup (&dict, "branch", "&s", &branch))
    branch = "";
  g_variant_dict_remove (&dict, "branch");

  if (branch != NULL && g_str_has_prefix (branch, "refs/heads/"))
    branch += strlen ("refs/heads/");

  /* Make sure we have a real URI to connect to */
  uristr = g_strstrip (g_strdup (uri));
  location = g_file_new_for_path (destination);
  if (!(vcs_uri = ide_vcs_uri_new (uristr)))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVAL,
                                 _("A valid Git URL is required"));
      return;
    }

  /* Always set a username if the transport is SSH */
  if (g_strcmp0 ("ssh", ide_vcs_uri_get_scheme (vcs_uri)) == 0)
    {
      if (ide_vcs_uri_get_user (vcs_uri) == NULL)
        {
          ide_vcs_uri_set_user (vcs_uri, g_get_user_name ());
          g_free (uristr);
          uristr = ide_vcs_uri_to_string (vcs_uri);
        }
    }

  fd_list = g_unix_fd_list_new ();

  /* Create state for the task */
  req = clone_request_new (vcs_uri, branch, location, notif);
  ide_task_set_task_data (task, req, clone_request_free);

  if (!(service = gbp_git_client_get_service (client, cancellable, &error)) ||
      !(connection = g_dbus_proxy_get_connection (G_DBUS_PROXY (service))) ||
      !(req->progress = gbp_git_progress_new (connection, notif, cancellable, &error)) ||
      -1 == (handle = g_unix_fd_list_append (fd_list, self->clone_pty, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      g_variant_dict_clear (&dict);
      return;
    }

  ipc_git_service_call_clone (service,
                              uristr,
                              g_file_peek_path (req->location),
                              req->branch ?: "",
                              g_variant_dict_end (&dict),
                              g_dbus_interface_skeleton_get_object_path (G_DBUS_INTERFACE_SKELETON (req->progress)),
                              g_variant_new_handle (handle),
                              fd_list,
                              cancellable,
                              gbp_git_vcs_cloner_clone_cb,
                              g_steal_pointer (&task));
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

static gboolean
should_ignore (const char *name)
{
  if (name == NULL)
    return TRUE;

  if (g_str_has_prefix (name, "refs/merge-requests/") ||
      g_str_has_prefix (name, "refs/tags/"))
    return TRUE;

  return FALSE;
}

enum {
  CLASS_HEAD,
  CLASS_MAIN,
  CLASS_MASTER,
  CLASS_FEATURE,
  CLASS_GNOME,
  CLASS_OTHER,
  CLASS_WIP,
};

static int
classify_branch_name (const char *branch)
{
  switch (branch[0])
    {
    case 'H':
      if (strcmp (branch, "HEAD") == 0)
        return CLASS_HEAD;
      break;

    case 'm':
      if (strcmp (branch, "main") == 0)
        return CLASS_MAIN;
      else if (strcmp (branch, "master") == 0)
        return CLASS_MASTER;
      break;

    case 'w':
      if (g_str_has_prefix (branch, "wip/"))
        return CLASS_WIP;
      break;

    case 'f':
      if (g_str_has_prefix (branch, "feature/"))
        return CLASS_FEATURE;
      break;

    case 'g':
      if (g_str_has_prefix (branch, "gnome-"))
        return CLASS_GNOME;
      break;

    default:
      break;
    }

  return CLASS_OTHER;
}

static inline const char *
get_name (const char *ref)
{
  if (ref[0] == 'r' && g_str_has_prefix (ref, "refs/heads/"))
    return ref + strlen ("refs/heads/");
  return ref;
}

static int
strptrcmp (const char * const *ptra,
           const char * const *ptrb)
{
  const char *name_a = get_name (*ptra);
  const char *name_b = get_name (*ptrb);
  int class_a = classify_branch_name (name_a);
  int class_b = classify_branch_name (name_b);

  if (class_a < class_b)
    return -1;
  else if (class_a > class_b)
    return 1;
  else
    return strcmp (name_a, name_b);
}

static void
gbp_git_vcs_cloner_list_remote_refs_by_kind_cb (GObject      *object,
                                                GAsyncResult *result,
                                                gpointer      user_data)
{
  IpcGitService *service = (IpcGitService *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GListStore) store = NULL;
  g_auto(GStrv) refs = NULL;

  IDE_ENTRY;

  g_assert (IPC_IS_GIT_SERVICE (service));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ipc_git_service_call_list_remote_refs_by_kind_finish (service, &refs, result, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  g_sort_array (refs, g_strv_length (refs), sizeof (char *),
                (GCompareDataFunc)strptrcmp, NULL);
  store = g_list_store_new (GBP_TYPE_GIT_BRANCH);

  for (guint i = 0; refs[i]; i++)
    {
      g_autoptr(GbpGitBranch) branch = NULL;

      if (should_ignore (refs[i]))
        continue;

      branch = gbp_git_branch_new (refs[i]);
      g_list_store_append (store, branch);
    }

  ide_task_return_pointer (task, g_steal_pointer (&store), g_object_unref);

  IDE_EXIT;
}

static void
gbp_git_vcs_cloner_list_branches_async (IdeVcsCloner        *cloner,
                                        IdeVcsUri           *uri,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  GbpGitVcsCloner *self = (GbpGitVcsCloner *)cloner;
  g_autoptr(IpcGitService) service = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *uri_str = NULL;
  GbpGitClient *client;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (GBP_IS_GIT_VCS_CLONER (cloner));
  g_assert (uri != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_git_vcs_cloner_list_branches_async);

  if (!(context = ide_object_get_context (IDE_OBJECT (self))) ||
      !(client = gbp_git_client_from_context (context)))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "Failed to locate git client object within context");
      IDE_EXIT;
    }

  /* We can make this async if/when necessary. It just spawns the process and
   * sets up a GDBusProxy to the subprocess. Not ideal, but we do it elsewhere
   * too when accessing the service.
   */
  if (!(service = gbp_git_client_get_service (client, cancellable, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  /* Always set a username if the transport is SSH */
  if (g_strcmp0 ("ssh", ide_vcs_uri_get_scheme (uri)) == 0)
    {
      if (ide_vcs_uri_get_user (uri) == NULL)
        {
          const char *host = ide_vcs_uri_get_host (uri);
          ide_vcs_uri_set_user (uri, guess_ssh_user_from_host (host));
        }
    }

  uri_str = ide_vcs_uri_to_string (uri);

  ipc_git_service_call_list_remote_refs_by_kind (service,
                                                 uri_str,
                                                 IPC_GIT_REF_BRANCH,
                                                 cancellable,
                                                 gbp_git_vcs_cloner_list_remote_refs_by_kind_cb,
                                                 g_steal_pointer (&task));

  IDE_EXIT;
}

static GListModel *
gbp_git_vcs_cloner_list_branches_finish (IdeVcsCloner  *cloner,
                                         GAsyncResult  *result,
                                         GError       **error)
{
  GListModel *ret;

  IDE_ENTRY;

  g_assert (GBP_IS_GIT_VCS_CLONER (cloner));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);
  g_assert (!ret || G_IS_LIST_MODEL (ret));

  IDE_RETURN (ret);
}

static char *
gbp_git_vcs_cloner_get_directory_name (IdeVcsCloner *cloner,
                                       IdeVcsUri    *uri)
{
  g_assert (GBP_IS_GIT_VCS_CLONER (cloner));
  g_assert (uri != NULL);

  return ide_vcs_uri_get_clone_name (uri);
}

static void
gbp_git_vcs_cloner_set_pty_fd (IdeVcsCloner *cloner,
                               int           pty_fd)
{
  GbpGitVcsCloner *self = (GbpGitVcsCloner *)cloner;

  g_assert (GBP_IS_GIT_VCS_CLONER (self));

  g_clear_fd (&self->clone_pty, NULL);
  self->clone_pty = dup (pty_fd);
}

static void
vcs_cloner_iface_init (IdeVcsClonerInterface *iface)
{
  iface->get_title = gbp_git_vcs_cloner_get_title;
  iface->validate_uri = gbp_git_vcs_cloner_validate_uri;
  iface->clone_async = gbp_git_vcs_cloner_clone_async;
  iface->clone_finish = gbp_git_vcs_cloner_clone_finish;
  iface->list_branches_async = gbp_git_vcs_cloner_list_branches_async;
  iface->list_branches_finish = gbp_git_vcs_cloner_list_branches_finish;
  iface->get_directory_name = gbp_git_vcs_cloner_get_directory_name;
  iface->set_pty_fd = gbp_git_vcs_cloner_set_pty_fd;
}
