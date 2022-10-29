/* ide-lsp-code-action.c
 *
 * Copyright 2021 Georg Vienna <georg.vienna@himbarsoft.com>
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

#define G_LOG_DOMAIN "ide-lsp-code-action"

#include "config.h"

#include <jsonrpc-glib.h>

#include <libide-code.h>
#include <libide-threading.h>

#include "ide-lsp-code-action.h"

typedef struct
{
  GVariant            *arguments;
  IdeLspClient        *client;
  char                *command;
  char                *title;
  IdeLspWorkspaceEdit *workspace_edit;
} IdeLspCodeActionPrivate;

enum {
  PROP_0,
  PROP_CLIENT,
  PROP_TITLE,
  N_PROPS
};

static void code_action_iface_init (IdeCodeActionInterface *iface);

G_DEFINE_TYPE_WITH_CODE (IdeLspCodeAction, ide_lsp_code_action, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (IdeLspCodeAction)
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_CODE_ACTION, code_action_iface_init))

static GParamSpec *properties [N_PROPS];


static char *
ide_lsp_code_action_get_title (IdeCodeAction *self)
{
  IdeLspCodeActionPrivate *priv = ide_lsp_code_action_get_instance_private (IDE_LSP_CODE_ACTION (self));

  g_return_val_if_fail (IDE_IS_CODE_ACTION (self), NULL);

  return g_strdup (priv->title);
}

static void
ide_lsp_code_action_set_title (IdeLspCodeAction *self,
                               const char       *title)
{
  IdeLspCodeActionPrivate *priv = ide_lsp_code_action_get_instance_private (self);

  g_return_if_fail (IDE_IS_CODE_ACTION (self));

  if (g_set_str (&priv->title, title))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TITLE]);
}

/**
 * ide_lsp_code_action_get_client:
 * @self: a #IdeLspCodeAction
 *
 * Gets the client to use for the code action.
 *
 * Returns: (transfer none): An #IdeLspClient or %NULL.
 */
IdeLspClient *
ide_lsp_code_action_get_client (IdeLspCodeAction *self)
{
  IdeLspCodeActionPrivate *priv = ide_lsp_code_action_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_LSP_CODE_ACTION (self), NULL);

  return priv->client;
}

void
ide_lsp_code_action_set_client (IdeLspCodeAction *self,
                                IdeLspClient     *client)
{
  IdeLspCodeActionPrivate *priv = ide_lsp_code_action_get_instance_private (self);

  g_return_if_fail (IDE_IS_LSP_CODE_ACTION (self));

  if (g_set_object (&priv->client, client))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CLIENT]);
}

static void
ide_lsp_code_action_dispose (GObject *object)
{
  IdeLspCodeAction *self = (IdeLspCodeAction *)object;
  IdeLspCodeActionPrivate *priv = ide_lsp_code_action_get_instance_private (self);

  g_clear_pointer (&priv->arguments, g_variant_unref);
  g_clear_object (&priv->client);
  g_clear_pointer (&priv->command, g_free);
  g_clear_pointer (&priv->title, g_free);
  g_clear_object (&priv->workspace_edit);

  G_OBJECT_CLASS (ide_lsp_code_action_parent_class)->dispose (object);
}

static void
ide_lsp_code_action_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  IdeLspCodeAction *self = IDE_LSP_CODE_ACTION (object);

  switch (prop_id)
    {
    case PROP_TITLE:
      g_value_take_string (value, ide_lsp_code_action_get_title (IDE_CODE_ACTION (self)));
      break;

    case PROP_CLIENT:
      g_value_set_object (value, ide_lsp_code_action_get_client (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_lsp_code_action_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  IdeLspCodeAction *self = IDE_LSP_CODE_ACTION (object);

  switch (prop_id)
    {
    case PROP_TITLE:
      ide_lsp_code_action_set_title (self, g_value_get_string (value));
      break;

    case PROP_CLIENT:
      ide_lsp_code_action_set_client (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_lsp_code_action_class_init (IdeLspCodeActionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_lsp_code_action_dispose;
  object_class->get_property = ide_lsp_code_action_get_property;
  object_class->set_property = ide_lsp_code_action_set_property;

  properties [PROP_TITLE] =
    g_param_spec_string ("title",
                         "Title",
                         "The title of the code action",
                         NULL,
                         (G_PARAM_READWRITE |G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_CLIENT] =
    g_param_spec_object ("client",
                         "Client",
                         "The client to communicate over",
                         IDE_TYPE_LSP_CLIENT,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_lsp_code_action_init (IdeLspCodeAction *self)
{
}

static void
ide_lsp_code_action_execute_call_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  IdeLspClient *client = (IdeLspClient *)object;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_LSP_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!ide_lsp_client_call_finish (client, result, &reply, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
ide_lsp_code_action_edits_applied (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  IdeBufferManager *manager = (IdeBufferManager *)object;

  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeLspCodeAction *self;
  GCancellable* cancellable;
  IdeLspCodeActionPrivate *priv;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER_MANAGER (object));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!ide_buffer_manager_apply_edits_finish (manager, result, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  self = ide_task_get_source_object (task);
  priv = ide_lsp_code_action_get_instance_private (self);
  cancellable = ide_task_get_cancellable (task);

  /* execute command if there is one */
  if (priv->command)
    {
      g_autoptr(GVariant) params = NULL;

      params = JSONRPC_MESSAGE_NEW (
        "command", JSONRPC_MESSAGE_PUT_STRING (priv->command),
        "arguments", "{", JSONRPC_MESSAGE_PUT_VARIANT (priv->arguments), "}"
      );

      ide_lsp_client_call_async (priv->client,
                                 "workspace/executeCommand",
                                 params,
                                 cancellable,
                                 ide_lsp_code_action_execute_call_cb,
                                 g_steal_pointer (&task));

      IDE_EXIT;
    }

  ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
ide_lsp_code_action_execute_async (IdeCodeAction       *code_action,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  IdeLspCodeAction *self = (IdeLspCodeAction *)code_action;
  IdeLspCodeActionPrivate *priv = ide_lsp_code_action_get_instance_private (self);
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_LSP_CODE_ACTION (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_lsp_code_action_execute_async);

  /* If a code action provides an edit and a command,
   * first the edit is executed and then the command.
   */
  if (priv->workspace_edit != NULL)
    {
      g_autoptr(GPtrArray) edits = NULL;
      IdeBufferManager* buffer_manager;
      IdeContext* context;

      edits = ide_lsp_workspace_edit_get_edits (priv->workspace_edit);
      context = ide_object_ref_context (IDE_OBJECT (priv->client));
      buffer_manager = ide_buffer_manager_from_context (context);

      ide_buffer_manager_apply_edits_async (buffer_manager,
                                           IDE_PTR_ARRAY_STEAL_FULL (&edits),
                                           cancellable,
                                           ide_lsp_code_action_edits_applied,
                                           g_steal_pointer (&task));
    }
  else
    {
      g_autoptr(GVariant) params = NULL;
      GVariantDict dict;
      g_variant_dict_init (&dict, NULL);
      g_variant_dict_insert_value (&dict, "arguments", priv->arguments);
      g_variant_dict_insert (&dict, "command", "s", priv->command);
      params =  g_variant_take_ref (g_variant_dict_end (&dict));
      ide_lsp_client_call_async (priv->client,
                                 "workspace/executeCommand",
                                 params,
                                 cancellable,
                                 ide_lsp_code_action_execute_call_cb,
                                 g_steal_pointer (&task));
    }

  IDE_EXIT;
}

static gboolean
ide_lsp_code_action_execute_finish (IdeCodeAction  *self,
                                    GAsyncResult   *result,
                                    GError        **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_CODE_ACTION (self));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
code_action_iface_init (IdeCodeActionInterface *iface)
{
  iface->get_title = ide_lsp_code_action_get_title;
  iface->execute_async = ide_lsp_code_action_execute_async;
  iface->execute_finish = ide_lsp_code_action_execute_finish;
}

IdeLspCodeAction *
ide_lsp_code_action_new (IdeLspClient        *client,
                         const char          *title,
                         const char          *command,
                         GVariant            *arguments,
                         IdeLspWorkspaceEdit *workspace_edit)
{
  IdeLspCodeActionPrivate *priv;
  IdeLspCodeAction *self;

  g_return_val_if_fail (!client || IDE_IS_LSP_CLIENT (client), NULL);
  g_return_val_if_fail (!workspace_edit || IDE_IS_LSP_WORKSPACE_EDIT (workspace_edit), NULL);

  self = g_object_new (IDE_TYPE_LSP_CODE_ACTION,
                       "client", client,
                       "title", title,
                       NULL);
  priv = ide_lsp_code_action_get_instance_private (self);

  priv->command = g_strdup (command);
  priv->arguments = arguments ? g_variant_ref_sink (arguments) : NULL;
  g_set_object (&priv->workspace_edit, workspace_edit);

  return self;
}

