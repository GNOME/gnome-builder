/* gbp-gdiagnose-diagnostic-provider.c
 *
 * Copyright 2021 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-gdiagnose-diagnostic-provider"

#include "gbp-gdiagnose-diagnostic-provider.h"
#include "gbp-gdiagnose-chainups.h"

struct _GbpGdiagnoseDiagnosticProvider
{
  IdeObject parent_instance;
};

static void
gbp_gdiagnose_diagnostic_provider_diagnose_async (IdeDiagnosticProvider *provider,
                                                  GFile                 *file,
                                                  GBytes                *contents,
                                                  const char            *lang_id,
                                                  GCancellable          *cancellable,
                                                  GAsyncReadyCallback    callback,
                                                  gpointer               user_data)
{
  g_autoptr(IdeDiagnostics) diagnostics = NULL;
  g_autoptr(GtkSourceBuffer) buffer = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autofree gchar *name = NULL;

  g_assert (GBP_IS_GDIAGNOSE_DIAGNOSTIC_PROVIDER (provider));
  g_assert (G_IS_FILE (file));
  g_assert (contents != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (provider, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_gdiagnose_diagnostic_provider_diagnose_async);

  diagnostics = ide_diagnostics_new ();

  if (g_bytes_get_size (contents) > 0)
    {
      name = g_file_get_basename (file);
      buffer = gtk_source_buffer_new (NULL);
      gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer),
                                g_bytes_get_data (contents, NULL),
                                g_bytes_get_size (contents));
      gbp_gdiagnose_check_chainups (buffer, file, name, diagnostics);
    }

  ide_task_return_pointer (task, g_steal_pointer (&diagnostics), g_object_unref);
}

static IdeDiagnostics *
gbp_gdiagnose_diagnostic_provider_diagnose_finish (IdeDiagnosticProvider  *provider,
                                                   GAsyncResult           *result,
                                                   GError                **error)
{
  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static void
provider_iface_init (IdeDiagnosticProviderInterface *iface)
{
  iface->diagnose_async = gbp_gdiagnose_diagnostic_provider_diagnose_async;
  iface->diagnose_finish = gbp_gdiagnose_diagnostic_provider_diagnose_finish;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpGdiagnoseDiagnosticProvider, gbp_gdiagnose_diagnostic_provider, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_DIAGNOSTIC_PROVIDER, provider_iface_init))

static void
gbp_gdiagnose_diagnostic_provider_class_init (GbpGdiagnoseDiagnosticProviderClass *klass)
{
}

static void
gbp_gdiagnose_diagnostic_provider_init (GbpGdiagnoseDiagnosticProvider *self)
{
}
