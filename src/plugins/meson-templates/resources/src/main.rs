use gtk::prelude::*;
use gio::prelude::*;
use gettextrs::*;
use std::env;

mod config;
mod window;
use crate::window::Window;

fn main() {
    gtk::init().unwrap_or_else(|_| panic!("Failed to initialize GTK."));

    setlocale(LocaleCategory::LcAll, "");
    bindtextdomain("{{name}}", config::LOCALEDIR);
    textdomain("{{name}}");

    let res = gio::Resource::load(config::PKGDATADIR.to_owned() + "/{{name}}.gresource")
                                .expect("Could not load resources");
    gio::resources_register(&res);

    let app = gtk::Application::new(Some("{{appid}}"), Default::default()).unwrap();
    app.connect_activate(move |app| {
        let window = Window::new();

        window.widget.set_application(Some(app));
        app.add_window(&window.widget);
        window.widget.present();
    });


    let args: Vec<String> = env::args().collect();
    app.run(&args);

}

