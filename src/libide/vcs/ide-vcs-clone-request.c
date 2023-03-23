/* ide-vcs-clone-request.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-vcs-clone-request"

#include "config.h"

#include <libide-plugins.h>
#include <libide-threading.h>

#include "ide-vcs-cloner.h"
#include "ide-vcs-clone-request.h"
#include "ide-vcs-uri.h"

struct _IdeVcsCloneRequest
{
  IdeObject     parent_instance;

  GListModel   *branch_model;
  GCancellable *cancellable;
  GFile        *directory;

  IdeVcsCloner *cloner;

  char         *author_email;
  char         *author_name;
  char         *branch_name;
  char         *module_name;
  char         *uri;

  guint         fetching_branches;
};

enum {
  PROP_0,
  PROP_AUTHOR_EMAIL,
  PROP_AUTHOR_NAME,
  PROP_CAN_SELECT_BRANCH,
  PROP_BRANCH_MODEL,
  PROP_BRANCH_MODEL_BUSY,
  PROP_BRANCH_NAME,
  PROP_DIRECTORY,
  PROP_MODULE_NAME,
  PROP_URI,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeVcsCloneRequest, ide_vcs_clone_request, IDE_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static gboolean
ide_vcs_clone_request_get_can_select_branch (IdeVcsCloneRequest *self)
{
  g_assert (IDE_IS_VCS_CLONE_REQUEST (self));

  return !ide_str_empty0 (self->uri) &&
         self->cloner != NULL &&
         IDE_VCS_CLONER_GET_IFACE (self->cloner)->list_branches_async != NULL;
}

static void
ide_vcs_clone_request_set_cloner (IdeVcsCloneRequest *self,
                                  IdeVcsCloner       *cloner)
{
  IDE_ENTRY;

  g_assert (IDE_IS_VCS_CLONE_REQUEST (self));
  g_assert (!cloner || IDE_IS_VCS_CLONER (cloner));

  g_debug ("Setting cloner to %s",
           cloner ? G_OBJECT_TYPE_NAME (cloner) : "NULL");

  if (g_set_object (&self->cloner, cloner))
    {
      g_clear_object (&self->branch_model);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BRANCH_MODEL]);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CAN_SELECT_BRANCH]);
    }

  IDE_EXIT;
}

static void
ide_vcs_clone_request_destroy (IdeObject *object)
{
  IdeVcsCloneRequest *self = (IdeVcsCloneRequest *)object;

  g_clear_object (&self->branch_model);
  g_clear_object (&self->cancellable);
  g_clear_object (&self->directory);

  ide_clear_and_destroy_object (&self->cloner);

  ide_clear_string (&self->author_email);
  ide_clear_string (&self->author_name);
  ide_clear_string (&self->branch_name);
  ide_clear_string (&self->module_name);
  ide_clear_string (&self->uri);

  IDE_OBJECT_CLASS (ide_vcs_clone_request_parent_class)->destroy (object);
}

static void
ide_vcs_clone_request_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  IdeVcsCloneRequest *self = IDE_VCS_CLONE_REQUEST (object);

  switch (prop_id)
    {
    case PROP_AUTHOR_EMAIL:
      g_value_set_string (value, ide_vcs_clone_request_get_author_email (self));
      break;

    case PROP_AUTHOR_NAME:
      g_value_set_string (value, ide_vcs_clone_request_get_author_name (self));
      break;

    case PROP_BRANCH_MODEL:
      g_value_set_object (value, ide_vcs_clone_request_get_branch_model (self));
      break;

    case PROP_BRANCH_MODEL_BUSY:
      g_value_set_boolean (value, self->fetching_branches);
      break;

    case PROP_BRANCH_NAME:
      g_value_set_string (value, ide_vcs_clone_request_get_branch_name (self));
      break;

    case PROP_CAN_SELECT_BRANCH:
      g_value_set_boolean (value, ide_vcs_clone_request_get_can_select_branch (self));
      break;

    case PROP_DIRECTORY:
      g_value_set_object (value, ide_vcs_clone_request_get_directory (self));
      break;

    case PROP_MODULE_NAME:
      g_value_set_string (value, ide_vcs_clone_request_get_module_name (self));
      break;

    case PROP_URI:
      g_value_set_string (value, ide_vcs_clone_request_get_uri (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_vcs_clone_request_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  IdeVcsCloneRequest *self = IDE_VCS_CLONE_REQUEST (object);

  switch (prop_id)
    {
    case PROP_AUTHOR_EMAIL:
      ide_vcs_clone_request_set_author_email (self, g_value_get_string (value));
      break;

    case PROP_AUTHOR_NAME:
      ide_vcs_clone_request_set_author_name (self, g_value_get_string (value));
      break;

    case PROP_BRANCH_NAME:
      ide_vcs_clone_request_set_branch_name (self, g_value_get_string (value));
      break;

    case PROP_DIRECTORY:
      ide_vcs_clone_request_set_directory (self, g_value_get_object (value));
      break;

    case PROP_MODULE_NAME:
      ide_vcs_clone_request_set_module_name (self, g_value_get_string (value));
      break;

    case PROP_URI:
      ide_vcs_clone_request_set_uri (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_vcs_clone_request_class_init (IdeVcsCloneRequestClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  object_class->get_property = ide_vcs_clone_request_get_property;
  object_class->set_property = ide_vcs_clone_request_set_property;

  i_object_class->destroy = ide_vcs_clone_request_destroy;

  properties [PROP_AUTHOR_EMAIL] =
    g_param_spec_string ("author-email", NULL, NULL, NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_AUTHOR_NAME] =
    g_param_spec_string ("author-name", NULL, NULL, NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeVcsCloneRequest:branch-model:
   *
   * The "branch-model" contains a #GListModel of #IdeVcsBranch that
   * represents the names of branches that may be available on the peer.
   *
   * This model is not automatically updated until
   * ide_vcs_clone_request_populate_branches() is called. This is to make it
   * clear to the user that it is being done in response to an action (such
   * as showing a popover) since user/password information may be requested
   * from the user.
   *
   * The UI may use this to show a popover/selection of branches for the
   * user to select.
   */
  properties [PROP_BRANCH_MODEL] =
    g_param_spec_object ("branch-model", NULL, NULL, G_TYPE_LIST_MODEL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_BRANCH_MODEL_BUSY] =
    g_param_spec_boolean ("branch-model-busy", NULL, NULL, FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_BRANCH_NAME] =
    g_param_spec_string ("branch-name", NULL, NULL, NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_CAN_SELECT_BRANCH] =
    g_param_spec_boolean ("can-select-branch", NULL, NULL, FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_DIRECTORY] =
    g_param_spec_object ("directory", NULL, NULL, G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_MODULE_NAME] =
    g_param_spec_string ("module-name", NULL, NULL, NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_URI] =
    g_param_spec_string ("uri", NULL, NULL, NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_vcs_clone_request_init (IdeVcsCloneRequest *self)
{
  /* Set to default projects directory */
  ide_vcs_clone_request_set_directory (self, NULL);
}

const char *
ide_vcs_clone_request_get_author_email (IdeVcsCloneRequest *self)
{
  g_return_val_if_fail (IDE_IS_VCS_CLONE_REQUEST (self), NULL);

  return self->author_email ? self->author_email : "";
}

const char *
ide_vcs_clone_request_get_author_name (IdeVcsCloneRequest *self)
{
  const char *ret;

  g_return_val_if_fail (IDE_IS_VCS_CLONE_REQUEST (self), NULL);

  ret = self->author_name;

  if (ide_str_empty0 (ret))
    ret = g_get_real_name ();

  if (ide_str_empty0 (ret))
    return "";

  return ret;
}

const char *
ide_vcs_clone_request_get_branch_name (IdeVcsCloneRequest *self)
{
  g_return_val_if_fail (IDE_IS_VCS_CLONE_REQUEST (self), NULL);

  return self->branch_name ? self->branch_name : "";
}

const char *
ide_vcs_clone_request_get_module_name (IdeVcsCloneRequest *self)
{
  g_return_val_if_fail (IDE_IS_VCS_CLONE_REQUEST (self), NULL);

  return self->module_name;
}

/**
 * ide_vcs_clone_request_get_directory:
 * @self: a #IdeVcsCloneRequest
 *
 * Gets the directory to use which will contain the new subdirectory
 * created when checking out the project.
 *
 * Returns: (transfer none) (not nullable): a #GFile
 */
GFile *
ide_vcs_clone_request_get_directory (IdeVcsCloneRequest *self)
{
  g_return_val_if_fail (IDE_IS_VCS_CLONE_REQUEST (self), NULL);
  g_return_val_if_fail (G_IS_FILE (self->directory), NULL);

  return self->directory;
}

const char *
ide_vcs_clone_request_get_uri (IdeVcsCloneRequest *self)
{
  g_return_val_if_fail (IDE_IS_VCS_CLONE_REQUEST (self), NULL);

  return self->uri ? self->uri : "";
}

void
ide_vcs_clone_request_set_author_email (IdeVcsCloneRequest *self,
                                        const char         *author_email)
{
  g_return_if_fail (IDE_IS_VCS_CLONE_REQUEST (self));

  if (g_set_str (&self->author_email, author_email))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_AUTHOR_EMAIL]);
}

void
ide_vcs_clone_request_set_author_name (IdeVcsCloneRequest *self,
                                       const char         *author_name)
{
  g_return_if_fail (IDE_IS_VCS_CLONE_REQUEST (self));

  if (g_set_str (&self->author_name, author_name))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_AUTHOR_NAME]);
}

void
ide_vcs_clone_request_set_branch_name (IdeVcsCloneRequest *self,
                                       const char         *branch_name)
{
  g_return_if_fail (IDE_IS_VCS_CLONE_REQUEST (self));

  if (g_set_str (&self->branch_name, branch_name))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BRANCH_NAME]);
}

void
ide_vcs_clone_request_set_module_name (IdeVcsCloneRequest *self,
                                       const char         *module_name)
{
  g_return_if_fail (IDE_IS_VCS_CLONE_REQUEST (self));

  if (g_set_str (&self->module_name, module_name))
    {
      g_autoptr(GObject) exten = NULL;

      if (module_name != NULL)
        {
          PeasEngine *engine = peas_engine_get_default ();
          PeasPluginInfo *plugin_info = peas_engine_get_plugin_info (engine, module_name);

          if (plugin_info != NULL &&
              peas_engine_provides_extension (engine, plugin_info, IDE_TYPE_VCS_CLONER))
            exten = peas_engine_create_extension (engine,
                                                  plugin_info,
                                                  IDE_TYPE_VCS_CLONER,
                                                  "parent", IDE_OBJECT (self),
                                                  NULL);
        }

      ide_vcs_clone_request_set_cloner (self, IDE_VCS_CLONER (exten));
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MODULE_NAME]);
    }
}

void
ide_vcs_clone_request_set_directory (IdeVcsCloneRequest *self,
                                     GFile              *directory)
{
  g_autoptr(GFile) default_directory = NULL;

  g_return_if_fail (IDE_IS_VCS_CLONE_REQUEST (self));
  g_return_if_fail (!directory || G_IS_FILE (directory));

  if (directory == NULL)
    {
      default_directory = g_file_new_for_path (ide_get_projects_dir ());
      directory = default_directory;
    }

  g_assert (G_IS_FILE (directory));

  if (g_set_object (&self->directory, directory))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DIRECTORY]);
}

void
ide_vcs_clone_request_set_uri (IdeVcsCloneRequest *self,
                               const char         *uri)
{
  g_return_if_fail (IDE_IS_VCS_CLONE_REQUEST (self));

  if (g_set_str (&self->uri, uri))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CAN_SELECT_BRANCH]);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_URI]);
    }
}

/**
 * ide_vcs_clone_request_get_branch_model:
 * @self: a #IdeVcsCloneRequest
 *
 * Gets the #GListModel of #IdeVcsBranch once available.
 *
 * Returns: (transfer none): a #GListModel of #IdeVcsBranch, or %NULL
 *   if the model has not yet been created.
 */
GListModel *
ide_vcs_clone_request_get_branch_model (IdeVcsCloneRequest *self)
{
  g_return_val_if_fail (IDE_IS_VCS_CLONE_REQUEST (self), NULL);

  return self->branch_model;
}

static void
ide_vcs_clone_request_populate_branches_cb (GObject *object,
                                            GAsyncResult *result,
                                            gpointer user_data)
{
  IdeVcsCloner *cloner = (IdeVcsCloner *)object;
  g_autoptr(IdeVcsCloneRequest) self = user_data;
  g_autoptr(GListModel) branches = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_VCS_CLONER (cloner));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_VCS_CLONE_REQUEST (self));

  if (!(branches = ide_vcs_cloner_list_branches_finish (cloner, result, &error)))
    g_warning ("Failed to list branches: %s", error->message);

  if (g_set_object (&self->branch_model, branches))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BRANCH_MODEL]);

  self->fetching_branches--;
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BRANCH_MODEL_BUSY]);

  IDE_EXIT;
}

void
ide_vcs_clone_request_populate_branches (IdeVcsCloneRequest *self)
{
  g_autoptr(IdeVcsUri) uri = NULL;
  const char *uri_str;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_VCS_CLONE_REQUEST (self));
  g_return_if_fail (IDE_IS_VCS_CLONER (self->cloner));

  if (!ide_vcs_clone_request_get_can_select_branch (self))
    {
      g_message ("IdeVcsCloner does not support listing branches");
      IDE_EXIT;
    }

  if (!(uri_str = ide_vcs_clone_request_get_uri (self)) ||
      !ide_vcs_uri_is_valid (uri_str) ||
      !(uri = ide_vcs_uri_new (uri_str)))
    {
      g_debug ("Unvalid VCS uri, cannot populate branches");
      IDE_EXIT;
    }

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  self->cancellable = g_cancellable_new ();

  self->fetching_branches++;

  ide_vcs_cloner_list_branches_async (self->cloner,
                                      uri,
                                      self->cancellable,
                                      ide_vcs_clone_request_populate_branches_cb,
                                      g_object_ref (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BRANCH_MODEL_BUSY]);

  IDE_EXIT;
}

IdeVcsCloneRequestValidation
ide_vcs_clone_request_validate (IdeVcsCloneRequest *self)
{
  IdeVcsCloneRequestValidation flags = 0;
  g_autoptr(IdeVcsUri) uri = NULL;

  g_return_val_if_fail (IDE_IS_VCS_CLONE_REQUEST (self), 0);
  g_return_val_if_fail (!self->cloner || IDE_IS_VCS_CLONER (self->cloner), 0);

  if (self->cloner == NULL)
    return IDE_VCS_CLONE_REQUEST_INVAL_URI;

  if (ide_str_empty0 (self->uri) || !ide_vcs_uri_is_valid (self->uri))
    flags |= IDE_VCS_CLONE_REQUEST_INVAL_URI;
  else
    uri = ide_vcs_uri_new (self->uri);

  if (uri != NULL)
    {
      const char *path;

      if ((path = ide_vcs_uri_get_path (uri)) && !ide_str_empty0 (path))
        {
          g_autofree char *name = ide_vcs_cloner_get_directory_name (self->cloner, uri);

          if (!ide_str_empty0 (name))
            {
              g_autoptr(GFile) new_directory = g_file_get_child (self->directory, name);

              if (g_file_query_exists (new_directory, NULL))
                flags |= IDE_VCS_CLONE_REQUEST_INVAL_DIRECTORY;
            }
        }
    }

  /* I mean, who really wants to validate email anyway */
  if (!ide_str_empty0 (self->author_email) &&
      strchr (self->author_email, '@') == NULL)
    flags |= IDE_VCS_CLONE_REQUEST_INVAL_EMAIL;

  return flags;
}

static void
ide_vcs_clone_request_clone_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  IdeVcsCloner *cloner = (IdeVcsCloner *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  GFile *file;

  IDE_ENTRY;

  g_assert (IDE_IS_VCS_CLONER (cloner));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  file = ide_task_get_task_data (task);
  g_assert (G_IS_FILE (file));

  if (!ide_vcs_cloner_clone_finish (cloner, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_pointer (task, g_object_ref (file), g_object_unref);

  IDE_EXIT;
}

void
ide_vcs_clone_request_clone_async (IdeVcsCloneRequest  *self,
                                   IdeNotification     *notif,
                                   int                  pty_fd,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(IdeVcsUri) uri = NULL;
  g_autoptr(GFile) clone_dir = NULL;
  g_autofree char *name = NULL;
  GVariantDict params;
  const char *uri_str;
  const char *author_name;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_VCS_CLONE_REQUEST (self));
  g_return_if_fail (IDE_IS_NOTIFICATION (notif));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_vcs_clone_request_clone_async);

  if (ide_vcs_clone_request_validate (self) != 0)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVALID_ARGUMENT,
                                 "Cannot clone, invalid arguments for request");
      IDE_EXIT;
    }

  author_name = ide_vcs_clone_request_get_author_name (self);

  uri_str = ide_vcs_clone_request_get_uri (self);
  uri = ide_vcs_uri_new (uri_str);
  name = ide_vcs_cloner_get_directory_name (self->cloner, uri);
  clone_dir = g_file_get_child (self->directory, name);
  ide_task_set_task_data (task, g_object_ref (clone_dir), g_object_unref);

  g_variant_dict_init (&params, NULL);
  if (!ide_str_empty0 (author_name) &&
      !ide_str_equal0 (author_name, g_get_real_name ()))
    g_variant_dict_insert (&params, "user.name", "s", author_name);
  if (!ide_str_empty0 (self->author_email))
    g_variant_dict_insert (&params, "user.email", "s", self->author_email);
  if (!ide_str_empty0 (self->branch_name))
    g_variant_dict_insert (&params, "branch", "s", self->branch_name);

  ide_vcs_cloner_set_pty_fd (self->cloner, pty_fd);

  ide_vcs_cloner_clone_async (self->cloner,
                              uri_str,
                              g_file_peek_path (clone_dir),
                              g_variant_dict_end (&params),
                              notif,
                              cancellable,
                              ide_vcs_clone_request_clone_cb,
                              g_steal_pointer (&task));

  IDE_EXIT;
}

/**
 * ide_vcs_clone_request_clone_finish:
 * @self: a #IdeVcsCloneRequest
 * @result: a #GAsyncResult
 * @error: a location for a #GError
 *
 * Complete a clone request.
 *
 * The result of the request is the directory that the clone was
 * completed within. This is the subdirectory within
 * #IdeVcsCloneRequest:directory.
 *
 * Returns: (transfer full): a #GFile or %NULL and @error is set.
 */
GFile *
ide_vcs_clone_request_clone_finish (IdeVcsCloneRequest  *self,
                                    GAsyncResult        *result,
                                    GError             **error)
{
  GFile *ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_VCS_CLONE_REQUEST (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  IDE_RETURN (ret);
}
