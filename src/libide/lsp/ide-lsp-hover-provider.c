/* ide-lsp-hover-provider.c
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

#define G_LOG_DOMAIN "ide-lsp-hover-provider"

#include "config.h"

#include <jsonrpc-glib.h>
#include <libide-code.h>
#include <libide-sourceview.h>
#include <libide-threading.h>

#include "ide-lsp-hover-provider.h"

/**
 * SECTION:ide-lsp-hover-provider
 * @title: IdeLspHoverProvider
 * @short_description: Interactive hover integration for language servers
 *
 * The #IdeLspHoverProvider provides integration with language servers
 * that support hover requests. This can display markup in the interactive
 * tooltip that is displayed in the editor.
 *
 * Since: 3.30
 */

typedef struct
{
  IdeLspClient *client;
  gchar *category;
  gint priority;
} IdeLspHoverProviderPrivate;

static void hover_provider_iface_init (IdeHoverProviderInterface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (IdeLspHoverProvider,
                                  ide_lsp_hover_provider,
                                  IDE_TYPE_OBJECT,
                                  G_ADD_PRIVATE (IdeLspHoverProvider)
                                  G_IMPLEMENT_INTERFACE (IDE_TYPE_HOVER_PROVIDER, hover_provider_iface_init))

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
  g_autoptr(GString) gstr = g_string_new (NULL);
  g_autoptr(GVariant) child = NULL;
  GVariant *item;
  GVariantIter iter;

  g_assert (v != NULL);

  /*
   * @v can be (MarkedString | MarkedString[] | MarkupContent)
   *
   * MarkedString is (string | { language: string, value: string })
   */

  if (g_variant_is_of_type (v, G_VARIANT_TYPE_STRING))
    {
      gsize len = 0;
      const gchar *str = g_variant_get_string (v, &len);

      if (str && *str == '\0')
        return NULL;

      return ide_marked_content_new_from_data (str, len, IDE_MARKED_KIND_PLAINTEXT);
    }

  if (g_variant_is_of_type (v, G_VARIANT_TYPE_VARIANT))
    v = child = g_variant_get_variant (v);

  g_variant_iter_init (&iter, v);

  if ((item = g_variant_iter_next_value (&iter)))
    {
      GVariant *asv = item;
      g_autoptr(GVariant) child2 = NULL;

      if (g_variant_is_of_type (item, G_VARIANT_TYPE_VARIANT))
        asv = child2 = g_variant_get_variant (item);

      if (g_variant_is_of_type (asv, G_VARIANT_TYPE_STRING))
        g_string_append (gstr, g_variant_get_string (asv, NULL));
      else if (g_variant_is_of_type (asv, G_VARIANT_TYPE_VARDICT))
        {
          const gchar *lang = "";
          const gchar *value = "";

          g_variant_lookup (asv, "language", "&s", &lang);
          g_variant_lookup (asv, "value", "&s", &value);

#if 0
          if (!ide_str_empty0 (lang) && !ide_str_empty0 (value))
            g_string_append_printf (str, "```%s\n%s\n```", lang, value);
          else if (!ide_str_empty0 (value))
            g_string_append (str, value);
#else
          if (!ide_str_empty0 (value))
            g_string_append_printf (gstr, "```\n%s\n```", value);
#endif
        }

      g_variant_unref (item);
    }

  if (gstr->len)
    return ide_marked_content_new_from_data (gstr->str, gstr->len, IDE_MARKED_KIND_MARKDOWN);

  return NULL;
}

static void
ide_lsp_hover_provider_dispose (GObject *object)
{
  IdeLspHoverProvider *self = (IdeLspHoverProvider *)object;
  IdeLspHoverProviderPrivate *priv = ide_lsp_hover_provider_get_instance_private (self);

  IDE_ENTRY;

  g_clear_object (&priv->client);
  g_clear_pointer (&priv->category, g_free);

  G_OBJECT_CLASS (ide_lsp_hover_provider_parent_class)->dispose (object);

  IDE_EXIT;
}

static void
ide_lsp_hover_provider_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  IdeLspHoverProvider *self = IDE_LSP_HOVER_PROVIDER (object);
  IdeLspHoverProviderPrivate *priv = ide_lsp_hover_provider_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_CATEGORY:
      g_value_set_string (value, priv->category);
      break;

    case PROP_CLIENT:
      g_value_set_object (value, ide_lsp_hover_provider_get_client (self));
      break;

    case PROP_PRIORITY:
      g_value_set_int (value, priv->priority);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_lsp_hover_provider_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  IdeLspHoverProvider *self = IDE_LSP_HOVER_PROVIDER (object);
  IdeLspHoverProviderPrivate *priv = ide_lsp_hover_provider_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_CATEGORY:
      g_free (priv->category);
      priv->category = g_value_dup_string (value);
      break;

    case PROP_CLIENT:
      ide_lsp_hover_provider_set_client (self, g_value_get_object (value));
      break;

    case PROP_PRIORITY:
      priv->priority = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_lsp_hover_provider_class_init (IdeLspHoverProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_lsp_hover_provider_dispose;
  object_class->get_property = ide_lsp_hover_provider_get_property;
  object_class->set_property = ide_lsp_hover_provider_set_property;

  /**
   * IdeLspHoverProvider:client:
   *
   * The "client" property is the #IdeLspClient that should be used to
   * communicate with the Language Server peer process.
   *
   * Since: 3.30
   */
  properties [PROP_CLIENT] =
    g_param_spec_object ("client",
                         "Client",
                         "The client to communicate with",
                         IDE_TYPE_LSP_CLIENT,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeLspHoverProvider:category:
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
ide_lsp_hover_provider_init (IdeLspHoverProvider *self)
{
}

static void
ide_lsp_hover_provider_hover_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  IdeLspClient *client = (IdeLspClient *)object;
  IdeLspHoverProvider *self;
  IdeLspHoverProviderPrivate *priv;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GVariant) contents = NULL;
  g_autoptr(IdeMarkedContent) marked = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeHoverContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_LSP_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  priv = ide_lsp_hover_provider_get_instance_private (self);

  g_assert (IDE_IS_LSP_HOVER_PROVIDER (self));

  if (!ide_lsp_client_call_finish (client, result, &reply, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  if (!g_variant_is_of_type (reply, G_VARIANT_TYPE_VARDICT) ||
      !(contents = g_variant_lookup_value (reply, "contents", NULL)))
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
ide_lsp_hover_provider_hover_async (IdeHoverProvider    *provider,
                                         IdeHoverContext     *context,
                                         const GtkTextIter   *iter,
                                         GCancellable        *cancellable,
                                         GAsyncReadyCallback  callback,
                                         gpointer             user_data)
{
  IdeLspHoverProvider *self = (IdeLspHoverProvider *)provider;
  IdeLspHoverProviderPrivate *priv = ide_lsp_hover_provider_get_instance_private (self);
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GVariant) params = NULL;
  g_autofree gchar *uri = NULL;
  IdeBuffer *buffer;
  gint line;
  gint column;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_LSP_HOVER_PROVIDER (self));
  g_assert (IDE_IS_HOVER_CONTEXT (context));
  g_assert (iter != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_task_data (task, g_object_ref (context), g_object_unref);
  ide_task_set_source_tag (task, ide_lsp_hover_provider_hover_async);

  if (priv->client == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_CONNECTED,
                                 "No client to deliver request");
      return;
    }

  buffer = IDE_BUFFER (gtk_text_iter_get_buffer (iter));
  uri = ide_buffer_dup_uri (buffer);
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

  g_assert (IDE_IS_LSP_CLIENT (priv->client));

  ide_lsp_client_call_async (priv->client,
                                  "textDocument/hover",
                                  params,
                                  cancellable,
                                  ide_lsp_hover_provider_hover_cb,
                                  g_steal_pointer (&task));

  IDE_EXIT;
}

static gboolean
ide_lsp_hover_provider_hover_finish (IdeHoverProvider  *provider,
                                          GAsyncResult      *result,
                                          GError           **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_LSP_HOVER_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
ide_lsp_hover_provider_real_load (IdeHoverProvider *provider,
                                       IdeSourceView    *view)
{
  IdeLspHoverProvider *self = (IdeLspHoverProvider *)provider;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_LSP_HOVER_PROVIDER (self));

  if (IDE_LSP_HOVER_PROVIDER_GET_CLASS (self)->prepare)
    IDE_LSP_HOVER_PROVIDER_GET_CLASS (self)->prepare (self);
}

static void
hover_provider_iface_init (IdeHoverProviderInterface *iface)
{
  iface->load = ide_lsp_hover_provider_real_load;
  iface->hover_async = ide_lsp_hover_provider_hover_async;
  iface->hover_finish = ide_lsp_hover_provider_hover_finish;
}

/**
 * ide_lsp_hover_provider_get_client:
 * @self: an #IdeLspHoverProvider
 *
 * Gets the client that is used for communication.
 *
 * Returns: (transfer none) (nullable): an #IdeLspClient or %NULL
 *
 * Since: 3.30
 */
IdeLspClient *
ide_lsp_hover_provider_get_client (IdeLspHoverProvider *self)
{
  IdeLspHoverProviderPrivate *priv = ide_lsp_hover_provider_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_LSP_HOVER_PROVIDER (self), NULL);

  return priv->client;
}

/**
 * ide_lsp_hover_provider_set_client:
 * @self: an #IdeLspHoverProvider
 * @client: an #IdeLspClient
 *
 * Sets the client to be used to query for hover information.
 *
 * Since: 3.30
 */
void
ide_lsp_hover_provider_set_client (IdeLspHoverProvider *self,
                                        IdeLspClient        *client)
{
  IdeLspHoverProviderPrivate *priv = ide_lsp_hover_provider_get_instance_private (self);

  g_return_if_fail (IDE_IS_LSP_HOVER_PROVIDER (self));
  g_return_if_fail (!client || IDE_IS_LSP_CLIENT (client));

  if (g_set_object (&priv->client, client))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CLIENT]);
}
