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

#include "editor-spell-menu.h"

#include "gbp-spell-buffer-addin.h"
#include "gbp-spell-editor-page-addin.h"

struct _GbpSpellEditorPageAddin
{
  GObject              parent_instance;
  GbpSpellBufferAddin *buffer_addin;
  GMenuModel          *menu;
  GSimpleActionGroup  *actions;
};

static void
gbp_spell_editor_page_addin_add (GSimpleAction *action,
                                 GVariant      *param,
                                 gpointer       user_data)
{
  GbpSpellEditorPageAddin *self = user_data;
  const char *word;

  IDE_ENTRY;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (param != NULL);
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE_STRING));

  word = g_variant_get_string (param, NULL);
  gbp_spell_buffer_addin_add_word (self->buffer_addin, word);

  IDE_EXIT;
}

static void
gbp_spell_editor_page_addin_ignore (GSimpleAction *action,
                                    GVariant      *param,
                                    gpointer       user_data)
{
  GbpSpellEditorPageAddin *self = user_data;
  const char *word;

  IDE_ENTRY;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (param != NULL);
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE_STRING));

  word = g_variant_get_string (param, NULL);
  gbp_spell_buffer_addin_ignore_word (self->buffer_addin, word);

  IDE_EXIT;
}

static const GActionEntry actions[] = {
  { "add", gbp_spell_editor_page_addin_add, "s" },
  { "ignore", gbp_spell_editor_page_addin_ignore, "s" },
};

static void
gbp_spell_editor_page_addin_load (IdeEditorPageAddin *addin,
                                  IdeEditorPage      *page)
{
  GbpSpellEditorPageAddin *self = (GbpSpellEditorPageAddin *)addin;
  IdeBufferAddin *buffer_addin;
  IdeSourceView *view;
  IdeBuffer *buffer;

  IDE_ENTRY;

  g_assert (GBP_IS_SPELL_EDITOR_PAGE_ADDIN (self));
  g_assert (IDE_IS_EDITOR_PAGE (page));

  buffer = ide_editor_page_get_buffer (page);
  view = ide_editor_page_get_view (page);

  buffer_addin = ide_buffer_addin_find_by_module_name (buffer, "spellcheck");
  self->buffer_addin = GBP_SPELL_BUFFER_ADDIN (buffer_addin);

  self->menu = editor_spell_menu_new ();
  ide_source_view_append_menu (view, self->menu);

  self->actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (self->actions),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   self);
  gtk_widget_insert_action_group (GTK_WIDGET (page),
                                  "spelling",
                                  G_ACTION_GROUP (self->actions));

  IDE_EXIT;
}

static void
gbp_spell_editor_page_addin_unload (IdeEditorPageAddin *addin,
                                    IdeEditorPage      *page)
{
  GbpSpellEditorPageAddin *self = (GbpSpellEditorPageAddin *)addin;
  IdeSourceView *view;

  IDE_ENTRY;

  g_assert (GBP_IS_SPELL_EDITOR_PAGE_ADDIN (self));
  g_assert (IDE_IS_EDITOR_PAGE (page));

  gtk_widget_insert_action_group (GTK_WIDGET (page), "spelling", NULL);

  view = ide_editor_page_get_view (page);
  ide_source_view_remove_menu (view, self->menu);

  g_clear_object (&self->menu);
  g_clear_object (&self->actions);

  self->buffer_addin = NULL;

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
