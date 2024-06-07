/*
 * gbp-git-commit-entry.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#include <libide-gui.h>
#include <libide-sourceview.h>

#include "ide-source-view-private.h"

#include "gbp-git-commit-entry.h"

struct _GbpGitCommitEntry
{
  GtkSourceView   parent_instance;
  GSettings      *editor_settings;
  GtkCssProvider *css_provider;
};

enum {
  PROP_0,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GbpGitCommitEntry, gbp_git_commit_entry, GTK_SOURCE_TYPE_VIEW)

static GParamSpec *properties[N_PROPS];

static gboolean
style_scheme_name_to_object (GBinding     *binding,
                             const GValue *value,
                             GValue       *to_value,
                             gpointer      user_data)
{
  const char *name = g_value_get_string (value);

  if (name != NULL)
    {
      GtkSourceStyleSchemeManager *m = gtk_source_style_scheme_manager_get_default ();
      g_value_set_object (to_value, gtk_source_style_scheme_manager_get_scheme (m, name));
    }

  return TRUE;
}

static void
editor_settings_changed_cb (GbpGitCommitEntry *self,
                            const char        *key,
                            GSettings         *settings)
{
  GtkSourceBuffer *buffer;
  gboolean update_css = FALSE;

  g_assert (GBP_IS_GIT_COMMIT_ENTRY (self));
  g_assert (G_IS_SETTINGS (settings));

  buffer = GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (self)));

  if (!key || ide_str_equal0 (key, "show-grid-lines"))
    gtk_source_view_set_background_pattern (GTK_SOURCE_VIEW (self),
                                            g_settings_get_boolean (settings, "show-grid-lines") ?
                                              GTK_SOURCE_BACKGROUND_PATTERN_TYPE_GRID :
                                              GTK_SOURCE_BACKGROUND_PATTERN_TYPE_NONE);

  if (!key || ide_str_equal0 (key, "line-height"))
    update_css = TRUE;

  if (!key || ide_str_equal0 (key, "font-name"))
    update_css = TRUE;

  if (update_css)
    {
      g_autofree char *css = NULL;
      g_autofree char *font_name = NULL;
      PangoFontDescription *font_desc;
      double line_height;

      line_height = g_settings_get_double (settings, "line-height");
      font_name = g_settings_get_string (settings, "font-name");
      font_desc = pango_font_description_from_string (font_name);

      if ((css = _ide_source_view_generate_css (GTK_SOURCE_VIEW (self), font_desc, 1, line_height)))
        gtk_css_provider_load_from_data (self->css_provider, css, -1);

      g_clear_pointer (&font_desc, pango_font_description_free);
    }

  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
gbp_git_commit_entry_measure (GtkWidget      *widget,
                              GtkOrientation  orientation,
                              int             for_size,
                              int            *minimum,
                              int            *natural,
                              int            *minimum_baseline,
                              int            *natural_baseline)
{
  GbpGitCommitEntry *self = (GbpGitCommitEntry *)widget;

  g_assert (GBP_IS_GIT_COMMIT_ENTRY (self));

  GTK_WIDGET_CLASS (gbp_git_commit_entry_parent_class)->measure (widget,
                                                                 orientation,
                                                                 for_size,
                                                                 minimum,
                                                                 natural,
                                                                 minimum_baseline,
                                                                 natural_baseline);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      static const char empty72[] = "mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm";
      g_autoptr(PangoLayout) layout = gtk_widget_create_pango_layout (widget, empty72);
      int width;

      pango_layout_get_pixel_size (layout, &width, NULL);

      *natural = MAX (*natural, width);
    }
  else
    {
      g_autoptr(GString) str = g_string_new (NULL);
      g_autoptr(PangoLayout) layout = gtk_widget_create_pango_layout (widget, NULL);
      GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
      GtkTextIter begin, end;
      int n_lines;
      int height;

      gtk_text_buffer_get_bounds (buffer, &begin, &end);
      n_lines = gtk_text_iter_get_line (&end) + 1;

      for (int i = 0; i < n_lines; i++)
        g_string_append_len (str, "m\n", 2);

      if (n_lines > 1)
        g_string_truncate (str, str->len-1);

      pango_layout_set_text (layout, str->str, -1);

      pango_layout_get_pixel_size (layout, NULL, &height);

      *natural = MAX (*natural, MAX (150, height));
    }
}

static void
gbp_git_commit_entry_dispose (GObject *object)
{
  GbpGitCommitEntry *self = (GbpGitCommitEntry *)object;

  g_clear_object (&self->editor_settings);
  g_clear_object (&self->css_provider);

  G_OBJECT_CLASS (gbp_git_commit_entry_parent_class)->dispose (object);
}

static void
gbp_git_commit_entry_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GbpGitCommitEntry *self = GBP_GIT_COMMIT_ENTRY (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_git_commit_entry_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GbpGitCommitEntry *self = GBP_GIT_COMMIT_ENTRY (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_git_commit_entry_class_init (GbpGitCommitEntryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gbp_git_commit_entry_dispose;
  object_class->get_property = gbp_git_commit_entry_get_property;
  object_class->set_property = gbp_git_commit_entry_set_property;

  widget_class->measure = gbp_git_commit_entry_measure;
}

static void
gbp_git_commit_entry_init (GbpGitCommitEntry *self)
{
  GtkSourceLanguageManager *lm;
  GtkSourceLanguage *lang;
  GtkTextBuffer *buffer;

  self->editor_settings = g_settings_new ("org.gnome.builder.editor");

  gtk_text_view_set_left_margin (GTK_TEXT_VIEW (self), 12);
  gtk_text_view_set_right_margin (GTK_TEXT_VIEW (self), 12);
  gtk_text_view_set_top_margin (GTK_TEXT_VIEW (self), 12);
  gtk_text_view_set_bottom_margin (GTK_TEXT_VIEW (self), 12);
  gtk_text_view_set_monospace (GTK_TEXT_VIEW (self), TRUE);
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (self), GTK_WRAP_NONE);
  gtk_source_view_set_right_margin_position (GTK_SOURCE_VIEW (self), 72);
  gtk_source_view_set_show_right_margin (GTK_SOURCE_VIEW (self), TRUE);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));

  g_signal_connect_object (buffer,
                           "changed",
                           G_CALLBACK (gtk_widget_queue_resize),
                           self,
                           G_CONNECT_SWAPPED);

  lm = gtk_source_language_manager_get_default ();
  lang = gtk_source_language_manager_get_language (lm, "git-commit");
  gtk_source_buffer_set_language (GTK_SOURCE_BUFFER (buffer), lang);

  self->css_provider = gtk_css_provider_new ();
  gtk_style_context_add_provider (gtk_widget_get_style_context (GTK_WIDGET (self)),
                                  GTK_STYLE_PROVIDER (self->css_provider),
                                  G_MAXINT);

  g_object_bind_property_full (IDE_APPLICATION_DEFAULT, "style-scheme",
                               buffer, "style-scheme",
                               G_BINDING_SYNC_CREATE,
                               style_scheme_name_to_object, NULL, NULL, NULL);

  g_signal_connect_object (self->editor_settings,
                           "changed",
                           G_CALLBACK (editor_settings_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
  editor_settings_changed_cb (self, NULL, self->editor_settings);
}
