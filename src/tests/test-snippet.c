/* test-snippet.c
 *
 * Copyright 2016 Christian Hergert <chergert@redhat.com>
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
#include "plugins/gnome-builder-plugins.h"
#include "snippets/ide-source-snippet-private.h"
#include "util/ide-gdk.h"

static void
dump_selection (IdeSourceView *view)
{
  GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
  GtkTextIter begin;
  GtkTextIter end;

  gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);

  g_message ("Selection: %d:%d to %d:%d\n",
             gtk_text_iter_get_line (&begin) + 1,
             gtk_text_iter_get_line_offset (&begin) + 1,
             gtk_text_iter_get_line (&end) + 1,
             gtk_text_iter_get_line_offset (&end) + 1);
}

static gboolean
mark_done (gpointer data)
{
  gboolean *done = data;
  *done = TRUE;
  return G_SOURCE_REMOVE;
}

static void
pump_loop (void)
{
  gboolean done = FALSE;

  /*
   * timeout value of 100 was found experimentally.  this is utter crap, but i
   * don't have a clear event to key off with signals. we are sort of just
   * waiting for the textview to complete processing events.
   */

  g_timeout_add (100, mark_done, &done);

  for (;;)
    {
      gtk_main_iteration_do (TRUE);
      if (done)
        break;
    }
}

static void
emit_and_pump_loop (gpointer     instance,
                    const gchar *name,
                    ...)
{
  guint signal_id;

  va_list args;

  signal_id = g_signal_lookup (name, G_OBJECT_TYPE (instance));
  g_assert (signal_id != 0);

  va_start (args, name);
  g_signal_emit_valist (instance, signal_id, 0, args);
  va_end (args);

  pump_loop ();
}

static void
on_event (GdkFrameClock *clock,
          gpointer       data)
{
  *(gint *)data += 1;
}

static void
send_event_and_wait_for_flush (GdkEventKey *event)
{
  GdkFrameClock *clock = gdk_window_get_frame_clock (event->window);
  gint events = 0;

  g_signal_connect (clock, "after-paint", G_CALLBACK (on_event), &events);
  g_signal_connect (clock, "before-paint", G_CALLBACK (on_event), &events);

  gtk_main_do_event ((GdkEvent *)event);

  while (events < 2 || gtk_events_pending ())
    gtk_main_iteration ();

  g_signal_handlers_disconnect_by_func (clock, on_event, &events);
}

static void
move_next (IdeSourceView *view)
{
  GdkWindow *window;
  GdkEventKey *event;

  g_assert (IDE_IS_SOURCE_VIEW (view));

  while (gtk_events_pending ())
    gtk_main_iteration ();

  window = gtk_text_view_get_window (GTK_TEXT_VIEW (view), GTK_TEXT_WINDOW_TEXT);
  event = ide_gdk_synthesize_event_key (window, '\t');
  event->keyval = GDK_KEY_Tab;
  send_event_and_wait_for_flush (event);
  gdk_event_free ((GdkEvent *)event);

  pump_loop ();
}

static void
move_previous (IdeSourceView *view)
{
  GdkWindow *window;
  GdkEventKey *event;

  g_assert (IDE_IS_SOURCE_VIEW (view));

  while (gtk_events_pending ())
    gtk_main_iteration ();

  window = gtk_text_view_get_window (GTK_TEXT_VIEW (view), GTK_TEXT_WINDOW_TEXT);
  event = ide_gdk_synthesize_event_key (window, '\t');
  event->keyval = GDK_KEY_ISO_Left_Tab;
  send_event_and_wait_for_flush (event);
  gdk_event_free ((GdkEvent *)event);

  pump_loop ();
}

static void
new_context_cb (GObject      *object,
                GAsyncResult *result,
                gpointer      user_data)
{
  g_autoptr(GTask) task = user_data;
  g_autoptr(IdeSourceSnippet) snippet = NULL;
  g_autoptr(IdeSourceSnippetChunk) chunk1 = NULL;
  g_autoptr(IdeSourceSnippetChunk) chunk2 = NULL;
  g_autoptr(IdeSourceSnippetChunk) chunk3 = NULL;
  g_autoptr(IdeSourceSnippetChunk) chunk4 = NULL;
  g_autoptr(IdeSourceSnippetChunk) chunk5 = NULL;
  g_autoptr(IdeBuffer) buffer = NULL;
  g_autoptr(IdeFile) file = NULL;
  IdeSourceView *view;
  IdeContext *context;
  IdeProject *project;
  GtkWidget *window;
  GError *error = NULL;

  context = ide_context_new_finish (result, &error);
  g_assert_no_error (error);
  g_assert (context != NULL);
  g_assert (IDE_IS_CONTEXT (context));

  project = ide_context_get_project (context);

  /*
   * This test creates a new source view and adds a snippet to it. We
   * create snippet and chunks manually so we can see how they are modified
   * based on edits we make to the buffer manually.
   */

  snippet = ide_source_snippet_new ("foobarbaz", "c");

  chunk1 = ide_source_snippet_chunk_new ();
  ide_source_snippet_chunk_set_spec (chunk1, "this is\nchunk 1 ");
  ide_source_snippet_add_chunk (snippet, chunk1);

  chunk2 = ide_source_snippet_chunk_new ();
  ide_source_snippet_chunk_set_spec (chunk2, "this is tab stop 1");
  ide_source_snippet_chunk_set_tab_stop (chunk2, 1);
  ide_source_snippet_add_chunk (snippet, chunk2);

  chunk3 = ide_source_snippet_chunk_new ();
  ide_source_snippet_chunk_set_spec (chunk3, ",\nthis is chunk 3");
  ide_source_snippet_add_chunk (snippet, chunk3);

  chunk4 = ide_source_snippet_chunk_new ();
  ide_source_snippet_chunk_set_spec (chunk4, "$1");
  ide_source_snippet_add_chunk (snippet, chunk4);

  chunk5 = ide_source_snippet_chunk_new ();
  ide_source_snippet_chunk_set_spec (chunk5, "this is tab stop 2");
  ide_source_snippet_chunk_set_tab_stop (chunk5, 2);
  ide_source_snippet_add_chunk (snippet, chunk5);

  file = ide_project_get_file_for_path (project, "test.txt");
  buffer = g_object_new (IDE_TYPE_BUFFER,
                         "context", context,
                         "file", file,
                         "highlight-diagnostics", FALSE,
                         "highlight-syntax", FALSE,
                         NULL);

  window = gtk_offscreen_window_new ();
  view = g_object_new (IDE_TYPE_SOURCE_VIEW,
                       "auto-indent", TRUE,
                       "buffer", buffer,
                       "visible", TRUE,
                       NULL);
  gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (view));
  gtk_window_present (GTK_WINDOW (window));

  gtk_source_completion_block_interactive (gtk_source_view_get_completion (GTK_SOURCE_VIEW (view)));

  ide_source_view_push_snippet (view, snippet, NULL);

  pump_loop ();

  ide_source_snippet_dump (snippet);

  g_assert_cmpstr ("this is\nchunk 1 ", ==, ide_source_snippet_chunk_get_text (chunk1));
  g_assert_cmpstr ("this is tab stop 1", ==, ide_source_snippet_chunk_get_text (chunk2));
  g_assert_cmpstr (",\nthis is chunk 3", ==, ide_source_snippet_chunk_get_text (chunk3));
  g_assert_cmpstr ("this is tab stop 1", ==, ide_source_snippet_chunk_get_text (chunk4));
  g_assert_cmpstr ("this is tab stop 2", ==, ide_source_snippet_chunk_get_text (chunk5));

  /*
   * Now is where we start getting tricky. We want to move to
   * to various locations and remove/insert text ensure that
   * our run-length detecters in the snippets reaction to
   * insert-text/delete-range are effective.
   */

  /* overwrite the current snippet text at tab stop 1, our current focus */
  emit_and_pump_loop (view, "backspace");
  emit_and_pump_loop (view, "insert-at-cursor", "this is tab stop 1, edit 1");

  ide_source_snippet_dump (snippet);

  g_assert_cmpstr ("this is\nchunk 1 ", ==, ide_source_snippet_chunk_get_text (chunk1));
  g_assert_cmpstr ("this is tab stop 1, edit 1", ==, ide_source_snippet_chunk_get_text (chunk2));
  g_assert_cmpstr (",\nthis is chunk 3", ==, ide_source_snippet_chunk_get_text (chunk3));
  g_assert_cmpstr ("this is tab stop 1, edit 1", ==, ide_source_snippet_chunk_get_text (chunk4));
  g_assert_cmpstr ("this is tab stop 2", ==, ide_source_snippet_chunk_get_text (chunk5));

  /* Now move to our second tab stop, but exercise forward/backward/forward */
  move_next (view);
  move_previous (view);
  move_next (view);
  move_previous (view);
  move_next (view);

  ide_source_snippet_dump (snippet);

  dump_selection (view);

  /* Now tweak tab stop 2 values, and see what happens */
  emit_and_pump_loop (view, "backspace");
  emit_and_pump_loop (view, "insert-at-cursor", "this is tab stop 2, edit 1");

  ide_source_snippet_dump (snippet);

  g_assert_cmpstr ("this is\nchunk 1 ", ==, ide_source_snippet_chunk_get_text (chunk1));
  g_assert_cmpstr ("this is tab stop 1, edit 1", ==, ide_source_snippet_chunk_get_text (chunk2));
  g_assert_cmpstr (",\nthis is chunk 3", ==, ide_source_snippet_chunk_get_text (chunk3));
  g_assert_cmpstr ("this is tab stop 1, edit 1", ==, ide_source_snippet_chunk_get_text (chunk4));
  g_assert_cmpstr ("this is tab stop 2, edit 1", ==, ide_source_snippet_chunk_get_text (chunk5));

  g_task_return_boolean (task, TRUE);
}

static void
test_snippets_basic (GCancellable        *cancellable,
                     GAsyncReadyCallback  callback,
                     gpointer             user_data)
{
  g_autoptr(GFile) project_file = NULL;
  g_autoptr(GSettings) settings = NULL;
  GTask *task;

  settings = g_settings_new ("org.gnome.builder.code-insight");
  g_settings_set_boolean (settings, "semantic-highlighting", FALSE);

  task = g_task_new (NULL, cancellable, callback, user_data);
  project_file = g_file_new_for_path (TEST_DATA_DIR"/project1/configure.ac");
  ide_context_new_async (project_file, NULL, new_context_cb, task);
}

gint
main (gint   argc,
      gchar *argv[])
{
  IdeApplication *app;
  gint ret;

  g_test_init (&argc, &argv, NULL);

  ide_log_init (TRUE, NULL);
  ide_log_set_verbosity (4);

  app = ide_application_new (IDE_APPLICATION_MODE_TESTS);
  ide_application_add_test (app, "/Ide/Snippets/basic", test_snippets_basic, NULL);
  gnome_builder_plugins_init ();
  ret = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);

  return ret;
}
