/* gbp-spell-editor-view-addin.c
 *
 * Copyright © 2016 Sebastien Lafargue <slafargue@gnome.org>
 * Copyright © 2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-spell-editor-view-addin"

#include "config.h"

#include <dazzle.h>

#include <gspell/gspell.h>
#include <glib/gi18n.h>

#include "gbp-spell-buffer-addin.h"
#include "gbp-spell-editor-addin.h"
#include "gbp-spell-editor-view-addin.h"
#include "gbp-spell-navigator.h"
#include "gbp-spell-private.h"
#include "gbp-spell-utils.h"

#define SPELLCHECKER_SUBREGION_LENGTH 500

#define I_(s) g_intern_static_string(s)

struct _GbpSpellEditorViewAddin
{
  GObject          parent_instance;

  /* Borrowed references */
  IdeEditorView   *view;
  GtkTextMark     *word_begin;
  GtkTextMark     *word_end;
  GtkTextMark     *start_boundary;
  GtkTextMark     *end_boundary;

  /* Owned references */
  DzlBindingGroup *buffer_addin_bindings;
  GspellNavigator *navigator;

  gint             checking_count;
};

static void
gbp_spell_editor_view_addin_begin (GSimpleAction *action,
                                   GVariant      *variant,
                                   gpointer       user_data)
{
  GbpSpellEditorViewAddin *self = user_data;
  IdeEditorAddin *addin;
  GtkWidget *editor;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_SPELL_EDITOR_VIEW_ADDIN (self));

  editor = gtk_widget_get_ancestor (GTK_WIDGET (self->view), IDE_TYPE_EDITOR_PERSPECTIVE);
  addin = ide_editor_addin_find_by_module_name (IDE_EDITOR_PERSPECTIVE (editor), "spellcheck-plugin");
  _gbp_spell_editor_addin_begin (GBP_SPELL_EDITOR_ADDIN (addin), self->view);
}

static void
gbp_spell_editor_view_addin_cancel (GSimpleAction *action,
                                    GVariant      *variant,
                                    gpointer       user_data)
{
  GbpSpellEditorViewAddin *self = user_data;
  IdeEditorAddin *addin;
  GtkWidget *editor;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_SPELL_EDITOR_VIEW_ADDIN (self));

  editor = gtk_widget_get_ancestor (GTK_WIDGET (self->view), IDE_TYPE_EDITOR_PERSPECTIVE);
  addin = ide_editor_addin_find_by_module_name (IDE_EDITOR_PERSPECTIVE (editor), "spellcheck-plugin");
  _gbp_spell_editor_addin_cancel (GBP_SPELL_EDITOR_ADDIN (addin), self->view);
}

static const GActionEntry actions[] = {
  { "spellcheck", gbp_spell_editor_view_addin_begin },
  { "cancel-spellcheck", gbp_spell_editor_view_addin_cancel },
};

  static const DzlShortcutEntry spellchecker_shortcut_entry[] = {
    { "org.gnome.builder.editor-view.spellchecker",
      0, NULL,
      NC_("shortcut window", "Editor shortcuts"),
      NC_("shortcut window", "Editing"),
      NC_("shortcut window", "Show the spellchecker panel") },
  };

static void
gbp_spell_editor_view_addin_load (IdeEditorViewAddin *addin,
                                  IdeEditorView      *view)
{
  GbpSpellEditorViewAddin *self = (GbpSpellEditorViewAddin *)addin;
  g_autoptr(GSimpleActionGroup) group = NULL;
  g_autoptr(GPropertyAction) enabled_action = NULL;
  DzlShortcutController *controller;
  IdeBufferAddin *buffer_addin;
  GspellTextView *wrapper;
  IdeSourceView *source_view;
  IdeBuffer *buffer;

  g_assert (GBP_IS_SPELL_EDITOR_VIEW_ADDIN (self));
  g_assert (IDE_IS_EDITOR_VIEW (view));

  self->view = view;

  source_view = ide_editor_view_get_view (view);
  g_assert (source_view != NULL);
  g_assert (IDE_IS_SOURCE_VIEW (source_view));

  buffer = ide_editor_view_get_buffer (view);
  g_assert (buffer != NULL);
  g_assert (IDE_IS_BUFFER (buffer));

  buffer_addin = ide_buffer_addin_find_by_module_name (buffer, "spellcheck-plugin");

  if (!GBP_IS_SPELL_BUFFER_ADDIN (buffer_addin))
    {
      /* We might find ourselves in a race here and the buffer
       * addins are already in destruction. Therefore, silently
       * fail any further setup.
       */
      g_warning ("Failed to locate buffer addin, spellcheck disabled");
      return;
    }

  wrapper = gspell_text_view_get_from_gtk_text_view (GTK_TEXT_VIEW (source_view));
  g_assert (wrapper != NULL);
  g_assert (GSPELL_IS_TEXT_VIEW (wrapper));

  self->buffer_addin_bindings = dzl_binding_group_new ();
  dzl_binding_group_bind (self->buffer_addin_bindings, "enabled",
                          wrapper, "enable-language-menu",
                          G_BINDING_SYNC_CREATE);
  dzl_binding_group_bind (self->buffer_addin_bindings, "enabled",
                          wrapper, "inline-spell-checking",
                          G_BINDING_SYNC_CREATE);
  dzl_binding_group_set_source (self->buffer_addin_bindings, buffer_addin);

  group = g_simple_action_group_new ();
  enabled_action = g_property_action_new ("enabled", buffer_addin, "enabled");
  g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (enabled_action));
  g_action_map_add_action_entries (G_ACTION_MAP (group), actions, G_N_ELEMENTS (actions), self);
  gtk_widget_insert_action_group (GTK_WIDGET (view), "spellcheck", G_ACTION_GROUP (group));

  controller = dzl_shortcut_controller_find (GTK_WIDGET (view));
  dzl_shortcut_controller_add_command_action (controller,
                                              "org.gnome.builder.editor-view.spellchecker",
                                              I_("<shift>F7"),
                                              DZL_SHORTCUT_PHASE_CAPTURE,
                                              "spellcheck.spellcheck");

  dzl_shortcut_manager_add_shortcut_entries (NULL,
                                             spellchecker_shortcut_entry,
                                             G_N_ELEMENTS (spellchecker_shortcut_entry),
                                             GETTEXT_PACKAGE);
}

static void
gbp_spell_editor_view_addin_unload (IdeEditorViewAddin *addin,
                                    IdeEditorView      *view)
{
  GbpSpellEditorViewAddin *self = (GbpSpellEditorViewAddin *)addin;

  g_assert (GBP_IS_SPELL_EDITOR_VIEW_ADDIN (self));
  g_assert (IDE_IS_EDITOR_VIEW (view));

  gtk_widget_insert_action_group (GTK_WIDGET (view), "spellcheck", NULL);

  if (self->buffer_addin_bindings != NULL)
    {
      dzl_binding_group_set_source (self->buffer_addin_bindings, NULL);
      g_clear_object (&self->buffer_addin_bindings);
    }

  g_clear_object (&self->navigator);

  self->view = NULL;
}

static void
editor_view_addin_iface_init (IdeEditorViewAddinInterface *iface)
{
  iface->load = gbp_spell_editor_view_addin_load;
  iface->unload = gbp_spell_editor_view_addin_unload;
}

G_DEFINE_TYPE_WITH_CODE (GbpSpellEditorViewAddin, gbp_spell_editor_view_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_EDITOR_VIEW_ADDIN, editor_view_addin_iface_init))

static void
gbp_spell_editor_view_addin_class_init (GbpSpellEditorViewAddinClass *klass)
{
}

static void
gbp_spell_editor_view_addin_init (GbpSpellEditorViewAddin *self)
{
}

/**
 * gbp_spell_editor_view_addin_begin_checking:
 * @self: a #GbpSpellEditorViewAddin
 *
 * This function should be called by the #GbpSpellWidget to enable
 * spellchecking on the textview and underlying buffer. Doing so allows the
 * inline-spellchecking and language-menu to be dynamically enabled even if
 * spellchecking is typically disabled in the buffer.
 *
 * The caller should call gbp_spell_editor_view_addin_end_checking() when they
 * have completed the spellchecking process.
 *
 * Since: 3.26
 */
void
gbp_spell_editor_view_addin_begin_checking (GbpSpellEditorViewAddin *self)
{
  GObject *buffer_addin;

  g_return_if_fail (GBP_IS_SPELL_EDITOR_VIEW_ADDIN (self));
  g_return_if_fail (self->view != NULL);
  g_return_if_fail (self->checking_count >= 0);

  self->checking_count++;

  buffer_addin = dzl_binding_group_get_source (self->buffer_addin_bindings);

  if (buffer_addin == NULL)
    {
      g_warning ("Attempt to start spellchecking without a buffer addin");
      return;
    }

  if (self->checking_count == 1)
    {
      IdeSourceView *view;
      GtkTextBuffer *buffer;
      GtkTextIter begin;
      GtkTextIter end;

      gbp_spell_buffer_addin_begin_checking (GBP_SPELL_BUFFER_ADDIN (buffer_addin));

      view = ide_editor_view_get_view (self->view);
      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

      /* Use the selected range, otherwise whole buffer */
      if (!gtk_text_buffer_get_selection_bounds (buffer, &begin, &end))
        gtk_text_buffer_get_bounds (buffer, &begin, &end);

      /* The selection might begin in the middle of a word */
      if (gbp_spell_utils_text_iter_inside_word (&begin) &&
          !gbp_spell_utils_text_iter_starts_word (&begin))
        gbp_spell_utils_text_iter_backward_word_start (&begin);

      /* And also at the end */
      if (gbp_spell_utils_text_iter_inside_word (&end))
        gbp_spell_utils_text_iter_forward_word_end (&end);

      /* Place current position at the beginning of the selection */
      self->word_begin = gtk_text_buffer_create_mark (buffer, NULL, &begin, TRUE);
      self->word_end = gtk_text_buffer_create_mark (buffer, NULL, &begin, FALSE);

      /* Setup our acceptable range for checking */
      self->start_boundary = gtk_text_buffer_create_mark (buffer, NULL, &begin, TRUE);
      self->end_boundary = gtk_text_buffer_create_mark (buffer, NULL, &end, FALSE);
    }
}

/**
 * gbp_spell_editor_view_addin_end_checking:
 * @self: a #GbpSpellEditorViewAddin
 *
 * Completes a spellcheck operation and potentially restores the buffer to
 * the visual state before spellchecking started.
 *
 * Since: 3.26
 */
void
gbp_spell_editor_view_addin_end_checking (GbpSpellEditorViewAddin *self)
{
  g_return_if_fail (GBP_IS_SPELL_EDITOR_VIEW_ADDIN (self));
  g_return_if_fail (self->checking_count >= 0);

  self->checking_count--;

  if (self->checking_count == 0)
    {
      GObject *buffer_addin;

      buffer_addin = dzl_binding_group_get_source (self->buffer_addin_bindings);

      if (GBP_IS_SPELL_BUFFER_ADDIN (buffer_addin))
        gbp_spell_buffer_addin_end_checking (GBP_SPELL_BUFFER_ADDIN (buffer_addin));

      if (self->view != NULL)
        {
          IdeBuffer *buffer = ide_editor_view_get_buffer (self->view);

          /*
           * We could be in disposal here, so its possible the buffer has
           * already been cleared and released.
           */

          if (buffer != NULL)
            {
              gtk_text_buffer_delete_mark (GTK_TEXT_BUFFER (buffer), self->word_begin);
              gtk_text_buffer_delete_mark (GTK_TEXT_BUFFER (buffer), self->word_end);
              gtk_text_buffer_delete_mark (GTK_TEXT_BUFFER (buffer), self->start_boundary);
              gtk_text_buffer_delete_mark (GTK_TEXT_BUFFER (buffer), self->end_boundary);
            }
        }

      self->word_begin = NULL;
      self->word_end = NULL;
      self->start_boundary = NULL;
      self->end_boundary = NULL;

      g_clear_object (&self->navigator);
    }
}

/**
 * gbp_spell_editor_view_addin_get_checker:
 * @self: a #GbpSpellEditorViewAddin
 *
 * This function may return %NULL before
 * gbp_spell_editor_view_addin_begin_checking() has been called.
 *
 * Returns: (nullable) (transfer none): a #GspellChecker or %NULL
 *
 * Since: 3.26
 */
GspellChecker *
gbp_spell_editor_view_addin_get_checker (GbpSpellEditorViewAddin *self)
{
  GObject *buffer_addin;

  g_return_val_if_fail (GBP_IS_SPELL_EDITOR_VIEW_ADDIN (self), NULL);

  buffer_addin = dzl_binding_group_get_source (self->buffer_addin_bindings);
  if (GBP_IS_SPELL_BUFFER_ADDIN (buffer_addin))
    return gbp_spell_buffer_addin_get_checker (GBP_SPELL_BUFFER_ADDIN (buffer_addin));

  return NULL;
}

/**
 * gbp_spell_editor_view_addin_get_navigator:
 * @self: a #GbpSpellEditorViewAddin
 *
 * This function may return %NULL before
 * gbp_spell_editor_view_addin_begin_checking() has been called.
 *
 * Returns: (nullable) (transfer none): a #GspellNavigator or %NULL
 *
 * Since: 3.26
 */
GspellNavigator *
gbp_spell_editor_view_addin_get_navigator (GbpSpellEditorViewAddin *self)
{
  g_return_val_if_fail (GBP_IS_SPELL_EDITOR_VIEW_ADDIN (self), NULL);

  if (self->navigator == NULL)
    {
      if (self->view != NULL)
        {
          IdeSourceView *view = ide_editor_view_get_view (self->view);

          self->navigator = gbp_spell_navigator_new (GTK_TEXT_VIEW (view));
          if (self->navigator)
            g_object_ref_sink (self->navigator);
        }
    }

  return self->navigator;
}
