/* ide-lsp-symbol-resolver.c
 *
 * Copyright 2016-2023 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-lsp-symbol-resolver"

#include "config.h"

#include <jsonrpc-glib.h>

#include <libide-code.h>
#include <libide-threading.h>

#include "ide-lsp-symbol-node.h"
#include "ide-lsp-symbol-node-private.h"
#include "ide-lsp-symbol-resolver.h"
#include "ide-lsp-symbol-tree.h"
#include "ide-lsp-symbol-tree-private.h"
#include "ide-lsp-util.h"

typedef struct
{
  gint64 line;
  gint64 column;
} SymbolLoc;

typedef struct
{
  SymbolLoc begin;
  SymbolLoc end;
} SymbolLocRange;

/*
 * Raw document symbol data extracted from textDocument/documentSymbol
 * responses.
 */
typedef struct
{
  const gchar *uri;
  const gchar *name;
  const gchar *container_name;
  gboolean deprecated;
  gint64 kind;
  SymbolLocRange location;
} SymbolData;

typedef gboolean (*SymbolDataIterator) (GFile      *file,
                                        SymbolData *symbol_data,
                                        gpointer    user_data);

typedef struct
{
  SymbolLoc target;
  SymbolLocRange best_location;
  IdeSymbol *best;
} FindNearestScopeData;

typedef struct
{
  IdeLspClient *client;
} IdeLspSymbolResolverPrivate;

static void symbol_resolver_iface_init (IdeSymbolResolverInterface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (IdeLspSymbolResolver, ide_lsp_symbol_resolver, IDE_TYPE_OBJECT,
                                  G_ADD_PRIVATE (IdeLspSymbolResolver)
                                  G_IMPLEMENT_INTERFACE (IDE_TYPE_SYMBOL_RESOLVER, symbol_resolver_iface_init))

enum {
  PROP_0,
  PROP_CLIENT,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static int
symbol_loc_cmp (SymbolLoc *l1,
                SymbolLoc *l2)
{
  g_assert (l1 != NULL);
  g_assert (l2 != NULL);

  if (l1->line < l2->line)
    return -1;
  else if (l1->line > l2->line)
    return 1;
  else
    return l1->column - l2->column;
}

static gboolean
symbol_loc_range_inclues_range (SymbolLocRange *r1,
                                SymbolLocRange *r2)
{
  g_assert (r1 != NULL);
  g_assert (r2 != NULL);

  return symbol_loc_cmp (&r1->begin, &r2->begin) <= 0 &&
         symbol_loc_cmp (&r1->end, &r2->end) >= 0;
}

static gboolean
symbol_loc_range_inclues_point (SymbolLoc      *point,
                                SymbolLocRange *range)
{
  g_assert (point != NULL);
  g_assert (range != NULL);

  return symbol_loc_cmp (&range->begin, point) <= 0 &&
         symbol_loc_cmp (&range->end, point) >= 0;
}

static inline void
symbol_data_init (SymbolData *data)
{
  g_assert (data != NULL);

  data->uri = NULL;
  data->name = NULL;
  data->container_name = NULL;
  data->deprecated = FALSE;
  data->kind = -1;
}

static IdeLspSymbolNode*
symbol_data_to_new_lsp_symbol_node (GFile      *file,
                                    SymbolData *data)
{
  g_assert (G_IS_FILE (file));
  g_assert (data != NULL);

  return ide_lsp_symbol_node_new (file,
                                  data->name,
                                  data->container_name,
                                  data->kind,
                                  data->location.begin.line,
                                  data->location.begin.column,
                                  data->location.end.line,
                                  data->location.end.column,
                                  data->deprecated);
}

static IdeSymbol *
symbol_data_to_new_symbol (GFile      *file,
                           SymbolData *data)
{
  IdeLocation *location;
  IdeSymbolKind kind;
  IdeSymbolFlags flags = IDE_SYMBOL_FLAGS_NONE;

  g_assert (G_IS_FILE (file));
  g_assert (data != NULL);

  kind = ide_lsp_decode_symbol_kind (data->kind);
  location = ide_location_new (file, data->location.begin.line, data->location.begin.column);

  if (data->deprecated)
    flags |= IDE_SYMBOL_FLAGS_IS_DEPRECATED;

  return ide_symbol_new (data->name, kind, flags, location, NULL);
}

static gboolean
is_symbol_information (GVariant *v)
{
  g_auto(GVariantDict) dict = G_VARIANT_DICT_INIT (v);
  return g_variant_dict_contains (&dict, "location");
}

static gboolean
is_document_symbol (GVariant *v)
{
  g_auto(GVariantDict) dict = G_VARIANT_DICT_INIT (v);
  return g_variant_dict_contains (&dict, "range");
}

static gboolean
process_document_symbols (IdeLspClient       *client,
                          GAsyncResult       *result,
                          IdeTask            *task,
                          GFile              *file,
                          SymbolDataIterator  iter_fun,
                          gpointer            iter_fun_data)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) return_value = NULL;
  GVariantIter iter;
  GVariant *node;

  IDE_ENTRY;

  g_assert (IDE_IS_LSP_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));
  g_assert (iter_fun != NULL);

  if (!ide_lsp_client_call_finish (client, result, &return_value, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_RETURN (FALSE);
    }

  if (!g_variant_is_of_type (return_value, G_VARIANT_TYPE ("av")))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVALID_DATA,
                                 "Invalid result for textDocument/documentSymbol");
      IDE_RETURN (FALSE);
    }

  g_variant_iter_init (&iter, return_value);

  while (g_variant_iter_loop (&iter, "v", &node))
    {
      gboolean success;
      SymbolData sym;

      symbol_data_init (&sym);

      if (is_symbol_information (node))
        {
          success = JSONRPC_MESSAGE_PARSE (node,
            "name", JSONRPC_MESSAGE_GET_STRING (&sym.name),
            "kind", JSONRPC_MESSAGE_GET_INT64 (&sym.kind),
            "location", "{",
              "uri", JSONRPC_MESSAGE_GET_STRING (&sym.uri),
              "range", "{",
                "start", "{",
                  "line", JSONRPC_MESSAGE_GET_INT64 (&sym.location.begin.line),
                  "character", JSONRPC_MESSAGE_GET_INT64 (&sym.location.begin.column),
                "}",
                "end", "{",
                  "line", JSONRPC_MESSAGE_GET_INT64 (&sym.location.end.line),
                  "character", JSONRPC_MESSAGE_GET_INT64 (&sym.location.end.column),
                "}",
              "}",
            "}"
          );
        }
      else if (is_document_symbol (node))
        {
          success = JSONRPC_MESSAGE_PARSE (node,
            "name", JSONRPC_MESSAGE_GET_STRING (&sym.name),
            "kind", JSONRPC_MESSAGE_GET_INT64 (&sym.kind),
            "range", "{",
              "start", "{",
                "line", JSONRPC_MESSAGE_GET_INT64 (&sym.location.begin.line),
                "character", JSONRPC_MESSAGE_GET_INT64 (&sym.location.begin.column),
              "}",
              "end", "{",
                "line", JSONRPC_MESSAGE_GET_INT64 (&sym.location.end.line),
                "character", JSONRPC_MESSAGE_GET_INT64 (&sym.location.end.column),
              "}",
            "}"
          );
        }
      else
        {
          g_autofree gchar *vstr = g_variant_print (node, TRUE);
          g_debug ("Unknown element in textDocument/documentSymbol reply: %s", vstr);
          continue;
        }

      if (!success)
        {
#ifdef IDE_ENABLE_TRACE
          g_autofree gchar *replystr = g_variant_print (node, TRUE);
          IDE_TRACE_MSG ("Failed to parse reply from language server: %s", replystr);
#endif
          continue;
        }

      /* Optional fields */
      JSONRPC_MESSAGE_PARSE (node, "containerName",
                             JSONRPC_MESSAGE_GET_STRING (&sym.container_name));
      JSONRPC_MESSAGE_PARSE (node, "deprecated",
                             JSONRPC_MESSAGE_GET_BOOLEAN (&sym.deprecated));

      if (!iter_fun (file, &sym, iter_fun_data))
        IDE_RETURN (TRUE);
    }

  iter_fun (file, NULL, iter_fun_data);

  IDE_RETURN (TRUE);
}

static void
ide_lsp_symbol_resolver_finalize (GObject *object)
{
  IdeLspSymbolResolver *self = (IdeLspSymbolResolver *)object;
  IdeLspSymbolResolverPrivate *priv = ide_lsp_symbol_resolver_get_instance_private (self);

  g_clear_object (&priv->client);

  G_OBJECT_CLASS (ide_lsp_symbol_resolver_parent_class)->finalize (object);
}

static void
ide_lsp_symbol_resolver_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  IdeLspSymbolResolver *self = IDE_LSP_SYMBOL_RESOLVER (object);

  switch (prop_id)
    {
    case PROP_CLIENT:
      g_value_set_object (value, ide_lsp_symbol_resolver_get_client (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_lsp_symbol_resolver_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  IdeLspSymbolResolver *self = IDE_LSP_SYMBOL_RESOLVER (object);

  switch (prop_id)
    {
    case PROP_CLIENT:
      ide_lsp_symbol_resolver_set_client (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_lsp_symbol_resolver_class_init (IdeLspSymbolResolverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_lsp_symbol_resolver_finalize;
  object_class->get_property = ide_lsp_symbol_resolver_get_property;
  object_class->set_property = ide_lsp_symbol_resolver_set_property;

  properties [PROP_CLIENT] =
    g_param_spec_object ("client",
                         "Client",
                         "The Language Server client",
                         IDE_TYPE_LSP_CLIENT,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_lsp_symbol_resolver_init (IdeLspSymbolResolver *self)
{
}

/**
 * ide_lsp_symbol_resolver_get_client:
 *
 * Gets the client used by the symbol resolver.
 *
 * Returns: (transfer none) (nullable): An #IdeLspClient or %NULL.
 */
IdeLspClient *
ide_lsp_symbol_resolver_get_client (IdeLspSymbolResolver *self)
{
  IdeLspSymbolResolverPrivate *priv = ide_lsp_symbol_resolver_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_LSP_SYMBOL_RESOLVER (self), NULL);

  return priv->client;
}

void
ide_lsp_symbol_resolver_set_client (IdeLspSymbolResolver *self,
                                    IdeLspClient         *client)
{
  IdeLspSymbolResolverPrivate *priv = ide_lsp_symbol_resolver_get_instance_private (self);

  g_return_if_fail (IDE_IS_LSP_SYMBOL_RESOLVER (self));
  g_return_if_fail (!client || IDE_IS_LSP_CLIENT (client));

  if (g_set_object (&priv->client, client))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CLIENT]);
}

static void
ide_lsp_symbol_resolver_definition_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  IdeLspClient *client = (IdeLspClient *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) return_value = NULL;
  g_autoptr(IdeSymbol) symbol = NULL;
  g_autoptr(GFile) gfile = NULL;
  g_autoptr(IdeLocation) location = NULL;
  g_autoptr(GVariant) variant = NULL;
  GVariantIter iter;
  const gchar *uri;
  struct {
    gint64 line;
    gint64 column;
  } begin, end;
  gboolean success = FALSE;

  IDE_ENTRY;

  g_assert (IDE_IS_LSP_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_lsp_client_call_finish (client, result, &return_value, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

#if 0
  {
    g_autofree gchar *str = g_variant_print (return_value, TRUE);
    IDE_TRACE_MSG ("Got reply: %s", str);
  }
#endif

  /* We only take the first result here */
  if (g_variant_is_of_type (return_value, G_VARIANT_TYPE ("av")))
    {
      g_variant_iter_init (&iter, return_value);
      if (g_variant_iter_next (&iter, "v", &variant))
        goto parse;
    }

  if (g_variant_is_of_type (return_value, G_VARIANT_TYPE ("a{sv}")))
    {
      variant = g_variant_ref (return_value);
      goto parse;
    }

  IDE_GOTO (failure);

parse:
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

  if (!success)
    {
    failure:
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVALID_DATA,
                                 "Got invalid reply for textDocument/definition");
      IDE_EXIT;
    }

  IDE_TRACE_MSG ("Definition location is %s %d:%d",
                 uri, (gint)begin.line + 1, (gint)begin.column + 1);

  gfile = g_file_new_for_uri (uri);
  location = ide_location_new (gfile, begin.line, begin.column);
  symbol = ide_symbol_new ("", IDE_SYMBOL_KIND_NONE, IDE_SYMBOL_FLAGS_NONE, location, location);

  ide_task_return_pointer (task, g_steal_pointer (&symbol), g_object_unref);

  IDE_EXIT;
}

static void
ide_lsp_symbol_resolver_lookup_symbol_async (IdeSymbolResolver   *resolver,
                                             IdeLocation         *location,
                                             GCancellable        *cancellable,
                                             GAsyncReadyCallback  callback,
                                             gpointer             user_data)
{
  IdeLspSymbolResolver *self = (IdeLspSymbolResolver *)resolver;
  IdeLspSymbolResolverPrivate *priv = ide_lsp_symbol_resolver_get_instance_private (self);
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GVariant) params = NULL;
  g_autofree gchar *uri = NULL;
  GFile *gfile;
  gint line;
  gint column;

  IDE_ENTRY;

  g_assert (IDE_IS_LSP_SYMBOL_RESOLVER (self));
  g_assert (location != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_lsp_symbol_resolver_lookup_symbol_async);

  if (priv->client == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_CONNECTED,
                                 "%s requires a client to resolve symbols",
                                 G_OBJECT_TYPE_NAME (self));
      IDE_EXIT;
    }

  if (!(gfile = ide_location_get_file (location)))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "Cannot resolve symbol, invalid source location");
      IDE_EXIT;
    }

  uri = g_file_get_uri (gfile);
  line = ide_location_get_line (location);
  column = ide_location_get_line_offset (location);

  params = JSONRPC_MESSAGE_NEW (
    "textDocument", "{",
      "uri", JSONRPC_MESSAGE_PUT_STRING (uri),
    "}",
    "position", "{",
      "line", JSONRPC_MESSAGE_PUT_INT32 (line),
      "character", JSONRPC_MESSAGE_PUT_INT32 (column),
    "}"
  );

  ide_lsp_client_call_async (priv->client,
                             "textDocument/definition",
                             params,
                             cancellable,
                             ide_lsp_symbol_resolver_definition_cb,
                             g_steal_pointer (&task));

  IDE_EXIT;
}

static IdeSymbol *
ide_lsp_symbol_resolver_lookup_symbol_finish (IdeSymbolResolver  *resolver,
                                              GAsyncResult       *result,
                                              GError            **error)
{
  IdeSymbol *ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_LSP_SYMBOL_RESOLVER (resolver), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static gboolean
collect_document_symbols_cb (GFile      *file,
                             SymbolData *data,
                             gpointer    user_data)
{
  g_autoptr(IdeLspSymbolNode) symbol = NULL;
  GPtrArray* symbols = (GPtrArray *)user_data;

  if (data == NULL)
    return FALSE;

  symbol = symbol_data_to_new_lsp_symbol_node (file, data);
  g_ptr_array_add (symbols, g_steal_pointer (&symbol));
  return TRUE;
}

static void
ide_lsp_symbol_resolver_get_symbol_tree_cb (GObject      *object,
                                            GAsyncResult *result,
                                            gpointer      user_data)
{
  IdeLspClient *client = (IdeLspClient *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(IdeLspSymbolTree) tree = NULL;
  g_autoptr(GPtrArray) symbols = NULL;
  GFile *file;

  IDE_ENTRY;

  g_assert (IDE_IS_LSP_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  file = ide_task_get_task_data (task);
  g_assert (G_IS_FILE (file));

  symbols = g_ptr_array_new_with_free_func (g_object_unref);

  if (!process_document_symbols (client,
                                 result,
                                 task,
                                 file,
                                 collect_document_symbols_cb,
                                 symbols))
    IDE_EXIT;

  tree = ide_lsp_symbol_tree_new (IDE_PTR_ARRAY_STEAL_FULL (&symbols));
  ide_task_return_pointer (task, g_steal_pointer (&tree), g_object_unref);

  IDE_EXIT;
}

static void
ide_lsp_symbol_resolver_get_symbol_tree_async (IdeSymbolResolver   *resolver,
                                               GFile               *file,
                                               GBytes              *bytes,
                                               GCancellable        *cancellable,
                                               GAsyncReadyCallback  callback,
                                               gpointer             user_data)
{
  IdeLspSymbolResolver *self = (IdeLspSymbolResolver *)resolver;
  IdeLspSymbolResolverPrivate *priv = ide_lsp_symbol_resolver_get_instance_private (self);
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GVariant) params = NULL;
  g_autofree gchar *uri = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_LSP_SYMBOL_RESOLVER (self));
  g_assert (G_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_lsp_symbol_resolver_get_symbol_tree_async);
  ide_task_set_task_data (task, g_file_dup (file), g_object_unref);

  if (priv->client == NULL)
    {
      ide_task_return_new_error (task,
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

  ide_lsp_client_call_async (priv->client,
                             "textDocument/documentSymbol",
                             params,
                             cancellable,
                             ide_lsp_symbol_resolver_get_symbol_tree_cb,
                             g_steal_pointer (&task));

  IDE_EXIT;
}

static IdeSymbolTree *
ide_lsp_symbol_resolver_get_symbol_tree_finish (IdeSymbolResolver  *resolver,
                                                GAsyncResult       *result,
                                                GError            **error)
{
  IdeSymbolTree *ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_LSP_SYMBOL_RESOLVER (resolver), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
ide_lsp_symbol_resolver_find_references_cb (GObject      *object,
                                            GAsyncResult *result,
                                            gpointer      user_data)
{
  IdeLspClient *client = (IdeLspClient *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GPtrArray) references = NULL;
  g_autoptr(GError) error = NULL;
  GVariant *locationv;
  GVariantIter iter;

  IDE_ENTRY;

  g_assert (IDE_IS_LSP_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_lsp_client_call_finish (client, result, &reply, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  if (!g_variant_is_of_type (reply, G_VARIANT_TYPE ("av")))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVALID_DATA,
                                 "Invalid reply type from peer: %s",
                                 g_variant_get_type_string (reply));
      IDE_EXIT;
    }

  references = g_ptr_array_new_with_free_func (g_object_unref);

  g_variant_iter_init (&iter, reply);

  while (g_variant_iter_loop (&iter, "v", &locationv))
    {
      g_autoptr(IdeLocation) begin_loc = NULL;
      g_autoptr(IdeLocation) end_loc = NULL;
      g_autoptr(IdeRange) range = NULL;
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
          ide_task_return_new_error (task,
                                     G_IO_ERROR,
                                     G_IO_ERROR_INVALID_DATA,
                                     "Failed to parse location object");
          IDE_EXIT;
        }

      gfile = g_file_new_for_uri (uri);

      begin_loc = ide_location_new (gfile, begin.line, begin.line_offset);
      end_loc = ide_location_new (gfile, end.line, end.line_offset);
      range = ide_range_new (begin_loc, end_loc);

      g_ptr_array_add (references, g_steal_pointer (&range));
    }

  ide_task_return_pointer (task, g_steal_pointer (&references), g_ptr_array_unref);

  IDE_EXIT;
}

static void
ide_lsp_symbol_resolver_find_references_async (IdeSymbolResolver   *resolver,
                                               IdeLocation         *location,
                                               const gchar         *language_id,
                                               GCancellable        *cancellable,
                                               GAsyncReadyCallback  callback,
                                               gpointer             user_data)
{
  IdeLspSymbolResolver *self = (IdeLspSymbolResolver *)resolver;
  IdeLspSymbolResolverPrivate *priv = ide_lsp_symbol_resolver_get_instance_private (self);
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GVariant) params = NULL;
  g_autofree gchar *uri = NULL;
  GFile *gfile;
  guint line;
  guint line_offset;

  IDE_ENTRY;

  g_assert (IDE_IS_LSP_SYMBOL_RESOLVER (self));
  g_assert (location != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_lsp_symbol_resolver_find_references_async);

  gfile = ide_location_get_file (location);
  uri = g_file_get_uri (gfile);

  line = ide_location_get_line (location);
  line_offset = ide_location_get_line_offset (location);

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

  ide_lsp_client_call_async (priv->client,
                             "textDocument/references",
                             params,
                             cancellable,
                             ide_lsp_symbol_resolver_find_references_cb,
                             g_steal_pointer (&task));

  IDE_EXIT;
}

static GPtrArray *
ide_lsp_symbol_resolver_find_references_finish (IdeSymbolResolver  *self,
                                                GAsyncResult       *result,
                                                GError            **error)
{
  GPtrArray *ret;

  IDE_ENTRY;

  g_assert (IDE_IS_LSP_SYMBOL_RESOLVER (self));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  IDE_PTR_ARRAY_CLEAR_FREE_FUNC (ret);

  IDE_RETURN (ret);
}

static gboolean
find_nearest_document_symbol_cb (GFile      *file,
                                 SymbolData *data,
                                 gpointer    user_data)
{
  FindNearestScopeData *find_data = (FindNearestScopeData *)user_data;

  g_assert (G_IS_FILE (file));
  g_assert (find_data != NULL);

  if (data == NULL)
    return FALSE;

  if (symbol_loc_range_inclues_point (&find_data->target, &data->location))
    {
      if (find_data->best == NULL ||
          symbol_loc_range_inclues_range (&find_data->best_location, &data->location))
        {
          g_clear_object (&find_data->best);
          find_data->best = symbol_data_to_new_symbol (file, data);
          find_data->best_location = data->location;
        }
    }

  return TRUE;
}

static void
ide_lsp_symbol_resolver_find_nearest_scope_cb (GObject      *object,
                                               GAsyncResult *result,
                                               gpointer      user_data)
{
  IdeLspClient *client = (IdeLspClient *)object;
  g_autoptr(IdeTask) task = user_data;
  FindNearestScopeData find_data;
  IdeLocation *location;

  IDE_ENTRY;

  g_assert (IDE_IS_LSP_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  location = ide_task_get_task_data (task);
  g_assert (IDE_IS_LOCATION (location));

  memset (&find_data, 0, sizeof find_data);
  find_data.target.line = ide_location_get_line (location);
  find_data.target.column = ide_location_get_line_offset (location);

  if (!process_document_symbols (client,
                                 result,
                                 task,
                                 ide_location_get_file (location),
                                 find_nearest_document_symbol_cb,
                                 &find_data))
    IDE_EXIT;

  g_assert (find_data.best == NULL || IDE_IS_SYMBOL (find_data.best));

  if (find_data.best == NULL)
    ide_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               "No symbols could be found");
  else
    ide_task_return_pointer (task,
                             g_steal_pointer (&find_data.best),
                             g_object_unref);

  IDE_EXIT;
}

static void
ide_lsp_symbol_resolver_find_nearest_scope_async (IdeSymbolResolver   *resolver,
                                                  IdeLocation         *location,
                                                  GCancellable        *cancellable,
                                                  GAsyncReadyCallback  callback,
                                                  gpointer             user_data)
{
  IdeLspSymbolResolver *self = (IdeLspSymbolResolver *)resolver;
  IdeLspSymbolResolverPrivate *priv = ide_lsp_symbol_resolver_get_instance_private (self);
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GVariant) params = NULL;
  g_autofree gchar *uri = NULL;
  GFile *file;

  IDE_ENTRY;

  g_assert (IDE_IS_LSP_SYMBOL_RESOLVER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_lsp_symbol_resolver_find_nearest_scope_async);

  if (!(file = ide_location_get_file (location)))
  {
    ide_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               "Cannot find nearest symbol, invalid source location");
    IDE_EXIT;
  }

  g_assert (G_IS_FILE (file));
  uri = g_file_get_uri (file);

  ide_task_set_task_data (task, ide_location_dup (location), g_object_unref);

  if (priv->client == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_CONNECTED,
                                 "Cannot query language server, not connected");
      IDE_EXIT;
    }

  params = JSONRPC_MESSAGE_NEW (
    "textDocument", "{",
      "uri", JSONRPC_MESSAGE_PUT_STRING (uri),
    "}"
  );

  ide_lsp_client_call_async (priv->client,
                             "textDocument/documentSymbol",
                             params,
                             cancellable,
                             ide_lsp_symbol_resolver_find_nearest_scope_cb,
                             g_steal_pointer (&task));

  IDE_EXIT;
}

static IdeSymbol *
ide_lsp_symbol_resolver_find_nearest_scope_finish (IdeSymbolResolver  *self,
                                                   GAsyncResult       *result,
                                                   GError            **error)
{
  IdeSymbol* ret;

  IDE_ENTRY;

  g_assert (IDE_IS_SYMBOL_RESOLVER (self));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);
  IDE_RETURN (ret);
}

static void
symbol_resolver_iface_init (IdeSymbolResolverInterface *iface)
{
  iface->lookup_symbol_async = ide_lsp_symbol_resolver_lookup_symbol_async;
  iface->lookup_symbol_finish = ide_lsp_symbol_resolver_lookup_symbol_finish;
  iface->get_symbol_tree_async = ide_lsp_symbol_resolver_get_symbol_tree_async;
  iface->get_symbol_tree_finish = ide_lsp_symbol_resolver_get_symbol_tree_finish;
  iface->find_references_async = ide_lsp_symbol_resolver_find_references_async;
  iface->find_references_finish = ide_lsp_symbol_resolver_find_references_finish;
  iface->find_nearest_scope_async = ide_lsp_symbol_resolver_find_nearest_scope_async;
  iface->find_nearest_scope_finish = ide_lsp_symbol_resolver_find_nearest_scope_finish;
}

