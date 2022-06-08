{{include "license.rs"}}

{{if is_adwaita}}
use adw::subclass::prelude::*;
{{end}}
use gtk::prelude::*;
use gtk::subclass::prelude::*;
use gtk::{gio, glib, CompositeTemplate};

mod imp {
    use super::*;

    #[derive(Debug, Default, CompositeTemplate)]
    #[template(resource = "{{appid_path}}/{{ui_file}}")]
    pub struct {{PreFix}}Window {
        // Template widgets
        #[template_child]
        pub header_bar: TemplateChild<gtk::HeaderBar>,
        #[template_child]
        pub label: TemplateChild<gtk::Label>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for {{PreFix}}Window {
        const NAME: &'static str = "{{PreFix}}Window";
        type Type = super::{{PreFix}}Window;
        type ParentType = {{if is_adwaita}}adw{{else}}gtk{{end}}::ApplicationWindow;

        fn class_init(klass: &mut Self::Class) {
            Self::bind_template(klass);
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
    impl AdwApplicationWindowImpl for {{Prefix}}Window {}
{{end}}
}

glib::wrapper! {
    pub struct {{PreFix}}Window(ObjectSubclass<imp::{{PreFix}}Window>)
        @extends gtk::Widget, gtk::Window, gtk::ApplicationWindow,{{if is_adwaita}} adw::ApplicationWindow,{{end}}
        @implements gio::ActionGroup, gio::ActionMap;
}

impl {{PreFix}}Window {
    pub fn new<P: glib::IsA<gtk::Application>>(application: &P) -> Self {
        glib::Object::new(&[("application", application)])
            .expect("Failed to create {{PreFix}}Window")
    }
}
