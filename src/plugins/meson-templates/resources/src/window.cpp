{{include "license.cpp"}}

#include "{{prefix}}-window.h"

{{Prefix}}Window::{{Prefix}}Window(BaseObjectType *base,
                           const Glib::RefPtr<Gtk::Builder> &builder)
                           : Gtk::ApplicationWindow(base) {
    builder->get_widget("label", label);
}

//static function we've declared in '...-window.h'
{{Prefix}}Window *{{Prefix}}Window::create() {
    auto builder = Gtk::Builder::create_from_resource("{{appid_path}}/{{ui_file}}");

    TrycppWindow *parent_instance;
    builder->get_widget_derived("{{Prefix}}Window", parent_instance);
    
    return parent_instance;
}
