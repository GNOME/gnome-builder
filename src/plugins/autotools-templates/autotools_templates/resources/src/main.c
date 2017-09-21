{{include "license.c"}}

#include <gtk/gtk.h>

static void
activate (GtkApplication *app,
          gpointer        user_data)
{
  GtkWidget *window;

  window = gtk_application_window_new (app);
  gtk_window_set_title (GTK_WINDOW (window), "{{name}}");
  gtk_window_set_default_size (GTK_WINDOW (window), 200, 200);

  /* You can add GTK+ widgets to your window here.
   * See https://developer.gnome.org/ for help.
   */

  gtk_widget_show (window);
}

int main(int   argc,
         char *argv[])
{
  g_autoptr(GtkApplication) app = NULL;
  int status;

  app = gtk_application_new ("org.gnome.{{PreFix}}", G_APPLICATION_FLAGS_NONE);
  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
  status = g_application_run (G_APPLICATION (app), argc, argv);

  return status;
}
