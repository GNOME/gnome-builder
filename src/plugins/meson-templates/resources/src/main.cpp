{{include "license.cpp"}}

#include <glibmm/i18n.h>

#include "{{prefix}}-config.h"
#include "{{prefix}}-window.h"

void on_activate(Glib::RefPtr<Gtk::Application> app) {
    // Get the current window. If there is not one, we will create it.
    auto window = app->get_active_window();
    
    if (!window) {
        window = {{Prefix}}Window::create();
        app->add_window(*window);
    }
    
    // Ask the window manager/compositor to present the window to the user.
    window->present();
}

int main (int argc, char *argv[]) {
    // Create a new Gtk::Application. The application manages our main loop,
    // application windows, integration with the window manager/compositor, and
    // desktop features such as file opening and single-instance applications.
    auto app = Gtk::Application::create("{{appid}}", Gio::APPLICATION_FLAGS_NONE);
    
    // Set up gettext translations
    bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);
    
    // We connect to the activate signal to create a window when the application
    // has been lauched. Additionally, this signal notifies us when the user
    // tries to launch a "second instance" of the application. When they try
    // to do that, we'll just present any existing window.
    //
    // Bind the app object to be passed to the callback "on_activate"
    app->signal_activate().connect(sigc::bind(&on_activate, app));
    
    // Run the application. This function will block until the applicaiton
    // exits. Upon return, we have our exit code to return to the shell. (This
    // is the code you see when you do `echo $?` after running a command in a
    // terminal.
    return app->run(argc, argv);
}
