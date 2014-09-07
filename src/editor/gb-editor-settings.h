/* gb-editor-settings.h
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

#ifndef GB_EDITOR_SETTINGS_H
#define GB_EDITOR_SETTINGS_H

#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>

G_BEGIN_DECLS

#define GB_TYPE_EDITOR_SETTINGS            (gb_editor_settings_get_type())
#define GB_EDITOR_SETTINGS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_EDITOR_SETTINGS, GbEditorSettings))
#define GB_EDITOR_SETTINGS_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_EDITOR_SETTINGS, GbEditorSettings const))
#define GB_EDITOR_SETTINGS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_EDITOR_SETTINGS, GbEditorSettingsClass))
#define GB_IS_EDITOR_SETTINGS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_EDITOR_SETTINGS))
#define GB_IS_EDITOR_SETTINGS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_EDITOR_SETTINGS))
#define GB_EDITOR_SETTINGS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_EDITOR_SETTINGS, GbEditorSettingsClass))

typedef struct _GbEditorSettings        GbEditorSettings;
typedef struct _GbEditorSettingsClass   GbEditorSettingsClass;
typedef struct _GbEditorSettingsPrivate GbEditorSettingsPrivate;

struct _GbEditorSettings
{
  GObject parent;

  /*< private >*/
  GbEditorSettingsPrivate *priv;
};

struct _GbEditorSettingsClass
{
  GObjectClass parent_class;
};

GType                       gb_editor_settings_get_type                          (void) G_GNUC_CONST;
gboolean                    gb_editor_settings_get_auto_indent                   (GbEditorSettings           *settings);
void                        gb_editor_settings_set_auto_indent                   (GbEditorSettings           *settings,
                                                                                  gboolean                    auto_indent);
gboolean                    gb_editor_settings_get_show_line_numbers             (GbEditorSettings           *settings);
void                        gb_editor_settings_set_show_line_numbers             (GbEditorSettings           *settings,
                                                                                  gboolean                    show_line_numbers);
gboolean                    gb_editor_settings_get_show_line_marks               (GbEditorSettings           *settings);
void                        gb_editor_settings_set_show_line_marks               (GbEditorSettings           *settings,
                                                                                  gboolean                    show_line_marks);
gboolean                    gb_editor_settings_get_show_right_margin             (GbEditorSettings           *settings);
void                        gb_editor_settings_set_show_right_margin             (GbEditorSettings           *settings,
                                                                                  gboolean                    show_right_margin);
guint                       gb_editor_settings_get_right_margin_position         (GbEditorSettings           *settings);
void                        gb_editor_settings_set_right_margin_position         (GbEditorSettings           *settings,
                                                                                  guint                       right_margin_position);
gboolean                    gb_editor_settings_get_smart_home_end                (GbEditorSettings           *settings);
void                        gb_editor_settings_set_smart_home_end                (GbEditorSettings           *settings,
                                                                                  gboolean                    smart_home_end);
guint                       gb_editor_settings_get_tab_width                     (GbEditorSettings           *settings);
void                        gb_editor_settings_set_tab_width                     (GbEditorSettings           *settings,
                                                                                  guint                       tab_width);
gboolean                    gb_editor_settings_get_highlight_current_line        (GbEditorSettings           *settings);
void                        gb_editor_settings_set_highlight_current_line        (GbEditorSettings           *settings,
                                                                                  gboolean                    highlight_current_line);
gboolean                    gb_editor_settings_get_indent_on_tab                 (GbEditorSettings           *settings);
void                        gb_editor_settings_set_indent_on_tab                 (GbEditorSettings           *settings,
                                                                                  gboolean                    indent_on_tab);
guint                       gb_editor_settings_get_indent_width                  (GbEditorSettings           *settings);
void                        gb_editor_settings_set_indent_width                  (GbEditorSettings           *settings,
                                                                                  guint                       indent_width);
gboolean                    gb_editor_settings_get_insert_spaces_instead_of_tabs (GbEditorSettings           *settings);
void                        gb_editor_settings_set_insert_spaces_instead_of_tabs (GbEditorSettings           *settings,
                                                                                  gboolean                    insert_spaces_instead_of_tabs);
const PangoFontDescription *gb_editor_settings_get_font_desc                     (GbEditorSettings           *settings);
void                        gb_editor_settings_set_font_desc                     (GbEditorSettings           *settings,
                                                                                  const PangoFontDescription *font_desc);
GtkSourceStyleScheme       *gb_editor_settings_get_style_scheme                  (GbEditorSettings           *settings);
void                        gb_editor_settings_set_style_scheme                  (GbEditorSettings           *settings,
                                                                                  GtkSourceStyleScheme       *style_scheme);


G_END_DECLS

#endif /* GB_EDITOR_SETTINGS_H */
