/* ide-langserv-symbol-resolver.c
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

#define G_LOG_DOMAIN "ide-langserv-symbol-resolver"

#include <jsonrpc-glib.h>

#include "ide-debug.h"

#include "diagnostics/ide-source-location.h"
#include "files/ide-file.h"
#include "langserv/ide-langserv-symbol-resolver.h"

typedef struct
{
  IdeLangservClient *client;
} IdeLangservSymbolResolverPrivate;

static void symbol_resolver_iface_init (IdeSymbolResolverInterface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (IdeLangservSymbolResolver, ide_langserv_symbol_resolver, IDE_TYPE_OBJECT,
                                  G_ADD_PRIVATE (IdeLangservSymbolResolver)
                                  G_IMPLEMENT_INTERFACE (IDE_TYPE_SYMBOL_RESOLVER, symbol_resolver_iface_init))

static void
ide_langserv_symbol_resolver_finalize (GObject *object)
{
  IdeLangservSymbolResolver *self = (IdeLangservSymbolResolver *)object;
  IdeLangservSymbolResolverPrivate *priv = ide_langserv_symbol_resolver_get_instance_private (self);

  g_clear_object (&priv->client);

  G_OBJECT_CLASS (ide_langserv_symbol_resolver_parent_class)->finalize (object);
}

static void
ide_langserv_symbol_resolver_class_init (IdeLangservSymbolResolverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_langserv_symbol_resolver_finalize;
}

static void
ide_langserv_symbol_resolver_init (IdeLangservSymbolResolver *self)
{
}

/**
 * ide_langserv_symbol_resolver_get_client:
 *
 * Gets the client used by the symbol resolver.
 *
 * Returns: (transfer none) (nullable): An #IdeLangservClient or %NULL.
 */
IdeLangservClient *
ide_langserv_symbol_resolver_get_client (IdeLangservSymbolResolver *self)
{
  IdeLangservSymbolResolverPrivate *priv = ide_langserv_symbol_resolver_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_LANGSERV_SYMBOL_RESOLVER (self), NULL);

  return priv->client;
}

void
ide_langserv_symbol_resolver_set_client (IdeLangservSymbolResolver *self,
                                         IdeLangservClient         *client)
{
  IdeLangservSymbolResolverPrivate *priv = ide_langserv_symbol_resolver_get_instance_private (self);

  g_return_if_fail (IDE_IS_LANGSERV_SYMBOL_RESOLVER (self));
  g_return_if_fail (!client || IDE_IS_LANGSERV_CLIENT (client));

  g_set_object (&priv->client, client);
}

static void
ide_langserv_symbol_resolver_definition_cb (GObject      *object,
                                            GAsyncResult *result,
                                            gpointer      user_data)
{
  IdeLangservClient *client = (IdeLangservClient *)object;
  IdeLangservSymbolResolver *self;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(JsonNode) return_value = NULL;
  g_autoptr(IdeSymbol) symbol = NULL;
  g_autoptr(IdeFile) ifile = NULL;
  g_autoptr(GFile) gfile = NULL;
  g_autoptr(IdeSourceLocation) location = NULL;
  JsonNode *location_node = NULL;
  const gchar *uri;
  gboolean success;
  struct {
    gint line;
    gint column;
  } begin, end;

  IDE_ENTRY;

  g_assert (IDE_IS_LANGSERV_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));
  self = g_task_get_source_object (task);
  g_assert (IDE_IS_LANGSERV_SYMBOL_RESOLVER (self));

  if (!ide_langserv_client_call_finish (client, result, &return_value, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  /*
   * We can either get a Location or a Location[] from the peer. We only
   * care about the first node in this case, so extract Location[0] if
   * we need to.
   */
  if (JSON_NODE_HOLDS_ARRAY (return_value))
    {
      JsonArray *ar = json_node_get_array (return_value);
      if (json_array_get_length (ar) > 0)
        {
          JsonNode *first = json_array_get_element (ar, 0);
          if (JSON_NODE_HOLDS_OBJECT (first))
            location_node = first;
        }
    }
  else if (JSON_NODE_HOLDS_OBJECT (return_value))
    location_node = return_value;

  /*
   * If we failed to extract the appropriate node, we can just bail
   * as a failure.
   */
  if (location_node == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_DATA,
                               "Got invalid reply for textDocument/definition");
      IDE_EXIT;
    }

  g_assert (JSON_NODE_HOLDS_OBJECT (location_node));

  success = JCON_EXTRACT (location_node,
    "uri", JCONE_STRING (uri),
    "range", "{",
      "start", "{",
        "line", JCONE_INT (begin.line),
        "character", JCONE_INT (begin.column),
      "}",
      "end", "{",
        "line", JCONE_INT (end.line),
        "character", JCONE_INT (end.column),
      "}",
    "}"
  );

  if (!success)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_DATA,
                               "Got invalid reply for textDocument/definition");
      IDE_EXIT;
    }

  IDE_TRACE_MSG ("Definition location is %s %d:%d",
                 uri, begin.line + 1, begin.column + 1);

  gfile = g_file_new_for_uri (uri);
  ifile = ide_file_new (ide_object_get_context (IDE_OBJECT (self)), gfile);
  location = ide_source_location_new (ifile, begin.line, begin.column, 0);
  symbol = ide_symbol_new ("", IDE_SYMBOL_NONE, IDE_SYMBOL_FLAGS_NONE, location, location, location);

  g_task_return_pointer (task, g_steal_pointer (&symbol), (GDestroyNotify)ide_symbol_unref);

  IDE_EXIT;
}

static void
ide_langserv_symbol_resolver_lookup_symbol_async (IdeSymbolResolver   *resolver,
                                                  IdeSourceLocation   *location,
                                                  GCancellable        *cancellable,
                                                  GAsyncReadyCallback  callback,
                                                  gpointer             user_data)
{
  IdeLangservSymbolResolver *self = (IdeLangservSymbolResolver *)resolver;
  IdeLangservSymbolResolverPrivate *priv = ide_langserv_symbol_resolver_get_instance_private (self);
  g_autoptr(GTask) task = NULL;
  g_autoptr(JsonNode) params = NULL;
  g_autofree gchar *uri = NULL;
  IdeFile *ifile;
  GFile *gfile;
  gint line;
  gint column;

  IDE_ENTRY;

  g_assert (IDE_IS_LANGSERV_SYMBOL_RESOLVER (self));
  g_assert (location != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_langserv_symbol_resolver_lookup_symbol_async);

  if (priv->client == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_CONNECTED,
                               "%s requires a client to resolve symbols",
                               G_OBJECT_TYPE_NAME (self));
      IDE_EXIT;
    }

  if (NULL == (ifile = ide_source_location_get_file (location)) ||
      NULL == (gfile = ide_file_get_file (ifile)))
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               "Cannot resolve symbol, invalid source location");
      IDE_EXIT;
    }

  uri = g_file_get_uri (gfile);
  line = ide_source_location_get_line (location);
  column = ide_source_location_get_line_offset (location);

  params = JCON_NEW (
    "textDocument", "{",
      "uri", JCON_STRING (uri),
    "}",
    "position", "{",
      "line", JCON_INT (line),
      "character", JCON_INT (column),
    "}"
  );

  ide_langserv_client_call_async (priv->client,
                                  "textDocument/definition",
                                  g_steal_pointer (&params),
                                  cancellable,
                                  ide_langserv_symbol_resolver_definition_cb,
                                  g_steal_pointer (&task));

  IDE_EXIT;
}

static IdeSymbol *
ide_langserv_symbol_resolver_lookup_symbol_finish (IdeSymbolResolver  *resolver,
                                                   GAsyncResult       *result,
                                                   GError            **error)
{
  IdeSymbol *ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_LANGSERV_SYMBOL_RESOLVER (resolver), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);

  ret = g_task_propagate_pointer (G_TASK (result), error);

  IDE_RETURN (ret);
}

static void
symbol_resolver_iface_init (IdeSymbolResolverInterface *iface)
{
  iface->lookup_symbol_async = ide_langserv_symbol_resolver_lookup_symbol_async;
  iface->lookup_symbol_finish = ide_langserv_symbol_resolver_lookup_symbol_finish;
}
