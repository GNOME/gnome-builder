/* ide-langserv-diagnostic-provider.c
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

#define G_LOG_DOMAIN "ide-langserv-diagnostic-provider"

#include <egg-signal-group.h>
#include <json-glib/json-glib.h>

#include "ide-context.h"
#include "ide-debug.h"

#include "buffers/ide-unsaved-file.h"
#include "buffers/ide-unsaved-files.h"
#include "diagnostics/ide-diagnostics.h"
#include "files/ide-file.h"
#include "langserv/ide-langserv-diagnostic-provider.h"

typedef struct
{
  IdeLangservClient *client;
} IdeLangservDiagnosticProviderPrivate;

static void diagnostic_provider_iface_init (IdeDiagnosticProviderInterface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (IdeLangservDiagnosticProvider, ide_langserv_diagnostic_provider, IDE_TYPE_OBJECT,
                                  G_ADD_PRIVATE (IdeLangservDiagnosticProvider)
                                  G_IMPLEMENT_INTERFACE (IDE_TYPE_DIAGNOSTIC_PROVIDER, diagnostic_provider_iface_init))

static void
ide_langserv_diagnostic_provider_get_diagnostics_cb (GObject      *object,
                                                     GAsyncResult *result,
                                                     gpointer      user_data)
{
  IdeLangservClient *client = (IdeLangservClient *)object;
  g_autoptr(IdeDiagnostics) diagnostics = NULL;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_LANGSERV_CLIENT (client));
  g_assert (G_IS_TASK (task));

  if (!ide_langserv_client_get_diagnostics_finish (client, result, &diagnostics, &error))
    g_task_return_error (task, error);
  else
    g_task_return_pointer (task, g_steal_pointer (&diagnostics), (GDestroyNotify)ide_diagnostics_unref);
}

static void
ide_langserv_diagnostic_provider_diagnose_async (IdeDiagnosticProvider *provider,
                                                 IdeFile               *file,
                                                 GCancellable          *cancellable,
                                                 GAsyncReadyCallback    callback,
                                                 gpointer               user_data)
{
  IdeLangservDiagnosticProvider *self = (IdeLangservDiagnosticProvider *)provider;
  IdeLangservDiagnosticProviderPrivate *priv = ide_langserv_diagnostic_provider_get_instance_private (self);
  g_autoptr(GTask) task = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_LANGSERV_DIAGNOSTIC_PROVIDER (self));
  g_assert (IDE_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_langserv_diagnostic_provider_diagnose_async);

  if (priv->client == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               "Improperly configured %s is missing IdeLangservClient",
                               G_OBJECT_TYPE_NAME (self));
      IDE_EXIT;
    }

  ide_langserv_client_get_diagnostics_async (priv->client,
                                             ide_file_get_file (file),
                                             cancellable,
                                             ide_langserv_diagnostic_provider_get_diagnostics_cb,
                                             g_steal_pointer (&task));

  IDE_EXIT;
}

static IdeDiagnostics *
ide_langserv_diagnostic_provider_diagnose_finish (IdeDiagnosticProvider  *provider,
                                                  GAsyncResult           *result,
                                                  GError                **error)
{
  IdeDiagnostics *ret;

  IDE_ENTRY;

  g_assert (IDE_IS_LANGSERV_DIAGNOSTIC_PROVIDER (provider));
  g_assert (G_IS_TASK (result));

  ret = g_task_propagate_pointer (G_TASK (result), error);

  IDE_RETURN (ret);
}

static void
diagnostic_provider_iface_init (IdeDiagnosticProviderInterface *iface)
{
  iface->diagnose_async = ide_langserv_diagnostic_provider_diagnose_async;
  iface->diagnose_finish = ide_langserv_diagnostic_provider_diagnose_finish;
}

static void
ide_langserv_diagnostic_provider_finalize (GObject *object)
{
  IdeLangservDiagnosticProvider *self = (IdeLangservDiagnosticProvider *)object;
  IdeLangservDiagnosticProviderPrivate *priv = ide_langserv_diagnostic_provider_get_instance_private (self);

  g_clear_object (&priv->client);

  G_OBJECT_CLASS (ide_langserv_diagnostic_provider_parent_class)->finalize (object);
}

static void
ide_langserv_diagnostic_provider_class_init (IdeLangservDiagnosticProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_langserv_diagnostic_provider_finalize;
}

static void
ide_langserv_diagnostic_provider_init (IdeLangservDiagnosticProvider *self)
{
}

IdeLangservDiagnosticProvider *
ide_langserv_diagnostic_provider_new (void)
{
  return g_object_new (IDE_TYPE_LANGSERV_DIAGNOSTIC_PROVIDER, NULL);
}

/**
 * ide_langserv_diagnostic_provider_get_client:
 *
 * Gets the client used by diagnostic provider.
 *
 * Returns: (nullable) (transfer none): An #IdeLangservClient or %NULL.
 */
IdeLangservClient *
ide_langserv_diagnostic_provider_get_client (IdeLangservDiagnosticProvider *self)
{
  IdeLangservDiagnosticProviderPrivate *priv = ide_langserv_diagnostic_provider_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_LANGSERV_DIAGNOSTIC_PROVIDER (self), NULL);

  return priv->client;
}

void
ide_langserv_diagnostic_provider_set_client (IdeLangservDiagnosticProvider *self,
                                             IdeLangservClient             *client)
{
  IdeLangservDiagnosticProviderPrivate *priv = ide_langserv_diagnostic_provider_get_instance_private (self);

  g_return_if_fail (IDE_IS_LANGSERV_DIAGNOSTIC_PROVIDER (self));
  g_return_if_fail (!client || IDE_IS_LANGSERV_CLIENT (client));

  g_set_object (&priv->client, client);
}
