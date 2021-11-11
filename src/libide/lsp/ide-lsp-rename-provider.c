/* ide-lsp-rename-provider.c
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

#define G_LOG_DOMAIN "ide-lsp-rename-provider"

#include "config.h"

#include <jsonrpc-glib.h>
#include <libide-code.h>
#include <libide-threading.h>

#include "ide-lsp-client.h"
#include "ide-lsp-rename-provider.h"
#include "ide-lsp-util.h"
#include "ide-lsp-workspace-edit.h"

typedef struct
{
  IdeLspClient *client;
  IdeBuffer         *buffer;
} IdeLspRenameProviderPrivate;

static void rename_provider_iface_init (IdeRenameProviderInterface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (IdeLspRenameProvider, ide_lsp_rename_provider, IDE_TYPE_OBJECT,
                                  G_ADD_PRIVATE (IdeLspRenameProvider)
                                  G_IMPLEMENT_INTERFACE (IDE_TYPE_RENAME_PROVIDER, rename_provider_iface_init))

enum {
  PROP_0,
  PROP_BUFFER,
  PROP_CLIENT,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
ide_lsp_rename_provider_set_buffer (IdeLspRenameProvider *self,
                                         IdeBuffer                 *buffer)
{
  IdeLspRenameProviderPrivate *priv = ide_lsp_rename_provider_get_instance_private (self);

  g_set_weak_pointer (&priv->buffer, buffer);
}

static void
ide_lsp_rename_provider_finalize (GObject *object)
{
  IdeLspRenameProvider *self = (IdeLspRenameProvider *)object;
  IdeLspRenameProviderPrivate *priv = ide_lsp_rename_provider_get_instance_private (self);

  g_clear_object (&priv->client);
  g_clear_weak_pointer (&priv->buffer);

  G_OBJECT_CLASS (ide_lsp_rename_provider_parent_class)->finalize (object);
}

static void
ide_lsp_rename_provider_get_property (GObject    *object,
                                           guint       prop_id,
                                           GValue     *value,
                                           GParamSpec *pspec)
{
  IdeLspRenameProvider *self = IDE_LSP_RENAME_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_CLIENT:
      g_value_set_object (value, ide_lsp_rename_provider_get_client (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_lsp_rename_provider_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  IdeLspRenameProvider *self = IDE_LSP_RENAME_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      ide_lsp_rename_provider_set_buffer (self, g_value_get_object (value));
      break;

    case PROP_CLIENT:
      ide_lsp_rename_provider_set_client (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_lsp_rename_provider_class_init (IdeLspRenameProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_lsp_rename_provider_finalize;
  object_class->get_property = ide_lsp_rename_provider_get_property;
  object_class->set_property = ide_lsp_rename_provider_set_property;

  properties [PROP_CLIENT] =
    g_param_spec_object ("client",
                         "Client",
                         "The Language Server client",
                         IDE_TYPE_LSP_CLIENT,
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
ide_lsp_rename_provider_init (IdeLspRenameProvider *self)
{
}

static void
ide_lsp_rename_provider_rename_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  IdeLspClient *client = (IdeLspClient *)object;
  g_autoptr(GVariant) return_value = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(IdeLspWorkspaceEdit) workspace_edit = NULL;
  g_autoptr(GPtrArray) ret = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_LSP_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_lsp_client_call_finish (client, result, &return_value, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  workspace_edit = ide_lsp_workspace_edit_new(return_value);
  ret = ide_lsp_workspace_edit_get_edits(workspace_edit);
  ide_task_return_pointer (task, g_steal_pointer (&ret), g_ptr_array_unref);

  IDE_EXIT;
}

static void
ide_lsp_rename_provider_rename_async (IdeRenameProvider   *provider,
                                           IdeLocation   *location,
                                           const gchar         *new_name,
                                           GCancellable        *cancellable,
                                           GAsyncReadyCallback  callback,
                                           gpointer             user_data)
{
  IdeLspRenameProvider *self = (IdeLspRenameProvider *)provider;
  IdeLspRenameProviderPrivate *priv = ide_lsp_rename_provider_get_instance_private (self);
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GVariant) params = NULL;
  g_autofree gchar *text = NULL;
  g_autofree gchar *uri = NULL;
  GtkTextIter begin;
  GtkTextIter end;
  GFile *gfile;
  gint64 version;
  gint line;
  gint column;

  IDE_ENTRY;

  g_assert (IDE_IS_LSP_RENAME_PROVIDER (self));
  g_assert (location != NULL);
  g_assert (new_name != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_lsp_rename_provider_rename_async);

  if (priv->client == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_CONNECTED,
                                 "No client set, cannot rename symbol");
      IDE_EXIT;
    }

  gfile = ide_location_get_file (location);
  uri = g_file_get_uri (gfile);

  line = ide_location_get_line (location);
  column = ide_location_get_line_offset (location);

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

  ide_lsp_client_call_async (priv->client,
                                  "textDocument/rename",
                                  params,
                                  cancellable,
                                  ide_lsp_rename_provider_rename_cb,
                                  g_steal_pointer (&task));

  IDE_EXIT;
}

static gboolean
ide_lsp_rename_provider_rename_finish (IdeRenameProvider  *provider,
                                       GAsyncResult       *result,
                                       GPtrArray         **edits,
                                       GError            **error)
{
  g_autoptr(GPtrArray) ar = NULL;
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_LSP_RENAME_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  ar = ide_task_propagate_pointer (IDE_TASK (result), error);
  ret = (ar != NULL);

  if (edits != NULL)
    *edits = IDE_PTR_ARRAY_STEAL_FULL (&ar);

  IDE_RETURN (ret);
}

static void
rename_provider_iface_init (IdeRenameProviderInterface *iface)
{
  iface->rename_async = ide_lsp_rename_provider_rename_async;
  iface->rename_finish = ide_lsp_rename_provider_rename_finish;
}

/**
 * ide_lsp_rename_provider_get_client:
 *
 * Returns: (transfer none) (nullable): an #IdeLspClient or %NULL.
 */
IdeLspClient *
ide_lsp_rename_provider_get_client (IdeLspRenameProvider *self)
{
  IdeLspRenameProviderPrivate *priv = ide_lsp_rename_provider_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_LSP_RENAME_PROVIDER (self), NULL);

  return priv->client;
}

void
ide_lsp_rename_provider_set_client (IdeLspRenameProvider *self,
                                         IdeLspClient         *client)
{
  IdeLspRenameProviderPrivate *priv = ide_lsp_rename_provider_get_instance_private (self);

  g_return_if_fail (IDE_IS_LSP_RENAME_PROVIDER (self));
  g_return_if_fail (!client || IDE_IS_LSP_CLIENT (client));

  if (g_set_object (&priv->client, client))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CLIENT]);
}
