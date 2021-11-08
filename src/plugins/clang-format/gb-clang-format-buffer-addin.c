/* gb-clang-format-buffer-addin.c
 *
 * Copyright 2021 Tomi Lähteenmäki <lihis@lihis.net>
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

#define G_LOG_DOMAIN "clang-format-buffer"

#include "config.h"

#include <json-glib/json-glib.h>
#include <libide-code.h>
#include <libide-editor.h>
#include <sys/wait.h>

#include "gb-clang-format-buffer-addin.h"

struct _GbClangFormatBufferAddin
{
  GObject parent_instance;
};

static GSettings *settings;

static void
scroll_page_to_insert (GtkWidget *widget,
                       gpointer   data)
{
  IdePage *page = (IdePage *)widget;
  IdeBuffer *buffer = data;

  g_assert (IDE_IS_PAGE (page));
  g_assert (IDE_IS_BUFFER (buffer));

  if (!IDE_IS_EDITOR_PAGE (page))
    return;

  if (buffer != ide_editor_page_get_buffer (IDE_EDITOR_PAGE (page)))
    return;

  gtk_text_view_scroll_mark_onscreen (GTK_TEXT_VIEW (ide_editor_page_get_view (IDE_EDITOR_PAGE (page))),
                                      gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (buffer)));
}

static gboolean
clang_supports_language (GtkSourceLanguage *language)
{
  static const char *supported[] = { "c", "chdr", "cpp", "cpphdr", "objc", NULL };
  g_assert (!language || GTK_SOURCE_IS_LANGUAGE (language));
  return language != NULL &&
         g_strv_contains (supported, gtk_source_language_get_id (language));
}

static int
get_cursor_position (const char *str,
                     guint       length)
{
  g_autoptr(JsonParser) parser = NULL;
  JsonObject *object;
  JsonNode *root;

  g_assert (str != NULL);

  parser = json_parser_new ();
  if (!json_parser_load_from_data (parser, str, length, NULL))
    return -1;

  if (!(root = json_parser_get_root (parser)) ||
      !JSON_NODE_HOLDS_OBJECT (root) ||
      !(object = json_node_get_object (root)) ||
      !json_object_has_member (object, "Cursor"))
    return -1;

  return json_object_get_int_member (object, "Cursor");
}

static void
gb_clang_format_buffer_addin_save_file (IdeBufferAddin *addin,
                                        IdeBuffer      *buffer,
                                        GFile          *file)
{
  GbClangFormatBufferAddin *self = (GbClangFormatBufferAddin *)addin;
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) workdir = NULL;
  g_autoptr(GFile) dot_clang_format = NULL;
  g_autofree char *cursor_arg = NULL;
  g_autofree char *stdout_buf = NULL;
  GtkSourceLanguage *language;
  IdeWorkbench *workbench;
  const char *stdin_buf;
  const char *formatted;
  GtkTextIter iter;
  int cursor_position;

  IDE_ENTRY;

  g_assert (GB_IS_CLANG_FORMAT_BUFFER_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_FILE (file));

  if (!g_settings_get_boolean (settings, "format-on-save"))
    IDE_EXIT;

  if (!(context = ide_buffer_ref_context (buffer)))
    IDE_EXIT;

  if (!(workdir = ide_context_ref_workdir (context)) || !g_file_is_native (workdir))
    IDE_EXIT;

  if (!(language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (buffer))) ||
      !clang_supports_language (language))
    IDE_EXIT;

  dot_clang_format = g_file_get_child (workdir, ".clang-format");
  if (!g_file_query_exists (dot_clang_format, NULL))
    IDE_EXIT;

  gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer),
                                    &iter,
                                    gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (buffer)));
  cursor_arg = g_strdup_printf ("--cursor=%u", gtk_text_iter_get_offset (&iter));
  stdin_buf = g_bytes_get_data (ide_buffer_dup_content (buffer), NULL);

  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDIN_PIPE |
                                          G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                          G_SUBPROCESS_FLAGS_STDERR_SILENCE);
  ide_subprocess_launcher_set_cwd (launcher, g_file_peek_path (workdir));
  ide_subprocess_launcher_push_argv (launcher, "clang-format");
  ide_subprocess_launcher_push_argv (launcher, cursor_arg);

  if (!(subprocess = ide_subprocess_launcher_spawn (launcher, NULL, &error)))
    {
      g_debug ("Failed to spawn clang-format: %s", error->message);
      IDE_EXIT;
    }

  if (!ide_subprocess_communicate_utf8 (subprocess, stdin_buf, NULL, &stdout_buf, NULL, &error))
    {
      g_debug ("Failed to communicate with subprocess: %s", error->message);
      IDE_EXIT;
    }

  if (!(formatted = strchr (stdout_buf, '\n')))
    {
      g_debug ("Missing or corrupted data from clang-format");
      IDE_EXIT;
    }

  if ((cursor_position = get_cursor_position (stdout_buf, formatted - stdout_buf)) < 0)
    IDE_EXIT;

  gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (buffer));
  gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), formatted + 1, -1);
  gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), &iter, cursor_position);
  gtk_text_buffer_select_range (GTK_TEXT_BUFFER (buffer), &iter, &iter);
  gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (buffer));

  workbench = ide_workbench_from_context (context);
  ide_workbench_foreach_page (workbench, scroll_page_to_insert, buffer);

  IDE_EXIT;
}

static void
buffer_addin_iface_init (IdeBufferAddinInterface *iface)
{
  iface->save_file = gb_clang_format_buffer_addin_save_file;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbClangFormatBufferAddin, gb_clang_format_buffer_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_BUFFER_ADDIN, buffer_addin_iface_init))

static void
gb_clang_format_buffer_addin_class_init (GbClangFormatBufferAddinClass *klass)
{
  settings = g_settings_new ("org.gnome.builder");
}

static void
gb_clang_format_buffer_addin_init (GbClangFormatBufferAddin *self)
{
}
