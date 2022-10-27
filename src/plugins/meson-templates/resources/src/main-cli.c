{{include "license.c"}}

#include "config.h"

#include <glib.h>
{{if enable_i18n}}
#include <glib/gi18n.h>
{{end}}
#include <stdlib.h>

gint
main (gint   argc,
      gchar *argv[])
{
	g_autoptr(GOptionContext) context = NULL;
	g_autoptr(GError) error = NULL;
	gboolean version = FALSE;
	GOptionEntry main_entries[] = {
{{if enable_i18n}}
		{ "version", 0, 0, G_OPTION_ARG_NONE, &version, N_("Show program version") },
{{else}}
		{ "version", 0, 0, G_OPTION_ARG_NONE, &version, "Show program version" },
{{end}}
		{ NULL }
	};

{{if enable_i18n}}
	context = g_option_context_new (_("- my command line tool"));
	g_option_context_add_main_entries (context, main_entries, GETTEXT_PACKAGE);
{{else}}
	context = g_option_context_new ("- my command line tool");
	g_option_context_add_main_entries (context, main_entries, NULL);
{{end}}

	if (!g_option_context_parse (context, &argc, &argv, &error))
	{
		g_printerr ("%s\n", error->message);
		return EXIT_FAILURE;
	}

	if (version)
	{
		g_printerr ("%s\n", PACKAGE_VERSION);
		return EXIT_SUCCESS;
	}

	return EXIT_SUCCESS;
}
