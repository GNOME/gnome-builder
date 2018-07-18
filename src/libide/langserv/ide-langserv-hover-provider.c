/* ide-langserv-hover-provider.c
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#define G_LOG_DOMAIN "ide-langserv-hover-provider"

#include <jsonrpc-glib.h>

#include "ide-debug.h"

#include "buffers/ide-buffer.h"
#include "hover/ide-hover-context.h"
#include "hover/ide-hover-provider.h"
#include "langserv/ide-langserv-hover-provider.h"
#include "threading/ide-task.h"
#include "util/ide-marked-content.h"

/**
 * SECTION:ide-langserv-hover-provider
 * @title: IdeLangservHoverProvider
 * @short_description: Interactive hover integration for language servers
 *
 * The #IdeLangservHoverProvider provides integration with language servers
 * that support hover requests. This can display markup in the interactive
 * tooltip that is displayed in the editor.
 *
 * Since: 3.30
 */

typedef struct
{
  IdeLangservClient *client;
  gchar *category;
  gint priority;
} IdeLangservHoverProviderPrivate;

static void hover_provider_iface_init (IdeHoverProviderInterface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (IdeLangservHoverProvider,
                                  ide_langserv_hover_provider,
                                  IDE_TYPE_OBJECT,
                                  G_IMPLEMENT_INTERFACE (IDE_TYPE_HOVER_PROVIDER,
                                                         hover_provider_iface_init))

enum {
  PROP_0,
  PROP_CATEGORY,
  PROP_CLIENT,
  PROP_PRIORITY,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static IdeMarkedContent *
parse_marked_string (GVariant *v)
{
  g_assert (v != NULL);

  /*
   * @v can be (MarkedString | MarkedString[] | MarkupContent)
   *
   * MarkedString is (string | { language: string, value: string })
   */

  return NULL;
}

static void
ide_langserv_hover_provider_dispose (GObject *object)
{
  IdeLangservHoverProvider *self = (IdeLangservHoverProvider *)object;
  IdeLangservHoverProviderPrivate *priv = ide_langserv_hover_provider_get_instance_private (self);

  g_clear_object (&priv->client);

  G_OBJECT_CLASS (ide_langserv_hover_provider_parent_class)->dispose (object);
}

static void
ide_langserv_hover_provider_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  IdeLangservHoverProvider *self = IDE_LANGSERV_HOVER_PROVIDER (object);
  IdeLangservHoverProviderPrivate *priv = ide_langserv_hover_provider_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_CATEGORY:
      g_value_set_string (value, priv->category);
      break;

    case PROP_CLIENT:
      g_value_set_object (value, ide_langserv_hover_provider_get_client (self));
      break;

    case PROP_PRIORITY:
      g_value_set_int (value, priv->priority);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_langserv_hover_provider_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  IdeLangservHoverProvider *self = IDE_LANGSERV_HOVER_PROVIDER (object);
  IdeLangservHoverProviderPrivate *priv = ide_langserv_hover_provider_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_CATEGORY:
      g_free (priv->category);
      priv->category = g_value_dup_string (value);
      break;

    case PROP_CLIENT:
      ide_langserv_hover_provider_set_client (self, g_value_get_object (value));
      break;

    case PROP_PRIORITY:
      priv->priority = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_langserv_hover_provider_class_init (IdeLangservHoverProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_langserv_hover_provider_dispose;
  object_class->get_property = ide_langserv_hover_provider_get_property;
  object_class->set_property = ide_langserv_hover_provider_set_property;

  /**
   * IdeLangservHoverProvider:client:
   *
   * The "client" property is the #IdeLangservClient that should be used to
   * communicate with the Language Server peer process.
   *
   * Since: 3.30
   */
  properties [PROP_CLIENT] =
    g_param_spec_object ("client",
                         "Client",
                         "The client to communicate with",
                         IDE_TYPE_LANGSERV_CLIENT,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeLangservHoverProvider:category:
   *
   * The "category" property is the category name to use when displaying
   * the hover contents.
   *
   * Since: 3.30
   */
  properties [PROP_CATEGORY] =
    g_param_spec_string ("category",
                         "Category",
                         "The category to display in the hover popover",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_PRIORITY] =
    g_param_spec_int ("priority",
                      "Priority",
                      "Priority for hover content",
                      G_MININT,
                      G_MAXINT,
                      0,
                      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_langserv_hover_provider_init (IdeLangservHoverProvider *self)
{
}

static void
ide_langserv_hover_provider_hover_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  IdeLangservClient *client = (IdeLangservClient *)object;
  IdeLangservHoverProvider *self;
  IdeLangservHoverProviderPrivate *priv;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GVariant) contents = NULL;
  g_autoptr(IdeMarkedContent) marked = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeHoverContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_LANGSERV_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  priv = ide_langserv_hover_provider_get_instance_private (self);

  if (!ide_langserv_client_call_finish (client, result, &reply, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  if (!(contents = g_variant_lookup_value (reply, "contents", NULL)))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVALID_DATA,
                                 "Expected 'contents' in reply");
      IDE_EXIT;
    }

  if (!(marked = parse_marked_string (contents)))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVALID_DATA,
                                 "Unusable contents from language server");
      IDE_EXIT;
    }

  context = ide_task_get_task_data (task);

  g_assert (context != NULL);
  g_assert (IDE_IS_HOVER_CONTEXT (context));

  ide_hover_context_add_content (context,
                                 priv->priority,
                                 priv->category,
                                 marked);

  ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
ide_langserv_hover_provider_hover_async (IdeHoverProvider    *provider,
                                         IdeHoverContext     *context,
                                         const GtkTextIter   *iter,
                                         GCancellable        *cancellable,
                                         GAsyncReadyCallback  callback,
                                         gpointer             user_data)
{
  IdeLangservHoverProvider *self = (IdeLangservHoverProvider *)provider;
  IdeLangservHoverProviderPrivate *priv = ide_langserv_hover_provider_get_instance_private (self);
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GVariant) params = NULL;
  g_autofree gchar *uri = NULL;
  IdeBuffer *buffer;
  gint line;
  gint column;

  IDE_ENTRY;

  g_assert (IDE_IS_LANGSERV_HOVER_PROVIDER (provider));
  g_assert (IDE_IS_HOVER_CONTEXT (context));
  g_assert (iter != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_langserv_hover_provider_hover_async);

  if (priv->client == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_CONNECTED,
                                 "No client to deliver request");
      return;
    }

  buffer = IDE_BUFFER (gtk_text_iter_get_buffer (iter));
  uri = ide_buffer_get_uri (buffer);
  line = gtk_text_iter_get_line (iter);
  column = gtk_text_iter_get_line_offset (iter);

  params = JSONRPC_MESSAGE_NEW (
    "textDocument", "{",
      "uri", JSONRPC_MESSAGE_PUT_STRING (uri),
    "}",
    "position", "{",
      "line", JSONRPC_MESSAGE_PUT_INT32 (line),
      "character", JSONRPC_MESSAGE_PUT_INT32 (column),
    "}"
  );

  ide_langserv_client_call_async (priv->client,
                                  "textDocument/hover",
                                  params,
                                  cancellable,
                                  ide_langserv_hover_provider_hover_cb,
                                  g_steal_pointer (&task));

  IDE_EXIT;
}

static gboolean
ide_langserv_hover_provider_hover_finish (IdeHoverProvider  *provider,
                                          GAsyncResult      *result,
                                          GError           **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_LANGSERV_HOVER_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
hover_provider_iface_init (IdeHoverProviderInterface *iface)
{
  iface->hover_async = ide_langserv_hover_provider_hover_async;
  iface->hover_finish = ide_langserv_hover_provider_hover_finish;
}

/**
 * ide_langserv_hover_provider_get_client:
 * @self: an #IdeLangservHoverProvider
 *
 * Gets the client that is used for communication.
 *
 * Returns: (transfer none) (nullable): an #IdeLangservClient or %NULL
 *
 * Since: 3.30
 */
IdeLangservClient *
ide_langserv_hover_provider_get_client (IdeLangservHoverProvider *self)
{
  IdeLangservHoverProviderPrivate *priv = ide_langserv_hover_provider_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_LANGSERV_HOVER_PROVIDER (self), NULL);

  return priv->client;
}

/**
 * ide_langserv_hover_provider_set_client:
 * @self: an #IdeLangservHoverProvider
 * @client: an #IdeLangservClient
 *
 * Sets the client to be used to query for hover information.
 *
 * Since: 3.30
 */
void
ide_langserv_hover_provider_set_client (IdeLangservHoverProvider *self,
                                        IdeLangservClient        *client)
{
  IdeLangservHoverProviderPrivate *priv = ide_langserv_hover_provider_get_instance_private (self);

  g_return_if_fail (IDE_IS_LANGSERV_HOVER_PROVIDER (self));
  g_return_if_fail (!client || IDE_IS_LANGSERV_CLIENT (client));

  if (g_set_object (&priv->client, client))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CLIENT]);
}
