{{include "license.rs"}}

use gtk::prelude::*;
{{if is_adwaita}}
use adw::subclass::prelude::*;
{{else}}
use gtk::subclass::prelude::*;
{{end}}
use gtk::{gio, glib};

mod imp {
    use super::*;

    #[derive(Debug, Default, gtk::CompositeTemplate)]
    #[template(resource = "{{appid_path}}/{{ui_file}}")]
    pub struct {{PreFix}}Window {
        // Template widgets
        #[template_child]
        pub label: TemplateChild<gtk::Label>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for {{PreFix}}Window {
        const NAME: &'static str = "{{PreFix}}Window";
        type Type = super::{{PreFix}}Window;
        type ParentType = {{if is_adwaita}}adw{{else}}gtk{{end}}::ApplicationWindow;

        fn class_init(klass: &mut Self::Class) {
            klass.bind_template();
        }

        fn instance_init(obj: &glib::subclass::InitializingObject<Self>) {
            obj.init_template();
        }
    }

    impl ObjectImpl for {{PreFix}}Window {}
    impl WidgetImpl for {{PreFix}}Window {}
    impl WindowImpl for {{PreFix}}Window {}
    impl ApplicationWindowImpl for {{PreFix}}Window {}
{{if is_adwaita}}
    impl AdwApplicationWindowImpl for {{PreFix}}Window {}
{{end}}
}

glib::wrapper! {
    pub struct {{PreFix}}Window(ObjectSubclass<imp::{{PreFix}}Window>)
        @extends gtk::Widget, gtk::Window, gtk::ApplicationWindow,{{if is_adwaita}} adw::ApplicationWindow,{{end}}
        @implements gio::ActionGroup, gio::ActionMap;
}

impl {{PreFix}}Window {
    pub fn new<P: IsA<gtk::Application>>(application: &P) -> Self {
        glib::Object::builder()
            .property("application", application)
            .build()
    }
}
