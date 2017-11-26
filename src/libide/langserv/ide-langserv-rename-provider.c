/* ide-langserv-rename-provider.c
 *
 * Copyright © 2016 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-langserv-rename-provider"

#include <dazzle.h>
#include <jsonrpc-glib.h>

#include "ide-debug.h"
#include "ide-macros.h"

#include "buffers/ide-buffer.h"
#include "files/ide-file.h"
#include "langserv/ide-langserv-client.h"
#include "langserv/ide-langserv-rename-provider.h"
#include "diagnostics/ide-source-location.h"

typedef struct
{
  IdeLangservClient *client;
  IdeBuffer         *buffer;
} IdeLangservRenameProviderPrivate;

static void rename_provider_iface_init (IdeRenameProviderInterface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (IdeLangservRenameProvider, ide_langserv_rename_provider, IDE_TYPE_OBJECT,
                                  G_ADD_PRIVATE (IdeLangservRenameProvider)
                                  G_IMPLEMENT_INTERFACE (IDE_TYPE_RENAME_PROVIDER, rename_provider_iface_init))

enum {
  PROP_0,
  PROP_BUFFER,
  PROP_CLIENT,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
ide_langserv_rename_provider_set_buffer (IdeLangservRenameProvider *self,
                                         IdeBuffer                 *buffer)
{
  IdeLangservRenameProviderPrivate *priv = ide_langserv_rename_provider_get_instance_private (self);

  dzl_set_weak_pointer (&priv->buffer, buffer);
}

static void
ide_langserv_rename_provider_finalize (GObject *object)
{
  IdeLangservRenameProvider *self = (IdeLangservRenameProvider *)object;
  IdeLangservRenameProviderPrivate *priv = ide_langserv_rename_provider_get_instance_private (self);

  g_clear_object (&priv->client);
  dzl_clear_weak_pointer (&priv->buffer);

  G_OBJECT_CLASS (ide_langserv_rename_provider_parent_class)->finalize (object);
}

static void
ide_langserv_rename_provider_get_property (GObject    *object,
                                           guint       prop_id,
                                           GValue     *value,
                                           GParamSpec *pspec)
{
  IdeLangservRenameProvider *self = IDE_LANGSERV_RENAME_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_CLIENT:
      g_value_set_object (value, ide_langserv_rename_provider_get_client (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_langserv_rename_provider_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  IdeLangservRenameProvider *self = IDE_LANGSERV_RENAME_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      ide_langserv_rename_provider_set_buffer (self, g_value_get_object (value));
      break;

    case PROP_CLIENT:
      ide_langserv_rename_provider_set_client (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_langserv_rename_provider_class_init (IdeLangservRenameProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_langserv_rename_provider_finalize;
  object_class->get_property = ide_langserv_rename_provider_get_property;
  object_class->set_property = ide_langserv_rename_provider_set_property;

  properties [PROP_CLIENT] =
    g_param_spec_object ("client",
                         "Client",
                         "The Language Server client",
                         IDE_TYPE_LANGSERV_CLIENT,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_BUFFER] =
    g_param_spec_object ("buffer",
                         "Buffer",
                         "The buffer for renames",
                         IDE_TYPE_BUFFER,
                         (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_langserv_rename_provider_init (IdeLangservRenameProvider *self)
{
}

static void
ide_langserv_rename_provider_rename_cb (GObject      *object,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
  IdeLangservClient *client = (IdeLangservClient *)object;
  IdeLangservRenameProvider *self;
  g_autoptr(GVariant) return_value = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GPtrArray) ret = NULL;
  g_autoptr(GVariantIter) changes_by_uri = NULL;
  IdeContext *context;
  const gchar *uri;
  GVariant *changes;

  IDE_ENTRY;

  g_assert (IDE_IS_LANGSERV_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));
  self = g_task_get_source_object (task);
  g_assert (IDE_IS_LANGSERV_RENAME_PROVIDER (self));

  if (!ide_langserv_client_call_finish (client, result, &return_value, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  if (!JSONRPC_MESSAGE_PARSE (return_value, "changes", JSONRPC_MESSAGE_GET_ITER (&changes_by_uri)))
    IDE_EXIT;

  context = ide_object_get_context (IDE_OBJECT (self));

  ret = g_ptr_array_new_with_free_func (g_object_unref);

  while (g_variant_iter_loop (changes_by_uri, "{sv}", &uri, &changes))
    {
      g_autoptr(GFile) gfile = g_file_new_for_uri (uri);
      g_autoptr(IdeFile) ifile = ide_file_new (context, gfile);
      GVariantIter changes_iter;
      GVariant *change;

      if (!g_variant_is_container (changes))
        continue;

      g_variant_iter_init (&changes_iter, changes);

      while (g_variant_iter_loop (&changes_iter, "v", &change))
        {
          g_autoptr(IdeSourceLocation) begin_location = NULL;
          g_autoptr(IdeSourceLocation) end_location = NULL;
          g_autoptr(IdeSourceRange) range = NULL;
          g_autoptr(IdeProjectEdit) edit = NULL;
          const gchar *new_text = NULL;
          gboolean success;
          struct {
            gint64 line;
            gint64 column;
          } begin, end;

          success = JSONRPC_MESSAGE_PARSE (change,
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
            "newText", JSONRPC_MESSAGE_GET_STRING (&new_text)
          );

          if (!success)
            {
              IDE_TRACE_MSG ("Failed to extract change from variant");
              continue;
            }

          begin_location = ide_source_location_new (ifile, begin.line, begin.column, 0);
          end_location = ide_source_location_new (ifile, end.line, end.column, 0);
          range = ide_source_range_new (begin_location, end_location);

          edit = g_object_new (IDE_TYPE_PROJECT_EDIT,
                               "range", range,
                               "replacement", new_text,
                               NULL);

          g_ptr_array_add (ret, g_steal_pointer (&edit));
        }
    }

  g_task_return_pointer (task, g_steal_pointer (&ret), (GDestroyNotify)g_ptr_array_unref);

  IDE_EXIT;
}

static void
ide_langserv_rename_provider_rename_async (IdeRenameProvider   *provider,
                                           IdeSourceLocation   *location,
                                           const gchar         *new_name,
                                           GCancellable        *cancellable,
                                           GAsyncReadyCallback  callback,
                                           gpointer             user_data)
{
  IdeLangservRenameProvider *self = (IdeLangservRenameProvider *)provider;
  IdeLangservRenameProviderPrivate *priv = ide_langserv_rename_provider_get_instance_private (self);
  g_autoptr(GTask) task = NULL;
  g_autoptr(GVariant) params = NULL;
  g_autofree gchar *text = NULL;
  g_autofree gchar *uri = NULL;
  GtkTextIter begin;
  GtkTextIter end;
  IdeFile *ifile;
  GFile *gfile;
  gint64 version;
  gint line;
  gint column;

  IDE_ENTRY;

  g_assert (IDE_IS_LANGSERV_RENAME_PROVIDER (self));
  g_assert (location != NULL);
  g_assert (new_name != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_langserv_rename_provider_rename_async);

  if (priv->client == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_CONNECTED,
                               "No client set, cannot rename symbol");
      IDE_EXIT;
    }

  ifile = ide_source_location_get_file (location);
  gfile = ide_file_get_file (ifile);
  uri = g_file_get_uri (gfile);

  line = ide_source_location_get_line (location);
  column = ide_source_location_get_line_offset (location);

  version = ide_buffer_get_change_count (priv->buffer);

  gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (priv->buffer), &begin, &end);
  text = gtk_text_buffer_get_text (GTK_TEXT_BUFFER (priv->buffer), &begin, &end, TRUE);

  params = JSONRPC_MESSAGE_NEW (
    "textDocument", "{",
      "uri", JSONRPC_MESSAGE_PUT_STRING (uri),
      "version", JSONRPC_MESSAGE_PUT_INT64 (version),
      "text", JSONRPC_MESSAGE_PUT_STRING (text),
    "}",
    "position", "{",
      "line", JSONRPC_MESSAGE_PUT_INT32 (line),
      "character", JSONRPC_MESSAGE_PUT_INT32 (column),
    "}",
    "newName", JSONRPC_MESSAGE_PUT_STRING (new_name)
  );

  ide_langserv_client_call_async (priv->client,
                                  "textDocument/rename",
                                  g_steal_pointer (&params),
                                  cancellable,
                                  ide_langserv_rename_provider_rename_cb,
                                  g_steal_pointer (&task));

  IDE_EXIT;
}

static gboolean
ide_langserv_rename_provider_rename_finish (IdeRenameProvider  *provider,
                                            GAsyncResult       *result,
                                            GPtrArray         **edits,
                                            GError            **error)
{
  IdeLangservRenameProvider *self = (IdeLangservRenameProvider *)provider;
  g_autoptr(GPtrArray) ar = NULL;
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_LANGSERV_RENAME_PROVIDER (self));
  g_assert (G_IS_TASK (result));

  ar = g_task_propagate_pointer (G_TASK (result), error);
  ret = (ar != NULL);

  if (edits != NULL)
    *edits = g_steal_pointer (&ar);

  IDE_RETURN (ret);
}

static void
rename_provider_iface_init (IdeRenameProviderInterface *iface)
{
  iface->rename_async = ide_langserv_rename_provider_rename_async;
  iface->rename_finish = ide_langserv_rename_provider_rename_finish;
}

/**
 * ide_langserv_rename_provider_get_client:
 *
 * Returns: (transfer none) (nullable): an #IdeLangservClient or %NULL.
 */
IdeLangservClient *
ide_langserv_rename_provider_get_client (IdeLangservRenameProvider *self)
{
  IdeLangservRenameProviderPrivate *priv = ide_langserv_rename_provider_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_LANGSERV_RENAME_PROVIDER (self), NULL);

  return priv->client;
}

void
ide_langserv_rename_provider_set_client (IdeLangservRenameProvider *self,
                                         IdeLangservClient         *client)
{
  IdeLangservRenameProviderPrivate *priv = ide_langserv_rename_provider_get_instance_private (self);

  g_return_if_fail (IDE_IS_LANGSERV_RENAME_PROVIDER (self));
  g_return_if_fail (!client || IDE_IS_LANGSERV_CLIENT (client));

  if (g_set_object (&priv->client, client))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CLIENT]);
}
