{{include "license.cpp"}}

#include <gtkmm.h>

int main(int   argc,
         char *argv[])
{
    auto app =
      Gtk::Application::create(argc, argv,
                               "org.gnome.{{PreFix}}");

    Gtk::Window window;
    window.set_title ("{{name}}");
    window.set_default_size(200, 200);

    /* You can add GTK+ widgets to your window here.
     * See https://developer.gnome.org/ for help.
     */

    return app->run(window);
}
