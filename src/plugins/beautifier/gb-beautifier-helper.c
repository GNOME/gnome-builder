/* gb-beautifier-helper.c
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

#define G_LOG_DOMAIN "gb-beautifier-helper"

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtksourceview/gtksource.h>
#include <libide-editor.h>
#include <string.h>

#include "gb-beautifier-helper.h"
#include "gb-beautifier-private.h"

static gboolean
check_path_is_in_tmp_dir (const gchar *path,
                          const gchar *tmp_dir)
{
  g_autofree gchar *with_slash = NULL;

  g_assert (!dzl_str_empty0 (path));
  g_assert (!dzl_str_empty0 (tmp_dir));

  if (dzl_str_equal0 (path, tmp_dir))
    return TRUE;

  if (!g_str_has_suffix (tmp_dir, G_DIR_SEPARATOR_S))
    tmp_dir = with_slash = g_strconcat (tmp_dir, G_DIR_SEPARATOR_S, NULL);

  return g_str_has_prefix (path, tmp_dir);
}

void
gb_beautifier_helper_remove_temp_for_path (GbBeautifierEditorAddin *self,
                                           const gchar             *path)
{
  g_assert (path != NULL);

  if (check_path_is_in_tmp_dir (path, self->tmp_dir))
    g_unlink (path);
  else
    {
      ide_object_warning (self,
                          /* translators: %s and %s are replaced with the temporary dir and the file path */
                          _("Beautifier plugin: blocked attempt to remove a file outside of the “%s” temporary directory: “%s”"),
                          self->tmp_dir,
                          path);
      return;
    }
}

void
gb_beautifier_helper_remove_temp_for_file (GbBeautifierEditorAddin *self,
                                           GFile                   *file)
{
  g_autofree gchar *path = NULL;

  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));
  g_assert (file != NULL);
  g_assert (G_IS_FILE (file));

  path = g_file_get_path (file);

  if (check_path_is_in_tmp_dir (path, self->tmp_dir))
    g_file_delete (file, NULL, NULL);
  else
    {
      ide_object_warning (self,
                          /* translators: %s and %s are replaced with the temporary dir and the file path */
                          _("Beautifier plugin: blocked attempt to remove a file outside of the “%s” temporary directory: “%s”"),
                          self->tmp_dir,
                          path);
      return;
    }
}

void
gb_beautifier_helper_config_entry_remove_temp_files (GbBeautifierEditorAddin *self,
                                                     GbBeautifierConfigEntry *config_entry)
{
  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));
  g_assert (config_entry != NULL);

  if (config_entry->is_config_file_temp)
    {
      if (G_IS_FILE (config_entry->config_file))
        {
          g_autofree gchar *config_path = g_file_get_path (config_entry->config_file);

          if (check_path_is_in_tmp_dir (config_path, self->tmp_dir))
            g_file_delete (config_entry->config_file, NULL, NULL);
          else
            {
              ide_object_warning (self,
                                  /* translators: %s and %s are replaced with the temporary dir and the file path */
                                  _("Beautifier plugin: blocked attempt to remove a file outside of the “%s” temporary directory: “%s”"),
                                  self->tmp_dir,
                                  config_path);
              return;
            }
        }
    }

  if (config_entry->command_args != NULL)
    {
      for (guint i = 0; i < config_entry->command_args->len; i++)
        {
          const GbBeautifierCommandArg *arg = &g_array_index (config_entry->command_args, GbBeautifierCommandArg, i);

          if (arg->is_temp && !dzl_str_empty0 (arg->str))
            {
              if (check_path_is_in_tmp_dir (arg->str, self->tmp_dir))
                g_unlink (arg->str);
              else
                {
                  ide_object_warning (self,
                                      _("Beautifier plugin: blocked attempt to remove a file outside of the “%s” temporary directory: “%s”"),
                                      self->tmp_dir,
                                      arg->str);
                  continue;
                }
            }
        }
    }
}

static void
gb_beautifier_helper_create_tmp_file_cb (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
  GFile *file = (GFile *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeTask) task = (IdeTask *)user_data;

  g_assert (G_IS_FILE (file));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!g_file_replace_contents_finish (file, result, NULL, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  if (ide_task_return_error_if_cancelled (task))
    g_file_delete (file, NULL, NULL);
  else
    ide_task_return_pointer (task, g_object_ref (file), g_object_unref);
}

void
gb_beautifier_helper_create_tmp_file_async (GbBeautifierEditorAddin *self,
                                            const gchar             *text,
                                            GAsyncReadyCallback      callback,
                                            GCancellable            *cancellable,
                                            gpointer                 user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GFile) file = NULL;
  g_autoptr(GBytes) bytes = NULL;
  g_autofree gchar *tmp_path = NULL;
  gint fd;

  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));
  g_assert (!dzl_str_empty0 (text));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (callback != NULL);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gb_beautifier_helper_create_tmp_file_async);

  tmp_path = g_build_filename (self->tmp_dir, "XXXXXX.txt", NULL);
  if (-1 == (fd = g_mkstemp (tmp_path)))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "Failed to create temporary file for the Beautifier plugin");
      return;
    }

  g_close (fd, NULL);
  file = g_file_new_for_path (tmp_path);

  bytes = g_bytes_new (text, strlen (text));

  g_file_replace_contents_bytes_async (file,
                                       bytes,
                                       NULL,
                                       FALSE,
                                       G_FILE_CREATE_REPLACE_DESTINATION,
                                       NULL,
                                       gb_beautifier_helper_create_tmp_file_cb,
                                       g_steal_pointer (&task));
}

GFile *
gb_beautifier_helper_create_tmp_file_finish (GbBeautifierEditorAddin  *self,
                                             GAsyncResult             *result,
                                             GError                  **error)
{
  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));
  g_assert (IDE_IS_TASK (result));
  g_assert (ide_task_is_valid (IDE_TASK (result), self));

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

gchar *
gb_beautifier_helper_match_and_replace (const gchar *str,
                                        const gchar *pattern,
                                        const gchar *replacement)
{
  g_autofree gchar *head = NULL;
  g_autofree gchar *tail = NULL;
  gchar *needle;
  gsize head_len;

  g_assert (!dzl_str_empty0 (str));
  g_assert (!dzl_str_empty0 (pattern));

  if (NULL != (needle = g_strstr_len (str, -1, pattern)))
    {
      head_len = needle - str;
      if (head_len > 0)
        head = g_strndup (str, head_len);
      else
        head = g_strdup ("");

      tail = needle + strlen (pattern);
      if (*tail != '\0')
        tail = g_strdup (tail);
      else
        tail = g_strdup ("");

      return g_strconcat (head, replacement, tail, NULL);
    }
  else
    return NULL;
}

const gchar *
gb_beautifier_helper_get_lang_id (GbBeautifierEditorAddin *self,
                                  IdeSourceView           *view)
{
  GtkTextBuffer *buffer;
  GtkSourceLanguage *lang;

  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));
  g_assert (IDE_IS_SOURCE_VIEW (view));

  buffer = GTK_TEXT_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));
  if (NULL == (lang = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (buffer))))
    {
      g_debug ("Beautifier plugin: Can't find a GtkSourceLanguage for the buffer");
      return NULL;
    }

  return gtk_source_language_get_id (lang);
}
