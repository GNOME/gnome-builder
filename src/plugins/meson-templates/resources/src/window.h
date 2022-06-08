{{include "license.h"}}

#pragma once

{{if is_adwaita}}
#include <adwaita.h>
{{else}}
#include <gtk/gtk.h>
{{end}}

G_BEGIN_DECLS

#define {{PREFIX}}_TYPE_WINDOW ({{prefix_}}_window_get_type())

G_DECLARE_FINAL_TYPE ({{PreFix}}Window, {{prefix_}}_window, {{PREFIX}}, WINDOW, {{if is_adwaita}}Adw{{else}}Gtk{{end}}ApplicationWindow)

G_END_DECLS
