{{include "license.rs"}}

use gettextrs::gettext;
{{if is_adwaita}}
use adw::prelude::*;
use adw::subclass::prelude::*;
{{else}}
use gtk::prelude::*;
use gtk::subclass::prelude::*;
{{end}}
use gtk::{gio, glib};

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
        fn constructed(&self) {
            self.parent_constructed();
            let obj = self.obj();
            obj.setup_gactions();
            obj.set_accels_for_action("app.quit", &["<primary>q"]);
        }
    }

    impl ApplicationImpl for {{PreFix}}Application {
        // We connect to the activate callback to create a window when the application
        // has been launched. Additionally, this callback notifies us when the user
        // tries to launch a "second instance" of the application. When they try
        // to do that, we'll just present any existing window.
        fn activate(&self) {
            let application = self.obj();
            // Get the current window or create one if necessary
            let window = application.active_window().unwrap_or_else(|| {
                let window = {{PreFix}}Window::new(&*application);
                window.upcast()
            });

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
        glib::Object::builder()
            .property("application-id", application_id)
            .property("flags", flags)
            .property("resource-base-path", "{{appid_path}}")
            .build()
    }

    fn setup_gactions(&self) {
        let quit_action = gio::ActionEntry::builder("quit")
            .activate(move |app: &Self, _, _| app.quit())
            .build();
        let about_action = gio::ActionEntry::builder("about")
            .activate(move |app: &Self, _, _| app.show_about())
            .build();
        self.add_action_entries([quit_action, about_action]);
    }

    fn show_about(&self) {
        let window = self.active_window().unwrap();
{{if is_adwaita}}
        let about = adw::AboutDialog::builder()
            .application_name("{{name}}")
            .application_icon("{{appid}}")
            .developer_name("{{author}}")
            .version(VERSION)
            .developers(vec!["{{author}}"])
            // Translators: Replace "translator-credits" with your name/username, and optionally an email or URL.
            .translator_credits(&gettext("translator-credits"))
            .copyright("© {{year}} {{author}}")
            .build();

        about.present(Some(&window));
{{else}}
        let about = gtk::AboutDialog::builder()
            .transient_for(&window)
            .modal(true)
            .program_name("{{name}}")
            .logo_icon_name("{{appid}}")
            .version(VERSION)
            .authors(vec!["{{author}}"])
            // Translators: Replace translator-credits with your name/username, and optionally an email or URL.
            .translator_credits(&gettext("translator-credits"))
            .copyright("© {{year}} {{author}}")
            .build();

        about.present();
{{end}}
    }
}
