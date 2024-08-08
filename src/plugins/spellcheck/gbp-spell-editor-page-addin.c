/* gbp-spell-editor-page-addin.c
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

#define G_LOG_DOMAIN "gbp-spell-editor-page-addin"

#include "config.h"

#include <libide-editor.h>

#include "gbp-spell-buffer-addin.h"
#include "gbp-spell-editor-page-addin.h"

struct _GbpSpellEditorPageAddin
{
  GObject              parent_instance;
  IdeEditorPage       *page;
  GbpSpellBufferAddin *buffer_addin;
};

static void
gbp_spell_editor_page_addin_populate_menu_cb (GbpSpellEditorPageAddin *self,
                                              IdeSourceView           *view)
{
  IDE_ENTRY;

  g_assert (GBP_IS_SPELL_EDITOR_PAGE_ADDIN (self));
  g_assert (IDE_IS_SOURCE_VIEW (view));

  gbp_spell_buffer_addin_update_menu (self->buffer_addin);

  IDE_EXIT;
}

static void
gbp_spell_editor_page_addin_load (IdeEditorPageAddin *addin,
                                  IdeEditorPage      *page)
{
  GbpSpellEditorPageAddin *self = (GbpSpellEditorPageAddin *)addin;
  IdeBufferAddin *buffer_addin;
  IdeSourceView *view;
  GActionGroup *actions;
  GMenuModel *menu;
  IdeBuffer *buffer;

  IDE_ENTRY;

  g_assert (GBP_IS_SPELL_EDITOR_PAGE_ADDIN (self));
  g_assert (IDE_IS_EDITOR_PAGE (page));

  buffer = ide_editor_page_get_buffer (page);
  view = ide_editor_page_get_view (page);
  buffer_addin = ide_buffer_addin_find_by_module_name (buffer, "spellcheck");

  self->page = page;
  self->buffer_addin = GBP_SPELL_BUFFER_ADDIN (buffer_addin);

  actions = gbp_spell_buffer_addin_get_actions (self->buffer_addin);
  gtk_widget_insert_action_group (GTK_WIDGET (view), "spelling", actions);

  menu = gbp_spell_buffer_addin_get_menu (self->buffer_addin);
  ide_source_view_append_menu (view, menu);

  g_signal_connect_object (view,
                           "populate-menu",
                           G_CALLBACK (gbp_spell_editor_page_addin_populate_menu_cb),
                           self,
                           G_CONNECT_SWAPPED);

  IDE_EXIT;
}

static void
gbp_spell_editor_page_addin_unload (IdeEditorPageAddin *addin,
                                    IdeEditorPage      *page)
{
  GbpSpellEditorPageAddin *self = (GbpSpellEditorPageAddin *)addin;
  IdeSourceView *view;
  GMenuModel *menu;

  IDE_ENTRY;

  g_assert (GBP_IS_SPELL_EDITOR_PAGE_ADDIN (self));
  g_assert (IDE_IS_EDITOR_PAGE (page));

  view = ide_editor_page_get_view (page);

  gtk_widget_insert_action_group (GTK_WIDGET (view), "spelling", NULL);

  menu = gbp_spell_buffer_addin_get_menu (self->buffer_addin);
  ide_source_view_remove_menu (view, menu);

  g_signal_handlers_disconnect_by_func (view,
                                        G_CALLBACK (gbp_spell_editor_page_addin_populate_menu_cb),
                                        self);

  self->buffer_addin = NULL;
  self->page = NULL;

  IDE_EXIT;
}

static void
editor_page_addin_iface_init (IdeEditorPageAddinInterface *iface)
{
  iface->load = gbp_spell_editor_page_addin_load;
  iface->unload = gbp_spell_editor_page_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpSpellEditorPageAddin, gbp_spell_editor_page_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_EDITOR_PAGE_ADDIN, editor_page_addin_iface_init))

static void
gbp_spell_editor_page_addin_class_init (GbpSpellEditorPageAddinClass *klass)
{
}

static void
gbp_spell_editor_page_addin_init (GbpSpellEditorPageAddin *self)
{
}
