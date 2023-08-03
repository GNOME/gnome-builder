/* gbp-trim-spaces-buffer-addin.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-trim-spaces-buffer-addin"

#include "config.h"

#include <libide-code.h>

#include "ide-buffer-private.h"

#include "gbp-trim-spaces-buffer-addin.h"

struct _GbpTrimSpacesBufferAddin
{
  GObject parent_instance;
};

static gboolean
move_to_line_end (GtkTextIter *iter)
{
  GtkTextIter before = *iter;
  gboolean moved;

  moved = gtk_text_iter_forward_to_line_end (iter);

  if (moved == FALSE)
    moved = !gtk_text_iter_equal (iter, &before);

  return moved;
}

static void
trim_trailing_whitespace (GtkTextBuffer *buffer,
                          GtkTextIter   *iter)
{
  /*
   * Preserve all whitespace that isn't space or tab.
   * This could include line feed, form feed, etc.
   */
#define TEXT_ITER_IS_SPACE(ptr) \
    ({  \
      gunichar ch = gtk_text_iter_get_char (ptr); \
      (ch == ' ' || ch == '\t'); \
    })

  /*
   * Move to the first character at the end of the line (skipping the newline)
   * and progress to trip if it is white space.
   */
  if (move_to_line_end (iter) &&
      !gtk_text_iter_starts_line (iter) &&
      gtk_text_iter_backward_char (iter) &&
      TEXT_ITER_IS_SPACE (iter))
    {
      GtkTextIter begin = *iter;

      gtk_text_iter_forward_to_line_end (iter);

      while (TEXT_ITER_IS_SPACE (&begin))
        {
          if (gtk_text_iter_starts_line (&begin))
            break;

          if (!gtk_text_iter_backward_char (&begin))
            break;
        }

      if (!TEXT_ITER_IS_SPACE (&begin) && !gtk_text_iter_ends_line (&begin))
        gtk_text_iter_forward_char (&begin);

      if (!gtk_text_iter_equal (&begin, iter))
        gtk_text_buffer_delete (buffer, &begin, iter);
    }
}

static void
trim_trailing_whitespace_cb (guint               line,
                             IdeBufferLineChange change,
                             gpointer            user_data)
{
  GtkTextBuffer *buffer = user_data;
  GtkTextIter iter;

  g_assert (GTK_IS_TEXT_BUFFER (buffer));

  if (!(change & (IDE_BUFFER_LINE_CHANGE_ADDED | IDE_BUFFER_LINE_CHANGE_CHANGED)))
    return;

  gtk_text_buffer_get_iter_at_line (buffer, &iter, line);

  trim_trailing_whitespace (buffer, &iter);
}

static void
gbp_trim_spaces_buffer_addin_save_file (IdeBufferAddin *addin,
                                        IdeBuffer      *buffer,
                                        GFile          *file)
{
  IdeFileSettings *file_settings;
  IdeBufferChangeMonitor *changes;
  GtkTextIter end;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_TRIM_SPACES_BUFFER_ADDIN (addin));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_FILE (file));

  if (!(file_settings = ide_buffer_get_file_settings (buffer)) ||
      !ide_file_settings_get_trim_trailing_whitespace (file_settings) ||
      !(changes = ide_buffer_get_change_monitor (buffer)))
    return;

  gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (buffer), &end);

  /* TODO: Fallback when file is untracked */

  ide_buffer_change_monitor_foreach_change (changes,
                                            0,
                                            gtk_text_iter_get_line (&end),
                                            trim_trailing_whitespace_cb,
                                            buffer);
}

static void
buffer_addin_iface_init (IdeBufferAddinInterface *iface)
{
  iface->save_file = gbp_trim_spaces_buffer_addin_save_file;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpTrimSpacesBufferAddin, gbp_trim_spaces_buffer_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_BUFFER_ADDIN, buffer_addin_iface_init))

static void
gbp_trim_spaces_buffer_addin_class_init (GbpTrimSpacesBufferAddinClass *klass)
{
}

static void
gbp_trim_spaces_buffer_addin_init (GbpTrimSpacesBufferAddin *self)
{
}
