/* gb-editor-settings.c
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

#define G_LOG_DOMAIN "editor-settings"

#include <glib/gi18n.h>

#include "gb-editor-settings.h"
#include "gb-log.h"

#define DEFAULT_FONT "Monospace 11"
#define DEFAULT_SCHEME "tango"

struct _GbEditorSettingsPrivate
{
  PangoFontDescription *font_desc;
  GtkSourceStyleScheme *style_scheme;

  gboolean              auto_indent;
  gboolean              highlight_current_line;
  gboolean              highlight_matching_brackets;
  gboolean              insert_spaces_instead_of_tabs;
  gboolean              show_line_marks;
  gboolean              show_line_numbers;
  gboolean              show_right_margin;
  gboolean              smart_home_end;

  guint                 right_margin_position;
  guint                 tab_width;
};

enum {
  PROP_0,
  PROP_AUTO_INDENT,
  PROP_FONT_DESC,
  PROP_HIGHLIGHT_CURRENT_LINE,
  PROP_HIGHLIGHT_MATCHING_BRACKETS,
  PROP_INSERT_SPACES_INSTEAD_OF_TABS,
  PROP_RIGHT_MARGIN_POSITION,
  PROP_SHOW_LINE_MARKS,
  PROP_SHOW_LINE_NUMBERS,
  PROP_SHOW_RIGHT_MARGIN,
  PROP_SMART_HOME_END,
  PROP_STYLE_SCHEME,
  PROP_STYLE_SCHEME_NAME,
  PROP_TAB_WIDTH,
  LAST_PROP
};

G_DEFINE_TYPE_WITH_PRIVATE (GbEditorSettings, gb_editor_settings, G_TYPE_OBJECT)

static GParamSpec * gParamSpecs[LAST_PROP];

GbEditorSettings *
gb_editor_settings_new_for_language (const gchar *language_id)
{
  GbEditorSettings *ret;
  GSettings *settings;
  gchar *path;

  g_return_val_if_fail (language_id, NULL);

  path = g_strdup_printf ("/org/gnome/builder/editor/language/%s/",
                          language_id);
  settings = g_settings_new_with_path ("org.gnome.builder.editor.language",
                                       path);

  ret = g_object_new (GB_TYPE_EDITOR_SETTINGS, NULL);

#define BIND(name) \
  g_settings_bind (settings, name, ret, name, G_SETTINGS_BIND_DEFAULT)

  BIND ("auto-indent");
  BIND ("highlight-current-line");
  BIND ("highlight-matching-brackets");
  BIND ("insert-spaces-instead-of-tabs");
  BIND ("right-margin-position");
  BIND ("show-line-marks");
  BIND ("show-line-numbers");
  BIND ("show-right-margin");
  BIND ("smart-home-end");
  BIND ("tab-width");

#undef BIND

  g_object_unref (settings);
  g_free (path);

  return ret;
}

GtkSourceStyleScheme *
gb_editor_settings_get_style_scheme (GbEditorSettings *settings)
{
  g_return_val_if_fail (GB_IS_EDITOR_SETTINGS (settings), NULL);

  return settings->priv->style_scheme;
}

void
gb_editor_settings_set_style_scheme (GbEditorSettings     *settings,
                                     GtkSourceStyleScheme *style_scheme)
{
  g_return_if_fail (GB_IS_EDITOR_SETTINGS (settings));
  g_return_if_fail (!style_scheme || GTK_SOURCE_IS_STYLE_SCHEME (style_scheme));

  if (style_scheme == settings->priv->style_scheme)
    return;

  if (settings->priv->style_scheme)
    g_clear_object (&settings->priv->style_scheme);

  if (style_scheme)
    settings->priv->style_scheme = g_object_ref (style_scheme);

  g_object_notify_by_pspec (G_OBJECT (settings),
                            gParamSpecs[PROP_STYLE_SCHEME]);
}

const gchar *
gb_editor_settings_get_style_scheme_name (GbEditorSettings *settings)
{
  g_return_val_if_fail (GB_IS_EDITOR_SETTINGS (settings), NULL);

  if (settings->priv->style_scheme)
    return gtk_source_style_scheme_get_name (settings->priv->style_scheme);

  return NULL;
}

void
gb_editor_settings_set_style_scheme_name (GbEditorSettings *settings,
                                          const gchar      *name)
{
  GtkSourceStyleSchemeManager *manager;
  GtkSourceStyleScheme *scheme = NULL;

  g_return_if_fail (GB_IS_EDITOR_SETTINGS (settings));

  if (name)
    {
      manager = gtk_source_style_scheme_manager_get_default ();
      scheme = gtk_source_style_scheme_manager_get_scheme (manager, name);
    }

  gb_editor_settings_set_style_scheme (settings, scheme);
}

const PangoFontDescription *
gb_editor_settings_get_font_desc (GbEditorSettings *settings)
{
  g_return_val_if_fail (GB_IS_EDITOR_SETTINGS (settings), NULL);

  return settings->priv->font_desc;
}

void
gb_editor_settings_set_font_desc (GbEditorSettings           *settings,
                                  const PangoFontDescription *font_desc)
{
  GbEditorSettingsPrivate *priv;

  g_return_if_fail (GB_IS_EDITOR_SETTINGS (settings));

  priv = settings->priv;

  if (priv->font_desc != font_desc)
    {
      g_clear_pointer (&priv->font_desc, pango_font_description_free);

      if (font_desc)
        priv->font_desc = pango_font_description_copy (font_desc);
      else
        priv->font_desc = pango_font_description_from_string (DEFAULT_FONT);

      g_object_notify_by_pspec (G_OBJECT (settings),
                                gParamSpecs[PROP_FONT_DESC]);
    }
}

gboolean
gb_editor_settings_get_auto_indent (GbEditorSettings *settings)
{
  g_return_val_if_fail (GB_IS_EDITOR_SETTINGS (settings), FALSE);

  return settings->priv->auto_indent;
}

void
gb_editor_settings_set_auto_indent (GbEditorSettings *settings,
                                    gboolean          auto_indent)
{
  g_return_if_fail (GB_IS_EDITOR_SETTINGS (settings));

  settings->priv->auto_indent = auto_indent;
  g_object_notify_by_pspec (G_OBJECT (settings),
                            gParamSpecs[PROP_AUTO_INDENT]);
}

gboolean
gb_editor_settings_get_highlight_current_line (GbEditorSettings *settings)
{
  g_return_val_if_fail (GB_IS_EDITOR_SETTINGS (settings), FALSE);

  return settings->priv->highlight_current_line;
}

void
gb_editor_settings_set_highlight_current_line (GbEditorSettings *settings,
                                               gboolean          highlight_current_line)
{
  g_return_if_fail (GB_IS_EDITOR_SETTINGS (settings));

  settings->priv->highlight_current_line = highlight_current_line;
  g_object_notify_by_pspec (G_OBJECT (settings),
                            gParamSpecs[PROP_HIGHLIGHT_CURRENT_LINE]);
}

gboolean
gb_editor_settings_get_highlight_matching_brackets (GbEditorSettings *settings)
{
  g_return_val_if_fail (GB_IS_EDITOR_SETTINGS (settings), FALSE);

  return settings->priv->highlight_matching_brackets;
}

void
gb_editor_settings_set_highlight_matching_brackets (GbEditorSettings *settings,
                                                    gboolean          highlight_matching_brackets)
{
  g_return_if_fail (GB_IS_EDITOR_SETTINGS (settings));

  settings->priv->highlight_matching_brackets = highlight_matching_brackets;
  g_object_notify_by_pspec (G_OBJECT (settings),
                            gParamSpecs [PROP_HIGHLIGHT_MATCHING_BRACKETS]);
}

gboolean
gb_editor_settings_get_insert_spaces_instead_of_tabs (GbEditorSettings *settings)
{
  g_return_val_if_fail (GB_IS_EDITOR_SETTINGS (settings), FALSE);

  return settings->priv->insert_spaces_instead_of_tabs;
}

void
gb_editor_settings_set_insert_spaces_instead_of_tabs (GbEditorSettings *settings,
                                                      gboolean          insert_spaces_instead_of_tabs)
{
  g_return_if_fail (GB_IS_EDITOR_SETTINGS (settings));

  settings->priv->insert_spaces_instead_of_tabs = insert_spaces_instead_of_tabs;
  g_object_notify_by_pspec (G_OBJECT (settings),
                            gParamSpecs[PROP_INSERT_SPACES_INSTEAD_OF_TABS]);
}

gboolean
gb_editor_settings_get_show_line_marks (GbEditorSettings *settings)
{
  g_return_val_if_fail (GB_IS_EDITOR_SETTINGS (settings), FALSE);

  return settings->priv->show_line_marks;
}

void
gb_editor_settings_set_show_line_marks (GbEditorSettings *settings,
                                        gboolean          show_line_marks)
{
  g_return_if_fail (GB_IS_EDITOR_SETTINGS (settings));

  settings->priv->show_line_marks = show_line_marks;
  g_object_notify_by_pspec (G_OBJECT (settings),
                            gParamSpecs[PROP_SHOW_LINE_MARKS]);
}

gboolean
gb_editor_settings_get_show_line_numbers (GbEditorSettings *settings)
{
  g_return_val_if_fail (GB_IS_EDITOR_SETTINGS (settings), FALSE);

  return settings->priv->show_line_numbers;
}

void
gb_editor_settings_set_show_line_numbers (GbEditorSettings *settings,
                                          gboolean          show_line_numbers)
{
  g_return_if_fail (GB_IS_EDITOR_SETTINGS (settings));

  settings->priv->show_line_numbers = show_line_numbers;
  g_object_notify_by_pspec (G_OBJECT (settings),
                            gParamSpecs[PROP_SHOW_LINE_NUMBERS]);
}

gboolean
gb_editor_settings_get_show_right_margin (GbEditorSettings *settings)
{
  g_return_val_if_fail (GB_IS_EDITOR_SETTINGS (settings), FALSE);

  return settings->priv->show_right_margin;
}

void
gb_editor_settings_set_show_right_margin (GbEditorSettings *settings,
                                          gboolean          show_right_margin)
{
  g_return_if_fail (GB_IS_EDITOR_SETTINGS (settings));

  settings->priv->show_right_margin = show_right_margin;
  g_object_notify_by_pspec (G_OBJECT (settings),
                            gParamSpecs[PROP_SHOW_RIGHT_MARGIN]);
}

gboolean
gb_editor_settings_get_smart_home_end (GbEditorSettings *settings)
{
  g_return_val_if_fail (GB_IS_EDITOR_SETTINGS (settings), FALSE);

  return settings->priv->smart_home_end;
}

void
gb_editor_settings_set_smart_home_end (GbEditorSettings *settings,
                                       gboolean          smart_home_end)
{
  g_return_if_fail (GB_IS_EDITOR_SETTINGS (settings));

  settings->priv->smart_home_end = smart_home_end;
  g_object_notify_by_pspec (G_OBJECT (settings),
                            gParamSpecs[PROP_SMART_HOME_END]);
}

guint
gb_editor_settings_get_tab_width (GbEditorSettings *settings)
{
  g_return_val_if_fail (GB_IS_EDITOR_SETTINGS (settings), 0);

  return settings->priv->tab_width;
}

void
gb_editor_settings_set_tab_width (GbEditorSettings *settings,
                                  guint             tab_width)
{
  g_return_if_fail (GB_IS_EDITOR_SETTINGS (settings));

  settings->priv->tab_width = tab_width;
  g_object_notify_by_pspec (G_OBJECT (settings),
                            gParamSpecs[PROP_TAB_WIDTH]);
}

guint
gb_editor_settings_get_right_margin_position (GbEditorSettings *settings)
{
  g_return_val_if_fail (GB_IS_EDITOR_SETTINGS (settings), 0);

  return settings->priv->right_margin_position;
}

void
gb_editor_settings_set_right_margin_position (GbEditorSettings *settings,
                                              guint             right_margin_position)
{
  g_return_if_fail (GB_IS_EDITOR_SETTINGS (settings));

  settings->priv->right_margin_position = right_margin_position;
  g_object_notify_by_pspec (G_OBJECT (settings),
                            gParamSpecs[PROP_RIGHT_MARGIN_POSITION]);
}

static void
gb_editor_settings_finalize (GObject *object)
{
  GbEditorSettingsPrivate *priv;

  ENTRY;

  priv = GB_EDITOR_SETTINGS (object)->priv;

  g_clear_object (&priv->style_scheme);
  g_clear_pointer (&priv->font_desc, pango_font_description_free);

  G_OBJECT_CLASS (gb_editor_settings_parent_class)->finalize (object);

  EXIT;
}

static void
gb_editor_settings_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GbEditorSettings *settings = GB_EDITOR_SETTINGS (object);

  switch (prop_id)
    {
    case PROP_AUTO_INDENT:
      g_value_set_boolean (value, gb_editor_settings_get_auto_indent (settings));
      break;

    case PROP_HIGHLIGHT_CURRENT_LINE:
      g_value_set_boolean (value, gb_editor_settings_get_highlight_current_line (settings));
      break;

    case PROP_HIGHLIGHT_MATCHING_BRACKETS:
      g_value_set_boolean (value, gb_editor_settings_get_highlight_matching_brackets (settings));
      break;

    case PROP_INSERT_SPACES_INSTEAD_OF_TABS:
      g_value_set_boolean (value, gb_editor_settings_get_insert_spaces_instead_of_tabs (settings));
      break;

    case PROP_SHOW_LINE_MARKS:
      g_value_set_boolean (value, gb_editor_settings_get_show_line_marks (settings));
      break;

    case PROP_SHOW_LINE_NUMBERS:
      g_value_set_boolean (value, gb_editor_settings_get_show_line_numbers (settings));
      break;

    case PROP_SHOW_RIGHT_MARGIN:
      g_value_set_boolean (value, gb_editor_settings_get_show_right_margin (settings));
      break;

    case PROP_SMART_HOME_END:
      g_value_set_boolean (value, gb_editor_settings_get_smart_home_end (settings));
      break;

    case PROP_TAB_WIDTH:
      g_value_set_uint (value, gb_editor_settings_get_tab_width (settings));
      break;

    case PROP_RIGHT_MARGIN_POSITION:
      g_value_set_uint (value, gb_editor_settings_get_right_margin_position (settings));
      break;

    case PROP_FONT_DESC:
      g_value_set_boxed (value, gb_editor_settings_get_font_desc (settings));
      break;

    case PROP_STYLE_SCHEME:
      g_value_set_object (value, gb_editor_settings_get_style_scheme (settings));
      break;

    case PROP_STYLE_SCHEME_NAME:
      g_value_set_string (value, gb_editor_settings_get_style_scheme_name (settings));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_editor_settings_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GbEditorSettings *settings = GB_EDITOR_SETTINGS (object);

  switch (prop_id)
    {
    case PROP_AUTO_INDENT:
      gb_editor_settings_set_auto_indent (settings, g_value_get_boolean (value));
      break;

    case PROP_HIGHLIGHT_CURRENT_LINE:
      gb_editor_settings_set_highlight_current_line (settings, g_value_get_boolean (value));
      break;

    case PROP_HIGHLIGHT_MATCHING_BRACKETS:
      gb_editor_settings_set_highlight_matching_brackets (settings, g_value_get_boolean (value));
      break;

    case PROP_INSERT_SPACES_INSTEAD_OF_TABS:
      gb_editor_settings_set_insert_spaces_instead_of_tabs (settings, g_value_get_boolean (value));
      break;

    case PROP_SHOW_LINE_MARKS:
      gb_editor_settings_set_show_line_marks (settings, g_value_get_boolean (value));
      break;

    case PROP_SHOW_LINE_NUMBERS:
      gb_editor_settings_set_show_line_numbers (settings, g_value_get_boolean (value));
      break;

    case PROP_SHOW_RIGHT_MARGIN:
      gb_editor_settings_set_show_right_margin (settings, g_value_get_boolean (value));
      break;

    case PROP_SMART_HOME_END:
      gb_editor_settings_set_smart_home_end (settings, g_value_get_boolean (value));
      break;

    case PROP_TAB_WIDTH:
      gb_editor_settings_set_tab_width (settings, g_value_get_uint (value));
      break;

    case PROP_RIGHT_MARGIN_POSITION:
      gb_editor_settings_set_right_margin_position (settings, g_value_get_uint (value));
      break;

    case PROP_FONT_DESC:
      gb_editor_settings_set_font_desc (settings, g_value_get_boxed (value));
      break;

    case PROP_STYLE_SCHEME:
      gb_editor_settings_set_style_scheme (settings, g_value_get_object (value));
      break;

    case PROP_STYLE_SCHEME_NAME:
      gb_editor_settings_set_style_scheme_name (settings, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_editor_settings_class_init (GbEditorSettingsClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = gb_editor_settings_finalize;
  object_class->get_property = gb_editor_settings_get_property;
  object_class->set_property = gb_editor_settings_set_property;

  gParamSpecs[PROP_AUTO_INDENT] =
    g_param_spec_boolean ("auto-indent",
                          _("auto indent"),
                          _("auto indent"),
                          TRUE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_AUTO_INDENT,
                                   gParamSpecs[PROP_AUTO_INDENT]);

  gParamSpecs[PROP_HIGHLIGHT_CURRENT_LINE] =
    g_param_spec_boolean ("highlight-current-line",
                          _("highlight current line"),
                          _("highlight current line"),
                          TRUE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_HIGHLIGHT_CURRENT_LINE,
                                   gParamSpecs[PROP_HIGHLIGHT_CURRENT_LINE]);

  gParamSpecs [PROP_HIGHLIGHT_MATCHING_BRACKETS] =
    g_param_spec_boolean ("highlight-matching-brackets",
                         _("Highlight Matching Brackets"),
                         _("Highlight Matching Brackets"),
                         TRUE,
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_HIGHLIGHT_MATCHING_BRACKETS,
                                   gParamSpecs [PROP_HIGHLIGHT_MATCHING_BRACKETS]);

  gParamSpecs[PROP_INSERT_SPACES_INSTEAD_OF_TABS] =
    g_param_spec_boolean ("insert-spaces-instead-of-tabs",
                          _("insert spaces instead of tabs"),
                          _("insert spaces instead of tabs"),
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_INSERT_SPACES_INSTEAD_OF_TABS,
                                   gParamSpecs[PROP_INSERT_SPACES_INSTEAD_OF_TABS]);

  gParamSpecs[PROP_SHOW_LINE_MARKS] =
    g_param_spec_boolean ("show-line-marks",
                          _("show line marks"),
                          _("show line marks"),
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SHOW_LINE_MARKS,
                                   gParamSpecs[PROP_SHOW_LINE_MARKS]);

  gParamSpecs[PROP_SHOW_LINE_NUMBERS] =
    g_param_spec_boolean ("show-line-numbers",
                          _("show line numbers"),
                          _("show line numbers"),
                          TRUE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SHOW_LINE_NUMBERS,
                                   gParamSpecs[PROP_SHOW_LINE_NUMBERS]);

  gParamSpecs[PROP_SHOW_RIGHT_MARGIN] =
    g_param_spec_boolean ("show-right-margin",
                          _("show right margin"),
                          _("show right margin"),
                          TRUE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SHOW_RIGHT_MARGIN,
                                   gParamSpecs[PROP_SHOW_RIGHT_MARGIN]);

  gParamSpecs[PROP_SMART_HOME_END] =
    g_param_spec_boolean ("smart-home-end",
                          _("smart home end"),
                          _("smart home end"),
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SMART_HOME_END,
                                   gParamSpecs[PROP_SMART_HOME_END]);

  gParamSpecs[PROP_RIGHT_MARGIN_POSITION] =
    g_param_spec_uint ("right-margin-position",
                       _("Right Margin Position"),
                       _("The position of the right margin, if any."),
                       1,
                       1000,
                       80,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_RIGHT_MARGIN_POSITION,
                                   gParamSpecs[PROP_RIGHT_MARGIN_POSITION]);

  gParamSpecs[PROP_TAB_WIDTH] =
    g_param_spec_uint ("tab-width",
                       _("Tab Width"),
                       _("The width of tabs."),
                       1,
                       32,
                       2,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_TAB_WIDTH,
                                   gParamSpecs[PROP_TAB_WIDTH]);

  gParamSpecs[PROP_FONT_DESC] =
    g_param_spec_boxed ("font-desc",
                        _("Font Description"),
                        _("A PangoFontDescription to be used."),
                        PANGO_TYPE_FONT_DESCRIPTION,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_FONT_DESC,
                                   gParamSpecs[PROP_FONT_DESC]);

  gParamSpecs[PROP_STYLE_SCHEME] =
    g_param_spec_object ("style-scheme",
                         _("Style Scheme"),
                         _("The style scheme to use in the source view."),
                         GTK_SOURCE_TYPE_STYLE_SCHEME,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_STYLE_SCHEME,
                                   gParamSpecs[PROP_STYLE_SCHEME]);

  gParamSpecs [PROP_STYLE_SCHEME_NAME] =
    g_param_spec_string ("style-scheme-name",
                         _("Style Scheme Name"),
                         _("The name of the style scheme."),
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_STYLE_SCHEME_NAME,
                                   gParamSpecs [PROP_STYLE_SCHEME_NAME]);
}

static void
gb_editor_settings_init (GbEditorSettings *settings)
{
  GtkSourceStyleSchemeManager *ssm;
  GtkSourceStyleScheme *scheme;
  PangoFontDescription *font_desc;

  ENTRY;

  settings->priv = gb_editor_settings_get_instance_private (settings);

  ssm = gtk_source_style_scheme_manager_get_default ();
  scheme = gtk_source_style_scheme_manager_get_scheme (ssm, DEFAULT_SCHEME);

  font_desc = pango_font_description_from_string (DEFAULT_FONT);

  settings->priv->auto_indent = FALSE;
  settings->priv->show_right_margin = TRUE;
  settings->priv->highlight_current_line = TRUE;
  settings->priv->highlight_matching_brackets = TRUE;
  settings->priv->show_line_numbers = TRUE;
  settings->priv->right_margin_position = 80;
  settings->priv->insert_spaces_instead_of_tabs = TRUE;
  settings->priv->tab_width = 2;
  settings->priv->font_desc = font_desc;
  settings->priv->style_scheme = g_object_ref (scheme);

  EXIT;
}
