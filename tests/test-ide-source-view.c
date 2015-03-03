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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <ide.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <stdlib.h>

#define ADD_CLASS(widget,name) \
  gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(widget)), name)

static IdeContext  *gContext;
static GtkWindow   *gWindow;
static GtkStack    *gDocStack;
static GHashTable  *gBufferToView;
static GList       *gFilesToOpen;
static gint         gExitCode = EXIT_SUCCESS;
static const gchar *gAppCss = "";

static void
quit (int exit_code)
{
  gExitCode = exit_code;
  gtk_main_quit ();
  return;
}

static void
idedit__context_unload_cb (GObject      *object,
                           GAsyncResult *result,
                           gpointer      user_data)
{
  IdeContext *context = (IdeContext *)object;
  GError *error = NULL;

  if (!ide_context_unload_finish (context, result, &error))
    {
      g_printerr ("%s\n", error->message);
      g_clear_error (&error);
    }

  gtk_window_close (gWindow);
}

static gboolean
delete_event_cb (GtkWindow *window,
                 GdkEvent  *event,
                 gpointer   user_data)
{
  if (gContext)
    {
      ide_context_unload_async (gContext,
                                NULL,
                                idedit__context_unload_cb,
                                NULL);
      g_clear_object (&gContext);
      return TRUE;
    }

  gtk_main_quit ();

  return FALSE;
}

static void
add_buffer (IdeBuffer *buffer)
{
  IdeSourceView *view;

  view = g_hash_table_lookup (gBufferToView, buffer);

  if (!view)
    {
      GtkScrolledWindow *scroller;

      scroller = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                               "visible", TRUE,
                               NULL);
      view = g_object_new (IDE_TYPE_SOURCE_VIEW,
                           "auto-indent", TRUE,
                           "buffer", buffer,
                           "highlight-current-line", TRUE,
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
      gtk_container_add (GTK_CONTAINER (scroller), GTK_WIDGET (view));
      gtk_container_add (GTK_CONTAINER (gDocStack), GTK_WIDGET (scroller));
      g_hash_table_insert (gBufferToView, buffer, view);
    }
}

static void
load_buffer_cb (IdeBufferManager *bufmgr,
                IdeBuffer        *buffer,
                gpointer          user_data)
{
  add_buffer (buffer);
}

static void
buffer_loaded_cb (IdeBufferManager *bufmgr,
                  IdeBuffer        *buffer,
                  gpointer          user_data)
{
  add_buffer (buffer);
}

static void
idedit__bufmgr_load_file_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  g_autoptr(IdeBuffer) buf = NULL;
  GError *error = NULL;
  GtkWidget *view;

  buf = ide_buffer_manager_load_file_finish (IDE_BUFFER_MANAGER (object), result, &error);

  if (!buf)
    {
      g_printerr ("%s\n", error->message);
      g_clear_error (&error);
    }

  view = g_hash_table_lookup (gBufferToView, buf);
  if (view)
    {
      GtkSourceStyleScheme *scheme;
      GtkSourceStyleSchemeManager *schememgr;

      schememgr = gtk_source_style_scheme_manager_get_default ();
      scheme = gtk_source_style_scheme_manager_get_scheme (schememgr, "builder");
      gtk_source_buffer_set_style_scheme (GTK_SOURCE_BUFFER (buf), scheme);

      ide_buffer_set_highlight_diagnostics (buf, TRUE);

      gtk_widget_set_sensitive (view, TRUE);
      gtk_widget_grab_focus (view);
    }
}

static void
notify_visible_child_cb (GtkStack   *stack,
                         GParamSpec *pspec,
                         gpointer    user_data)
{
  GtkScrolledWindow *child;
  IdeSourceView *view;
  IdeBuffer *buffer;

  child = GTK_SCROLLED_WINDOW (gtk_stack_get_visible_child (stack));

  if (child)
    {
      view = IDE_SOURCE_VIEW (gtk_bin_get_child (GTK_BIN (child)));
      buffer = IDE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));
      gtk_window_set_title (gWindow, ide_buffer_get_title (buffer));
    }
}

static void
create_window (void)
{
  GtkHeaderBar *header;
  GtkMenuButton *docname;
  GtkMenuButton *langbtn;
  GtkBox *box;
  GtkBox *hbox;
  GtkBox *hbox2;
  GtkButton *back;
  GtkButton *forward;
  GtkSeparator *sep;
  GtkButton *closebtn;

  gWindow = g_object_new (GTK_TYPE_WINDOW,
                          "default-width", 800,
                          "default-height", 600,
                          "title", _("idedit"),
                          NULL);
  g_signal_connect (gWindow, "delete-event", G_CALLBACK (delete_event_cb), NULL);

  header = g_object_new (GTK_TYPE_HEADER_BAR,
                         "show-close-button", TRUE,
                         "title", "idedit",
                         "visible", TRUE,
                         NULL);
  gtk_window_set_titlebar (gWindow, GTK_WIDGET (header));

  box = g_object_new (GTK_TYPE_BOX,
                      "orientation", GTK_ORIENTATION_VERTICAL,
                      "visible", TRUE,
                      NULL);
  gtk_container_add (GTK_CONTAINER (gWindow), GTK_WIDGET (box));

  hbox = g_object_new (GTK_TYPE_BOX,
                       "orientation", GTK_ORIENTATION_HORIZONTAL,
                       "expand", FALSE,
                       "visible", TRUE,
                       NULL);
  ADD_CLASS (hbox, "notebook");
  ADD_CLASS (hbox, "header");
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (hbox));

  /* hack so we can do this with css */
  hbox2 = g_object_new (GTK_TYPE_BOX,
                        "margin-top", 3,
                        "margin-bottom", 3,
                        "margin-left", 6,
                        "margin-right", 6,
                        "orientation", GTK_ORIENTATION_HORIZONTAL,
                        "expand", TRUE,
                        "visible", TRUE,
                        NULL);
  gtk_container_add (GTK_CONTAINER (hbox), GTK_WIDGET (hbox2));

  back = g_object_new (GTK_TYPE_BUTTON,
                       "child", g_object_new (GTK_TYPE_IMAGE,
                                              "icon-name", "go-previous-symbolic",
                                              "visible", TRUE,
                                              NULL),
                       "visible", TRUE,
                       NULL);
  ADD_CLASS (back, "image-button");
  ADD_CLASS (back, "flat");
  gtk_box_pack_start (hbox2, GTK_WIDGET (back), FALSE, FALSE, 0);

  forward = g_object_new (GTK_TYPE_BUTTON,
                          "child", g_object_new (GTK_TYPE_IMAGE,
                                                 "icon-name", "go-next-symbolic",
                                                 "visible", TRUE,
                                                 NULL),
                          "visible", TRUE,
                          NULL);
  ADD_CLASS (forward, "image-button");
  ADD_CLASS (forward, "flat");
  gtk_box_pack_start (hbox2, GTK_WIDGET (forward), FALSE, FALSE, 0);

  sep = g_object_new (GTK_TYPE_SEPARATOR,
                      "margin-top", 3,
                      "margin-bottom", 3,
                      "orientation", GTK_ORIENTATION_VERTICAL,
                      "visible", TRUE,
                      NULL);
  gtk_box_pack_start (hbox2, GTK_WIDGET (sep), FALSE, FALSE, 0);

  /* document name */
  docname = g_object_new (GTK_TYPE_MENU_BUTTON,
                          "label", "my-document.c",
                          "hexpand", TRUE,
                          "visible", TRUE,
                          NULL);
  ADD_CLASS (docname, "text-button");
  ADD_CLASS (docname, "flat");
  gtk_box_set_center_widget (hbox2, GTK_WIDGET (docname));

  closebtn = g_object_new (GTK_TYPE_BUTTON,
                           "child", g_object_new (GTK_TYPE_IMAGE,
                                                  "visible", TRUE,
                                                  "icon-name", "window-close-symbolic",
                                                  NULL),
                           "visible", TRUE,
                           NULL);
  ADD_CLASS (closebtn, "image-button");
  ADD_CLASS (closebtn, "flat");
  gtk_box_pack_end (hbox2, GTK_WIDGET (closebtn), FALSE, FALSE, 0);

  sep = g_object_new (GTK_TYPE_SEPARATOR,
                      "margin-top", 3,
                      "margin-bottom", 3,
                      "orientation", GTK_ORIENTATION_VERTICAL,
                      "visible", TRUE,
                      NULL);
  gtk_box_pack_end (hbox2, GTK_WIDGET (sep), FALSE, FALSE, 0);

  /* language button */
  langbtn = g_object_new (GTK_TYPE_MENU_BUTTON,
                          "label", "C",
                          "hexpand", FALSE,
                          "visible", TRUE,
                          NULL);
  ADD_CLASS (langbtn, "text-button");
  ADD_CLASS (langbtn, "flat");
  gtk_box_pack_end (hbox2, GTK_WIDGET (langbtn), FALSE, FALSE, 0);

  sep = g_object_new (GTK_TYPE_SEPARATOR,
                      "margin-top", 3,
                      "margin-bottom", 3,
                      "orientation", GTK_ORIENTATION_VERTICAL,
                      "visible", TRUE,
                      NULL);
  gtk_box_pack_end (hbox2, GTK_WIDGET (sep), FALSE, FALSE, 0);

  gDocStack = g_object_new (GTK_TYPE_STACK,
                            "expand", TRUE,
                            "visible", TRUE,
                            NULL);
  g_signal_connect (gDocStack, "notify::visible-child", G_CALLBACK (notify_visible_child_cb), NULL);
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (gDocStack));
}

static void
idedit__context_new_cb (GObject      *object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  GError *error = NULL;
  IdeBufferManager *bufmgr;
  g_autoptr(GPtrArray) bufs = NULL;
  GList *iter;
  gsize i;

  gContext = ide_context_new_finish (result, &error);

  if (!gContext)
    {
      g_printerr ("%s\n", error->message);
      g_clear_error (&error);
      quit (EXIT_FAILURE);
      return;
    }

  create_window ();

  /* now open all the requested buffers */
  gBufferToView = g_hash_table_new (g_direct_hash, g_direct_equal);

  bufmgr = ide_context_get_buffer_manager (gContext);
  g_signal_connect (bufmgr, "load-buffer", G_CALLBACK (load_buffer_cb), NULL);
  g_signal_connect (bufmgr, "buffer-loaded", G_CALLBACK (buffer_loaded_cb), NULL);

  bufs = ide_buffer_manager_get_buffers (bufmgr);
  for (i = 0; i < bufs->len; i++)
    add_buffer (g_ptr_array_index (bufs, i));

  for (iter = gFilesToOpen; iter; iter = iter->next)
    {
      IdeProject *project;
      IdeFile *file;

      project = ide_context_get_project (gContext);
      file = ide_project_get_file_for_path (project, iter->data);

      ide_buffer_manager_load_file_async (bufmgr, file, FALSE, NULL, NULL,
                                          idedit__bufmgr_load_file_cb, NULL);
    }

  gtk_window_present (gWindow);
}

static gboolean
increase_verbosity (void)
{
  ide_log_increase_verbosity ();
  return TRUE;
}

int
main (int argc,
      char *argv[])
{
  g_autoptr(GOptionContext) context = NULL;
  g_autoptr(GFile) project_dir = NULL;
  GtkCssProvider *provider;
  GError *error = NULL;
  gsize i;
  const GOptionEntry entries[] = {
    { "verbose", 'v', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
      increase_verbosity, N_("Increase logging verbosity.") },
    { NULL }
  };

  ide_log_init (TRUE, NULL);

  context = g_option_context_new (_("[FILES...] - A mini editor for libide"));
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
  g_option_context_add_group (context, gtk_get_option_group (TRUE));

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("%s\n", error->message);
      g_clear_error (&error);
      return EXIT_FAILURE;
    }

  if (argc < 2)
    {
      g_printerr (_("Please specify a file to edit.\n"));
      return EXIT_FAILURE;
    }

  for (i = 1; i < argc; i++)
    gFilesToOpen = g_list_append (gFilesToOpen, argv [i]);

  project_dir = g_file_new_for_path (".");

  ide_context_new_async (project_dir,
                         NULL,
                         idedit__context_new_cb,
                         NULL);

  provider = gtk_css_provider_new ();
  if (!gtk_css_provider_load_from_data (provider, gAppCss, -1, &error))
    {
      g_printerr ("%s\n", error->message);
      g_clear_error (&error);
    }
  gtk_style_context_add_provider_for_screen (gdk_screen_get_default(),
                                             GTK_STYLE_PROVIDER (provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  gtk_main ();

  return gExitCode;
}
