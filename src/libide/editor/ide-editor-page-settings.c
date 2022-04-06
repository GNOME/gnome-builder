/* ide-editor-page-settings.c
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

#define G_LOG_DOMAIN "ide-editor-page-settings"

#include "config.h"

#include "ide-editor-page-private.h"

static gboolean
indent_style_to_insert_spaces (GBinding     *binding,
                               const GValue *from,
                               GValue       *to,
                               gpointer      user_data)
{
  g_assert (G_IS_BINDING (binding));
  g_assert (G_VALUE_HOLDS_ENUM (from));
  g_assert (G_VALUE_HOLDS_BOOLEAN (to));
  g_assert (user_data == NULL);

  if (g_value_get_enum (from) == IDE_INDENT_STYLE_TABS)
    g_value_set_boolean (to, FALSE);
  else
    g_value_set_boolean (to, TRUE);

  return TRUE;
}

void
_ide_editor_page_settings_reload (IdeEditorPage *self)
{
  IdeFileSettings *file_settings;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_EDITOR_PAGE (self));
  g_return_if_fail (IDE_IS_BUFFER (self->buffer));
  g_return_if_fail (IDE_IS_SOURCE_VIEW (self->view));
  g_return_if_fail (IDE_IS_BINDING_GROUP (self->buffer_file_settings));

  file_settings = ide_buffer_get_file_settings (self->buffer);

  ide_binding_group_set_source (self->buffer_file_settings, file_settings);
  ide_binding_group_set_source (self->view_file_settings, file_settings);

  IDE_EXIT;
}

static gboolean
show_map_to_vscrollbar_policy (GValue   *value,
                               GVariant *variant,
                               gpointer  user_data)
{
  if (g_variant_get_boolean (variant))
    g_value_set_enum (value, GTK_POLICY_EXTERNAL);
  else
    g_value_set_enum (value, GTK_POLICY_AUTOMATIC);

  return TRUE;
}

void
_ide_editor_page_settings_init (IdeEditorPage *self)
{
  static GSettings *editor_settings;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_EDITOR_PAGE (self));
  g_return_if_fail (IDE_IS_SOURCE_VIEW (self->view));
  g_return_if_fail (IDE_IS_BUFFER (self->buffer));
  g_return_if_fail (self->buffer_file_settings == NULL);
  g_return_if_fail (self->view_file_settings == NULL);

  if (editor_settings == NULL)
    editor_settings = g_settings_new ("org.gnome.builder.editor");

  g_object_bind_property (IDE_APPLICATION_DEFAULT, "style-scheme",
                          self->buffer, "style-scheme-name",
                          G_BINDING_SYNC_CREATE);

  self->buffer_file_settings = ide_binding_group_new ();
  ide_binding_group_bind (self->buffer_file_settings,
                          "insert-trailing-newline", self->buffer, "implicit-trailing-newline",
                          G_BINDING_SYNC_CREATE);

  self->view_file_settings = ide_binding_group_new ();
  ide_binding_group_bind (self->view_file_settings,
                          "auto-indent", self->view, "auto-indent",
                          G_BINDING_SYNC_CREATE);
  ide_binding_group_bind_full (self->view_file_settings,
                               "indent-style", self->view, "insert-spaces-instead-of-tabs",
                               G_BINDING_SYNC_CREATE,
                               indent_style_to_insert_spaces, NULL, NULL, NULL);
  ide_binding_group_bind (self->view_file_settings,
                          "indent-width", self->view, "indent-width",
                          G_BINDING_SYNC_CREATE);
  ide_binding_group_bind (self->view_file_settings,
                          "right-margin-position", self->view, "right-margin-position",
                          G_BINDING_SYNC_CREATE);
  ide_binding_group_bind (self->view_file_settings,
                          "show-right-margin", self->view, "show-right-margin",
                          G_BINDING_SYNC_CREATE);
  ide_binding_group_bind (self->view_file_settings,
                          "tab-width", self->view, "tab-width",
                          G_BINDING_SYNC_CREATE);

  g_settings_bind (editor_settings, "show-map",
                   self->map_revealer, "reveal-child",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind_with_mapping (editor_settings, "show-map",
                                self->scroller, "vscrollbar-policy",
                                G_SETTINGS_BIND_GET,
                                show_map_to_vscrollbar_policy,
                                NULL, NULL, NULL);

#if 0
  ide_binding_group_bind (self->view_file_settings,
                          "insert-matching-brace", self->view, "insert-matching-brace",
                          G_BINDING_SYNC_CREATE);
  ide_binding_group_bind (self->view_file_settings,
                          "overwrite-braces", self->view, "overwrite-braces",
                          G_BINDING_SYNC_CREATE);
#endif

  _ide_editor_page_settings_reload (self);

  IDE_EXIT;
}
