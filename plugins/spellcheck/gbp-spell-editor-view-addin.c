/* gbp-spell-editor-view-addin.c
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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

#include "gbp-spell-buffer-addin.h"
#include "gbp-spell-editor-view-addin.h"

struct _GbpSpellEditorViewAddin
{
  GObject          parent_instance;
  DzlBindingGroup *buffer_addin_bindings;
};

static void
gbp_spell_editor_view_addin_load (IdeEditorViewAddin *addin,
                                  IdeEditorView      *view)
{
  GbpSpellEditorViewAddin *self = (GbpSpellEditorViewAddin *)addin;
  g_autoptr(DzlPropertiesGroup) group = NULL;
  IdeBufferAddin *buffer_addin;
  GspellTextView *wrapper;
  IdeSourceView *source_view;
  IdeBuffer *buffer;

  g_assert (GBP_IS_SPELL_EDITOR_VIEW_ADDIN (self));
  g_assert (IDE_IS_EDITOR_VIEW (view));

  source_view = ide_editor_view_get_view (view);
  g_assert (source_view != NULL);
  g_assert (IDE_IS_SOURCE_VIEW (source_view));

  buffer = ide_editor_view_get_buffer (view);
  g_assert (buffer != NULL);
  g_assert (IDE_IS_BUFFER (buffer));

  buffer_addin = ide_buffer_addin_find_by_module_name (buffer, "spellcheck-plugin");
  g_assert (buffer_addin != NULL);
  g_assert (GBP_IS_SPELL_BUFFER_ADDIN (buffer_addin));

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

  group = dzl_properties_group_new (G_OBJECT (buffer_addin));
  dzl_properties_group_add_all_properties (group);
  gtk_widget_insert_action_group (GTK_WIDGET (view), "spellcheck", G_ACTION_GROUP (group));
}

static void
gbp_spell_editor_view_addin_unload (IdeEditorViewAddin *addin,
                                    IdeEditorView      *view)
{
  GbpSpellEditorViewAddin *self = (GbpSpellEditorViewAddin *)addin;

  g_assert (GBP_IS_SPELL_EDITOR_VIEW_ADDIN (self));
  g_assert (IDE_IS_EDITOR_VIEW (view));

  gtk_widget_insert_action_group (GTK_WIDGET (view), "spellcheck", NULL);
  dzl_binding_group_set_source (self->buffer_addin_bindings, NULL);
  g_clear_object (&self->buffer_addin_bindings);
}

static void
editor_view_addin_iface_init (IdeEditorViewAddinInterface *iface)
{
  iface->load = gbp_spell_editor_view_addin_load;
  iface->unload = gbp_spell_editor_view_addin_unload;
}

G_DEFINE_TYPE_WITH_CODE (GbpSpellEditorViewAddin, gbp_spell_editor_view_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_EDITOR_VIEW_ADDIN,
                                                editor_view_addin_iface_init))

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

  buffer_addin = dzl_binding_group_get_source (self->buffer_addin_bindings);

  if (buffer_addin == NULL)
    {
      g_warning ("Attempt to start spellchecking after disposal");
      return;
    }

  gbp_spell_buffer_addin_begin_checking (GBP_SPELL_BUFFER_ADDIN (buffer_addin));
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
  GObject *buffer_addin;

  g_return_if_fail (GBP_IS_SPELL_EDITOR_VIEW_ADDIN (self));

  buffer_addin = dzl_binding_group_get_source (self->buffer_addin_bindings);

  if (GBP_IS_SPELL_BUFFER_ADDIN (buffer_addin))
    gbp_spell_buffer_addin_end_checking (GBP_SPELL_BUFFER_ADDIN (buffer_addin));
}
