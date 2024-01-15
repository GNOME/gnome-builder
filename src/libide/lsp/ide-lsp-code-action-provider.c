/* ide-lsp-code-action-provider.c
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

#define G_LOG_DOMAIN "ide-lsp-code-action-provider"

#include "config.h"

#include <glib/gi18n.h>

#include <jsonrpc-glib.h>

#include <libide-code.h>
#include <libide-threading.h>

#include "ide-lsp-code-action.h"
#include "ide-lsp-diagnostic.h"
#include "ide-lsp-code-action-provider.h"
#include "ide-lsp-workspace-edit.h"

typedef struct
{
  IdeLspClient *client;
  IdeDiagnostics *diagnostics;
} IdeLspCodeActionProviderPrivate;

enum {
  PROP_0,
  PROP_CLIENT,
  PROP_DIAGNOSTICS,
  N_PROPS
};

static void code_action_provider_iface_init              (IdeCodeActionProviderInterface *iface);
static void ide_lsp_code_action_provider_set_diagnostics (IdeCodeActionProvider          *code_action_provider,
                                                          IdeDiagnostics                 *diags);


G_DEFINE_TYPE_WITH_CODE (IdeLspCodeActionProvider, ide_lsp_code_action_provider, IDE_TYPE_OBJECT,
                         G_ADD_PRIVATE (IdeLspCodeActionProvider)
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_CODE_ACTION_PROVIDER, code_action_provider_iface_init))

static GParamSpec *properties [N_PROPS];

/**
 * ide_lsp_code_action_provider_get_client:
 * @self: a #IdeLspCodeActionProvider
 *
 * Gets the client to use for the code action query.
 *
 * Returns: (transfer none): An #IdeLspClient or %NULL.
 */
IdeLspClient *
ide_lsp_code_action_provider_get_client (IdeLspCodeActionProvider *self)
{
  IdeLspCodeActionProviderPrivate *priv = ide_lsp_code_action_provider_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_LSP_CODE_ACTION_PROVIDER (self), NULL);

  return priv->client;
}

/**
 * ide_lsp_code_action_provider_get_diagnostics:
 * @self: a #IdeLspCodeActionProvider
 *
 * Gets the diagnostics to use for the code action query.
 *
 * Returns: (transfer none) (nullable): An #IdeDiagnostics or %NULL.
 */
IdeDiagnostics *
ide_lsp_code_action_provider_get_diagnostics (IdeLspCodeActionProvider *self)
{
  IdeLspCodeActionProviderPrivate *priv = ide_lsp_code_action_provider_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_LSP_CODE_ACTION_PROVIDER (self), NULL);

  return priv->diagnostics;
}

void
ide_lsp_code_action_provider_set_client (IdeLspCodeActionProvider *self,
                              IdeLspClient    *client)
{
  IdeLspCodeActionProviderPrivate *priv = ide_lsp_code_action_provider_get_instance_private (self);

  g_return_if_fail (IDE_IS_LSP_CODE_ACTION_PROVIDER (self));

  if (g_set_object (&priv->client, client))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CLIENT]);
}

static void
ide_lsp_code_action_provider_finalize (GObject *object)
{
  IdeLspCodeActionProvider *self = (IdeLspCodeActionProvider *)object;
  IdeLspCodeActionProviderPrivate *priv = ide_lsp_code_action_provider_get_instance_private (self);

  g_clear_object (&priv->client);
  g_clear_object (&priv->diagnostics);

  G_OBJECT_CLASS (ide_lsp_code_action_provider_parent_class)->finalize (object);
}

static void
ide_lsp_code_action_provider_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  IdeLspCodeActionProvider *self = IDE_LSP_CODE_ACTION_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_CLIENT:
      g_value_set_object (value, ide_lsp_code_action_provider_get_client (self));
      break;

    case PROP_DIAGNOSTICS:
      g_value_set_object (value, ide_lsp_code_action_provider_get_diagnostics (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_lsp_code_action_provider_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  IdeLspCodeActionProvider *self = IDE_LSP_CODE_ACTION_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_CLIENT:
      ide_lsp_code_action_provider_set_client (self, g_value_get_object (value));
      break;

    case PROP_DIAGNOSTICS:
      ide_code_action_provider_set_diagnostics (IDE_CODE_ACTION_PROVIDER (self), g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_lsp_code_action_provider_class_init (IdeLspCodeActionProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_lsp_code_action_provider_finalize;
  object_class->get_property = ide_lsp_code_action_provider_get_property;
  object_class->set_property = ide_lsp_code_action_provider_set_property;

  properties [PROP_CLIENT] =
    g_param_spec_object ("client",
                         "Client",
                         "The client to communicate over",
                         IDE_TYPE_LSP_CLIENT,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_DIAGNOSTICS] =
    g_param_spec_object ("diagnostics",
                         "Diagnostics",
                         "The diagnostics used to send to the codeAction RPC",
                         IDE_TYPE_DIAGNOSTICS,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_lsp_code_action_provider_init (IdeLspCodeActionProvider *self)
{
}

static void
ide_lsp_code_action_provider_query_call_cb (GObject      *object,
                                            GAsyncResult *result,
                                            gpointer      user_data)
{
  IdeLspClient *client = (IdeLspClient *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GPtrArray) code_actions = NULL;
  g_autoptr(GVariant) code_action = NULL;
  IdeLspCodeActionProvider *self;
  IdeBuffer *buffer;
  GVariantIter iter;

  g_return_if_fail (IDE_IS_LSP_CLIENT (client));
  g_return_if_fail (G_IS_ASYNC_RESULT (result));

  if (!ide_lsp_client_call_finish (client, result, &reply, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  self = ide_task_get_source_object (task);
  buffer = ide_task_get_task_data (task);

  g_assert (IDE_IS_LSP_CODE_ACTION_PROVIDER (self));
  g_assert (IDE_IS_BUFFER (buffer));

  code_actions = g_ptr_array_new_with_free_func (g_object_unref);

  g_variant_iter_init (&iter, reply);

  while (g_variant_iter_loop (&iter, "v", &code_action))
    {
      const gchar *title = NULL;
      const gchar *command = NULL;
      g_autoptr(GVariant) arguments = NULL;
      g_autoptr(GVariant) edit = NULL;
      g_autoptr(IdeLspWorkspaceEdit) workspace_edit = NULL;

      if (!JSONRPC_MESSAGE_PARSE (code_action,
        "title", JSONRPC_MESSAGE_GET_STRING (&title)
      ))
        {
          IDE_TRACE_MSG ("Failed to extract command title from variant");
          continue;
        }

      JSONRPC_MESSAGE_PARSE (code_action,
        "command", "{",
          "command", JSONRPC_MESSAGE_GET_STRING (&command),
          "arguments", JSONRPC_MESSAGE_GET_VARIANT (&arguments),
        "}"
      );

      if (JSONRPC_MESSAGE_PARSE (code_action,
        "edit", JSONRPC_MESSAGE_GET_VARIANT (&edit)
      ))
        {
          workspace_edit = ide_lsp_workspace_edit_new(edit);
        }

      g_ptr_array_add (code_actions, ide_lsp_code_action_new (client, title, command, arguments, workspace_edit));
    }

  ide_task_return_pointer(task, g_steal_pointer(&code_actions), g_ptr_array_unref);
}

static void
ide_lsp_code_action_provider_query_async (IdeCodeActionProvider *code_action_provider,
                                          IdeBuffer             *buffer,
                                          GCancellable          *cancellable,
                                          GAsyncReadyCallback    callback,
                                          gpointer               user_data)
{
  IdeLspCodeActionProvider *self = (IdeLspCodeActionProvider *)code_action_provider;
  IdeLspCodeActionProviderPrivate *priv = ide_lsp_code_action_provider_get_instance_private (self);
  g_autoptr(GVariant) params = NULL;
  g_autoptr(IdeTask) task = NULL;
  IdeRange *selection = NULL;
  IdeLocation *start = NULL;
  IdeLocation *end = NULL;
  g_autofree gchar *uri = NULL;
  g_autoptr(GVariant) diagnostics = NULL;
  g_autoptr(GVariant) diags_v = NULL;
  g_autoptr(GPtrArray) matching = NULL;
  GVariantDict dict;

  IDE_ENTRY;

  g_assert (IDE_IS_LSP_CODE_ACTION_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_lsp_code_action_provider_query_async);
  ide_task_set_task_data (task, g_object_ref (buffer), g_object_unref);

  if (priv->client == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_CONNECTED,
                                 _("No LSP client connection is available"));
      IDE_EXIT;
    }

  uri = ide_buffer_dup_uri (buffer);
  selection = ide_buffer_get_selection_range (buffer);
  start = ide_range_get_begin (selection);
  end = ide_range_get_end (selection);
  matching = g_ptr_array_new_with_free_func ((GDestroyNotify) g_variant_unref);

  if (priv->diagnostics != NULL)
    {
      guint n_items = ide_diagnostics_get_size (priv->diagnostics);

      for (guint i = 0; i < n_items; i++)
        {
          g_autoptr(IdeDiagnostic) diag = g_list_model_get_item (G_LIST_MODEL (priv->diagnostics), i);
          g_autoptr(GVariant) var = NULL;
          IdeLocation *location = NULL;
          gint line = 0;

          if (!IDE_IS_LSP_DIAGNOSTIC (diag))
            continue;

          location = ide_diagnostic_get_location (diag);
          if (!location || !start || !end)
              continue;

          line = ide_location_get_line (location);
          if (ide_location_get_line (start) > line || ide_location_get_line (end) < line)
              continue;

          if (!(var = ide_lsp_diagnostic_dup_raw (IDE_LSP_DIAGNOSTIC (diag))))
            continue;

          g_ptr_array_add (matching, g_steal_pointer (&var));
        }
    }

  diagnostics = g_variant_new_array (G_VARIANT_TYPE_VARDICT,
                                     (GVariant **)(gpointer)matching->pdata,
                                     matching->len);

  g_variant_dict_init (&dict, NULL);
  g_variant_dict_insert_value (&dict, "diagnostics", g_variant_ref (diagnostics));
  diags_v = g_variant_take_ref (g_variant_dict_end (&dict));

  params = JSONRPC_MESSAGE_NEW (
    "textDocument", "{",
      "uri", JSONRPC_MESSAGE_PUT_STRING (uri),
    "}",
    "range", "{",
          "start", "{",
            "line", JSONRPC_MESSAGE_PUT_INT64 (ide_location_get_line (start)),
            "character", JSONRPC_MESSAGE_PUT_INT64 (ide_location_get_line_offset (start)),
          "}",
          "end", "{",
            "line", JSONRPC_MESSAGE_PUT_INT64 (ide_location_get_line (end)),
            "character", JSONRPC_MESSAGE_PUT_INT64 (ide_location_get_line_offset (end)),
          "}",
        "}",
    "context", "{",
      JSONRPC_MESSAGE_PUT_VARIANT (diags_v),
    "}"
  );

  ide_lsp_client_call_async (priv->client,
                             "textDocument/codeAction",
                             params,
                             cancellable,
                             ide_lsp_code_action_provider_query_call_cb,
                             g_steal_pointer (&task));

  IDE_EXIT;
}

static GPtrArray*
ide_lsp_code_action_provider_query_finish (IdeCodeActionProvider  *self,
                                           GAsyncResult           *result,
                                           GError                **error)
{
  g_assert (IDE_IS_CODE_ACTION_PROVIDER (self));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static void
ide_lsp_code_action_provider_set_diagnostics (IdeCodeActionProvider *code_action_provider,
                                              IdeDiagnostics        *diags)
{
  IdeLspCodeActionProvider *self = (IdeLspCodeActionProvider *)code_action_provider;
  IdeLspCodeActionProviderPrivate *priv = ide_lsp_code_action_provider_get_instance_private (self);

  g_assert (IDE_IS_LSP_CODE_ACTION_PROVIDER (self));
  g_assert (!diags || IDE_IS_DIAGNOSTICS (diags));

  if (g_set_object (&priv->diagnostics, diags))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DIAGNOSTICS]);
}

static void
code_action_provider_iface_init (IdeCodeActionProviderInterface *iface)
{
  iface->query_async = ide_lsp_code_action_provider_query_async;
  iface->query_finish = ide_lsp_code_action_provider_query_finish;
  iface->set_diagnostics = ide_lsp_code_action_provider_set_diagnostics;
}
