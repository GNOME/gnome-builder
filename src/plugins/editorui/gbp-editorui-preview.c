/* gbp-editorui-preview.c
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

#define G_LOG_DOMAIN "gbp-editorui-preview"

#include "config.h"

#include <libide-gui.h>
#include <libide-sourceview.h>

#include "ide-source-view-private.h"

#include "gbp-editorui-preview.h"

struct _GbpEditoruiPreview
{
  GtkSourceView parent_instance;
  GSettings *editor_settings;
  GtkCssProvider *css_provider;
};

G_DEFINE_TYPE (GbpEditoruiPreview, gbp_editorui_preview, GTK_SOURCE_TYPE_VIEW)

static void
gbp_editorui_preview_load_text (GbpEditoruiPreview *self)
{
  GtkSourceLanguageManager *manager;
  GtkSourceLanguage *lang;
  GtkTextBuffer *buffer;

  IDE_ENTRY;

  g_assert (GBP_IS_EDITORUI_PREVIEW (self));

  manager = gtk_source_language_manager_get_default ();
  lang = gtk_source_language_manager_get_language (manager, "c");
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  gtk_source_buffer_set_language (GTK_SOURCE_BUFFER (buffer), lang);
  gtk_text_buffer_set_text (buffer, "\
#include <glib.h>\n\
typedef struct _type_t type_t;\n\
type_t *type_new (int id);\n\
void type_free (type_t *t);\
", -1);

  IDE_EXIT;
}

static void
notify_style_scheme_cb (GbpEditoruiPreview *self,
                        GParamSpec         *pspec,
                        IdeApplication     *app)
{
  GtkSourceStyleSchemeManager *manager;
  GtkSourceStyleScheme *scheme;
  GtkTextBuffer *buffer;
  const char *name;

  IDE_ENTRY;

  g_assert (GBP_IS_EDITORUI_PREVIEW (self));
  g_assert (IDE_IS_APPLICATION (app));

  name = ide_application_get_style_scheme (app);
  manager = gtk_source_style_scheme_manager_get_default ();
  scheme = gtk_source_style_scheme_manager_get_scheme (manager, name);
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));

  gtk_source_buffer_set_style_scheme (GTK_SOURCE_BUFFER (buffer), scheme);

  IDE_EXIT;
}

static void
gbp_editorui_preview_settings_changed_cb (GbpEditoruiPreview *self,
                                          const char         *key,
                                          GSettings          *settings)
{
  GtkSourceBuffer *buffer;
  gboolean update_css = FALSE;

  g_assert (GBP_IS_EDITORUI_PREVIEW (self));
  g_assert (G_IS_SETTINGS (settings));

  buffer = GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (self)));

  if (!key || ide_str_equal0 (key, "show-grid-lines"))
    gtk_source_view_set_background_pattern (GTK_SOURCE_VIEW (self),
                                            g_settings_get_boolean (settings, "show-grid-lines") ?
                                              GTK_SOURCE_BACKGROUND_PATTERN_TYPE_GRID :
                                              GTK_SOURCE_BACKGROUND_PATTERN_TYPE_NONE);

  if (!key || ide_str_equal0 (key, "highlight-current-line"))
    gtk_source_view_set_highlight_current_line (GTK_SOURCE_VIEW (self),
                                                g_settings_get_boolean (settings, "highlight-current-line"));

  if (!key || ide_str_equal0 (key, "highlight-matching-brackets"))
    gtk_source_buffer_set_highlight_matching_brackets (buffer,
                                                       g_settings_get_boolean (settings, "highlight-matching-brackets"));

  if (!key || ide_str_equal0 (key, "show-line-numbers"))
    gtk_source_view_set_show_line_numbers (GTK_SOURCE_VIEW (self),
                                           g_settings_get_boolean (settings, "show-line-numbers"));

  if (!key || ide_str_equal0 (key, "line-height"))
    update_css = TRUE;

  if (!key || ide_str_equal0 (key, "font-name"))
    update_css = TRUE;

  if (!key || ide_str_equal0 (key, "use-custom-font"))
    update_css = TRUE;

  if (update_css)
    {
      g_autofree char *css = NULL;
      g_autofree char *font_name = NULL;
      PangoFontDescription *font_desc;
      double line_height;

      line_height = g_settings_get_double (settings, "line-height");

      if (g_settings_get_boolean (settings, "use-custom-font"))
        font_name = g_settings_get_string (settings, "font-name");
      else
        font_name = g_strdup (ide_application_get_system_font_name (IDE_APPLICATION_DEFAULT));

      font_desc = pango_font_description_from_string (font_name);

      if ((css = _ide_source_view_generate_css (GTK_SOURCE_VIEW (self), font_desc, 1, line_height)))
        gtk_css_provider_load_from_data (self->css_provider, css, -1);

      g_clear_pointer (&font_desc, pango_font_description_free);
    }

  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
gbp_editorui_preview_constructed (GObject *object)
{
  GbpEditoruiPreview *self = (GbpEditoruiPreview *)object;

  G_OBJECT_CLASS (gbp_editorui_preview_parent_class)->constructed (object);

  g_signal_connect_object (IDE_APPLICATION_DEFAULT,
                           "notify::style-scheme",
                           G_CALLBACK (notify_style_scheme_cb),
                           self,
                           G_CONNECT_SWAPPED);

  notify_style_scheme_cb (self, NULL, IDE_APPLICATION_DEFAULT);

  gbp_editorui_preview_settings_changed_cb (self, NULL, self->editor_settings);

  gbp_editorui_preview_load_text (self);
}

static void
gbp_editorui_preview_dispose (GObject *object)
{
  GbpEditoruiPreview *self = (GbpEditoruiPreview *)object;

  g_clear_object (&self->css_provider);
  g_clear_object (&self->editor_settings);

  G_OBJECT_CLASS (gbp_editorui_preview_parent_class)->dispose (object);
}

static void
gbp_editorui_preview_class_init (GbpEditoruiPreviewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gbp_editorui_preview_constructed;
  object_class->dispose = gbp_editorui_preview_dispose;
}

static void
gbp_editorui_preview_init (GbpEditoruiPreview *self)
{
  static const char *keys[] = {
    "font-name",
    "highlight-current-line",
    "highlight-matching-brackets",
    "line-height",
    "show-grid-lines",
    "show-line-numbers",
  };

  self->editor_settings = g_settings_new ("org.gnome.builder.editor");

  gtk_text_view_set_editable (GTK_TEXT_VIEW (self), FALSE);

  self->css_provider = gtk_css_provider_new ();
  gtk_style_context_add_provider (gtk_widget_get_style_context (GTK_WIDGET (self)),
                                  GTK_STYLE_PROVIDER (self->css_provider),
                                  G_MAXINT);

  gtk_text_view_set_monospace (GTK_TEXT_VIEW (self), TRUE);
  gtk_source_view_set_show_line_numbers (GTK_SOURCE_VIEW (self), TRUE);

  g_object_set (self,
                "left-margin", 6,
                "top-margin", 6,
                "bottom-margin", 6,
                "right-margin", 6,
                NULL);

  g_signal_connect_object (self->editor_settings,
                           "changed",
                           G_CALLBACK (gbp_editorui_preview_settings_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  /* Fetch the key to ensure that changed::key is emitted */
  for (guint i = 0; i < G_N_ELEMENTS (keys); i++)
    g_variant_unref (g_settings_get_value (self->editor_settings, keys[i]));
}

GtkWidget *
gbp_editorui_preview_new (void)
{
  return g_object_new (GBP_TYPE_EDITORUI_PREVIEW, NULL);
}
