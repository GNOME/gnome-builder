{{include "license.c"}}

#include "config.h"

#include <glib/gi18n.h>

#include "{{prefix}}-application.h"

int
main (int   argc,
      char *argv[])
{
	g_autoptr({{PreFix}}Application) app = NULL;
	int ret;

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	app = {{prefix_}}_application_new ("{{appid}}", G_APPLICATION_DEFAULT_FLAGS);
	ret = g_application_run (G_APPLICATION (app), argc, argv);

	return ret;
}
