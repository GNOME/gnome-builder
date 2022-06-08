{{include "license.h"}}

#pragma once

{{if is_adwaita}}
#include <adwaita.h>
{{else}}
#include <gtk/gtk.h>
{{end}}

G_BEGIN_DECLS

#define {{PREFIX}}_TYPE_APPLICATION ({{prefix_}}_application_get_type())

G_DECLARE_FINAL_TYPE ({{PreFix}}Application, {{prefix_}}_application, {{PREFIX}}, APPLICATION, {{if is_adwaita}}AdwApplication{{else}}GtkApplication{{end}})

{{PreFix}}Application *{{prefix_}}_application_new (const char        *application_id,
{{Spaces}}{{spaces}}                               GApplicationFlags  flags);

G_END_DECLS
