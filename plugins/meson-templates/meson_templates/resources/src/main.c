{{include "license.c"}}

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

{{if enable_i18n}}#include <glib/gi18n.h>{{end}}
#include <glib.h>

int
main(int   argc,
     char *argv[])
{
{{if enable_i18n}}
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);
{{end}}

  return 0;
}
