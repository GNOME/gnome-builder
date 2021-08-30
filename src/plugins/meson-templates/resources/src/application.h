{{include "license.h"}}

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define {{PREFIX}}_TYPE_APPLICATION ({{prefix_}}_application_get_type())

G_DECLARE_TYPE ({{PreFix}}Application, {{prefix_}}_application, {{PREFIX}}, APPLICATION, GtkApplication)

{{PreFix}}Application *{{prefix_}}_application_new (gchar *application_id,
{{Spaces}}{{spaces}}                               GApplicationFlags  flags);

G_END_DECLS
