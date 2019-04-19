/* gbp-codeui-buffer-addin.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-codeui-buffer-addin"

#include "config.h"

#include <libide-code.h>

#include "ide-diagnostics-manager-private.h"

#include "gbp-codeui-buffer-addin.h"

struct _GbpCodeuiBufferAddin
{
  GObject    parent_instance;
  IdeBuffer *buffer;
  GFile     *file;
};

static void
gbp_codeui_buffer_addin_queue_diagnose (GbpCodeuiBufferAddin *self,
                                        IdeBuffer            *buffer)
{
  g_autoptr(IdeContext) context = NULL;
  IdeDiagnosticsManager *manager;
  g_autoptr(GBytes) contents = NULL;
  const gchar *lang_id;
  GFile *file;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODEUI_BUFFER_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  context = ide_buffer_ref_context (buffer);
  manager = ide_diagnostics_manager_from_context (context);
  file = ide_buffer_get_file (buffer);
  lang_id = ide_buffer_get_language_id (buffer);
  contents = ide_buffer_dup_content (buffer);

  _ide_diagnostics_manager_file_changed (manager, file, contents, lang_id);
}

static void
gbp_codeui_buffer_addin_change_settled (IdeBufferAddin *addin,
                                        IdeBuffer      *buffer)
{
  GbpCodeuiBufferAddin *self = (GbpCodeuiBufferAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODEUI_BUFFER_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  gbp_codeui_buffer_addin_queue_diagnose (self, buffer);
}

static void
gbp_codeui_buffer_addin_file_loaded (IdeBufferAddin *addin,
                                     IdeBuffer      *buffer,
                                     GFile          *file)
{
  GbpCodeuiBufferAddin *self = (GbpCodeuiBufferAddin *)addin;
  g_autoptr(IdeContext) context = NULL;
  IdeDiagnosticsManager *manager;
  const gchar *lang_id;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODEUI_BUFFER_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_FILE (file));

  g_set_object (&self->file, file);

  context = ide_buffer_ref_context (buffer);
  manager = ide_diagnostics_manager_from_context (context);
  lang_id = ide_buffer_get_language_id (buffer);

  _ide_diagnostics_manager_file_opened (manager, file, lang_id);
}

static void
gbp_codeui_buffer_addin_file_saved (IdeBufferAddin *addin,
                                    IdeBuffer      *buffer,
                                    GFile          *file)
{
  GbpCodeuiBufferAddin *self = (GbpCodeuiBufferAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODEUI_BUFFER_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_FILE (file));

  g_set_object (&self->file, file);

  gbp_codeui_buffer_addin_queue_diagnose (self, buffer);
}

static void
gbp_codeui_buffer_addin_changed_cb (GbpCodeuiBufferAddin  *self,
                                    IdeDiagnosticsManager *manager)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODEUI_BUFFER_ADDIN (self));
  g_assert (IDE_IS_DIAGNOSTICS_MANAGER (manager));

  if (self->file != NULL)
    {
      g_autoptr(IdeDiagnostics) diagnostics = NULL;

      diagnostics = ide_diagnostics_manager_get_diagnostics_for_file (manager, self->file);
      ide_buffer_set_diagnostics (self->buffer, diagnostics);
    }
}

static void
gbp_codeui_buffer_addin_language_set (IdeBufferAddin *addin,
                                      IdeBuffer      *buffer,
                                      const gchar    *language_id)
{
  g_autoptr(IdeContext) context = NULL;
  IdeDiagnosticsManager *diagnostics_manager;
  GFile *file;

  g_assert (GBP_IS_CODEUI_BUFFER_ADDIN (addin));
  g_assert (IDE_IS_BUFFER (buffer));

  context = ide_buffer_ref_context (buffer);
  file = ide_buffer_get_file (buffer);

  g_assert (IDE_IS_CONTEXT (context));
  g_assert (file != NULL);
  g_assert (G_IS_FILE (file));

  diagnostics_manager = ide_diagnostics_manager_from_context (context);
  _ide_diagnostics_manager_language_changed (diagnostics_manager, file, language_id);
}

static void
gbp_codeui_buffer_addin_load (IdeBufferAddin *addin,
                              IdeBuffer      *buffer)
{
  GbpCodeuiBufferAddin *self = (GbpCodeuiBufferAddin *)addin;
  g_autoptr(IdeContext) context = NULL;
  IdeDiagnosticsManager *manager;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODEUI_BUFFER_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  context = ide_buffer_ref_context (buffer);
  manager = ide_diagnostics_manager_from_context (context);

  self->buffer = g_object_ref (buffer);

  g_signal_connect_object (manager,
                           "changed",
                           G_CALLBACK (gbp_codeui_buffer_addin_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
gbp_codeui_buffer_addin_unload (IdeBufferAddin *addin,
                                IdeBuffer      *buffer)
{
  GbpCodeuiBufferAddin *self = (GbpCodeuiBufferAddin *)addin;
  g_autoptr(IdeContext) context = NULL;
  IdeDiagnosticsManager *manager;
  GFile *file;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODEUI_BUFFER_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  context = ide_buffer_ref_context (buffer);
  manager = ide_diagnostics_manager_from_context (context);
  file = ide_buffer_get_file (buffer);

  g_signal_handlers_disconnect_by_func (manager,
                                        G_CALLBACK (gbp_codeui_buffer_addin_changed_cb),
                                        self);

  _ide_diagnostics_manager_file_closed (manager, file);

  g_clear_object (&self->file);
}

static void
buffer_addin_iface_init (IdeBufferAddinInterface *iface)
{
  iface->change_settled = gbp_codeui_buffer_addin_change_settled;
  iface->file_saved = gbp_codeui_buffer_addin_file_saved;
  iface->file_loaded = gbp_codeui_buffer_addin_file_loaded;
  iface->language_set = gbp_codeui_buffer_addin_language_set;
  iface->load = gbp_codeui_buffer_addin_load;
  iface->unload = gbp_codeui_buffer_addin_unload;
}

G_DEFINE_TYPE_WITH_CODE (GbpCodeuiBufferAddin, gbp_codeui_buffer_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_BUFFER_ADDIN, buffer_addin_iface_init))

static void
gbp_codeui_buffer_addin_class_init (GbpCodeuiBufferAddinClass *klass)
{
}

static void
gbp_codeui_buffer_addin_init (GbpCodeuiBufferAddin *self)
{
}
