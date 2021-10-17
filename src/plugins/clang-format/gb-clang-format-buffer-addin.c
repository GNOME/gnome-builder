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
  gchar *working_directory;
  gssize cursor_position;
  IdePage *page;
  gssize header_len;
};

typedef struct {
  IdeBuffer *buffer;
  IdePage *page;
} Lookup;

static void
foreach_page_cb (GtkWidget *page,
                 gpointer   user_data)
{
  Lookup *l = user_data;

  if (l->page == NULL &&
      IDE_IS_EDITOR_PAGE (page) &&
      ide_editor_page_get_buffer (IDE_EDITOR_PAGE (page)) == l->buffer)
    l->page = IDE_PAGE (page);
}

static IdePage *
get_page (IdeBuffer *buffer)
{
  Lookup lookup = {buffer, NULL};
  IdeContext *context = ide_buffer_ref_context (buffer);
  IdeWorkbench *workbench = ide_workbench_from_context (context);
  ide_workbench_foreach_page (workbench, foreach_page_cb, &lookup);

  return lookup.page;
}

gboolean
format_on_save_enabled (IdeBuffer *buffer)
{
  g_autoptr (GSettings) settings = g_settings_new ("org.gnome.builder");

  return g_settings_get_boolean (settings, "format-on-save");
}

gboolean
is_formattable_language (IdeBuffer *buffer)
{
  const gchar *lang_id;

  lang_id = ide_buffer_get_language_id (buffer);
  if (lang_id == NULL)
    {
      g_debug ("Language ID was NULL");
      return FALSE;
    }

  if (strcmp (lang_id, "c") == 0 || strcmp (lang_id, "chdr") == 0 ||
      strcmp (lang_id, "cpp") == 0 || strcmp (lang_id, "cpphdr") == 0 ||
      strcmp (lang_id, "objc") == 0)
    return TRUE;

  return FALSE;
}

char *
project_root_directory (IdeBuffer *buffer)
{
  IdeContext *context;
  GFile *workdir;

  context = ide_buffer_ref_context (buffer);
  if (context == NULL)
    {
      g_warning ("Failed to get IdeContext");
      return NULL;
    }

  workdir = ide_context_ref_workdir (context);
  if (workdir == NULL)
    {
      g_warning ("Failed to get working directory");
      return NULL;
    }

  return g_file_get_path (workdir);
}

gboolean
clang_format_config_exists (GbClangFormatBufferAddin *self,
                            IdeBuffer *buffer)
{
  GFile *config;

  config = g_file_new_build_filename (self->working_directory, ".clang-format", NULL);

  return g_file_query_exists (config, NULL);
}

gint
get_cursor_position (IdeBuffer *buffer)
{
  GtkTextBuffer *textbuffer;
  gint cursor_position;

  textbuffer = GTK_TEXT_BUFFER (buffer);
  g_object_get (G_OBJECT (textbuffer), "cursor-position", &cursor_position, NULL);

  return cursor_position;
}

gssize
get_header_length (gchar *data)
{
  gssize len = g_utf8_strlen (data, G_MAXSSIZE);

  for (gssize i = 0; i < len; ++i)
      if (data[i] == '\n')
          return (i + 1 <= len ? (i + 1) : i);

  return 0;
}

gboolean
parse_header (GbClangFormatBufferAddin *self,
              gchar                    *data)
{
  JsonParser *parser;
  gboolean ret;
  GError *error;
  JsonNode *root;
  JsonObject *cursor;

  self->header_len = get_header_length (data);
  if (self->header_len <= 0)
    {
      g_warning ("Empty header");
      return FALSE;
    }

  error = NULL;
  parser = json_parser_new ();
  ret = json_parser_load_from_data (parser, data, self->header_len, &error);
  if (ret == FALSE)
    {
      g_warning ("Unable to parse JSON: %s", error->message);
      g_error_free (error);
      g_object_unref (parser);
      return FALSE;
    }

  root = json_parser_get_root (parser);
  cursor = json_node_get_object (root);
  if (cursor == NULL)
    {
      g_warning ("clang-format didn't return cursor position");
      g_object_unref (parser);
      return FALSE;
    }
  self->cursor_position = json_object_get_int_member (cursor, "Cursor");

  g_object_unref (parser);
  return TRUE;
}

gchar *
format_cursor_arg (gssize position)
{
  return g_strdup_printf ("--cursor=%zu", position);
}

IdeSubprocess *
create_process (GbClangFormatBufferAddin *self)
{
  g_autoptr (IdeSubprocessLauncher) launcher = NULL;
  IdeSubprocess *subprocess = NULL;
  GPtrArray *args;
  GError *error = NULL;
  gchar *cursor_arg;

  cursor_arg = format_cursor_arg (self->cursor_position);

  args = g_ptr_array_new ();
  g_ptr_array_add (args, (gchar *)"clang-format");
  g_ptr_array_add (args, cursor_arg);
  g_ptr_array_add (args, NULL);

  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDIN_PIPE
                                          | G_SUBPROCESS_FLAGS_STDOUT_PIPE
                                          | G_SUBPROCESS_FLAGS_STDERR_PIPE);
  ide_subprocess_launcher_set_cwd (launcher, self->working_directory);
  ide_subprocess_launcher_set_argv (launcher,
                                    (const gchar *const *)args->pdata);
  subprocess = ide_subprocess_launcher_spawn (launcher, NULL, &error);

  g_ptr_array_free (args, TRUE);
  g_free (cursor_arg);
  return subprocess;
}

gboolean
process_communicate (IdeSubprocess  *process,
                     IdeBuffer      *buffer,
                     gchar         **stdout_buf)
{
  const gchar *stdin_buf;
  gchar *stderr_buf = NULL;
  GError *error = NULL;
  gboolean ret;

  stdin_buf = g_bytes_get_data (ide_buffer_dup_content (buffer), NULL);

  ret = ide_subprocess_communicate_utf8 (process, stdin_buf, NULL, stdout_buf,
                                         &stderr_buf, &error);
  if (ret == FALSE)
    {
      g_warning ("clang-format failed: %s", error->message);
      g_free (error);
      return FALSE;
    }

  return TRUE;
}

void
run_clang_format (GbClangFormatBufferAddin *self,
                  IdeBuffer                *buffer)
{
  IdeSubprocess *process;
  gchar *stdout_buf = NULL;
  gchar *data_start;
  gsize data_len;
  GtkTextIter cursor;

  process = create_process (self);
  if (process_communicate (process, buffer, &stdout_buf) == FALSE)
    return;

  if (parse_header (self, stdout_buf) == FALSE)
    return;

  data_start = stdout_buf + self->header_len;
  data_len = strlen (data_start);
  if (data_len <= 0)
    {
      g_warning ("No output");
      return;
    }

  if (data_start[data_len - 1] == '\n')
      data_len--;

  gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (buffer));

  gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), data_start, data_len);

  gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &cursor);
  gtk_text_iter_set_offset (&cursor, self->cursor_position);
  gtk_text_buffer_place_cursor (GTK_TEXT_BUFFER (buffer), &cursor);

  gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (buffer));

  if (self->page != NULL)
    {
      IdeSourceView *source_view = ide_editor_page_get_view (IDE_EDITOR_PAGE (self->page));
      gtk_text_view_scroll_to_iter (GTK_TEXT_VIEW (source_view), &cursor, 0.25, FALSE, 0.5, 0.5);
    }
  else
      g_warning ("Failed to get page");
}

static void
gb_clang_format_buffer_addin_save_file (IdeBufferAddin *addin,
                                        IdeBuffer      *buffer,
                                        GFile          *file)
{
  GbClangFormatBufferAddin *self;

  if (format_on_save_enabled (buffer) == FALSE)
      return;

  if (is_formattable_language (buffer) == FALSE)
      return;

  self = (GbClangFormatBufferAddin *)addin;
  self->working_directory = project_root_directory (buffer);
  if (self->working_directory == NULL)
    {
      g_warning ("Failed to get working directory");
      return;
    }

  if (clang_format_config_exists (self, buffer) == FALSE)
    {
      g_debug ("No .clang-format");
      return;
    }
  self->cursor_position = get_cursor_position (buffer);
  self->page = get_page (buffer);

  run_clang_format (self, buffer);
}

static void
buffer_addin_iface_init (IdeBufferAddinInterface *iface)
{
  iface->load = NULL;
  iface->unload = NULL;
  iface->save_file = gb_clang_format_buffer_addin_save_file;
  iface->file_loaded = NULL;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbClangFormatBufferAddin, gb_clang_format_buffer_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_BUFFER_ADDIN, buffer_addin_iface_init))

static void
gb_clang_format_buffer_addin_class_init (GbClangFormatBufferAddinClass *klass)
{
}

static void
gb_clang_format_buffer_addin_init (GbClangFormatBufferAddin *self)
{
}
