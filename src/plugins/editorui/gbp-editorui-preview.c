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

#include "gbp-editorui-preview.h"

struct _GbpEditoruiPreview
{
  GtkSourceView parent_instance;
  GSettings *editor_settings;
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

static gboolean
show_grid_lines_to_bg (GValue   *value,
                       GVariant *variant,
                       gpointer  user_data)
{
  if (g_variant_get_boolean (variant))
    g_value_set_enum (value, GTK_SOURCE_BACKGROUND_PATTERN_TYPE_GRID);
  else
    g_value_set_enum (value, GTK_SOURCE_BACKGROUND_PATTERN_TYPE_NONE);
  return TRUE;
}

static void
gbp_editorui_preview_constructed (GObject *object)
{
  GbpEditoruiPreview *self = (GbpEditoruiPreview *)object;
  GtkTextBuffer *buffer;

  G_OBJECT_CLASS (gbp_editorui_preview_parent_class)->constructed (object);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));

  g_signal_connect_object (IDE_APPLICATION_DEFAULT,
                           "notify::style-scheme",
                           G_CALLBACK (notify_style_scheme_cb),
                           self,
                           G_CONNECT_SWAPPED);

  notify_style_scheme_cb (self, NULL, IDE_APPLICATION_DEFAULT);

  g_settings_bind_with_mapping (self->editor_settings,
                                "show-grid-lines", self, "background-pattern",
                                G_SETTINGS_BIND_GET,
                                show_grid_lines_to_bg, NULL, NULL, NULL);
  g_settings_bind (self->editor_settings,
                   "highlight-current-line", self, "highlight-current-line",
                   G_SETTINGS_BIND_GET);
  g_settings_bind (self->editor_settings,
                   "highlight-matching-brackets", buffer, "highlight-matching-brackets",
                   G_SETTINGS_BIND_GET);
  g_settings_bind (self->editor_settings,
                   "show-line-numbers", self, "show-line-numbers",
                   G_SETTINGS_BIND_GET);

  gbp_editorui_preview_load_text (self);
}

static void
gbp_editorui_preview_dispose (GObject *object)
{
  GbpEditoruiPreview *self = (GbpEditoruiPreview *)object;

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
  self->editor_settings = g_settings_new ("org.gnome.builder.editor");

  gtk_text_view_set_monospace (GTK_TEXT_VIEW (self), TRUE);
  gtk_source_view_set_show_line_numbers (GTK_SOURCE_VIEW (self), TRUE);

  g_object_set (self,
                "left-margin", 6,
                "top-margin", 6,
                "bottom-margin", 6,
                "right-margin", 6,
                NULL);
}

GtkWidget *
gbp_editorui_preview_new (void)
{
  return g_object_new (GBP_TYPE_EDITORUI_PREVIEW, NULL);
}
