/* gb-color-picker-helper.c
 *
 * Copyright 2016 sebastien lafargue <slafargue@gnome.org>
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

#include <string.h>

#include <libpeas/peas.h>
#include "gb-color-picker-private.h"
#include <libide-editor.h>

#include "gb-color-picker-helper.h"

static guint tag_count = 0;

/* We don't take the alpha part intop account because the
 * view background can be different depending of the used theme
 */
void
gb_color_picker_helper_get_matching_monochrome (GdkRGBA *src_rgba,
                                                GdkRGBA *dst_rgba)
{
  gdouble brightness;

  g_assert (src_rgba != NULL);
  g_assert (dst_rgba != NULL);

  /* TODO: take alpha into account */

  brightness = src_rgba->red * 299 + src_rgba->green * 587 + src_rgba->blue * 114;
  if (brightness > 500)
    {
      dst_rgba->red = 0.0;
      dst_rgba->green = 0.0;
      dst_rgba->blue = 0.0;
      dst_rgba->alpha = 1.0;
    }
  else
    {
      dst_rgba->red = 1.0;
      dst_rgba->green = 1.0;
      dst_rgba->blue = 1.0;
      dst_rgba->alpha = 1.0;
    }
}

static inline void
int_to_str (gchar *str,
            guint  value)
{
  guint i = 1000000000;

  g_assert (str != NULL);

  if (value == 0)
    i = 1;
  else
    while (i > value)
      i /= 10;

  do
    {
      *str = '0' + (value - value % i) / i % 10;
      ++str;
      i /= 10;
    } while (i);

  *str = '\0';
}

const gchar *
gb_color_picker_helper_get_color_picker_data_path (void)
{
  PeasEngine *engine;
  PeasPluginInfo *info;
  const gchar *datadir;

  engine = peas_engine_get_default ();
  info = peas_engine_get_plugin_info(engine, "color_picker_plugin");
  datadir = peas_plugin_info_get_data_dir (info);

  return datadir;
}

GtkTextTag *
gb_color_picker_helper_create_color_tag (GtkTextBuffer *buffer,
                                         GstyleColor   *color)
{
  gchar str [11];
  GtkTextTag *tag;
  g_autofree gchar *name = NULL;
  GdkRGBA fg_rgba;
  GdkRGBA bg_rgba;

  g_assert (GTK_IS_TEXT_BUFFER (buffer));
  g_assert (GSTYLE_IS_COLOR (color));

  int_to_str (str, tag_count);

  gstyle_color_fill_rgba (color, &bg_rgba);
  bg_rgba.alpha = 1.0;

  gb_color_picker_helper_get_matching_monochrome (&bg_rgba, &fg_rgba);
  name = g_strconcat (COLOR_TAG_PREFIX, str, NULL);
  ++tag_count;

  tag = gtk_text_buffer_create_tag (buffer, name,
                                    "foreground-rgba", &fg_rgba,
                                    "background-rgba", &bg_rgba,
                                    NULL);

  return tag;
}

void
gb_color_picker_helper_change_color_tag (GtkTextTag    *tag,
                                         GstyleColor   *color)
{
  GdkRGBA fg_rgba;
  GdkRGBA bg_rgba;

  g_assert (GTK_IS_TEXT_TAG (tag));
  g_assert (GSTYLE_IS_COLOR (color));

  gstyle_color_fill_rgba (color, &bg_rgba);
  bg_rgba.alpha = 1.0;

  gb_color_picker_helper_get_matching_monochrome (&bg_rgba, &fg_rgba);

  g_object_set (G_OBJECT (tag),
                "foreground-rgba", &fg_rgba,
                "background-rgba", &bg_rgba,
                NULL);
}

GtkTextTag *
gb_color_picker_helper_get_tag_at_iter (GtkTextIter  *cursor,
                                        GstyleColor **current_color,
                                        GtkTextIter  *begin,
                                        GtkTextIter  *end)
{
  GtkTextBuffer *buffer;
  GtkTextTag *tag;
  GSList *tags;
  const gchar *name;
  gchar *color_text;

  g_assert (cursor != NULL);
  g_assert (current_color != NULL);
  g_assert (begin != NULL);
  g_assert (end != NULL);

  tags = gtk_text_iter_get_tags (cursor);
  if (tags != NULL)
    {
      for (; tags != NULL; tags = g_slist_next (tags))
        {
          tag = tags->data;
          g_object_get (G_OBJECT (tag), "name", &name, NULL);
          if (!dzl_str_empty0 (name) && g_str_has_prefix (name, COLOR_TAG_PREFIX))
            {
              *begin = *cursor;
              *end = *cursor;
              if ((gtk_text_iter_starts_tag (begin, tag) || gtk_text_iter_backward_to_tag_toggle (begin, tag)) &&
                   (gtk_text_iter_ends_tag (end, tag) || gtk_text_iter_forward_to_tag_toggle (end, tag)))
                {
                  buffer = gtk_text_iter_get_buffer (cursor);
                  color_text = gtk_text_buffer_get_text (buffer, begin, end, FALSE);
                  *current_color = gstyle_color_new_from_string (NULL, color_text);
                  g_free (color_text);
                  if (*current_color != NULL)
                    {
                      g_slist_free (tags);
                      return tag;
                    }
                }
            }
        }

      g_slist_free (tags);
    }

  *current_color = NULL;
  return NULL;
}

GtkTextTag *
gb_color_picker_helper_set_color_tag (GtkTextIter *begin,
                                      GtkTextIter *end,
                                      GstyleColor *color,
                                      gboolean     preserve_cursor)
{
  GtkTextBuffer *buffer;
  GtkTextTag *tag;
  g_autofree gchar *tag_text = NULL;
  GtkTextMark *insert;
  GtkTextIter cursor;
  gint cursor_offset;

  g_assert (GSTYLE_IS_COLOR (color));
  g_assert (begin != NULL);
  g_assert (end != NULL);

  buffer = gtk_text_iter_get_buffer (begin);

  if (preserve_cursor)
    {
      insert = gtk_text_buffer_get_insert (buffer);
      gtk_text_buffer_get_iter_at_mark (buffer, &cursor, insert);
      cursor_offset = gtk_text_iter_get_offset (&cursor);
    }

  tag = gb_color_picker_helper_create_color_tag (buffer, color);
  tag_text = gstyle_color_to_string (color, GSTYLE_COLOR_KIND_ORIGINAL);

  gtk_text_buffer_delete (buffer, begin, end);
  gtk_text_buffer_insert_with_tags (buffer, begin, tag_text, -1, tag, NULL);

  /* TODO: remove old tag name from table */

  if (preserve_cursor)
    {
      gtk_text_buffer_get_iter_at_offset (buffer, &cursor, cursor_offset);
      gtk_text_buffer_place_cursor (buffer, &cursor);
    }

  return tag;
}

GtkTextTag *
gb_color_picker_helper_set_color_tag_at_iter (GtkTextIter *iter,
                                              GstyleColor *color,
                                              gboolean     preserve_cursor)
{
  g_autoptr (GstyleColor) current_color = NULL;
  g_autofree gchar *new_text = NULL;
  GtkTextBuffer *buffer;
  GtkTextTag *tag = NULL;
  GtkTextIter begin, end;
  gint cursor_offset;
  gint start_offset;
  gint dst_offset;

  g_assert (GSTYLE_IS_COLOR (color));
  g_assert (iter != NULL);

  tag = gb_color_picker_helper_get_tag_at_iter (iter, &current_color, &begin, &end);
  if (tag != NULL)
    {
      buffer = gtk_text_iter_get_buffer (&begin);
      new_text = gstyle_color_to_string (color, gstyle_color_get_kind (current_color));
      if (preserve_cursor)
        {
          start_offset = gtk_text_iter_get_line_offset (&begin);
          cursor_offset = gtk_text_iter_get_line_offset (iter);
          dst_offset = MIN (cursor_offset, start_offset + (gint)strlen (new_text) - 1);
        }

      gb_color_picker_helper_change_color_tag (tag, color);

      /* TODO: keep tags in tagtable so no need for ref/unref */
      g_object_ref (tag);

      gtk_text_buffer_delete (buffer, &begin, &end);
      gtk_text_buffer_insert_with_tags (buffer, &begin, new_text, -1, tag, NULL);

      if (preserve_cursor)
        {
          gtk_text_iter_set_line_offset (&begin, dst_offset);
          gtk_text_buffer_place_cursor (buffer, &begin);
        }

      g_object_unref (tag);
      return tag;
    }
  else
    {
      /* TODO: parse the line, check the limits and set a new tag */
      return NULL;
    }
}
