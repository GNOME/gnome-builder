/* editor-text-buffer-spell-adapter.h
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

#pragma once

#include <gtk/gtk.h>

#include "editor-types.h"

G_BEGIN_DECLS

#define EDITOR_TYPE_TEXT_BUFFER_SPELL_ADAPTER (editor_text_buffer_spell_adapter_get_type())

G_DECLARE_FINAL_TYPE (EditorTextBufferSpellAdapter, editor_text_buffer_spell_adapter, EDITOR, TEXT_BUFFER_SPELL_ADAPTER, GObject)

EditorTextBufferSpellAdapter *editor_text_buffer_spell_adapter_new                 (GtkTextBuffer                *buffer,
                                                                                    EditorSpellChecker           *checker);
GtkTextBuffer      *editor_text_buffer_spell_adapter_get_buffer          (EditorTextBufferSpellAdapter *self);
gboolean            editor_text_buffer_spell_adapter_get_enabled         (EditorTextBufferSpellAdapter *self);
void                editor_text_buffer_spell_adapter_set_enabled         (EditorTextBufferSpellAdapter *self,
                                                                          gboolean                      enabled);
EditorSpellChecker *editor_text_buffer_spell_adapter_get_checker         (EditorTextBufferSpellAdapter *self);
void                editor_text_buffer_spell_adapter_set_checker         (EditorTextBufferSpellAdapter *self,
                                                                          EditorSpellChecker           *checker);
void                editor_text_buffer_spell_adapter_before_insert_text  (EditorTextBufferSpellAdapter *self,
                                                                          guint                         offset,
                                                                          guint                         len);
void                editor_text_buffer_spell_adapter_after_insert_text   (EditorTextBufferSpellAdapter *self,
                                                                          guint                         offset,
                                                                          guint                         len);
void                editor_text_buffer_spell_adapter_before_delete_range (EditorTextBufferSpellAdapter *self,
                                                                          guint                         offset,
                                                                          guint                         len);
void                editor_text_buffer_spell_adapter_after_delete_range  (EditorTextBufferSpellAdapter *self,
                                                                          guint                         offset,
                                                                          guint                         len);
void                editor_text_buffer_spell_adapter_cursor_moved        (EditorTextBufferSpellAdapter *self,
                                                                          guint                         position);
const char         *editor_text_buffer_spell_adapter_get_language        (EditorTextBufferSpellAdapter *self);
void                editor_text_buffer_spell_adapter_set_language        (EditorTextBufferSpellAdapter *self,
                                                                          const char                   *language);
void                editor_text_buffer_spell_adapter_invalidate_all      (EditorTextBufferSpellAdapter *self);
GtkTextTag         *editor_text_buffer_spell_adapter_get_tag             (EditorTextBufferSpellAdapter *self);

G_END_DECLS
