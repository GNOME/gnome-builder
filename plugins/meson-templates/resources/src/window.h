{{include "license.h"}}

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define {{PREFIX}}_TYPE_WINDOW ({{prefix_}}_window_get_type())

G_DECLARE_FINAL_TYPE ({{PreFix}}Window, {{prefix_}}_window, {{PREFIX}}, WINDOW, GtkApplicationWindow)

G_END_DECLS
