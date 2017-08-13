{{include "license.c"}}

#include <gtk/gtk.h>
#include "{{prefix}}-config.h"
#include "{{prefix}}-window.h"

struct _{{PreFix}}Window
{
  GtkWindow     parent_instance;
  GtkHeaderBar *header_bar;
  GtkLabel     *label;
};

G_DEFINE_TYPE ({{PreFix}}Window, {{prefix_}}_window, GTK_TYPE_APPLICATION_WINDOW)

static void
{{prefix_}}_window_class_init ({{PreFix}}WindowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "{{appid_path}}/{{ui_file}}");
  gtk_widget_class_bind_template_child (widget_class, {{PreFix}}Window, label);
}

static void
{{prefix_}}_window_init ({{PreFix}}Window *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
