/* gbp-restore-cursor-buffer-addin.c
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

#define G_LOG_DOMAIN "gbp-restore-cursor-buffer-addin"

#include "config.h"

#include <libide-code.h>

#include "ide-buffer-private.h"

#include "gbp-restore-cursor-buffer-addin.h"

#define IDE_FILE_ATTRIBUTE_POSITION "metadata::libide-position"

struct _GbpRestoreCursorBufferAddin
{
  GObject parent_instance;
};

static GSettings *settings;

static void
gbp_restore_cursor_buffer_addin_file_saved (IdeBufferAddin *addin,
                                            IdeBuffer      *buffer,
                                            GFile          *file)
{
  g_autofree gchar *position = NULL;
  g_autoptr(GError) error = NULL;
  GtkTextMark *insert;
  GtkTextIter iter;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_RESTORE_CURSOR_BUFFER_ADDIN (addin));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_FILE (file));

  insert = gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (buffer));
  gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer), &iter, insert);
  position = g_strdup_printf ("%u:%u",
                              gtk_text_iter_get_line (&iter),
                              gtk_text_iter_get_line_offset (&iter));

  g_debug ("Saving insert mark at %s", position);

  if (!g_file_set_attribute_string (file, IDE_FILE_ATTRIBUTE_POSITION, position, 0, NULL, &error))
    g_warning ("Failed to persist cursor position: %s", error->message);

  IDE_EXIT;
}

static void
gbp_restore_cursor_buffer_addin_file_loaded_cb (GObject      *object,
                                                GAsyncResult *result,
                                                gpointer      user_data)
{
  GFile *file = (GFile *)object;
  g_autoptr(IdeBuffer) buffer = user_data;
  g_autoptr(GFileInfo) file_info = NULL;
  g_autoptr(GError) error = NULL;
  const gchar *attr;
  guint line_offset = 0;
  guint line = 0;

  IDE_ENTRY;

  g_assert (G_IS_FILE (file));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_BUFFER (buffer));

  /* Don't do anything if the user already moved */
  if (!_ide_buffer_can_restore_cursor (buffer))
    IDE_EXIT;

  if (!(file_info = g_file_query_info_finish (file, result, &error)))
    IDE_EXIT;

  if (!g_file_info_has_attribute (file_info, IDE_FILE_ATTRIBUTE_POSITION) ||
      !(attr = g_file_info_get_attribute_string (file_info, IDE_FILE_ATTRIBUTE_POSITION)))
    IDE_EXIT;

  if (sscanf (attr, "%u:%u", &line, &line_offset) >= 1)
    {
      GtkTextIter iter;

      g_debug ("Restoring insert mark to %u:%u", line + 1, line_offset + 1);
      gtk_text_buffer_get_iter_at_line_offset (GTK_TEXT_BUFFER (buffer),
                                               &iter,
                                               line,
                                               line_offset);
      gtk_text_buffer_select_range (GTK_TEXT_BUFFER (buffer), &iter, &iter);

      _ide_buffer_request_scroll_to_cursor (buffer);
    }

  IDE_EXIT;
}

static void
gbp_restore_cursor_buffer_addin_file_loaded (IdeBufferAddin *addin,
                                             IdeBuffer      *buffer,
                                             GFile          *file)
{
  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_RESTORE_CURSOR_BUFFER_ADDIN (addin));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_FILE (file));

  /* Make sure our setting isn't disabled */
  if (!g_settings_get_boolean (settings, "restore-insert-mark"))
    IDE_EXIT;

  g_file_query_info_async (file,
                           IDE_FILE_ATTRIBUTE_POSITION,
                           G_FILE_QUERY_INFO_NONE,
                           G_PRIORITY_HIGH,
                           NULL,
                           gbp_restore_cursor_buffer_addin_file_loaded_cb,
                           g_object_ref (buffer));

  IDE_EXIT;
}

static void
buffer_addin_iface_init (IdeBufferAddinInterface *iface)
{
  iface->file_loaded = gbp_restore_cursor_buffer_addin_file_loaded;
  iface->file_saved = gbp_restore_cursor_buffer_addin_file_saved;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpRestoreCursorBufferAddin, gbp_restore_cursor_buffer_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_BUFFER_ADDIN, buffer_addin_iface_init))

static void
gbp_restore_cursor_buffer_addin_class_init (GbpRestoreCursorBufferAddinClass *klass)
{
}

static void
gbp_restore_cursor_buffer_addin_init (GbpRestoreCursorBufferAddin *self)
{
  if (settings == NULL)
    settings = g_settings_new ("org.gnome.builder.editor");
}
