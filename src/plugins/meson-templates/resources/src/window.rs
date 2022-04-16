use gtk::prelude::*;

pub struct Window {
    pub widget: gtk::ApplicationWindow,
}

impl Window {
    pub fn new() -> Self {
        let builder = gtk::Builder::from_resource("{{appid_path}}/{{ui_file}}");
        let widget: gtk::ApplicationWindow = builder
            .object("window")
            .expect("Failed to find the window object");

        Self { widget }
    }
}
