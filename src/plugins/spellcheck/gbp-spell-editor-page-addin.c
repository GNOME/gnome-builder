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

  /* Borrowed references */
  IdeEditorPage       *page;
  GbpSpellBufferAddin *buffer_addin;

  /* Owned references */
  GMenuModel          *menu;
  GMenu               *spell_section;
  GSimpleActionGroup  *actions;
  char                *spelling_word;
};

static void
gbp_spell_editor_page_addin_add (GSimpleAction *action,
                                 GVariant      *param,
                                 gpointer       user_data)
{
  GbpSpellEditorPageAddin *self = user_data;

  IDE_ENTRY;

  g_assert (G_IS_SIMPLE_ACTION (action));

  gbp_spell_buffer_addin_add_word (self->buffer_addin, self->spelling_word);

  IDE_EXIT;
}

static void
gbp_spell_editor_page_addin_ignore (GSimpleAction *action,
                                    GVariant      *param,
                                    gpointer       user_data)
{
  GbpSpellEditorPageAddin *self = user_data;

  IDE_ENTRY;

  g_assert (G_IS_SIMPLE_ACTION (action));

  gbp_spell_buffer_addin_ignore_word (self->buffer_addin, self->spelling_word);

  IDE_EXIT;
}

static void
gbp_spell_editor_page_addin_correct (GSimpleAction *action,
                                     GVariant      *param,
                                     gpointer       user_data)
{
  GbpSpellEditorPageAddin *self = user_data;
  g_autofree char *slice = NULL;
  IdeSourceView *view;
  GtkTextBuffer *buffer;
  const char *word;
  GtkTextIter begin, end;

  g_assert (GBP_IS_SPELL_EDITOR_PAGE_ADDIN (self));
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE_STRING));
  g_assert (self->spelling_word != NULL);

  view = ide_editor_page_get_view (self->page);
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
  word = g_variant_get_string (param, NULL);

  /* We don't deal with selections (yet?) */
  if (gtk_text_buffer_get_selection_bounds (buffer, &begin, &end))
    return;

  if (!gtk_text_iter_starts_word (&begin))
    gtk_text_iter_backward_word_start (&begin);

  if (!gtk_text_iter_ends_word (&end))
    gtk_text_iter_forward_word_end (&end);

  slice = gtk_text_iter_get_slice (&begin, &end);

  if (g_strcmp0 (slice, self->spelling_word) != 0)
    {
      g_debug ("Words do not match, will not replace.");
      return;
    }

  gtk_text_buffer_begin_user_action (buffer);
  gtk_text_buffer_delete (buffer, &begin, &end);
  gtk_text_buffer_insert (buffer, &begin, word, -1);
  gtk_text_buffer_end_user_action (buffer);
}

static const GActionEntry actions[] = {
  { "add", gbp_spell_editor_page_addin_add },
  { "ignore", gbp_spell_editor_page_addin_ignore },
  { "correct", gbp_spell_editor_page_addin_correct, "s" },
};

static void
set_action_enabled (GSimpleActionGroup *group,
                    const char         *name,
                    gboolean            enabled)
{
  GAction *action;

  if ((action = g_action_map_lookup_action (G_ACTION_MAP (group), name)))
    {
      if (G_IS_SIMPLE_ACTION (action))
        g_simple_action_set_enabled (G_SIMPLE_ACTION (action), enabled);
    }
}

static void
gbp_spell_editor_page_addin_populate_menu_cb (GbpSpellEditorPageAddin *self,
                                              IdeSourceView           *view)
{
  g_auto(GStrv) corrections = NULL;
  g_autofree char *word = NULL;
  GtkTextBuffer *buffer;
  GtkTextIter begin, end;

  IDE_ENTRY;

  g_assert (GBP_IS_SPELL_EDITOR_PAGE_ADDIN (self));
  g_assert (IDE_IS_SOURCE_VIEW (view));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
  gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);

  if (gtk_text_iter_equal (&begin, &end))
    {
      GtkTextIter iter = begin;

      /* Get the word under the cursor */
      if (!gtk_text_iter_starts_word (&begin))
        gtk_text_iter_backward_word_start (&begin);
      end = begin;
      if (!gtk_text_iter_ends_word (&end))
        gtk_text_iter_forward_word_end (&end);
      if (!gtk_text_iter_equal (&begin, &end) &&
          gtk_text_iter_compare (&begin, &iter) <= 0 &&
          gtk_text_iter_compare (&iter, &end) <= 0)
        {
          word = gtk_text_iter_get_slice (&begin, &end);

          if (!gbp_spell_buffer_addin_check_spelling (self->buffer_addin, word))
            corrections = gbp_spell_buffer_addin_list_corrections (self->buffer_addin, word);
          else
            g_clear_pointer (&word, g_free);
        }
    }

  g_free (self->spelling_word);
  self->spelling_word = g_steal_pointer (&word);

  set_action_enabled (self->actions, "add", self->spelling_word != NULL);
  set_action_enabled (self->actions, "ignore", self->spelling_word != NULL);
  editor_spell_menu_set_corrections (self->menu,
                                     self->spelling_word,
                                     (const char * const *)corrections);

  IDE_EXIT;
}

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

  self->page = page;
  self->buffer_addin = GBP_SPELL_BUFFER_ADDIN (buffer_addin);

  self->menu = editor_spell_menu_new ();
  self->spell_section = g_menu_new ();
  g_menu_append_section (self->spell_section, NULL, self->menu);
  ide_source_view_append_menu (view, G_MENU_MODEL (self->spell_section));

  self->actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (self->actions),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   self);
  g_action_map_add_action (G_ACTION_MAP (self->actions),
                           gbp_spell_buffer_addin_get_enabled_action (self->buffer_addin));

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

  IDE_ENTRY;

  g_assert (GBP_IS_SPELL_EDITOR_PAGE_ADDIN (self));
  g_assert (IDE_IS_EDITOR_PAGE (page));

  view = ide_editor_page_get_view (page);
  ide_source_view_remove_menu (view, G_MENU_MODEL (self->spell_section));

  g_signal_handlers_disconnect_by_func (view,
                                        G_CALLBACK (gbp_spell_editor_page_addin_populate_menu_cb),
                                        self);

  g_clear_object (&self->menu);
  g_clear_object (&self->spell_section);
  g_clear_object (&self->actions);

  g_clear_pointer (&self->spelling_word, g_free);

  self->buffer_addin = NULL;
  self->page = NULL;

  IDE_EXIT;
}

static GActionGroup *
gbp_spell_editor_page_addin_ref_action_group (IdeEditorPageAddin *addin)
{
  return g_object_ref (G_ACTION_GROUP (GBP_SPELL_EDITOR_PAGE_ADDIN (addin)->actions));
}

static void
editor_page_addin_iface_init (IdeEditorPageAddinInterface *iface)
{
  iface->load = gbp_spell_editor_page_addin_load;
  iface->unload = gbp_spell_editor_page_addin_unload;
  iface->ref_action_group = gbp_spell_editor_page_addin_ref_action_group;
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
