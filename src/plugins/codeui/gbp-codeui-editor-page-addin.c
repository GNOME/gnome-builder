/* gbp-codeui-editor-page-addin.c
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

#define G_LOG_DOMAIN "gbp-codeui-editor-page-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-editor.h>

#include "gbp-codeui-editor-page-addin.h"
#include "gbp-codeui-rename-dialog.h"

struct _GbpCodeuiEditorPageAddin
{
  GObject        parent_instance;

  IdeEditorPage *page;

  IdeBuffer     *buffer;
  gulong         notify_rename_provider;
  gulong         notify_has_selection;
};

static void rename_symbol_action (GbpCodeuiEditorPageAddin *self,
                                  GVariant                 *params);

IDE_DEFINE_ACTION_GROUP (GbpCodeuiEditorPageAddin, gbp_codeui_editor_page_addin, {
  { "rename-symbol", rename_symbol_action },
})

static void
gbp_codeui_editor_page_addin_update_state (GbpCodeuiEditorPageAddin *self)
{
  IdeRenameProvider *provider;
  gboolean has_selection;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODEUI_EDITOR_PAGE_ADDIN (self));
  g_assert (IDE_IS_BUFFER (self->buffer));

  provider = ide_buffer_get_rename_provider (self->buffer);
  has_selection = gtk_text_buffer_get_has_selection (GTK_TEXT_BUFFER (self->buffer));

  gbp_codeui_editor_page_addin_set_action_enabled (self, "rename-symbol", has_selection && provider);

  IDE_EXIT;
}

static void
gbp_codeui_editor_page_addin_load (IdeEditorPageAddin *addin,
                                   IdeEditorPage      *page)
{
  GbpCodeuiEditorPageAddin *self = (GbpCodeuiEditorPageAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_CODEUI_EDITOR_PAGE_ADDIN (self));
  g_assert (IDE_IS_EDITOR_PAGE (page));

  self->page = page;
  self->buffer = g_object_ref (ide_editor_page_get_buffer (page));

  self->notify_rename_provider =
    g_signal_connect_object (self->buffer,
                             "notify::rename-provider",
                             G_CALLBACK (gbp_codeui_editor_page_addin_update_state),
                             self,
                             G_CONNECT_SWAPPED);

  self->notify_has_selection =
    g_signal_connect_object (self->buffer,
                             "notify::has-selection",
                             G_CALLBACK (gbp_codeui_editor_page_addin_update_state),
                             self,
                             G_CONNECT_SWAPPED);

  gbp_codeui_editor_page_addin_update_state (self);

  IDE_EXIT;
}

static void
gbp_codeui_editor_page_addin_unload (IdeEditorPageAddin *addin,
                                     IdeEditorPage      *page)
{
  GbpCodeuiEditorPageAddin *self = (GbpCodeuiEditorPageAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_CODEUI_EDITOR_PAGE_ADDIN (self));
  g_assert (IDE_IS_EDITOR_PAGE (page));

  g_clear_signal_handler (&self->notify_has_selection, self->buffer);
  g_clear_signal_handler (&self->notify_rename_provider, self->buffer);

  g_clear_object (&self->buffer);

  self->page = NULL;

  IDE_EXIT;
}

static void
editor_page_addin_iface_init (IdeEditorPageAddinInterface *iface)
{
  iface->load = gbp_codeui_editor_page_addin_load;
  iface->unload = gbp_codeui_editor_page_addin_unload;
}

static void
rename_symbol_action (GbpCodeuiEditorPageAddin *self,
                      GVariant                 *param)
{
  g_autoptr(IdeLocation) location = NULL;
  IdeRenameProvider *provider;
  GtkTextBuffer *buffer;
  GtkTextIter begin, end;
  GtkWidget *dialog;
  g_autofree char *word = NULL;
  GtkWindow *toplevel;
  gboolean failed = FALSE;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODEUI_EDITOR_PAGE_ADDIN (self));
  g_assert (IDE_IS_BUFFER (self->buffer));

  if (!(provider = ide_buffer_get_rename_provider (self->buffer)))
    IDE_EXIT;

  buffer = GTK_TEXT_BUFFER (self->buffer);

  if (!gtk_text_buffer_get_selection_bounds (buffer, &begin, &end))
    IDE_EXIT;

  gtk_text_iter_order (&begin, &end);

  for (GtkTextIter iter = begin;
       gtk_text_iter_compare (&iter, &end) < 0;
       gtk_text_iter_forward_char (&iter))
    {
      gunichar ch = gtk_text_iter_get_char (&iter);

      if (g_unichar_isspace (ch))
        {
          failed = TRUE;
          break;
        }
    }

  toplevel = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self->page)));

  if (failed)
    {
      dialog = adw_message_dialog_new (toplevel,
                                       _("Symbol Not Selected"),
                                       _("A symbol to rename must be selected"));
      adw_message_dialog_add_response (ADW_MESSAGE_DIALOG (dialog), "close", _("Close"));
      gtk_window_present (GTK_WINDOW (dialog));

      IDE_EXIT;
    }

  word = gtk_text_iter_get_slice (&begin, &end);
  location = ide_buffer_get_iter_location (self->buffer, &begin);

  dialog = gbp_codeui_rename_dialog_new (provider, location, word);
  gtk_window_set_transient_for (GTK_WINDOW (dialog), toplevel);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_present (GTK_WINDOW (dialog));

  IDE_EXIT;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpCodeuiEditorPageAddin, gbp_codeui_editor_page_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, gbp_codeui_editor_page_addin_init_action_group)
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_EDITOR_PAGE_ADDIN, editor_page_addin_iface_init))

static void
gbp_codeui_editor_page_addin_class_init (GbpCodeuiEditorPageAddinClass *klass)
{
}

static void
gbp_codeui_editor_page_addin_init (GbpCodeuiEditorPageAddin *self)
{
}
