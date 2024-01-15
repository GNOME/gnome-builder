/* ide-lsp-diagnostic-provider.c
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

#define G_LOG_DOMAIN "ide-lsp-diagnostic-provider"

#include "config.h"

#include <json-glib/json-glib.h>

#include <libide-code.h>
#include <libide-threading.h>

#include "ide-lsp-diagnostic-provider.h"

typedef struct
{
  IdeLspClient   *client;
  GSignalGroup   *client_signals;
} IdeLspDiagnosticProviderPrivate;

static void diagnostic_provider_iface_init (IdeDiagnosticProviderInterface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (IdeLspDiagnosticProvider, ide_lsp_diagnostic_provider, IDE_TYPE_OBJECT,
                                  G_ADD_PRIVATE (IdeLspDiagnosticProvider)
                                  G_IMPLEMENT_INTERFACE (IDE_TYPE_DIAGNOSTIC_PROVIDER, diagnostic_provider_iface_init))

enum {
  PROP_0,
  PROP_CLIENT,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
ide_lsp_diagnostic_provider_get_diagnostics_cb (GObject      *object,
                                                GAsyncResult *result,
                                                gpointer      user_data)
{
  IdeLspClient *client = (IdeLspClient *)object;
  g_autoptr(IdeDiagnostics) diagnostics = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_LSP_CLIENT (client));
  g_assert (IDE_IS_TASK (task));

  if (!ide_lsp_client_get_diagnostics_finish (client, result, &diagnostics, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_pointer (task, g_steal_pointer (&diagnostics), g_object_unref);
}

static void
ide_lsp_diagnostic_provider_diagnose_async (IdeDiagnosticProvider *provider,
                                            GFile                 *file,
                                            GBytes                *content,
                                            const gchar           *lang_id,
                                            GCancellable          *cancellable,
                                            GAsyncReadyCallback    callback,
                                            gpointer               user_data)
{
  IdeLspDiagnosticProvider *self = (IdeLspDiagnosticProvider *)provider;
  IdeLspDiagnosticProviderPrivate *priv = ide_lsp_diagnostic_provider_get_instance_private (self);
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_LSP_DIAGNOSTIC_PROVIDER (self));
  g_assert (G_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_lsp_diagnostic_provider_diagnose_async);

  if (priv->client == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "Improperly configured %s is missing IdeLspClient",
                                 G_OBJECT_TYPE_NAME (self));
      IDE_EXIT;
    }

  ide_lsp_client_get_diagnostics_async (priv->client,
                                        file,
                                        content,
                                        lang_id,
                                        cancellable,
                                        ide_lsp_diagnostic_provider_get_diagnostics_cb,
                                        g_steal_pointer (&task));

  IDE_EXIT;
}

static IdeDiagnostics *
ide_lsp_diagnostic_provider_diagnose_finish (IdeDiagnosticProvider  *provider,
                                             GAsyncResult           *result,
                                             GError                **error)
{
  IdeDiagnostics *ret;

  IDE_ENTRY;

  g_assert (IDE_IS_LSP_DIAGNOSTIC_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
diagnostic_provider_iface_init (IdeDiagnosticProviderInterface *iface)
{
  iface->diagnose_async = ide_lsp_diagnostic_provider_diagnose_async;
  iface->diagnose_finish = ide_lsp_diagnostic_provider_diagnose_finish;
}

static void
ide_lsp_diagnostic_provider_finalize (GObject *object)
{
  IdeLspDiagnosticProvider *self = (IdeLspDiagnosticProvider *)object;
  IdeLspDiagnosticProviderPrivate *priv = ide_lsp_diagnostic_provider_get_instance_private (self);

  g_clear_object (&priv->client_signals);
  g_clear_object (&priv->client);

  G_OBJECT_CLASS (ide_lsp_diagnostic_provider_parent_class)->finalize (object);
}

static void
ide_lsp_diagnostic_provider_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  IdeLspDiagnosticProvider *self = IDE_LSP_DIAGNOSTIC_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_CLIENT:
      g_value_set_object (value, ide_lsp_diagnostic_provider_get_client (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_lsp_diagnostic_provider_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  IdeLspDiagnosticProvider *self = IDE_LSP_DIAGNOSTIC_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_CLIENT:
      ide_lsp_diagnostic_provider_set_client (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_lsp_diagnostic_provider_class_init (IdeLspDiagnosticProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_lsp_diagnostic_provider_finalize;
  object_class->get_property = ide_lsp_diagnostic_provider_get_property;
  object_class->set_property = ide_lsp_diagnostic_provider_set_property;

  properties [PROP_CLIENT] =
    g_param_spec_object ("client",
                         "Client",
                         "The Language Server client",
                         IDE_TYPE_LSP_CLIENT,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_lsp_diagnostic_provider_init (IdeLspDiagnosticProvider *self)
{
  IdeLspDiagnosticProviderPrivate *priv = ide_lsp_diagnostic_provider_get_instance_private (self);

  priv->client_signals = g_signal_group_new (IDE_TYPE_LSP_CLIENT);

  g_signal_group_connect_object (priv->client_signals,
                                   "published-diagnostics",
                                   G_CALLBACK (ide_diagnostic_provider_emit_invalidated),
                                   self,
                                   G_CONNECT_SWAPPED);
}

/**
 * ide_lsp_diagnostic_provider_get_client:
 *
 * Gets the client used by diagnostic provider.
 *
 * Returns: (nullable) (transfer none): An #IdeLspClient or %NULL.
 */
IdeLspClient *
ide_lsp_diagnostic_provider_get_client (IdeLspDiagnosticProvider *self)
{
  IdeLspDiagnosticProviderPrivate *priv = ide_lsp_diagnostic_provider_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_LSP_DIAGNOSTIC_PROVIDER (self), NULL);

  return priv->client;
}

void
ide_lsp_diagnostic_provider_set_client (IdeLspDiagnosticProvider *self,
                                        IdeLspClient             *client)
{
  IdeLspDiagnosticProviderPrivate *priv = ide_lsp_diagnostic_provider_get_instance_private (self);

  g_return_if_fail (IDE_IS_LSP_DIAGNOSTIC_PROVIDER (self));
  g_return_if_fail (!client || IDE_IS_LSP_CLIENT (client));

  if (g_set_object (&priv->client, client))
    {
      g_signal_group_set_target (priv->client_signals, client);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CLIENT]);
    }
}
