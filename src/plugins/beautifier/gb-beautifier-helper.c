/* gb-beautifier-helper.c
 *
 * Copyright © 2016 sebastien lafargue <slafargue@gnome.org>
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
 */

#define G_LOG_DOMAIN "gb-beautifier-helper"

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtksourceview/gtksource.h>
#include <ide.h>
#include <string.h>

#include "gb-beautifier-helper.h"
#include "gb-beautifier-private.h"

typedef struct
{
  GbBeautifierEditorAddin *self;
  GFile                   *file;
  GFileIOStream           *stream;
  gsize                    len;
} SaveTmpState;

static gboolean
check_path_is_in_tmp_dir (const gchar *path,
                          const gchar *tmp_dir)
{
  g_assert (!dzl_str_empty0 (path));
  g_assert (!dzl_str_empty0 (tmp_dir));

  return  g_str_has_prefix (path, tmp_dir);
}

void
gb_beautifier_helper_remove_temp_for_path (GbBeautifierEditorAddin *self,
                                           const gchar             *path)
{
  const gchar *tmp_dir;

  g_assert (path != NULL);

  tmp_dir = g_get_tmp_dir ();

  if (check_path_is_in_tmp_dir (path, tmp_dir))
    g_unlink (path);
  else
    {
      /* translators: %s and %s are replaced with the temporary dir and the file path */
      ide_object_warning (self,
                          _("Beautifier plugin: blocked attempt to remove a file outside of the “%s” temporary dir: “%s”"),
                          tmp_dir,
                          path);
      return;
    }
}

void
gb_beautifier_helper_remove_temp_for_file (GbBeautifierEditorAddin *self,
                                           GFile                   *file)
{
  const gchar *tmp_dir;
  g_autofree gchar *path = NULL;

  g_assert (G_IS_FILE (file));

  tmp_dir = g_get_tmp_dir ();
  path = g_file_get_path (file);

  if (check_path_is_in_tmp_dir (path, tmp_dir))
    g_file_delete (file, NULL, NULL);
  else
    {
      /* translators: %s and %s are replaced with the temporary dir and the file path */
      ide_object_warning (self,
                          _("Beautifier plugin: blocked attempt to remove a file outside of the “%s” temporary dir: “%s”"),
                          tmp_dir,
                          path);
      return;
    }
}

void
gb_beautifier_helper_config_entry_remove_temp_files (GbBeautifierEditorAddin *self,
                                                     GbBeautifierConfigEntry *config_entry)
{
  GbBeautifierCommandArg *arg;
  g_autofree gchar *config_path = NULL;
  const gchar *tmp_dir;

  g_assert (config_entry != NULL);

  tmp_dir = g_get_tmp_dir ();
  if (config_entry->is_config_file_temp)
    {

      if (G_IS_FILE (config_entry->config_file))
        {
          config_path = g_file_get_path (config_entry->config_file);
          if (check_path_is_in_tmp_dir (config_path, tmp_dir))
            g_file_delete (config_entry->config_file, NULL, NULL);
          else
            {
              /* translators: %s and %s are replaced with the temporary dir and the file path */
              ide_object_warning (self,
                                  _("Beautifier plugin: blocked attempt to remove a file outside of the “%s” temporary dir: “%s”"),
                                  tmp_dir,
                                  config_path);
              return;
            }
        }
    }

  if (config_entry->command_args != NULL)
    {
      for (guint i = 0; i < config_entry->command_args->len; i++)
        {
          arg = &g_array_index (config_entry->command_args, GbBeautifierCommandArg, i);
          if (arg->is_temp && !dzl_str_empty0 (arg->str))
            {
              if (check_path_is_in_tmp_dir (arg->str, tmp_dir))
                g_unlink (arg->str);
              else
                {
                  ide_object_warning (self,
                                      _("Beautifier plugin: blocked attempt to remove a file outside of the “%s” temporary dir: “%s”"),
                                      tmp_dir,
                                      arg->str);
                  return;
                }
            }
        }
    }
}

static void
save_tmp_state_free (gpointer data)
{
  SaveTmpState *state = (SaveTmpState *)data;

  g_assert (data != NULL);

  if (!g_io_stream_is_closed ((GIOStream *)state->stream))
    g_io_stream_close ((GIOStream *)state->stream, NULL, NULL);

  g_clear_object (&state->file);

  g_slice_free (SaveTmpState, state);
}

static void
gb_beautifier_helper_create_tmp_file_cb (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
  GOutputStream *output_stream = (GOutputStream *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = (GTask *)user_data;
  SaveTmpState *state;
  gsize count;

  g_assert (G_IS_OUTPUT_STREAM (output_stream));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  state = (SaveTmpState *)g_task_get_task_data (task);
  if (!g_output_stream_write_all_finish (output_stream, result, &count, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else if (g_task_return_error_if_cancelled (task))
    g_file_delete (state->file, NULL, NULL);
  else
    g_task_return_pointer (task, g_steal_pointer (&state->file), g_object_unref);
}

void
gb_beautifier_helper_create_tmp_file_async (GbBeautifierEditorAddin *self,
                                            const gchar             *text,
                                            GAsyncReadyCallback      callback,
                                            GCancellable            *cancellable,
                                            gpointer                 user_data)
{
  SaveTmpState *state;
  GFile *file = NULL;
  GFileIOStream *stream;
  GOutputStream *output_stream;
  g_autoptr(GTask) task = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));
  g_assert (!dzl_str_empty0 (text));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (callback != NULL);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gb_beautifier_helper_create_tmp_file_async);
  state = g_slice_new0 (SaveTmpState);
  state->self = self;
  g_task_set_task_data (task, state, save_tmp_state_free);

  if (NULL == (file = g_file_new_tmp ("gnome-builder-beautifier-XXXXXX.txt", &stream, &error)))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  state->file = file;
  state->stream = stream;
  state->len = strlen (text);

  output_stream = g_io_stream_get_output_stream ((GIOStream *)stream);
  g_output_stream_write_all_async (output_stream,
                                   text,
                                   state->len,
                                   G_PRIORITY_DEFAULT,
                                   cancellable,
                                   gb_beautifier_helper_create_tmp_file_cb,
                                   g_steal_pointer (&task));
}

GFile *
gb_beautifier_helper_create_tmp_file_finish (GbBeautifierEditorAddin  *self,
                                             GAsyncResult             *result,
                                             GError                  **error)
{
  GTask *task = (GTask *)result;

  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));
  g_assert (g_task_is_valid (result, self));

  return g_task_propagate_pointer (task, error);
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
