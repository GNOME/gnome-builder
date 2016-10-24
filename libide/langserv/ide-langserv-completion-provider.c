/* ide-langserv-completion-provider.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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
 */

#define G_LOG_DOMAIN "ide-langserv-completion-provider"

#include <jsonrpc-glib.h>

#include "ide-debug.h"

#include "buffers/ide-buffer.h"
#include "langserv/ide-langserv-completion-provider.h"

typedef struct
{
  IdeLangservClient *client;
} IdeLangservCompletionProviderPrivate;

typedef struct
{
  IdeLangservCompletionProvider *self;
  GtkSourceCompletionContext    *context;
} CompletionState;

static void source_completion_provider_iface_init (GtkSourceCompletionProviderIface *iface);

G_DEFINE_TYPE_EXTENDED (IdeLangservCompletionProvider, ide_langserv_completion_provider, IDE_TYPE_OBJECT, 0,
                        G_ADD_PRIVATE (IdeLangservCompletionProvider)
                        G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROVIDER, source_completion_provider_iface_init)
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_COMPLETION_PROVIDER, NULL))

static void
completion_state_free (CompletionState *state)
{
  g_clear_object (&state->self);
  g_clear_object (&state->context);
  g_slice_free (CompletionState, state);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (CompletionState, completion_state_free);

CompletionState *
completion_state_new (IdeLangservCompletionProvider *self,
                      GtkSourceCompletionContext    *context)
{
  CompletionState *state;

  state = g_slice_new0 (CompletionState);
  state->self = g_object_ref (self);
  state->context = g_object_ref (context);

  return state;
}

static void
ide_langserv_completion_provider_finalize (GObject *object)
{
  IdeLangservCompletionProvider *self = (IdeLangservCompletionProvider *)object;
  IdeLangservCompletionProviderPrivate *priv = ide_langserv_completion_provider_get_instance_private (self);

  g_clear_object (&priv->client);

  G_OBJECT_CLASS (ide_langserv_completion_provider_parent_class)->finalize (object);
}

static void
ide_langserv_completion_provider_class_init (IdeLangservCompletionProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_langserv_completion_provider_finalize;
}

static void
ide_langserv_completion_provider_init (IdeLangservCompletionProvider *self)
{
}

/**
 * ide_langserv_completion_provider_get_client:
 * @self: An #IdeLangservCompletionProvider
 *
 * Gets the client for the completion provider.
 *
 * Returns: (transfer none) (nullable): An #IdeLangservClient or %NULL
 */
IdeLangservClient *
ide_langserv_completion_provider_get_client (IdeLangservCompletionProvider *self)
{
  IdeLangservCompletionProviderPrivate *priv = ide_langserv_completion_provider_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_LANGSERV_COMPLETION_PROVIDER (self), NULL);

  return priv->client;
}

void
ide_langserv_completion_provider_set_client (IdeLangservCompletionProvider *self,
                                             IdeLangservClient             *client)
{
  IdeLangservCompletionProviderPrivate *priv = ide_langserv_completion_provider_get_instance_private (self);

  g_return_if_fail (IDE_IS_LANGSERV_COMPLETION_PROVIDER (self));
  g_return_if_fail (!client || IDE_IS_LANGSERV_CLIENT (client));

  g_set_object (&priv->client, client);
}

static gchar *
ide_langserv_completion_provider_get_name (GtkSourceCompletionProvider *provider)
{
  return g_strdup ("Rust");
}

static gint
ide_langserv_completion_provider_get_priority (GtkSourceCompletionProvider *provider)
{
  return IDE_LANGSERV_COMPLETION_PROVIDER_PRIORITY;
}

static gboolean
ide_langserv_completion_provider_match (GtkSourceCompletionProvider *provider,
                                        GtkSourceCompletionContext  *context)
{
  GtkSourceCompletionActivation activation;
  GtkTextIter iter;

  g_assert (IDE_IS_LANGSERV_COMPLETION_PROVIDER (provider));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));

  if (!gtk_source_completion_context_get_iter (context, &iter))
    return FALSE;

  activation = gtk_source_completion_context_get_activation (context);

  if (activation == GTK_SOURCE_COMPLETION_ACTIVATION_INTERACTIVE)
    {
      if (gtk_text_iter_starts_line (&iter) ||
          !gtk_text_iter_backward_char (&iter) ||
          g_unichar_isspace (gtk_text_iter_get_char (&iter)))
        return FALSE;
    }

  if (ide_completion_provider_context_in_comment (context))
    return FALSE;

  return TRUE;
}

static void
ide_langserv_completion_provider_complete_cb (GObject      *object,
                                              GAsyncResult *result,
                                              gpointer      user_data)
{
  IdeLangservClient *client = (IdeLangservClient *)object;
  g_autoptr(CompletionState) state = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(JsonNode) return_value = NULL;
  GList *list = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_LANGSERV_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (state != NULL);
  g_assert (IDE_IS_LANGSERV_COMPLETION_PROVIDER (state->self));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (state->context));

  if (!ide_langserv_client_call_finish (client, result, &return_value, &error))
    {
      g_message ("%s", error->message);
      IDE_GOTO (failure);
    }

  /*
   * TODO: We will want to make a much more optimized version of this using
   *       the other completion result work we've done.
   */

  if (JSON_NODE_HOLDS_ARRAY (return_value))
    {
      JsonArray *array = json_node_get_array (return_value);
      guint length = json_array_get_length (array);

      for (guint i = length; i > 0; i--)
        {
          JsonNode *node = json_array_get_element (array, i - 1);
          g_autoptr(GtkSourceCompletionItem) item = NULL;
          g_autofree gchar *full_label = NULL;
          const gchar *label;
          const gchar *detail;
          gboolean success;

          success = JCON_EXTRACT (node,
            "label", JCONE_STRING (label),
            "detail", JCONE_STRING (detail)
          );

          if (!success)
            continue;

          if (label != NULL && detail != NULL)
            full_label = g_strdup_printf ("%s : %s", label, detail);
          else
            full_label = g_strdup (label);

          item = gtk_source_completion_item_new (full_label, label, NULL, NULL);
          list = g_list_prepend (list, g_steal_pointer (&item));
        }
    }

failure:
  gtk_source_completion_context_add_proposals (state->context,
                                               GTK_SOURCE_COMPLETION_PROVIDER (state->self),
                                               list,
                                               TRUE);

  g_list_free_full (list, g_object_unref);

  IDE_EXIT;
}

static void
ide_langserv_completion_provider_populate (GtkSourceCompletionProvider *provider,
                                           GtkSourceCompletionContext  *context)
{
  IdeLangservCompletionProvider *self = (IdeLangservCompletionProvider *)provider;
  IdeLangservCompletionProviderPrivate *priv = ide_langserv_completion_provider_get_instance_private (self);
  g_autoptr(JsonNode) params = NULL;
  g_autoptr(GCancellable) cancellable = NULL;
  g_autoptr(CompletionState) state = NULL;
  g_autofree gchar *uri = NULL;
  GtkTextIter iter;
  IdeBuffer *buffer;
  gint line;
  gint column;

  IDE_ENTRY;

  g_assert (IDE_IS_LANGSERV_COMPLETION_PROVIDER (self));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));

  if (priv->client == NULL)
    {
      IDE_TRACE_MSG ("No client set, cannot provide proposals");
      gtk_source_completion_context_add_proposals (context, provider, NULL, TRUE);
      IDE_EXIT;
    }

  gtk_source_completion_context_get_iter (context, &iter);

  buffer = IDE_BUFFER (gtk_text_iter_get_buffer (&iter));
  uri = ide_buffer_get_uri (buffer);

  line = gtk_text_iter_get_line (&iter);
  column = gtk_text_iter_get_line_offset (&iter);

  params = JCON_NEW (
    "textDocument", "{",
      "uri", JCON_STRING (uri),
    "}",
    "position", "{",
      "line", JCON_INT (line),
      "character", JCON_INT (column),
    "}"
  );

  cancellable = g_cancellable_new ();

  g_signal_connect_data (context,
                         "cancelled",
                         G_CALLBACK (g_cancellable_cancel),
                         g_object_ref (cancellable),
                         (GClosureNotify)g_object_unref,
                         G_CONNECT_SWAPPED);

  state = completion_state_new (self, context);

  ide_langserv_client_call_async (priv->client,
                                  "textDocument/completion",
                                  g_steal_pointer (&params),
                                  g_steal_pointer (&cancellable),
                                  ide_langserv_completion_provider_complete_cb,
                                  g_steal_pointer (&state));

  IDE_EXIT;
}

static void
source_completion_provider_iface_init (GtkSourceCompletionProviderIface *iface)
{
  iface->get_name = ide_langserv_completion_provider_get_name;
  iface->get_priority = ide_langserv_completion_provider_get_priority;
  iface->match = ide_langserv_completion_provider_match;
  iface->populate = ide_langserv_completion_provider_populate;
}
