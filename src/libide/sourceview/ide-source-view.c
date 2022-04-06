/* ide-source-view.c
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

#define G_LOG_DOMAIN "ide-source-view"

#include "config.h"

#include <glib/gi18n.h>
#include <math.h>

#include <libide-gtk.h>

#include "ide-source-view.h"

struct _IdeSourceView
{
  GtkSourceView source_view;
  GtkCssProvider *css_provider;
  PangoFontDescription *font_desc;
  int font_scale;
  double line_height;
};

enum {
  PROP_0,
  PROP_FONT_DESC,
  PROP_FONT_SCALE,
  PROP_LINE_HEIGHT,
  PROP_ZOOM_LEVEL,
  N_PROPS
};

G_DEFINE_TYPE (IdeSourceView, ide_source_view, GTK_SOURCE_TYPE_VIEW)

static GParamSpec *properties [N_PROPS];

static void
ide_source_view_update_css (IdeSourceView *self)
{
  const PangoFontDescription *font_desc;
  PangoFontDescription *scaled = NULL;
  PangoFontDescription *system_font = NULL;
  GtkSourceStyleScheme *scheme;
  GtkSourceStyle *style;
  GtkTextBuffer *buffer;
  g_autoptr(GString) str = NULL;
  g_autofree char *font_css = NULL;
  int size = 11; /* 11pt */
  char line_height_str[G_ASCII_DTOSTR_BUF_SIZE];

  g_assert (IDE_IS_SOURCE_VIEW (self));

  str = g_string_new (NULL);

  /* Get information for search bubbles */
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  if ((scheme = gtk_source_buffer_get_style_scheme (GTK_SOURCE_BUFFER (buffer))) &&
      (style = gtk_source_style_scheme_get_style (scheme, "search-match")))
    {
      g_autofree char *background = NULL;
      gboolean background_set = FALSE;

      g_object_get (style,
                    "background", &background,
                    "background-set", &background_set,
                    NULL);

      if (background != NULL && background_set)
        g_string_append_printf (str,
                                ".search-match {"
                                " background:mix(%s,currentColor,0.0125);"
                                " border-radius:7px;"
                                " box-shadow: 0 1px 3px mix(%s,currentColor,.2);"
                                "}\n",
                                background, background);
    }

  g_string_append (str, "textview {\n");

  /* Get font information to adjust line height and font changes */
  if ((font_desc = self->font_desc) == NULL)
    {
      g_object_get (g_application_get_default (),
                    "system-font", &system_font,
                    NULL);
      font_desc = system_font;
    }

  if (font_desc != NULL &&
      pango_font_description_get_set_fields (font_desc) & PANGO_FONT_MASK_SIZE)
    size = pango_font_description_get_size (font_desc) / PANGO_SCALE;

  if (size + self->font_scale < 1)
    self->font_scale = -size + 1;

  size = MAX (1, size + self->font_scale);

  if (size != 0)
    {
      if (font_desc)
        scaled = pango_font_description_copy (font_desc);
      else
        scaled = pango_font_description_new ();
      pango_font_description_set_size (scaled, size * PANGO_SCALE);
      font_desc = scaled;
    }

  if (font_desc)
    {
      font_css = ide_font_description_to_css (font_desc);
      g_string_append (str, font_css);
    }

  g_ascii_dtostr (line_height_str, sizeof line_height_str, self->line_height);
  line_height_str[6] = 0;
  g_string_append_printf (str, "\nline-height: %s;\n", line_height_str);

  g_string_append (str, "}\n");

  gtk_css_provider_load_from_data (self->css_provider, str->str, -1);

  g_clear_pointer (&scaled, pango_font_description_free);
  g_clear_pointer (&system_font, pango_font_description_free);
}

static void
tweak_gutter_spacing (GtkSourceView *view)
{
  GtkSourceGutter *gutter;
  GtkWidget *child;
  guint n = 0;

  g_assert (GTK_SOURCE_IS_VIEW (view));

  /* Ensure we have a line gutter renderer to tweak */
  gutter = gtk_source_view_get_gutter (view, GTK_TEXT_WINDOW_LEFT);
  gtk_source_view_set_show_line_numbers (view, TRUE);

  /* Add margin to first gutter renderer */
  for (child = gtk_widget_get_first_child (GTK_WIDGET (gutter));
       child != NULL;
       child = gtk_widget_get_next_sibling (child), n++)
    {
      if (GTK_SOURCE_IS_GUTTER_RENDERER (child))
        gtk_widget_set_margin_start (child, n == 0 ? 4 : 0);
    }
}

static void
ide_source_view_dispose (GObject *object)
{
  IdeSourceView *self = (IdeSourceView *)object;

  g_clear_object (&self->css_provider);
  g_clear_pointer (&self->font_desc, pango_font_description_free);

  G_OBJECT_CLASS (ide_source_view_parent_class)->dispose (object);
}

static void
ide_source_view_finalize (GObject *object)
{
  G_OBJECT_CLASS (ide_source_view_parent_class)->finalize (object);
}

static void
ide_source_view_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  IdeSourceView *self = IDE_SOURCE_VIEW (object);

  switch (prop_id)
    {
    case PROP_FONT_DESC:
      g_value_set_boxed (value, ide_source_view_get_font_desc (self));
      break;

    case PROP_FONT_SCALE:
      g_value_set_int (value, self->font_scale);
      break;

    case PROP_LINE_HEIGHT:
      g_value_set_double (value, self->line_height);
      break;

    case PROP_ZOOM_LEVEL:
      g_value_set_double (value, ide_source_view_get_zoom_level (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_source_view_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  IdeSourceView *self = IDE_SOURCE_VIEW (object);

  switch (prop_id)
    {
    case PROP_FONT_DESC:
      ide_source_view_set_font_desc (self, g_value_get_boxed (value));
      break;

    case PROP_FONT_SCALE:
      self->font_scale = g_value_get_int (value);
      ide_source_view_update_css (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ZOOM_LEVEL]);
      break;

    case PROP_LINE_HEIGHT:
      self->line_height = g_value_get_double (value);
      ide_source_view_update_css (self);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_source_view_class_init (IdeSourceViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_source_view_dispose;
  object_class->finalize = ide_source_view_finalize;
  object_class->get_property = ide_source_view_get_property;
  object_class->set_property = ide_source_view_set_property;

  properties [PROP_LINE_HEIGHT] =
    g_param_spec_double ("line-height",
                         "Line height",
                         "The line height of all lines",
                         0.5, 10.0, 1.2,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_FONT_DESC] =
    g_param_spec_boxed ("font-desc",
                         "Font Description",
                         "The font to use for text within the editor",
                         PANGO_TYPE_FONT_DESCRIPTION,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_FONT_SCALE] =
    g_param_spec_int ("font-scale",
                      "Font Scale",
                      "The font scale",
                      G_MININT, G_MAXINT, 0,
                      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_ZOOM_LEVEL] =
    g_param_spec_double ("zoom-level",
                         "Zoom Level",
                         "Zoom Level",
                         -G_MAXDOUBLE, G_MAXDOUBLE, 1.0,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_source_view_init (IdeSourceView *self)
{
  GtkStyleContext *style_context;

  g_signal_connect_object (g_application_get_default (),
                           "notify::system-font-name",
                           G_CALLBACK (ide_source_view_update_css),
                           self,
                           G_CONNECT_SWAPPED);

  self->css_provider = gtk_css_provider_new ();
  style_context = gtk_widget_get_style_context (GTK_WIDGET (self));
  gtk_style_context_add_provider (style_context,
                                  GTK_STYLE_PROVIDER (self->css_provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  tweak_gutter_spacing (GTK_SOURCE_VIEW (self));
}

void
ide_source_view_scroll_to_insert (IdeSourceView *self)
{
  GtkTextBuffer *buffer;
  GtkTextView *view;
  GtkTextMark *mark;

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  view = GTK_TEXT_VIEW (self);
  buffer = gtk_text_view_get_buffer (view);
  mark = gtk_text_buffer_get_insert (buffer);

  /* TODO: use margin to implement  "scroll offset" */
  gtk_text_view_scroll_to_mark (view, mark, .5, FALSE, .0, .0);
}

void
ide_source_view_get_visual_position (IdeSourceView *self,
                                     guint         *line,
                                     guint         *line_column)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GtkTextMark *mark;

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  mark = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, mark);

  if (line)
    *line = gtk_text_iter_get_line (&iter);

  if (line_column)
    *line_column = gtk_source_view_get_visual_column (GTK_SOURCE_VIEW (self), &iter);
}

char *
ide_source_view_dup_position_label (IdeSourceView *self)
{
  guint line;
  guint column;

  g_return_val_if_fail (IDE_IS_SOURCE_VIEW (self), NULL);

  ide_source_view_get_visual_position (self, &line, &column);

  return g_strdup_printf (_("Ln %u, Col %u"), line + 1, column + 1);
}

const PangoFontDescription *
ide_source_view_get_font_desc (IdeSourceView *self)
{
  g_return_val_if_fail (IDE_IS_SOURCE_VIEW (self), NULL);

  return self->font_desc;
}

void
ide_source_view_set_font_desc (IdeSourceView           *self,
                                  const PangoFontDescription *font_desc)
{
  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  if (self->font_desc == font_desc ||
      (self->font_desc != NULL && font_desc != NULL &&
       pango_font_description_equal (self->font_desc, font_desc)))
    return;

  g_clear_pointer (&self->font_desc, pango_font_description_free);

  if (font_desc)
    self->font_desc = pango_font_description_copy (font_desc);

  self->font_scale = 0;

  ide_source_view_update_css (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_FONT_DESC]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_FONT_SCALE]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ZOOM_LEVEL]);
}

double
ide_source_view_get_zoom_level (IdeSourceView *self)
{
  int alt_size;
  int size = 11; /* 11pt */

  g_return_val_if_fail (IDE_IS_SOURCE_VIEW (self), 0);

  if (self->font_desc != NULL &&
      pango_font_description_get_set_fields (self->font_desc) & PANGO_FONT_MASK_SIZE)
    size = pango_font_description_get_size (self->font_desc) / PANGO_SCALE;

  alt_size = MAX (1, size + self->font_scale);

  return (double)alt_size / (double)size;
}
