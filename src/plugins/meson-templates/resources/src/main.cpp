{{include "license.cpp"}}

#include <glibmm/i18n.h>

#include "{{prefix}}-config.h"
#include "{{prefix}}-window.h"

void on_activate(Glib::RefPtr<Gtk::Application> app) {
    auto window = app->get_active_window();
    
    if (!window) {
        window = {{Prefix}}Window::create();
        app->add_window(*window);
    }
    window->present();
}

int main (int argc, char *argv[]) {
    auto app = Gtk::Application::create("{{appid}}", Gio::APPLICATION_FLAGS_NONE);
    
	// Set up gettext translations
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

    app->signal_activate().connect(sigc::bind(&on_activate, app));

    return app->run(argc, argv);
}
