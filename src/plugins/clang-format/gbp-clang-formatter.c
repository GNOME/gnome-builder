/* gbb-clang-formatter.c
 *
 * Copyright 2023 Tomi Lähteenmäki <lihis@lihis.net>
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

#define G_LOG_DOMAIN "clang-formatter"

#include "config.h"

#include <sys/wait.h>

#include <glib/gi18n.h>

#include <json-glib/json-glib.h>

#include <libide-code.h>
#include <libide-editor.h>
#include <libide-lsp.h>

#include "ide-buffer-private.h"

#include "gbp-clang-formatter.h"

struct _GbpClangFormatter
{
  IdeObject parent_instance;
};

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

/**
 * Try to locate closest .clang-format file for the buffer.
 *
 * If the buffer is for a file in a subprojects/ and it does not contain
 * a .clang-format file, the working directory is not checked as we
 * assume that the subproject does not use clang-format for formatting.
 */
static gchar *
gbp_clang_format_get_config_file_dir (IdeBuffer    *buffer,
                                      GCancellable *cancellable)
{
  gchar *ret = NULL;
  g_autoptr (IdeContext) context = NULL;
  g_autoptr (GFile) workdir = NULL;
  g_autoptr (GFile) parent = NULL;
  GFile *file;

  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (!(context = ide_buffer_ref_context (buffer)))
    return NULL;

  if (!(workdir = ide_context_ref_workdir (context)))
    return NULL;

  file = ide_buffer_get_file (buffer);
  if (g_file_has_parent (file, NULL))
    parent = g_file_get_parent (file);

  while (parent != NULL)
    {
      g_autofree gchar *bname = NULL;
      GFileType ftype = G_FILE_TYPE_UNKNOWN;

      bname = g_file_get_basename (parent);
      ftype = g_file_query_file_type (parent, G_FILE_QUERY_INFO_NONE, cancellable);
      if (g_strcmp0(bname, "subprojects") == 0 && ftype == G_FILE_TYPE_DIRECTORY)
        {
          break;
        }
      else
        {
          g_autoptr(GFile) child = g_file_get_child (parent, ".clang-format");
          if (g_file_query_exists (child, cancellable))
            {
              ret = g_file_get_path (parent);
              break;
            }
        }

      if (g_strcmp0 (g_file_peek_path (parent), g_file_peek_path (workdir)) == 0)
        {
          g_clear_object (&parent);
        }
      else
        {
          GFile *tmp = g_file_get_parent (parent);
          g_object_unref (parent);
          parent = tmp;
        }
  }

  return ret;
}


static void
gbp_clang_format_communicate_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autofree gchar *stdout_buf = NULL;
  g_autofree gchar *stderr_buf = NULL;
  g_autoptr(GError) error = NULL;
  const char *formatted = NULL;
  IdeBuffer *buffer = NULL;
  g_autoptr(IdeContext) context = NULL;
  int cursor_position = -1;
  GtkTextIter start_iter, end_iter, pos_iter;

  IDE_ENTRY;

  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  buffer = ide_task_get_task_data (task);

  g_assert (buffer != NULL);
  g_assert (IDE_IS_BUFFER (buffer));

  context = ide_buffer_ref_context (buffer);

  g_assert (context != NULL);
  g_assert (IDE_IS_CONTEXT (context));

  if (!ide_subprocess_communicate_utf8_finish (subprocess, result, &stdout_buf, &stderr_buf, &error))
    {
      ide_object_warning (context,
                          _("Failed to execute clang-format: %s"),
                          error->message);
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  if (ide_task_return_error_if_cancelled (task))
    IDE_EXIT;

  if (ide_subprocess_get_exit_status (subprocess) != 0)
    {
      g_autofree char *message = g_strdup_printf (_("clang-format failed to format document: %s"), stderr_buf);

      ide_object_warning (context, "%s", message);
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVALID_DATA,
                                 "%s", message);
      IDE_EXIT;
    }

  if (!(formatted = strchr (stdout_buf, '\n')))
    {
      const char *message = _("Missing or corrupted data from clang-format");

      ide_object_warning (context, "%s", message);
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVALID_DATA,
                                 "%s", message);
      IDE_EXIT;
    }

  if ((cursor_position = get_cursor_position (stdout_buf, formatted - stdout_buf)) < 0)
    {
      const char *message = _("Invalid cursor position provided from clang-format");

      ide_object_warning (context, "%s", message);
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVALID_DATA,
                                 "%s", message);
      IDE_EXIT;
    }

  gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (buffer));
  gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &start_iter, &end_iter);
  gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &start_iter, &end_iter);
  gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &start_iter, formatted + 1, -1);
  gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), &pos_iter, cursor_position);
  gtk_text_buffer_select_range (GTK_TEXT_BUFFER (buffer), &pos_iter, &pos_iter);
  gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (buffer));

  _ide_buffer_request_scroll_to_cursor (buffer);

  ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
gbp_clang_format_format_async (IdeFormatter        *formatter,
                               IdeBuffer           *buffer,
                               IdeFormatterOptions *options,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  GbpClangFormatter *self = (GbpClangFormatter *)formatter;
  g_autoptr(IdeTask) task = NULL;
  GtkSourceLanguage *language;
  g_autofree gchar *config_dir = NULL;
  GtkTextIter iter;
  g_autofree char *cursor_arg = NULL;
  const char *stdin_buf = NULL;
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_CLANG_FORMATTER (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_clang_format_format_async);
  ide_task_set_task_data (task, g_object_ref (buffer), g_object_unref);

  if (!(language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (buffer))) ||
    !clang_supports_language (language))
  {
    ide_task_return_boolean (task, TRUE);
    IDE_EXIT;
  }

  /* Locate closest .clang-format file. If we cannot find one try to
   * format using our fallback formatter.
   */
  if (!(config_dir = gbp_clang_format_get_config_file_dir (buffer, cancellable)))
    {
      ide_object_warning (formatter,
                          _("Cannot locate .clang-format, please add one to your project. Using fallback GNU-style formatter."));
      config_dir = g_build_filename (PACKAGE_DATADIR, "clang-format", NULL);
    }

  gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer),
                                    &iter,
                                    gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (buffer)));
  cursor_arg = g_strdup_printf ("--cursor=%u", gtk_text_iter_get_offset (&iter));
  stdin_buf = g_bytes_get_data (ide_buffer_dup_content (buffer), NULL);

  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDIN_PIPE |
                                          G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                          G_SUBPROCESS_FLAGS_STDERR_PIPE);
  ide_subprocess_launcher_set_cwd (launcher, config_dir);
  ide_subprocess_launcher_push_argv (launcher, "clang-format");
  ide_subprocess_launcher_push_argv (launcher, cursor_arg);

  subprocess = ide_subprocess_launcher_spawn (launcher, NULL, &error);
  if (subprocess == NULL)
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  ide_subprocess_communicate_utf8_async (subprocess,
                                         stdin_buf,
                                         cancellable,
                                         gbp_clang_format_communicate_cb,
                                         g_steal_pointer (&task));
  IDE_EXIT;
}

static gboolean
gbp_clang_format_format_finish (IdeFormatter  *formatter,
                                GAsyncResult  *result,
                                GError       **error)
{
  IDE_ENTRY;

  g_assert (GBP_IS_CLANG_FORMATTER (formatter));
  g_assert (IDE_IS_TASK (result));

  IDE_RETURN (ide_task_propagate_boolean (IDE_TASK (result), error));
}

static void
ide_formatter_iface_init (IdeFormatterInterface *iface)
{
  iface->format_async = gbp_clang_format_format_async;
  iface->format_finish = gbp_clang_format_format_finish;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpClangFormatter, gbp_clang_formatter, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_FORMATTER, ide_formatter_iface_init))

static void
gbp_clang_formatter_class_init (GbpClangFormatterClass *klass)
{
}

static void
gbp_clang_formatter_init (GbpClangFormatter *self)
{
}
