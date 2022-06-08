use glib::clone;
use gtk::prelude::*;
use gtk::subclass::prelude::*;
use gtk::{gio, glib};
{{if is_adwaita}}
use adw::subclass::prelude::*;
{{end}}

use crate::config::VERSION;
use crate::{{PreFix}}Window;

mod imp {
    use super::*;

    #[derive(Debug, Default)]
    pub struct {{PreFix}}Application {}

    #[glib::object_subclass]
    impl ObjectSubclass for {{PreFix}}Application {
        const NAME: &'static str = "{{PreFix}}Application";
        type Type = super::{{PreFix}}Application;
        type ParentType = {{if is_adwaita}}adw::Application{{else}}gtk::Application{{end}};
    }

    impl ObjectImpl for {{PreFix}}Application {
        fn constructed(&self, obj: &Self::Type) {
            self.parent_constructed(obj);

            obj.setup_gactions();
            obj.set_accels_for_action("app.quit", &["<primary>q"]);
        }
    }

    impl ApplicationImpl for {{PreFix}}Application {
        // We connect to the activate callback to create a window when the application
        // has been launched. Additionally, this callback notifies us when the user
        // tries to launch a "second instance" of the application. When they try
        // to do that, we'll just present any existing window.
        fn activate(&self, application: &Self::Type) {
            // Get the current window or create one if necessary
            let window = if let Some(window) = application.active_window() {
                window
            } else {
                let window = {{PreFix}}Window::new(application);
                window.upcast()
            };

            // Ask the window manager/compositor to present the window
            window.present();
        }
    }

    impl GtkApplicationImpl for {{PreFix}}Application {}
    {{if is_adwaita}}
impl AdwApplicationImpl for {{PreFix}}Application {}
{{end}}
}

glib::wrapper! {
    pub struct {{PreFix}}Application(ObjectSubclass<imp::{{PreFix}}Application>)
        @extends gio::Application, gtk::Application, {{if is_adwaita}}adw::Application,{{end}}

        @implements gio::ActionGroup, gio::ActionMap;
}

impl {{PreFix}}Application {
    pub fn new(application_id: &str, flags: &gio::ApplicationFlags) -> Self {
        glib::Object::new(&[("application-id", &application_id), ("flags", flags)])
            .expect("Failed to create {{PreFix}}Application")
    }

    fn setup_gactions(&self) {
        let quit_action = gio::SimpleAction::new("quit", None);
        quit_action.connect_activate(clone!(@weak self as app => move |_, _| {
            app.quit();
        }));
        self.add_action(&quit_action);

        let about_action = gio::SimpleAction::new("about", None);
        about_action.connect_activate(clone!(@weak self as app => move |_, _| {
            app.show_about();
        }));
        self.add_action(&about_action);
    }

    fn show_about(&self) {
        let window = self.active_window().unwrap();
        let dialog = gtk::AboutDialog::builder()
            .transient_for(&window)
            .modal(true)
            .program_name("{{name}}")
            .version(VERSION)
            .authors(vec!["{{author}}".into()])
            .build();

        dialog.present();
    }
}
