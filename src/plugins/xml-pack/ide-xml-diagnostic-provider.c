/* ide-xml-diagnostic-provider.c
 *
 * Copyright (C) 2017 Sebastien Lafargue <slafargue@gnome.org>
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

#define G_LOG_DOMAIN "xml-diagnostic-provider"

#include "ide-xml-service.h"

#include "ide-xml-diagnostic-provider.h"

struct _IdeXmlDiagnosticProvider
{
  IdeObject parent_instance;
};

static void diagnostic_provider_iface_init (IdeDiagnosticProviderInterface *iface);

G_DEFINE_TYPE_EXTENDED (IdeXmlDiagnosticProvider, ide_xml_diagnostic_provider, IDE_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_DIAGNOSTIC_PROVIDER, diagnostic_provider_iface_init))

static void
ide_xml_diagnostic_provider_diagnose_cb (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
  IdeXmlService *service = (IdeXmlService *)object;
  g_autoptr(GTask) task = user_data;
  IdeDiagnostics *diagnostics;
  GError *error = NULL;

  IDE_ENTRY;

  if (NULL == (diagnostics = ide_xml_service_get_diagnostics_finish (service, result, &error)))
    g_task_return_error (task, error);
  else
    g_task_return_pointer (task,
                           ide_diagnostics_ref (diagnostics),
                           (GDestroyNotify)ide_diagnostics_unref);

  IDE_EXIT;
}

static void
ide_xml_diagnostic_provider_diagnose_async (IdeDiagnosticProvider *provider,
                                            IdeFile               *file,
                                            IdeBuffer             *buffer,
                                            GCancellable          *cancellable,
                                            GAsyncReadyCallback    callback,
                                            gpointer               user_data)
{
  IdeXmlDiagnosticProvider *self = (IdeXmlDiagnosticProvider *)provider;
  g_autoptr(GTask) task = NULL;
  IdeXmlService *service;
  IdeContext *context;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_XML_DIAGNOSTIC_PROVIDER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_xml_diagnostic_provider_diagnose_async);

  context = ide_object_get_context (IDE_OBJECT (provider));
  service = ide_context_get_service_typed (context, IDE_TYPE_XML_SERVICE);

  ide_xml_service_get_diagnostics_async (service,
                                         file,
                                         buffer,
                                         cancellable,
                                         ide_xml_diagnostic_provider_diagnose_cb,
                                         g_steal_pointer (&task));

  IDE_EXIT;
}

static IdeDiagnostics *
ide_xml_diagnostic_provider_diagnose_finish (IdeDiagnosticProvider  *provider,
                                             GAsyncResult           *result,
                                             GError                **error)
{
  GTask *task = (GTask *)result;
  IdeDiagnostics *ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_XML_DIAGNOSTIC_PROVIDER (provider), NULL);
  g_return_val_if_fail (G_IS_TASK (task), NULL);

  ret = g_task_propagate_pointer (task, error);

  IDE_RETURN (ret);
}

IdeXmlDiagnosticProvider *
ide_xml_diagnostic_provider_new (void)
{
  return g_object_new (IDE_TYPE_XML_DIAGNOSTIC_PROVIDER, NULL);
}

static void
ide_xml_diagnostic_provider_finalize (GObject *object)
{
  G_OBJECT_CLASS (ide_xml_diagnostic_provider_parent_class)->finalize (object);
}

static void
ide_xml_diagnostic_provider_class_init (IdeXmlDiagnosticProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_xml_diagnostic_provider_finalize;
}

static void
ide_xml_diagnostic_provider_init (IdeXmlDiagnosticProvider *self)
{
}

static void
diagnostic_provider_iface_init (IdeDiagnosticProviderInterface *iface)
{
  iface->diagnose_async = ide_xml_diagnostic_provider_diagnose_async;
  iface->diagnose_finish = ide_xml_diagnostic_provider_diagnose_finish;
}
