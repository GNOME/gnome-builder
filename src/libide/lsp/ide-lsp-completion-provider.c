/* ide-lsp-completion-provider.c
 *
 * Copyright 2016-2022 Christian Hergert <chergert@redhat.com>
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
  char          *word;
  char         **trigger_chars;
  char          *refilter_word;
  guint          has_loaded : 1;
} IdeLspCompletionProviderPrivate;

static void provider_iface_init (GtkSourceCompletionProviderInterface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (IdeLspCompletionProvider, ide_lsp_completion_provider, IDE_TYPE_OBJECT,
                                  G_ADD_PRIVATE (IdeLspCompletionProvider)
                                  G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROVIDER, provider_iface_init))

enum {
  PROP_0,
  PROP_CLIENT,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
ide_lsp_completion_provider_real_load (IdeLspCompletionProvider *self)
{
}

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

  klass->load = ide_lsp_completion_provider_real_load;

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
                                  GParamSpec               *pspec,
                                  IdeLspClient             *client)
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
      if (client != NULL)
        {
          g_signal_connect_object (client,
                                   "notify::server-capabilities",
                                   G_CALLBACK (on_notify_server_capabilities_cb),
                                   self,
                                   G_CONNECT_SWAPPED);
          on_notify_server_capabilities_cb (self, NULL, client);
        }

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CLIENT]);
    }
}

static gint
ide_lsp_completion_provider_get_priority (GtkSourceCompletionProvider *provider,
                                          GtkSourceCompletionContext  *context)
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
      IDE_TRACE_MSG ("Completion call failed: %s", error->message);
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  self = ide_task_get_source_object (task);
  priv = ide_lsp_completion_provider_get_instance_private (self);

  ret = ide_lsp_completion_results_new (return_value);

  g_debug ("%s populated initial result set of %u items",
           G_OBJECT_TYPE_NAME (self),
           g_list_model_get_n_items (G_LIST_MODEL (ret)));

  if (!ide_str_empty0 (priv->word))
    {
      IDE_TRACE_MSG ("Filtering results to %s", priv->word);
      ide_lsp_completion_results_refilter (ret, priv->word);
    }

  ide_task_return_object (task, g_steal_pointer (&ret));

  IDE_EXIT;
}

static void
ide_lsp_completion_provider_populate_async (GtkSourceCompletionProvider *provider,
                                            GtkSourceCompletionContext  *context,
                                            GCancellable                *cancellable,
                                            GAsyncReadyCallback          callback,
                                            gpointer                     user_data)
{
  IdeLspCompletionProvider *self = (IdeLspCompletionProvider *)provider;
  IdeLspCompletionProviderPrivate *priv = ide_lsp_completion_provider_get_instance_private (self);
  GtkSourceCompletionActivation activation;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GVariant) params = NULL;
  g_autofree char *uri = NULL;
  GtkTextIter iter, end;
  GtkSourceBuffer *buffer;
  gint trigger_kind;
  gint line;
  gint column;

  IDE_ENTRY;

  g_assert (IDE_IS_LSP_COMPLETION_PROVIDER (self));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));

  if (!priv->has_loaded)
    {
      priv->has_loaded = TRUE;
      IDE_LSP_COMPLETION_PROVIDER_GET_CLASS (self)->load (self);
    }

  g_clear_pointer (&priv->refilter_word, g_free);
  g_clear_pointer (&priv->word, g_free);

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

  gtk_source_completion_context_get_bounds (context, &iter, &end);

  activation = gtk_source_completion_context_get_activation (context);

  if (activation == GTK_SOURCE_COMPLETION_ACTIVATION_INTERACTIVE)
    trigger_kind = 2;
  else
    trigger_kind = 1;

  priv->word = gtk_source_completion_context_get_word (context);

  buffer = gtk_source_completion_context_get_buffer (context);
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
ide_lsp_completion_provider_populate_finish (GtkSourceCompletionProvider  *provider,
                                             GAsyncResult                 *result,
                                             GError                      **error)
{
  GListModel *ret;

  IDE_ENTRY;

  g_assert (GTK_SOURCE_IS_COMPLETION_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_object (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
ide_lsp_completion_provider_refilter (GtkSourceCompletionProvider *provider,
                                      GtkSourceCompletionContext  *context,
                                      GListModel                  *model)
{
  IdeLspCompletionProvider *self = (IdeLspCompletionProvider *)provider;
  IdeLspCompletionProviderPrivate *priv = ide_lsp_completion_provider_get_instance_private (self);
  IdeLspCompletionResults *results = (IdeLspCompletionResults *)model;

  g_assert (IDE_IS_LSP_COMPLETION_PROVIDER (self));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));
  g_assert (IDE_IS_LSP_COMPLETION_RESULTS (results));

  g_clear_pointer (&priv->refilter_word, g_free);
  priv->refilter_word = gtk_source_completion_context_get_word (context);

  ide_lsp_completion_results_refilter (results, priv->refilter_word);
}

static void
ide_lsp_completion_provider_display (GtkSourceCompletionProvider *provider,
                                     GtkSourceCompletionContext  *context,
                                     GtkSourceCompletionProposal *proposal,
                                     GtkSourceCompletionCell     *cell)
{
  IdeLspCompletionItem *item = (IdeLspCompletionItem *)proposal;
  IdeLspCompletionProvider *self = (IdeLspCompletionProvider *)provider;
  IdeLspCompletionProviderPrivate *priv = ide_lsp_completion_provider_get_instance_private (self);
  const char *typed_text;

  g_assert (IDE_IS_LSP_COMPLETION_PROVIDER (provider));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));
  g_assert (IDE_IS_LSP_COMPLETION_ITEM (proposal));
  g_assert (GTK_SOURCE_IS_COMPLETION_CELL (cell));

  if (priv->refilter_word)
    typed_text = priv->refilter_word;
  else
    typed_text = priv->word;

  ide_lsp_completion_item_display (item, cell, typed_text);
}

static void
ide_lsp_completion_provider_apply_additional_edits_cb (GObject      *object,
                                                       GAsyncResult *result,
                                                       gpointer      user_data)
{
  IdeBufferManager *bufmgr = (IdeBufferManager *)object;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER_MANAGER (bufmgr));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!ide_buffer_manager_apply_edits_finish (bufmgr, result, &error))
    g_warning ("Failed to apply additional text edits for completion: %s",
               error->message);

  IDE_EXIT;
}

static void
ide_lsp_completion_provider_activate (GtkSourceCompletionProvider *provider,
                                      GtkSourceCompletionContext  *context,
                                      GtkSourceCompletionProposal *proposal)
{
  g_autoptr(GPtrArray) additional_text_edits = NULL;
  g_autoptr(GtkSourceSnippet) snippet = NULL;
  GtkSourceSnippetChunk *chunk;
  GtkSourceBuffer *buffer;
  GtkSourceView *view;
  GtkTextIter begin, end;
  const char *text = NULL;

  g_assert (GTK_SOURCE_IS_COMPLETION_PROVIDER (provider));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));
  g_assert (IDE_IS_LSP_COMPLETION_ITEM (proposal));

  buffer = gtk_source_completion_context_get_buffer (context);
  g_assert (IDE_IS_BUFFER (buffer));

  view = gtk_source_completion_context_get_view (context);
  g_assert (IDE_IS_SOURCE_VIEW (view));

  snippet = ide_lsp_completion_item_get_snippet (IDE_LSP_COMPLETION_ITEM (proposal));
  if ((chunk = gtk_source_snippet_get_nth_chunk (snippet, 0)))
    text = gtk_source_snippet_chunk_get_text (chunk);

  gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (buffer));
  if (gtk_source_completion_context_get_bounds (context, &begin, &end))
    {
      gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &begin, &end);
      ide_text_util_remove_common_prefix (&begin, text);
    }
  gtk_source_view_push_snippet (GTK_SOURCE_VIEW (view), snippet, &begin);
  gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (buffer));

  additional_text_edits =
    ide_lsp_completion_item_get_additional_text_edits (IDE_LSP_COMPLETION_ITEM (proposal),
                                                       ide_buffer_get_file (IDE_BUFFER (buffer)));
  IDE_PTR_ARRAY_SET_FREE_FUNC (additional_text_edits, g_object_unref);

  if (additional_text_edits != NULL)
    ide_buffer_manager_apply_edits_async (ide_buffer_manager_from_context (ide_object_get_context (IDE_OBJECT (provider))),
                                          IDE_PTR_ARRAY_STEAL_FULL (&additional_text_edits),
                                          NULL,
                                          ide_lsp_completion_provider_apply_additional_edits_cb,
                                          NULL);
}

static gboolean
ide_lsp_completion_provider_is_trigger (GtkSourceCompletionProvider *provider,
                                        const GtkTextIter           *iter,
                                        gunichar                     ch)
{
  static const char * const default_trigger_chars[] = IDE_STRV_INIT (".");
  IdeLspCompletionProvider *self = (IdeLspCompletionProvider *)provider;
  IdeLspCompletionProviderPrivate *priv = ide_lsp_completion_provider_get_instance_private (self);
  const char * const *trigger_chars;

  g_assert (IDE_IS_LSP_COMPLETION_PROVIDER (self));
  g_assert (iter != NULL);

  trigger_chars = priv->trigger_chars ? (const char * const *)priv->trigger_chars : default_trigger_chars;

  for (guint i = 0; trigger_chars[i]; i++)
    {
      if (ch == g_utf8_get_char (trigger_chars[i]))
        return TRUE;
    }

  return FALSE;
}

static void
provider_iface_init (GtkSourceCompletionProviderInterface *iface)
{
  iface->get_priority = ide_lsp_completion_provider_get_priority;
  iface->populate_async = ide_lsp_completion_provider_populate_async;
  iface->populate_finish = ide_lsp_completion_provider_populate_finish;
  iface->refilter = ide_lsp_completion_provider_refilter;
  iface->display = ide_lsp_completion_provider_display;
  iface->activate = ide_lsp_completion_provider_activate;
  iface->is_trigger = ide_lsp_completion_provider_is_trigger;
}
