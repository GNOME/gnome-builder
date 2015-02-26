/* test-ide-source-view.c
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

static IdeContext *gContext;

static void
load_cb (GObject      *object,
         GAsyncResult *result,
         gpointer      user_data)
{
  IdeBufferManager *buffer_manager = (IdeBufferManager *)object;
  g_autoptr(IdeBuffer) buffer = NULL;
  g_autoptr(GError) error = NULL;
  IdeSourceView *source_view = user_data;
  GtkSourceStyleScheme *style;
  GtkSourceStyleSchemeManager *styles;

  g_assert (IDE_IS_SOURCE_VIEW (source_view));

  styles = gtk_source_style_scheme_manager_get_default ();
  style = gtk_source_style_scheme_manager_get_scheme (styles, "builder");

  buffer = ide_buffer_manager_load_file_finish (buffer_manager, result, &error);

  if (!buffer)
    {
      g_warning ("%s", error->message);
      gtk_main_quit ();
      return;
    }

  ide_buffer_set_highlight_diagnostics (buffer, TRUE);
  gtk_source_buffer_set_style_scheme (GTK_SOURCE_BUFFER (buffer), style);
  gtk_text_view_set_buffer (GTK_TEXT_VIEW (source_view), GTK_TEXT_BUFFER (buffer));
  gtk_widget_set_sensitive (GTK_WIDGET (source_view), TRUE);
  gtk_widget_grab_focus (GTK_WIDGET (source_view));
}

static void
context_cb (GObject      *object,
            GAsyncResult *result,
            gpointer      user_data)
{
  IdeSourceView *source_view = user_data;
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeFile) file = NULL;
  IdeProject *project;
  IdeBufferManager *buffer_manager;

  g_assert (IDE_IS_SOURCE_VIEW (source_view));

  if (!(context = ide_context_new_finish (result, &error)))
    {
      g_warning ("%s", error->message);
      gtk_main_quit ();
      return;
    }

  project = ide_context_get_project (context);
  file = ide_project_get_file_for_path (project, "test.c");

  buffer_manager = ide_context_get_buffer_manager (context);
  ide_buffer_manager_load_file_async (buffer_manager, file, FALSE,
                                      NULL, NULL, load_cb, source_view);

  gContext = g_object_ref (context);
}

static gboolean
cancel_ops (GCancellable *cancellable)
{
  g_cancellable_cancel (cancellable);
  return FALSE;
}

gint
main (gint   argc,
      gchar *argv[])
{
  GFile *project_file;
  GtkScrolledWindow *scroller;
  IdeSourceView *source_view;
  GtkWindow *window;
  GCancellable *cancellable;

  ide_set_program_name ("gnome-builder");

  gtk_init (&argc, &argv);

  cancellable = g_cancellable_new ();

  window = g_object_new (GTK_TYPE_WINDOW,
                         "title", "IdeSourceView Test",
                         "default-width", 600,
                         "default-height", 600,
                         NULL);
  scroller = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                           "visible", TRUE,
                           NULL);
  gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (scroller));
  source_view = g_object_new (IDE_TYPE_SOURCE_VIEW,
                              "auto-indent", TRUE,
                              "insert-matching-brace", TRUE,
                              "overwrite-braces", TRUE,
                              "sensitive", FALSE,
                              "show-grid-lines", TRUE,
                              "show-line-changes", TRUE,
                              "show-line-numbers", TRUE,
                              "show-right-margin", TRUE,
                              "snippet-completion", TRUE,
                              "visible", TRUE,
                              NULL);
  gtk_container_add (GTK_CONTAINER (scroller), GTK_WIDGET (source_view));

  project_file = g_file_new_for_path (TEST_DATA_DIR"/project1/configure.ac");
  ide_context_new_async (project_file, cancellable, context_cb, source_view);

  g_signal_connect_swapped (window, "delete-event", G_CALLBACK (cancel_ops), cancellable);
  g_signal_connect (window, "delete-event", gtk_main_quit, NULL);
  gtk_window_present (window);

  gtk_main ();

  g_clear_object (&project_file);
  g_clear_object (&cancellable);
  g_clear_object (&gContext);

  return 0;
}
