/* gbp-codeshot-window.c
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-codeshot-window"

#include "config.h"

#include "gbp-codeshot-window.h"

struct _GbpCodeshotWindow
{
  AdwWindow       parent_window;

  IdeBuffer      *buffer;
  GtkTextMark    *begin_mark;
  GtkTextMark    *end_mark;

  GtkSourceView  *view;
};

enum {
  PROP_0,
  PROP_BUFFER,
  PROP_BEGIN_ITER,
  PROP_END_ITER,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GbpCodeshotWindow, gbp_codeshot_window, ADW_TYPE_WINDOW)

static GParamSpec *properties [N_PROPS];

static inline GtkTextMark *
create_mark (const GtkTextIter *iter)
{
  if (iter == NULL)
    return NULL;

  return gtk_text_buffer_create_mark (gtk_text_iter_get_buffer (iter), NULL, iter, TRUE);
}

static inline void
clear_mark (GtkTextMark **mark)
{
  if (*mark != NULL)
    {
      gtk_text_buffer_delete_mark (gtk_text_mark_get_buffer (*mark), *mark);
      *mark = NULL;
    }
}

static void
gbp_codeshot_window_constructed (GObject *object)
{
  GbpCodeshotWindow *self = (GbpCodeshotWindow *)object;
  g_autoptr(PangoLayout) layout = NULL;
  IdeFileSettings *file_settings;
  g_autofree char *text = NULL;
  g_autofree char *title = NULL;
  GtkSourceBuffer *buffer;
  GtkTextIter begin, end;
  guint longest_column = 0;
  int width, height;
  guint n_lines = 1;
  GtkBorder border;

  G_OBJECT_CLASS (gbp_codeshot_window_parent_class)->constructed (object);

  if (self->buffer == NULL || self->begin_mark == NULL || self->end_mark == NULL)
    g_return_if_reached ();

  /* Update title */
  title = g_file_get_basename (ide_buffer_get_file (self->buffer));
  gtk_window_set_title (GTK_WINDOW (self), title);

  /* Get the text from original buffer */
  buffer = GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->view)));
  gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (self->buffer), &begin, self->begin_mark);
  gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (self->buffer), &end, self->end_mark);
  gtk_text_iter_order (&begin, &end);

  /* Copy language/style-scheme/etc */
  gtk_source_buffer_set_language (buffer,
                                  gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (self->buffer)));
  gtk_source_buffer_set_style_scheme (buffer,
                                      gtk_source_buffer_get_style_scheme (GTK_SOURCE_BUFFER (self->buffer)));

  /* Copy file settings */
  if ((file_settings = ide_buffer_get_file_settings (self->buffer)))
    {
      gtk_source_view_set_tab_width (self->view, ide_file_settings_get_tab_width (file_settings));
      gtk_source_view_set_indent_width (self->view, ide_file_settings_get_indent_width (file_settings));
    }

  /* Copy the text (but strip tail space only) */
  text = g_strchomp (gtk_text_iter_get_slice (&begin, &end));
  gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), text, -1);

  for (const char *c = text; *c; c = g_utf8_next_char (c))
    {
      if (*c == '\n')
        n_lines++;
    }

  /* Try to determine our width-request based on the longest line and the
   * visual position there.
   */
  for (GtkTextIter iter = begin;
       gtk_text_iter_compare (&iter, &end) < 0;
       gtk_text_iter_forward_line (&iter))
    {
      guint column;

      if (!gtk_text_iter_ends_line (&iter))
        gtk_text_iter_forward_to_line_end (&iter);

      column = gtk_source_view_get_visual_column (self->view, &iter);

      if (column > longest_column)
        longest_column = column;
    }

  gtk_style_context_get_padding (gtk_widget_get_style_context (GTK_WIDGET (self->view)), &border);

  layout = gtk_widget_create_pango_layout (GTK_WIDGET (self->view), "M");
  pango_layout_get_pixel_size (layout, &width, &height);
  gtk_widget_set_size_request (GTK_WIDGET (self->view),
                               width * longest_column + border.left + border.right,
                               height * 1.2 * n_lines + border.top + border.bottom);
}

static void
gbp_codeshot_window_dispose (GObject *object)
{
  GbpCodeshotWindow *self = (GbpCodeshotWindow *)object;

  clear_mark (&self->begin_mark);
  clear_mark (&self->end_mark);

  g_clear_object (&self->buffer);

  G_OBJECT_CLASS (gbp_codeshot_window_parent_class)->dispose (object);
}

static void
gbp_codeshot_window_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GbpCodeshotWindow *self = GBP_CODESHOT_WINDOW (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      g_value_set_object (value, self->buffer);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_codeshot_window_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GbpCodeshotWindow *self = GBP_CODESHOT_WINDOW (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      self->buffer = g_value_dup_object (value);
      break;

    case PROP_BEGIN_ITER:
      self->begin_mark = create_mark (g_value_get_boxed (value));
      break;

    case PROP_END_ITER:
      self->end_mark = create_mark (g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_codeshot_window_class_init (GbpCodeshotWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gbp_codeshot_window_constructed;
  object_class->dispose = gbp_codeshot_window_dispose;
  object_class->get_property = gbp_codeshot_window_get_property;
  object_class->set_property = gbp_codeshot_window_set_property;

  properties [PROP_BUFFER] =
    g_param_spec_object ("buffer", NULL, NULL,
                         IDE_TYPE_BUFFER,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_BEGIN_ITER] =
    g_param_spec_boxed ("begin-iter", NULL, NULL,
                        GTK_TYPE_TEXT_ITER,
                        (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_END_ITER] =
    g_param_spec_boxed ("end-iter", NULL, NULL,
                        GTK_TYPE_TEXT_ITER,
                        (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/codeshot/gbp-codeshot-window.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpCodeshotWindow, view);
}

static void
gbp_codeshot_window_init (GbpCodeshotWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
gbp_codeshot_window_new (IdeBuffer         *buffer,
                         const GtkTextIter *begin_iter,
                         const GtkTextIter *end_iter)
{
  g_return_val_if_fail (IDE_IS_BUFFER (buffer), NULL);
  g_return_val_if_fail (begin_iter != NULL, NULL);
  g_return_val_if_fail (end_iter != NULL, NULL);
  g_return_val_if_fail (gtk_text_iter_get_buffer (begin_iter) == GTK_TEXT_BUFFER (buffer), NULL);
  g_return_val_if_fail (gtk_text_iter_get_buffer (end_iter) == GTK_TEXT_BUFFER (buffer), NULL);

  return g_object_new (GBP_TYPE_CODESHOT_WINDOW,
                       "buffer", buffer,
                       "begin-iter", begin_iter,
                       "end-iter", end_iter,
                       NULL);
}
