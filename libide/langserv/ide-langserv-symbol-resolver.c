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
#include "diagnostics/ide-source-range.h"
#include "files/ide-file.h"
#include "langserv/ide-langserv-symbol-node.h"
#include "langserv/ide-langserv-symbol-node-private.h"
#include "langserv/ide-langserv-symbol-resolver.h"
#include "langserv/ide-langserv-symbol-tree.h"
#include "langserv/ide-langserv-symbol-tree-private.h"

typedef struct
{
  IdeLangservClient *client;
} IdeLangservSymbolResolverPrivate;

static void symbol_resolver_iface_init (IdeSymbolResolverInterface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (IdeLangservSymbolResolver, ide_langserv_symbol_resolver, IDE_TYPE_OBJECT,
                                  G_ADD_PRIVATE (IdeLangservSymbolResolver)
                                  G_IMPLEMENT_INTERFACE (IDE_TYPE_SYMBOL_RESOLVER, symbol_resolver_iface_init))

enum {
  PROP_0,
  PROP_CLIENT,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
ide_langserv_symbol_resolver_finalize (GObject *object)
{
  IdeLangservSymbolResolver *self = (IdeLangservSymbolResolver *)object;
  IdeLangservSymbolResolverPrivate *priv = ide_langserv_symbol_resolver_get_instance_private (self);

  g_clear_object (&priv->client);

  G_OBJECT_CLASS (ide_langserv_symbol_resolver_parent_class)->finalize (object);
}

static void
ide_langserv_symbol_resolver_get_property (GObject    *object,
                                           guint       prop_id,
                                           GValue     *value,
                                           GParamSpec *pspec)
{
  IdeLangservSymbolResolver *self = IDE_LANGSERV_SYMBOL_RESOLVER (object);

  switch (prop_id)
    {
    case PROP_CLIENT:
      g_value_set_object (value, ide_langserv_symbol_resolver_get_client (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_langserv_symbol_resolver_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  IdeLangservSymbolResolver *self = IDE_LANGSERV_SYMBOL_RESOLVER (object);

  switch (prop_id)
    {
    case PROP_CLIENT:
      ide_langserv_symbol_resolver_set_client (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_langserv_symbol_resolver_class_init (IdeLangservSymbolResolverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_langserv_symbol_resolver_finalize;
  object_class->get_property = ide_langserv_symbol_resolver_get_property;
  object_class->set_property = ide_langserv_symbol_resolver_set_property;

  properties [PROP_CLIENT] =
    g_param_spec_object ("client",
                         "Client",
                         "The Language Server client",
                         IDE_TYPE_LANGSERV_CLIENT,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
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

  if (g_set_object (&priv->client, client))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CLIENT]);
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
  g_autoptr(GVariant) return_value = NULL;
  g_autoptr(IdeSymbol) symbol = NULL;
  g_autoptr(IdeFile) ifile = NULL;
  g_autoptr(GFile) gfile = NULL;
  g_autoptr(IdeSourceLocation) location = NULL;
  g_autoptr(GVariant) variant = NULL;
  GVariantIter iter;
  const gchar *uri;
  struct {
    gint64 line;
    gint64 column;
  } begin, end;
  gboolean success = FALSE;

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

#if 0
  {
    g_autofree gchar *str = g_variant_print (return_value, TRUE);
    IDE_TRACE_MSG ("Got reply: %s", str);
  }
#endif

  g_variant_iter_init (&iter, return_value);

  if (g_variant_iter_next (&iter, "v", &variant))
    {
      success = JSONRPC_MESSAGE_PARSE (variant,
        "uri", JSONRPC_MESSAGE_GET_STRING (&uri),
        "range", "{",
          "start", "{",
            "line", JSONRPC_MESSAGE_GET_INT64 (&begin.line),
            "character", JSONRPC_MESSAGE_GET_INT64 (&begin.column),
          "}",
          "end", "{",
            "line", JSONRPC_MESSAGE_GET_INT64 (&end.line),
            "character", JSONRPC_MESSAGE_GET_INT64 (&end.column),
          "}",
        "}"
      );
    }

  if (!success)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_DATA,
                               "Got invalid reply for textDocument/definition");
      IDE_EXIT;
    }

  IDE_TRACE_MSG ("Definition location is %s %d:%d",
                 uri, (gint)begin.line + 1, (gint)begin.column + 1);

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
  g_autoptr(GVariant) params = NULL;
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
ide_langserv_symbol_resolver_document_symbol_cb (GObject      *object,
                                                 GAsyncResult *result,
                                                 gpointer      user_data)
{
  IdeLangservClient *client = (IdeLangservClient *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(IdeLangservSymbolTree) tree = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) return_value = NULL;
  g_autoptr(GPtrArray) symbols = NULL;
  GVariantIter iter;
  GVariant *node;

  IDE_ENTRY;

  g_assert (IDE_IS_LANGSERV_CLIENT (client));
  g_assert (G_IS_TASK (task));

  if (!ide_langserv_client_call_finish (client, result, &return_value, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  if (!g_variant_is_of_type (return_value, G_VARIANT_TYPE ("av")))
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_DATA,
                               "Invalid result for textDocument/documentSymbol");
      IDE_EXIT;
    }

  symbols = g_ptr_array_new_with_free_func (g_object_unref);

  g_variant_iter_init (&iter, return_value);

  while (g_variant_iter_loop (&iter, "v", &node))
    {
      g_autoptr(IdeLangservSymbolNode) symbol = NULL;
      g_autoptr(GFile) file = NULL;
      const gchar *name = NULL;
      const gchar *container_name = NULL;
      const gchar *uri = NULL;
      gboolean success;
      gint64 kind = -1;
      struct {
        gint64 line;
        gint64 column;
      } begin, end;

      /* Mandatory fields */
      success = JSONRPC_MESSAGE_PARSE (node,
        "name", JSONRPC_MESSAGE_GET_STRING (&name),
        "kind", JSONRPC_MESSAGE_GET_INT64 (&kind),
        "location", "{",
          "uri", JSONRPC_MESSAGE_GET_STRING (&uri),
          "range", "{",
            "start", "{",
              "line", JSONRPC_MESSAGE_GET_INT64 (&begin.line),
              "character", JSONRPC_MESSAGE_GET_INT64 (&begin.column),
            "}",
            "end", "{",
              "line", JSONRPC_MESSAGE_GET_INT64 (&end.line),
              "character", JSONRPC_MESSAGE_GET_INT64 (&end.column),
            "}",
          "}",
        "}"
      );

      if (!success)
        {
          IDE_TRACE_MSG ("Failed to parse reply from language server");
          continue;
        }

      /* Optional fields */
      JSONRPC_MESSAGE_PARSE (node, "containerName", JSONRPC_MESSAGE_GET_STRING (&container_name));

      file = g_file_new_for_uri (uri);

      symbol = ide_langserv_symbol_node_new (file, name, container_name, kind,
                                             begin.line, begin.column,
                                             end.line, end.column);

      g_ptr_array_add (symbols, g_steal_pointer (&symbol));
    }

  tree = ide_langserv_symbol_tree_new (g_steal_pointer (&symbols));

  g_task_return_pointer (task, g_steal_pointer (&tree), g_object_unref);

  IDE_EXIT;
}

static void
ide_langserv_symbol_resolver_get_symbol_tree_async (IdeSymbolResolver   *resolver,
                                                    GFile               *file,
                                                    IdeBuffer           *buffer,
                                                    GCancellable        *cancellable,
                                                    GAsyncReadyCallback  callback,
                                                    gpointer             user_data)
{
  IdeLangservSymbolResolver *self = (IdeLangservSymbolResolver *)resolver;
  IdeLangservSymbolResolverPrivate *priv = ide_langserv_symbol_resolver_get_instance_private (self);
  g_autoptr(GTask) task = NULL;
  g_autoptr(GVariant) params = NULL;
  g_autofree gchar *uri = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_LANGSERV_SYMBOL_RESOLVER (self));
  g_assert (G_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_langserv_symbol_resolver_get_symbol_tree_async);

  if (priv->client == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_CONNECTED,
                               "Cannot query language server, not connected");
      IDE_EXIT;
    }

  uri = g_file_get_uri (file);

  params = JSONRPC_MESSAGE_NEW (
    "textDocument", "{",
      "uri", JSONRPC_MESSAGE_PUT_STRING (uri),
    "}"
  );

  ide_langserv_client_call_async (priv->client,
                                  "textDocument/documentSymbol",
                                  g_steal_pointer (&params),
                                  cancellable,
                                  ide_langserv_symbol_resolver_document_symbol_cb,
                                  g_steal_pointer (&task));

  IDE_EXIT;
}

static IdeSymbolTree *
ide_langserv_symbol_resolver_get_symbol_tree_finish (IdeSymbolResolver  *resolver,
                                                     GAsyncResult       *result,
                                                     GError            **error)
{
  IdeSymbolTree *ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_LANGSERV_SYMBOL_RESOLVER (resolver), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);

  ret = g_task_propagate_pointer (G_TASK (result), error);

  IDE_RETURN (ret);
}

static void
ide_langserv_symbol_resolver_find_references_cb (GObject      *object,
                                                 GAsyncResult *result,
                                                 gpointer      user_data)
{
  IdeLangservClient *client = (IdeLangservClient *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GPtrArray) references = NULL;
  g_autoptr(GError) error = NULL;
  IdeLangservSymbolResolver *self;
  GVariant *locationv;
  GVariantIter iter;

  IDE_ENTRY;

  g_assert (IDE_IS_LANGSERV_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);

  if (!ide_langserv_client_call_finish (client, result, &reply, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  if (!g_variant_is_of_type (reply, G_VARIANT_TYPE ("av")))
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_DATA,
                               "Invalid reply type from peer: %s",
                               g_variant_get_type_string (reply));
      IDE_EXIT;
    }

  references = g_ptr_array_new_with_free_func ((GDestroyNotify)ide_source_range_unref);

  g_variant_iter_init (&iter, reply);

  while (g_variant_iter_loop (&iter, "v", &locationv))
    {
      g_autoptr(IdeSourceLocation) begin_loc = NULL;
      g_autoptr(IdeSourceLocation) end_loc = NULL;
      g_autoptr(IdeSourceRange) range = NULL;
      g_autoptr(IdeFile) ifile = NULL;
      const gchar *uri = NULL;
      GFile *gfile;
      gboolean success;
      struct {
        gint64 line;
        gint64 line_offset;
      } begin, end;

      success = JSONRPC_MESSAGE_PARSE (locationv,
        "uri", JSONRPC_MESSAGE_GET_STRING (&uri),
        "range", "{",
          "start", "{",
            "line", JSONRPC_MESSAGE_GET_INT64 (&begin.line),
            "character", JSONRPC_MESSAGE_GET_INT64 (&begin.line_offset),
          "}",
          "end", "{",
            "line", JSONRPC_MESSAGE_GET_INT64 (&end.line),
            "character", JSONRPC_MESSAGE_GET_INT64 (&end.line_offset),
          "}",
        "}"
      );

      if (!success)
        {
          g_task_return_new_error (task,
                                   G_IO_ERROR,
                                   G_IO_ERROR_INVALID_DATA,
                                   "Failed to parse location object");
          IDE_EXIT;
        }

      gfile = g_file_new_for_uri (uri);
      ifile = ide_file_new (ide_object_get_context (IDE_OBJECT (self)), gfile);

      begin_loc = ide_source_location_new (ifile, begin.line, begin.line_offset, 0);
      end_loc = ide_source_location_new (ifile, end.line, end.line_offset, 0);
      range = ide_source_range_new (begin_loc, end_loc);

      g_ptr_array_add (references, g_steal_pointer (&range));
    }

  g_task_return_pointer (task, g_steal_pointer (&references), (GDestroyNotify)g_ptr_array_unref);

  IDE_EXIT;
}

static void
ide_langserv_symbol_resolver_find_references_async (IdeSymbolResolver   *resolver,
                                                    IdeSourceLocation   *location,
                                                    GCancellable        *cancellable,
                                                    GAsyncReadyCallback  callback,
                                                    gpointer             user_data)
{
  IdeLangservSymbolResolver *self = (IdeLangservSymbolResolver *)resolver;
  IdeLangservSymbolResolverPrivate *priv = ide_langserv_symbol_resolver_get_instance_private (self);
  g_autoptr(GTask) task = NULL;
  g_autoptr(GVariant) params = NULL;
  g_autofree gchar *uri = NULL;
  const gchar *language_id;
  IdeFile *file;
  GFile *gfile;
  guint line;
  guint line_offset;

  IDE_ENTRY;

  g_assert (IDE_IS_LANGSERV_SYMBOL_RESOLVER (self));
  g_assert (location != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_langserv_symbol_resolver_find_references_async);

  file = ide_source_location_get_file (location);
  gfile = ide_file_get_file (file);
  uri = g_file_get_uri (gfile);

  line = ide_source_location_get_line (location);
  line_offset = ide_source_location_get_line_offset (location);

  language_id = ide_file_get_language_id (file);
  if (language_id == NULL)
    language_id = "plain";

  params = JSONRPC_MESSAGE_NEW (
    "textDocument", "{",
      "uri", JSONRPC_MESSAGE_PUT_STRING (uri),
      "languageId", JSONRPC_MESSAGE_PUT_STRING (language_id),
    "}",
    "position", "{",
      "line", JSONRPC_MESSAGE_PUT_INT32 (line),
      "character", JSONRPC_MESSAGE_PUT_INT32 (line_offset),
    "}",
    "context", "{",
      "includeDeclaration", JSONRPC_MESSAGE_PUT_BOOLEAN (TRUE),
    "}"
  );

  ide_langserv_client_call_async (priv->client,
                                  "textDocument/references",
                                  g_steal_pointer (&params),
                                  cancellable,
                                  ide_langserv_symbol_resolver_find_references_cb,
                                  g_steal_pointer (&task));

  IDE_EXIT;
}

static GPtrArray *
ide_langserv_symbol_resolver_find_references_finish (IdeSymbolResolver  *self,
                                                     GAsyncResult       *result,
                                                     GError            **error)
{
  GPtrArray *ret;

  IDE_ENTRY;

  g_assert (IDE_IS_LANGSERV_SYMBOL_RESOLVER (self));
  g_assert (G_IS_TASK (result));

  ret = g_task_propagate_pointer (G_TASK (result), error);

  IDE_RETURN (ret);
}

static void
symbol_resolver_iface_init (IdeSymbolResolverInterface *iface)
{
  iface->lookup_symbol_async = ide_langserv_symbol_resolver_lookup_symbol_async;
  iface->lookup_symbol_finish = ide_langserv_symbol_resolver_lookup_symbol_finish;
  iface->get_symbol_tree_async = ide_langserv_symbol_resolver_get_symbol_tree_async;
  iface->get_symbol_tree_finish = ide_langserv_symbol_resolver_get_symbol_tree_finish;
  iface->find_references_async = ide_langserv_symbol_resolver_find_references_async;
  iface->find_references_finish = ide_langserv_symbol_resolver_find_references_finish;
}
