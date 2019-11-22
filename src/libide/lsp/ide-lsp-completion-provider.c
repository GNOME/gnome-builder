/* ide-lsp-completion-provider.c
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-lsp-completion-provider"

#include "config.h"

#include <libide-code.h>
#include <libide-sourceview.h>
#include <libide-threading.h>
#include <jsonrpc-glib.h>

#include "ide-lsp-completion-provider.h"
#include "ide-lsp-completion-item.h"
#include "ide-lsp-completion-results.h"
#include "ide-lsp-util.h"

typedef struct
{
  IdeLspClient  *client;
  gchar         *word;
  gchar        **trigger_chars;
} IdeLspCompletionProviderPrivate;

static void provider_iface_init (IdeCompletionProviderInterface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (IdeLspCompletionProvider, ide_lsp_completion_provider, IDE_TYPE_OBJECT,
                                  G_ADD_PRIVATE (IdeLspCompletionProvider)
                                  G_IMPLEMENT_INTERFACE (IDE_TYPE_COMPLETION_PROVIDER, provider_iface_init))

enum {
  PROP_0,
  PROP_CLIENT,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
ide_lsp_completion_provider_finalize (GObject *object)
{
  IdeLspCompletionProvider *self = (IdeLspCompletionProvider *)object;
  IdeLspCompletionProviderPrivate *priv = ide_lsp_completion_provider_get_instance_private (self);

  g_clear_object (&priv->client);
  g_clear_pointer (&priv->word, g_free);
  g_clear_pointer (&priv->trigger_chars, g_free);

  G_OBJECT_CLASS (ide_lsp_completion_provider_parent_class)->finalize (object);
}

static void
ide_lsp_completion_provider_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  IdeLspCompletionProvider *self = IDE_LSP_COMPLETION_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_CLIENT:
      g_value_set_object (value, ide_lsp_completion_provider_get_client (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_lsp_completion_provider_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  IdeLspCompletionProvider *self = IDE_LSP_COMPLETION_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_CLIENT:
      ide_lsp_completion_provider_set_client (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_lsp_completion_provider_class_init (IdeLspCompletionProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_lsp_completion_provider_finalize;
  object_class->get_property = ide_lsp_completion_provider_get_property;
  object_class->set_property = ide_lsp_completion_provider_set_property;

  properties [PROP_CLIENT] =
    g_param_spec_object ("client",
                         "Client",
                         "The Language Server client",
                         IDE_TYPE_LSP_CLIENT,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_lsp_completion_provider_init (IdeLspCompletionProvider *self)
{
}

/**
 * ide_lsp_completion_provider_get_client:
 * @self: An #IdeLspCompletionProvider
 *
 * Gets the client for the completion provider.
 *
 * Returns: (transfer none) (nullable): An #IdeLspClient or %NULL
 */
IdeLspClient *
ide_lsp_completion_provider_get_client (IdeLspCompletionProvider *self)
{
  IdeLspCompletionProviderPrivate *priv = ide_lsp_completion_provider_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_LSP_COMPLETION_PROVIDER (self), NULL);

  return priv->client;
}

static void
on_notify_server_capabilities_cb (IdeLspCompletionProvider *self,
                                  GParamSpec *pspec,
                                  IdeLspClient *client)
{
  IdeLspCompletionProviderPrivate *priv = ide_lsp_completion_provider_get_instance_private (self);
  GVariant *capabilities;

  IDE_ENTRY;

  g_assert (IDE_LSP_COMPLETION_PROVIDER (self));
  g_assert (IDE_IS_LSP_CLIENT (client));

  capabilities = ide_lsp_client_get_server_capabilities (client);

  if (capabilities != NULL)
    {
      g_auto(GStrv) trigger_chars = NULL;
      gboolean r;

      r = JSONRPC_MESSAGE_PARSE (capabilities,
        "completionProvider", "{",
          "triggerCharacters", JSONRPC_MESSAGE_GET_STRV (&trigger_chars),
        "}"
      );

      if (r)
        {
          g_clear_pointer (&priv->trigger_chars, g_strfreev);
          priv->trigger_chars = g_steal_pointer (&trigger_chars);
        }
    }

  IDE_EXIT;
}

void
ide_lsp_completion_provider_set_client (IdeLspCompletionProvider *self,
                                        IdeLspClient             *client)
{
  IdeLspCompletionProviderPrivate *priv = ide_lsp_completion_provider_get_instance_private (self);

  g_return_if_fail (IDE_IS_LSP_COMPLETION_PROVIDER (self));
  g_return_if_fail (!client || IDE_IS_LSP_CLIENT (client));

  if (g_set_object (&priv->client, client))
    {
      g_signal_connect_object (client,
                               "notify::server-capabilities",
                               G_CALLBACK (on_notify_server_capabilities_cb),
                               self,
                               G_CONNECT_SWAPPED);
      on_notify_server_capabilities_cb (self, NULL, client);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CLIENT]);
    }
}

static gint
ide_lsp_completion_provider_get_priority (IdeCompletionProvider *provider,
                                          IdeCompletionContext  *context)
{
  return IDE_LSP_COMPLETION_PROVIDER_PRIORITY;
}

static void
ide_lsp_completion_provider_complete_cb (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
  IdeLspCompletionProviderPrivate *priv;
  IdeLspCompletionProvider *self;
  IdeLspClient *client = (IdeLspClient *)object;
  g_autoptr(GVariant) return_value = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeLspCompletionResults *ret;

  IDE_ENTRY;

  g_assert (IDE_IS_LSP_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!ide_lsp_client_call_finish (client, result, &return_value, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  self = ide_task_get_source_object (task);
  priv = ide_lsp_completion_provider_get_instance_private (self);

  ret = ide_lsp_completion_results_new (return_value);
  if (priv->word != NULL && *priv->word != 0)
    ide_lsp_completion_results_refilter (ret, priv->word);

  ide_task_return_object (task, g_steal_pointer (&ret));

  IDE_EXIT;
}

static void
ide_lsp_completion_provider_populate_async (IdeCompletionProvider *provider,
                                            IdeCompletionContext  *context,
                                            GCancellable          *cancellable,
                                            GAsyncReadyCallback    callback,
                                            gpointer               user_data)
{
  IdeLspCompletionProvider *self = (IdeLspCompletionProvider *)provider;
  IdeLspCompletionProviderPrivate *priv = ide_lsp_completion_provider_get_instance_private (self);
  IdeCompletionActivation activation;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GVariant) params = NULL;
  g_autofree gchar *uri = NULL;
  GtkTextIter iter, end;
  GtkTextBuffer *buffer;
  gint trigger_kind;
  gint line;
  gint column;

  IDE_ENTRY;

  g_assert (IDE_IS_LSP_COMPLETION_PROVIDER (self));
  g_assert (IDE_IS_COMPLETION_CONTEXT (context));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_lsp_completion_provider_populate_async);

  if (priv->client == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "No client for completion");
      IDE_EXIT;
    }

  ide_completion_context_get_bounds (context, &iter, &end);

  activation = ide_completion_context_get_activation (context);

  if (activation == IDE_COMPLETION_TRIGGERED)
    trigger_kind = 2;
  else
    trigger_kind = 1;

  g_clear_pointer (&priv->word, g_free);
  priv->word = ide_completion_context_get_word (context);

  buffer = ide_completion_context_get_buffer (context);
  uri = ide_buffer_dup_uri (IDE_BUFFER (buffer));

  line = gtk_text_iter_get_line (&iter);
  column = gtk_text_iter_get_line_offset (&iter);

  params = JSONRPC_MESSAGE_NEW (
    "textDocument", "{",
      "uri", JSONRPC_MESSAGE_PUT_STRING (uri),
    "}",
    "position", "{",
      "line", JSONRPC_MESSAGE_PUT_INT32 (line),
      "character", JSONRPC_MESSAGE_PUT_INT32 (column),
    "}",
    "context", "{",
      "triggerKind", JSONRPC_MESSAGE_PUT_INT32 (trigger_kind),
    "}"
  );

  ide_lsp_client_call_async (priv->client,
                             "textDocument/completion",
                             params,
                             cancellable,
                             ide_lsp_completion_provider_complete_cb,
                             g_steal_pointer (&task));

  IDE_EXIT;
}

static GListModel *
ide_lsp_completion_provider_populate_finish (IdeCompletionProvider  *provider,
                                             GAsyncResult           *result,
                                             GError                **error)
{
  GListModel *ret;

  IDE_ENTRY;

  g_assert (IDE_IS_COMPLETION_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_object (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static gboolean
ide_lsp_completion_provider_refilter (IdeCompletionProvider *provider,
                                      IdeCompletionContext  *context,
                                      GListModel            *model)
{
  IdeLspCompletionResults *results = (IdeLspCompletionResults *)model;
  g_autofree gchar *word = NULL;

  g_assert (IDE_IS_LSP_COMPLETION_PROVIDER (provider));
  g_assert (IDE_IS_COMPLETION_CONTEXT (context));
  g_assert (IDE_IS_LSP_COMPLETION_RESULTS (results));

  word = ide_completion_context_get_word (context);
  ide_lsp_completion_results_refilter (results, word);

  return TRUE;
}

static void
ide_lsp_completion_provider_display_proposal (IdeCompletionProvider   *provider,
                                              IdeCompletionListBoxRow *row,
                                              IdeCompletionContext    *context,
                                              const gchar             *typed_text,
                                              IdeCompletionProposal   *proposal)
{
  IdeLspCompletionItem *item = (IdeLspCompletionItem *)proposal;
  g_autofree gchar *markup = NULL;

  g_assert (IDE_IS_LSP_COMPLETION_PROVIDER (provider));
  g_assert (IDE_IS_COMPLETION_LIST_BOX_ROW (row));
  g_assert (IDE_IS_COMPLETION_CONTEXT (context));
  g_assert (IDE_IS_LSP_COMPLETION_ITEM (proposal));

  markup = ide_lsp_completion_item_get_markup (item, typed_text);

  ide_completion_list_box_row_set_icon_name (row, ide_lsp_completion_item_get_icon_name (item));
  ide_completion_list_box_row_set_left (row, NULL);
  ide_completion_list_box_row_set_center_markup (row, markup);
  ide_completion_list_box_row_set_right (row, NULL);
}

static void
ide_lsp_completion_provider_activate_proposal (IdeCompletionProvider *provider,
                                               IdeCompletionContext  *context,
                                               IdeCompletionProposal *proposal,
                                               const GdkEventKey     *key)
{
  g_autoptr(IdeSnippet) snippet = NULL;
  GtkTextBuffer *buffer;
  GtkTextView *view;
  GtkTextIter begin, end;

  g_assert (IDE_IS_COMPLETION_PROVIDER (provider));
  g_assert (IDE_IS_COMPLETION_CONTEXT (context));
  g_assert (IDE_IS_LSP_COMPLETION_ITEM (proposal));

  buffer = ide_completion_context_get_buffer (context);
  view = ide_completion_context_get_view (context);

  snippet = ide_lsp_completion_item_get_snippet (IDE_LSP_COMPLETION_ITEM (proposal));

  gtk_text_buffer_begin_user_action (buffer);
  if (ide_completion_context_get_bounds (context, &begin, &end))
    gtk_text_buffer_delete (buffer, &begin, &end);
  ide_source_view_push_snippet (IDE_SOURCE_VIEW (view), snippet, &begin);
  gtk_text_buffer_end_user_action (buffer);
}

static gchar *
ide_lsp_completion_provider_get_comment (IdeCompletionProvider *provider,
                                         IdeCompletionProposal *proposal)
{
  g_assert (IDE_IS_COMPLETION_PROVIDER (provider));
  g_assert (IDE_IS_LSP_COMPLETION_ITEM (proposal));

  return g_strdup (ide_lsp_completion_item_get_detail (IDE_LSP_COMPLETION_ITEM (proposal)));
}

static gboolean
ide_lsp_completion_provider_is_trigger (IdeCompletionProvider *provider,
                                        const GtkTextIter     *iter,
                                        gunichar               ch)
{
  IdeLspCompletionProvider *self = (IdeLspCompletionProvider *)provider;
  IdeLspCompletionProviderPrivate *priv = ide_lsp_completion_provider_get_instance_private (self);

  g_assert (IDE_IS_LSP_COMPLETION_PROVIDER (self));
  g_assert (iter != NULL);

  if (priv->trigger_chars != NULL)
    {
      for (guint i = 0; priv->trigger_chars[i]; i++)
        {
          const gchar *trigger = priv->trigger_chars[i];

          /* Technically, since these are strings they can be more than
           * one character long. But I haven't seen anything actually
           * do that in the wild yet.
           */
          if (ch == g_utf8_get_char (trigger))
            return TRUE;
        }
    }

  return FALSE;
}

static void
provider_iface_init (IdeCompletionProviderInterface *iface)
{
  iface->get_priority = ide_lsp_completion_provider_get_priority;
  iface->populate_async = ide_lsp_completion_provider_populate_async;
  iface->populate_finish = ide_lsp_completion_provider_populate_finish;
  iface->refilter = ide_lsp_completion_provider_refilter;
  iface->display_proposal = ide_lsp_completion_provider_display_proposal;
  iface->activate_proposal = ide_lsp_completion_provider_activate_proposal;
  iface->get_comment = ide_lsp_completion_provider_get_comment;
  iface->is_trigger = ide_lsp_completion_provider_is_trigger;
}
