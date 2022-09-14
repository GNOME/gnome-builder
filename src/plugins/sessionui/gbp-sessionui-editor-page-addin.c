/* gbp-sessionui-editor-page-addin.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-sessionui-editor-page-addin"

#include <libide-code.h>
#include <libide-editor.h>
#include <libide-gui.h>

#include "gbp-sessionui-editor-page-addin.h"

struct _GbpSessionuiEditorPageAddin
{
  GObject        parent_instance;
  IdeEditorPage *page;
  IdeBuffer     *buffer;
  gulong         modified_changed_handler;
  guint          logout_inhibited : 1;
};

static void
gbp_sessionui_editor_page_addin_modified_changed_cb (GbpSessionuiEditorPageAddin *self,
                                                     IdeBuffer                   *buffer)
{
  gboolean inhibit_logout;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SESSIONUI_EDITOR_PAGE_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  inhibit_logout = gtk_text_buffer_get_modified (GTK_TEXT_BUFFER (buffer));

  if (inhibit_logout != self->logout_inhibited)
    {
      IdeWorkspace *workspace = ide_widget_get_workspace (GTK_WIDGET (self->page));

      if (inhibit_logout)
        ide_workspace_inhibit_logout (workspace);
      else
        ide_workspace_uninhibit_logout (workspace);

      self->logout_inhibited = inhibit_logout;
    }
}

static void
gbp_sessionui_editor_page_addin_load (IdeEditorPageAddin *addin,
                                      IdeEditorPage      *page)
{
  GbpSessionuiEditorPageAddin *self = (GbpSessionuiEditorPageAddin *)addin;
  IdeBuffer *buffer;

  g_assert (GBP_IS_SESSIONUI_EDITOR_PAGE_ADDIN (self));
  g_assert (IDE_IS_EDITOR_PAGE (page));

  buffer = ide_editor_page_get_buffer (page);

  self->page = page;
  self->buffer = g_object_ref (buffer);
  self->modified_changed_handler =
    g_signal_connect_object (self->buffer,
                             "modified-changed",
                             G_CALLBACK (gbp_sessionui_editor_page_addin_modified_changed_cb),
                             self,
                             G_CONNECT_SWAPPED);
  gbp_sessionui_editor_page_addin_modified_changed_cb (self, self->buffer);
}

static void
gbp_sessionui_editor_page_addin_unload (IdeEditorPageAddin *addin,
                                        IdeEditorPage      *page)
{
  GbpSessionuiEditorPageAddin *self = (GbpSessionuiEditorPageAddin *)addin;

  g_assert (GBP_IS_SESSIONUI_EDITOR_PAGE_ADDIN (self));
  g_assert (IDE_IS_EDITOR_PAGE (page));

  g_clear_signal_handler (&self->modified_changed_handler, self->buffer);
  g_clear_object (&self->buffer);
  self->page = NULL;
}

static void
editor_page_addin_iface_init (IdeEditorPageAddinInterface *iface)
{
  iface->load = gbp_sessionui_editor_page_addin_load;
  iface->unload = gbp_sessionui_editor_page_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpSessionuiEditorPageAddin, gbp_sessionui_editor_page_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_EDITOR_PAGE_ADDIN, editor_page_addin_iface_init))

static void
gbp_sessionui_editor_page_addin_class_init (GbpSessionuiEditorPageAddinClass *klass)
{
}

static void
gbp_sessionui_editor_page_addin_init (GbpSessionuiEditorPageAddin *self)
{
}
