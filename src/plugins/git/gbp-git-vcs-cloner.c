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
#include <libide-threading.h>

#include "gbp-git-client.h"
#include "gbp-git-remote-callbacks.h"
#include "gbp-git-vcs-cloner.h"

struct _GbpGitVcsCloner
{
  IdeObject parent_instance;
};

static void vcs_cloner_iface_init (IdeVcsClonerInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GbpGitVcsCloner, gbp_git_vcs_cloner, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_VCS_CLONER, vcs_cloner_iface_init))

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
gbp_git_vcs_cloner_clone_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  GbpGitClient *client = (GbpGitClient *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (GBP_IS_GIT_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!gbp_git_client_clone_url_finish (client, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);
}

static void
gbp_git_vcs_cloner_clone_async (IdeVcsCloner        *cloner,
                                const gchar         *uri,
                                const gchar         *destination,
                                GVariantDict        *options,
                                IdeNotification     *notif,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  GbpGitVcsCloner *self = (GbpGitVcsCloner *)cloner;
  g_autoptr(IdeVcsUri) vcs_uri = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GFile) location = NULL;
  g_autofree gchar *uristr = NULL;
  g_autofree gchar *real_uri = NULL;
  GbpGitClient *client;
  IdeContext *context;
  const gchar *branch;

  g_assert (GBP_IS_GIT_VCS_CLONER (cloner));
  g_assert (uri != NULL);
  g_assert (destination != NULL);
  g_assert (!notif || IDE_IS_NOTIFICATION (notif));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_git_vcs_cloner_clone_async);

  context = ide_object_get_context (IDE_OBJECT (self));
  client = gbp_git_client_from_context (context);

  if (!g_variant_dict_lookup (options, "branch", "&s", &branch))
    branch = "master";

  location = g_file_new_for_path (destination);

  uristr = g_strstrip (g_strdup (uri));
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

  real_uri = ide_vcs_uri_to_string (vcs_uri);

  gbp_git_client_clone_url_async (client,
                                  real_uri,
                                  location,
                                  branch,
                                  notif,
                                  cancellable,
                                  gbp_git_vcs_cloner_clone_cb,
                                  g_steal_pointer (&task));

#if 0
  /* TODO: Write config file */
  g_variant_dict_lookup (options, "author-name", "s", &req->author_name);
  g_variant_dict_lookup (options, "author-email", "s", &req->author_email);
#endif
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
