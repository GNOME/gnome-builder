/* test-vim.c
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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
#include <string.h>

#include "application/ide-application-tests.h"

#include "../plugins/gnome-builder-plugins.h"

typedef void (*VimTestFunc) (IdeContext *context,
                             GtkWidget  *widget);

static void test_vim_basic_cb (IdeContext *context,
                               GtkWidget  *widget);

struct {
  const gchar *path;
  VimTestFunc  func;
} vim_tests [] = {
  { "test.c", test_vim_basic_cb },
  { NULL }
};

static void
new_context_cb (GObject      *object,
                GAsyncResult *result,
                gpointer      user_data)
{
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  GtkWidget *window;
  GtkWidget *widget;
  IdeBuffer *buffer;
  GtkSourceCompletion *completion;
  IdeContext *context;

  context = ide_context_new_finish (result, &error);
  g_assert_no_error (error);
  g_assert (context != NULL);
  g_assert (IDE_IS_CONTEXT (context));

  for (gint i = 0; vim_tests [i].path; i++)
    {
      g_autoptr(IdeFile) file = ide_file_new_for_path (context, vim_tests [i].path);

      buffer = g_object_new (IDE_TYPE_BUFFER,
                             "context", context,
                             "file", file,
                             NULL);

      window = gtk_offscreen_window_new ();
      widget = g_object_new (IDE_TYPE_SOURCE_VIEW,
                             "auto-indent", TRUE,
                             "buffer", buffer,
                             "visible", TRUE,
                             NULL);
      gtk_container_add (GTK_CONTAINER (window), widget);

      completion = gtk_source_view_get_completion (GTK_SOURCE_VIEW (widget));
      gtk_source_completion_block_interactive (completion);

      gtk_window_present (GTK_WINDOW (window));

      while (gtk_events_pending ())
        gtk_main_iteration ();

      vim_tests [i].func (context, widget);
    }

  g_task_return_boolean (task, TRUE);
}

static void
test_vim_basic (GCancellable        *cancellable,
                GAsyncReadyCallback  callback,
                gpointer             user_data)
{
  g_autoptr(GFile) project_file = NULL;
  GTask *task;

  task = g_task_new (NULL, cancellable, callback, user_data);
  project_file = g_file_new_for_path (TEST_DATA_DIR "/project1/configure.ac");
  g_assert (g_file_query_exists (project_file, NULL));
  ide_context_new_async (project_file,
                         NULL,
                         new_context_cb,
                         task);

}

/*
 * Converts the input_chars into GdkEventKeys and synthesizes them to
 * the widget. Then ensures that we get the proper string back out.
 */
static void
assert_keypress_equal (GtkWidget   *widget,
                       const gchar *input_chars,
                       const gchar *output_str)
{
  g_autofree gchar *result = NULL;
  GtkTextView *text_view = (GtkTextView *)widget;
  GtkTextBuffer *buffer;
  GtkTextIter begin;
  GtkTextIter end;
  GdkWindow *window;

  g_assert (GTK_IS_TEXT_VIEW (widget));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));
  g_assert (GTK_IS_TEXT_BUFFER (buffer));

  window = gtk_text_view_get_window (text_view, GTK_TEXT_WINDOW_TEXT);
  g_assert (GDK_IS_WINDOW (window));

  for (; *input_chars; input_chars = g_utf8_next_char (input_chars))
    {
      gunichar ch = g_utf8_get_char (input_chars);
      GdkEventKey *event;

      while (gtk_events_pending ())
        gtk_main_iteration ();

      event = dzl_gdk_synthesize_event_key (window, ch);
      gtk_main_do_event ((GdkEvent *)event);
      gdk_event_free ((GdkEvent *)event);
    }

  gtk_text_buffer_get_bounds (buffer, &begin, &end);
  result = gtk_text_buffer_get_text (buffer, &begin, &end, TRUE);

  g_assert_cmpstr (result, ==, output_str);

  gtk_text_buffer_set_text (buffer, "", 0);
}

static void
test_vim_basic_cb (IdeContext *context,
                   GtkWidget  *widget)
{
  g_object_set (widget,
                "insert-matching-brace", TRUE,
                "overwrite-braces", TRUE,
                NULL);

  assert_keypress_equal (widget, "ithis is a test.\e", "this is a test.");
  assert_keypress_equal (widget, "ithis is a test.\eI\e4x\e", " is a test.");
  assert_keypress_equal (widget, "ido_something (NULL)\ea;\ehhhciwfoo\e", "do_something (foo);");
  assert_keypress_equal (widget, "itesting.\edd\e", "");
  assert_keypress_equal (widget, "i\n\n\edd\e", "\n");
  assert_keypress_equal (widget, "dd\e", "");
  assert_keypress_equal (widget, "iabcd defg hijk\e02de\e", " hijk");
  assert_keypress_equal (widget, "iabcd defg hijk\e0d$\e", "");

#if 0
  /* this to fix in our vim */
  assert_keypress_equal (widget, "i\nabcd\n\ekcipfoo", "\nfoo\n");
#endif
}

gint
main (gint   argc,
      gchar *argv[])
{
  static const gchar *required_plugins[] = { "autotools-plugin", "directory-plugin", NULL };
  IdeApplication *app;
  gint ret;

  g_test_init (&argc, &argv, NULL);

  ide_log_init (TRUE, NULL);
  ide_log_set_verbosity (4);

  app = ide_application_new ();
  ide_application_add_test (app, "/Ide/Vim/basic", test_vim_basic, NULL, required_plugins);
  gnome_builder_plugins_init ();
  ret = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);

  return ret;
}
