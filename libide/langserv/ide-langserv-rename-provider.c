/* ide-langserv-rename-provider.c
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

#define G_LOG_DOMAIN "ide-langserv-rename-provider"

#include <jsonrpc-glib.h>

#include "ide-debug.h"

#include "files/ide-file.h"
#include "langserv/ide-langserv-client.h"
#include "langserv/ide-langserv-rename-provider.h"
#include "diagnostics/ide-source-location.h"

typedef struct
{
  IdeLangservClient *client;
} IdeLangservRenameProviderPrivate;

static void rename_provider_iface_init (IdeRenameProviderInterface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (IdeLangservRenameProvider, ide_langserv_rename_provider, IDE_TYPE_OBJECT,
                                  G_ADD_PRIVATE (IdeLangservRenameProvider)
                                  G_IMPLEMENT_INTERFACE (IDE_TYPE_RENAME_PROVIDER, rename_provider_iface_init))

enum {
  PROP_0,
  PROP_CLIENT,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
ide_langserv_rename_provider_finalize (GObject *object)
{
  IdeLangservRenameProvider *self = (IdeLangservRenameProvider *)object;
  IdeLangservRenameProviderPrivate *priv = ide_langserv_rename_provider_get_instance_private (self);

  g_clear_object (&priv->client);

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
  g_autoptr(JsonNode) return_value = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GPtrArray) ret = NULL;
  JsonObject *changes_by_uri = NULL;
  JsonObjectIter iter;
  const gchar *uri;
  IdeContext *context;
  JsonNode *changes;

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

  if (!JCON_EXTRACT (return_value, "changes", JCONE_OBJECT (changes_by_uri)))
    IDE_EXIT;

  context = ide_object_get_context (IDE_OBJECT (self));

  ret = g_ptr_array_new_with_free_func (g_object_unref);

  json_object_iter_init (&iter, changes_by_uri);

  while (json_object_iter_next (&iter, &uri, &changes))
    {
      g_autoptr(GFile) gfile = g_file_new_for_uri (uri);
      g_autoptr(IdeFile) ifile = ide_file_new (context, gfile);
      JsonArray *array;
      guint length;

      if (!JSON_NODE_HOLDS_ARRAY (changes))
        continue;

      array = json_node_get_array (changes);
      length = json_array_get_length (array);

      for (guint i = 0; i < length; i++)
        {
          JsonNode *change = json_array_get_element (array, i);
          g_autoptr(IdeSourceLocation) begin_location = NULL;
          g_autoptr(IdeSourceLocation) end_location = NULL;
          g_autoptr(IdeSourceRange) range = NULL;
          g_autoptr(IdeProjectEdit) edit = NULL;
          const gchar *new_text = NULL;
          gboolean success;
          struct {
            gint line;
            gint column;
          } begin, end;

          success = JCON_EXTRACT (change,
            "range", "{",
              "start", "{",
                "line", JCONE_INT (begin.line),
                "character", JCONE_INT (begin.column),
              "}",
              "end", "{",
                "line", JCONE_INT (end.line),
                "character", JCONE_INT (end.column),
              "}",
            "}",
            "newText", JCONE_STRING (new_text)
          );

          if (!success)
            continue;

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
  g_autoptr(JsonNode) params = NULL;
  g_autofree gchar *uri = NULL;
  IdeFile *ifile;
  GFile *gfile;
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

  params = JCON_NEW (
    "textDocument", "{",
      "uri", JCON_STRING (uri),
    "}",
    "position", "{",
      "line", JCON_INT (line),
      "character", JCON_INT (column),
    "}",
    "newName", JCON_STRING (new_name)
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
 * Returns: (transfer none) (nullable): A #IdeLangservClient or %NULL.
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
