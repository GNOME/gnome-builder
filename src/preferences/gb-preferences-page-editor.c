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

#define G_LOG_DOMAIN "prefs-page-editor"

#include <egg-settings-flag-action.h>
#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>

#include "gb-preferences-page-editor.h"
#include "gb-widget.h"

struct _GbPreferencesPageEditor
{
  GbPreferencesPage                  parent_instance;

  GSettings                         *editor_settings;
  GtkSwitch                         *restore_insert_mark_switch;
  GtkSwitch                         *show_diff_switch;
  GtkSwitch                         *show_line_numbers_switch;
  GtkSwitch                         *highlight_current_line_switch;
  GtkSwitch                         *highlight_matching_brackets_switch;
  GtkSpinButton                     *scroll_off_spin;
  GtkFontButton                     *font_button;
  GtkAdjustment                     *scroll_off_adjustment;
  GtkBox                            *scroll_off_container;
  GtkWidget                         *auto_hide_map_switch;
  GtkWidget                         *show_map_switch;
  GtkCheckButton                    *draw_spaces_space;
  GtkCheckButton                    *draw_spaces_tab;
  GtkCheckButton                    *draw_spaces_newline;
  GtkCheckButton                    *draw_spaces_nbsp;
  GtkCheckButton                    *draw_spaces_leading;
  GtkCheckButton                    *draw_spaces_trailing;
};

G_DEFINE_TYPE (GbPreferencesPageEditor, gb_preferences_page_editor, GB_TYPE_PREFERENCES_PAGE)

static void
gb_preferences_page_editor_constructed (GObject *object)
{
  GbPreferencesPageEditor *self = (GbPreferencesPageEditor *)object;

  g_assert (GB_IS_PREFERENCES_PAGE_EDITOR (self));

  G_OBJECT_CLASS (gb_preferences_page_editor_parent_class)->constructed (object);

  g_settings_bind (self->editor_settings, "scroll-offset",
                   self->scroll_off_adjustment, "value",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->editor_settings, "font-name",
                   self->font_button, "font-name",
                   G_SETTINGS_BIND_DEFAULT);
}

static void
gb_preferences_page_editor_class_init (GbPreferencesPageEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gb_preferences_page_editor_constructed;

  GB_WIDGET_CLASS_TEMPLATE (widget_class, "gb-preferences-page-editor.ui");

  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, show_map_switch);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, editor_settings);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, font_button);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, highlight_current_line_switch);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, highlight_matching_brackets_switch);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, auto_hide_map_switch);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, restore_insert_mark_switch);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, scroll_off_adjustment);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, scroll_off_container);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, scroll_off_spin);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, show_diff_switch);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, show_line_numbers_switch);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, draw_spaces_space);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, draw_spaces_tab);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, draw_spaces_newline);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, draw_spaces_nbsp);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, draw_spaces_leading);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, draw_spaces_trailing);
}

static void
gb_preferences_page_editor_init (GbPreferencesPageEditor *self)
{
  static const gchar *keys[] = { "space", "tab", "newline", "nbsp", "trailing", "leading", NULL };
  GActionMap *actions;
  guint i;

  gtk_widget_init_template (GTK_WIDGET (self));

  actions = G_ACTION_MAP (g_simple_action_group_new ());
  gtk_widget_insert_action_group (GTK_WIDGET (self), "editor", G_ACTION_GROUP (actions));

  for (i = 0; keys [i]; i++)
    {
      GAction *action;

      action = egg_settings_flag_action_new ("org.gnome.builder.editor", "draw-spaces", keys [i]);
      g_action_map_add_action (actions, action);
    }

  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
  /* To translators: This is a list of keywords for the preferences page */
                                               _("restore insert cursor mark"),
                                               self->restore_insert_mark_switch,
                                               NULL);
  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
  /* To translators: This is a list of keywords for the preferences page */
                                               _("diff renderer gutter changes git vcs"),
                                               self->show_diff_switch,
                                               NULL);
  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
  /* To translators: This is a list of keywords for the preferences page */
                                               _("line numbers"),
                                               self->show_line_numbers_switch,
                                               NULL);
  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
  /* To translators: This is a list of keywords for the preferences page */
                                               _("line lines highlight current"),
                                               self->highlight_current_line_switch,
                                               NULL);
  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
  /* To translators: This is a list of keywords for the preferences page */
                                               _("bracket brackets highlight matching"),
                                               self->highlight_matching_brackets_switch,
                                               NULL);
  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
  /* To translators: This is a list of keywords for the preferences page */
                                               _("lines margin scrolloff scroll off"),
                                               self->scroll_off_container,
                                               self->scroll_off_spin,
                                               NULL);
  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
  /* To translators: This is a list of keywords for the preferences page */
                                               _("font document editor monospace"),
                                               self->font_button,
                                               NULL);
  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
  /* To translators: This is a list of keywords for the preferences page */
                                               _("minimap mini map overview over view"),
                                               self->show_map_switch,
                                               self->auto_hide_map_switch,
                                               NULL);
  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
  /* To translators: This is a list of keywords for the preferences page */
                                               _("draw spaces space tab newline nbsp non-breaking whitespace trailing leading"),
                                               self->draw_spaces_space,
                                               self->draw_spaces_tab,
                                               self->draw_spaces_newline,
                                               self->draw_spaces_nbsp,
                                               self->draw_spaces_leading,
                                               self->draw_spaces_trailing,
                                               NULL);
}
