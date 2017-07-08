/* test-ide-file-settings.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#include <ide.h>

#include "editorconfig/ide-editorconfig-file-settings.h"

static void
test_filesettings (void)
{
  IdeFileSettings *settings = NULL;
  IdeContext *dummy;
  IdeFile *file;
  GFile *gfile;

  dummy = g_object_new (IDE_TYPE_CONTEXT, NULL);
  gfile = g_file_new_for_path ("test.c");
  file = g_object_new (IDE_TYPE_FILE,
                       "context", dummy,
                       "file", gfile,
                       "path", "test.c",
                       NULL);
  settings = g_object_new (IDE_TYPE_FILE_SETTINGS,
                           "file", file,
                           "context", dummy,
                           NULL);
  g_object_add_weak_pointer (G_OBJECT (settings), (gpointer *)&settings);

  ide_file_settings_set_tab_width (settings, 8);
  g_assert_cmpint (ide_file_settings_get_tab_width (settings), ==, 8);
  ide_file_settings_set_tab_width (settings, 2);
  g_assert_cmpint (ide_file_settings_get_tab_width (settings), ==, 2);

  ide_file_settings_set_indent_width (settings, 8);
  g_assert_cmpint (ide_file_settings_get_indent_width (settings), ==, 8);
  ide_file_settings_set_indent_width (settings, -1);
  g_assert_cmpint (ide_file_settings_get_indent_width (settings), ==, -1);

  ide_file_settings_set_encoding (settings, "ascii");
  g_assert_cmpstr (ide_file_settings_get_encoding (settings), ==, "ascii");
  ide_file_settings_set_encoding (settings, "utf-8");
  g_assert_cmpstr (ide_file_settings_get_encoding (settings), ==, "utf-8");

  ide_file_settings_set_insert_trailing_newline (settings, FALSE);
  g_assert_false (ide_file_settings_get_insert_trailing_newline (settings));
  ide_file_settings_set_insert_trailing_newline (settings, TRUE);
  g_assert_true (ide_file_settings_get_insert_trailing_newline (settings));

  ide_file_settings_set_newline_type (settings, GTK_SOURCE_NEWLINE_TYPE_CR);
  g_assert_true (ide_file_settings_get_newline_type_set (settings));
  g_assert_cmpint (ide_file_settings_get_newline_type (settings), ==, GTK_SOURCE_NEWLINE_TYPE_CR);
  ide_file_settings_set_newline_type (settings, GTK_SOURCE_NEWLINE_TYPE_CR_LF);
  g_assert_cmpint (ide_file_settings_get_newline_type (settings), ==, GTK_SOURCE_NEWLINE_TYPE_CR_LF);
  ide_file_settings_set_newline_type (settings, GTK_SOURCE_NEWLINE_TYPE_LF);
  g_assert_cmpint (ide_file_settings_get_newline_type (settings), ==, GTK_SOURCE_NEWLINE_TYPE_LF);

  ide_file_settings_set_right_margin_position (settings, 200);
  g_assert_cmpint (ide_file_settings_get_right_margin_position (settings), ==, 200);

  ide_file_settings_set_indent_style (settings, IDE_INDENT_STYLE_SPACES);
  g_assert_cmpint (ide_file_settings_get_indent_style (settings), ==, IDE_INDENT_STYLE_SPACES);

  ide_file_settings_set_trim_trailing_whitespace (settings, TRUE);
  g_assert_true (ide_file_settings_get_trim_trailing_whitespace (settings));
  ide_file_settings_set_trim_trailing_whitespace (settings, FALSE);
  g_assert_false (ide_file_settings_get_trim_trailing_whitespace (settings));

  ide_file_settings_set_show_right_margin (settings, TRUE);
  g_assert_true (ide_file_settings_get_show_right_margin_set (settings));
  g_assert_true (ide_file_settings_get_show_right_margin (settings));
  ide_file_settings_set_show_right_margin (settings, FALSE);
  g_assert_false (ide_file_settings_get_show_right_margin (settings));

  g_object_unref (settings);
  g_assert (settings == NULL);
  g_clear_object (&file);
  g_clear_object (&gfile);
  g_clear_object (&dummy);
}

static void
test_editorconfig_new_cb (GObject      *object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  GAsyncInitable *initable = (GAsyncInitable *)object;
  GMainLoop *main_loop = user_data;
  IdeFileSettings *settings;
  GObject *res;
  GError *error = NULL;

  g_assert (G_IS_ASYNC_INITABLE (initable));
  g_assert (main_loop != NULL);

  res = g_async_initable_new_finish (initable, result, &error);
  g_assert_no_error (error);
  g_assert (res != NULL);
  g_assert (IDE_IS_EDITORCONFIG_FILE_SETTINGS (res));

  settings = IDE_FILE_SETTINGS (res);
  g_assert (settings != NULL);

  g_assert_cmpint (ide_file_settings_get_tab_width (settings), ==, 4);
  g_assert_cmpint (ide_file_settings_get_indent_width (settings), ==, 2);
  g_assert_cmpstr (ide_file_settings_get_encoding (settings), ==, "utf-8");
  g_assert_cmpint (ide_file_settings_get_indent_style (settings), ==, IDE_INDENT_STYLE_SPACES);

  g_main_loop_quit (main_loop);
}

static void
test_editorconfig (void)
{
  GMainLoop *main_loop;
  IdeContext *dummy;
  IdeFile *file;
  GFile *gfile;

  dummy = g_object_new (IDE_TYPE_CONTEXT, NULL);
  gfile = g_file_new_for_path (TEST_DATA_DIR"/project1/test.c");
  file = g_object_new (IDE_TYPE_FILE,
                       "context", dummy,
                       "file", gfile,
                       "path", TEST_DATA_DIR"/project1/test.c",
                       NULL);

  main_loop = g_main_loop_new (NULL, FALSE);

  g_async_initable_new_async (IDE_TYPE_EDITORCONFIG_FILE_SETTINGS,
                              G_PRIORITY_DEFAULT,
                              NULL,
                              test_editorconfig_new_cb,
                              main_loop,
                              "file", file,
                              "context", dummy,
                              NULL);

  g_main_loop_run (main_loop);

  g_main_loop_unref (main_loop);
  g_clear_object (&file);
  g_clear_object (&gfile);
  g_clear_object (&dummy);
}

gint
main (gint argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Ide/FileSettings/basic", test_filesettings);
  g_test_add_func ("/Ide/EditorconfigFileSettings/basic", test_editorconfig);
  return g_test_run ();
}
