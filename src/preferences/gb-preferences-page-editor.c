/* gb-preferences-page-editor.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#include "gb-preferences-page-editor.h"
#include "gb-source-style-scheme-button.h"

struct _GbPreferencesPageEditorPrivate
{
  GSettings                 *settings;

  GtkSwitch                 *restore_insert_mark_switch;
  GtkSwitch                 *show_diff_switch;
  GtkSwitch                 *vim_mode_switch;
  GtkSwitch                 *word_completion_switch;
  GtkFontButton             *font_button;
  GbSourceStyleSchemeButton *style_scheme_button;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbPreferencesPageEditor, gb_preferences_page_editor,
                            GB_TYPE_PREFERENCES_PAGE)

static void
gb_preferences_page_editor_constructed (GObject *object)
{
  GbPreferencesPageEditorPrivate *priv;
  GbPreferencesPageEditor *editor = (GbPreferencesPageEditor *)object;

  g_return_if_fail (GB_IS_PREFERENCES_PAGE_EDITOR (editor));

  priv = editor->priv;

  priv->settings = g_settings_new ("org.gnome.builder.editor");

  g_settings_bind (priv->settings, "vim-mode", priv->vim_mode_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (priv->settings, "restore-insert-mark",
                   priv->restore_insert_mark_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (priv->settings, "show-diff",
                   priv->show_diff_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (priv->settings, "word-completion",
                   priv->word_completion_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (priv->settings, "font-name",
                   priv->font_button, "font-name",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (priv->settings, "style-scheme-name",
                   priv->style_scheme_button, "style-scheme-name",
                   G_SETTINGS_BIND_DEFAULT);

  G_OBJECT_CLASS (gb_preferences_page_editor_parent_class)->constructed (object);
}

static void
gb_preferences_page_editor_finalize (GObject *object)
{
  GbPreferencesPageEditorPrivate *priv = GB_PREFERENCES_PAGE_EDITOR (object)->priv;

  g_clear_object (&priv->settings);

  G_OBJECT_CLASS (gb_preferences_page_editor_parent_class)->finalize (object);
}

static void
gb_preferences_page_editor_class_init (GbPreferencesPageEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gb_preferences_page_editor_constructed;
  object_class->finalize = gb_preferences_page_editor_finalize;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/builder/ui/gb-preferences-page-editor.ui");

  gtk_widget_class_bind_template_child_private (widget_class, GbPreferencesPageEditor, font_button);
  gtk_widget_class_bind_template_child_private (widget_class, GbPreferencesPageEditor, restore_insert_mark_switch);
  gtk_widget_class_bind_template_child_private (widget_class, GbPreferencesPageEditor, show_diff_switch);
  gtk_widget_class_bind_template_child_private (widget_class, GbPreferencesPageEditor, style_scheme_button);
  gtk_widget_class_bind_template_child_private (widget_class, GbPreferencesPageEditor, vim_mode_switch);
  gtk_widget_class_bind_template_child_private (widget_class, GbPreferencesPageEditor, word_completion_switch);

  g_type_ensure (GB_TYPE_SOURCE_STYLE_SCHEME_BUTTON);
}

static void
gb_preferences_page_editor_init (GbPreferencesPageEditor *self)
{
  self->priv = gb_preferences_page_editor_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));
}
