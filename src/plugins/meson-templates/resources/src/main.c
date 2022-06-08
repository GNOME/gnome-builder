{{include "license.c"}}

#include "config.h"

#include <glib/gi18n.h>

#include "{{prefix}}-window.h"

static void
on_activate (GtkApplication *app)
{
	GtkWindow *window;

	g_assert (GTK_IS_APPLICATION (app));

	window = gtk_application_get_active_window (app);
	if (window == NULL)
		window = g_object_new ({{PREFIX}}_TYPE_WINDOW,
		                       "application", app,
		                       NULL);

	gtk_window_present (window);
}

int
main (int   argc,
      char *argv[])
{
	g_autoptr(GtkApplication) app = NULL;
	int ret;

	/* Set up gettext translations */
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	app = gtk_application_new ("{{appid}}", G_APPLICATION_FLAGS_NONE);
	g_signal_connect (app, "activate", G_CALLBACK (on_activate), NULL);
	ret = g_application_run (G_APPLICATION (app), argc, argv);

	return ret;
}
