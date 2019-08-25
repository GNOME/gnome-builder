
pub struct Window {
    pub widget: gtk::ApplicationWindow,
}

impl ApplicationWindow {

    pub fn new() -> Self {
        let builder = gtk::Builder::new_from_resource("{{appid_path}}/{{ui_file}}").unwrap();
        let widget: gtk::ApplicationWindow = builder.get_object("window").expect("Failed to find the window object");

        Self {
            widget,
        }
    }

}

