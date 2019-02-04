{{include "license.cpp"}}

#include "{{prefix}}-window.h"

{{Prefix}}Window::{{Prefix}}Window(BaseObjectType *base,
                           const Glib::RefPtr<Gtk::Builder> &builder)
                           : Gtk::ApplicationWindow(base) {
    // Widgets we've declared should be pointed to the builder (something that
    // constructs an UI from the template / 'glade' file for your GTK application).
    builder->get_widget("label", label);
    
    // You may bind widgets' signal to the functions in this class by typing,
    // for example:
    // my_widget->signal_something().connect(sigc::mem_fun(*this,
    //                                       &{{Prefix}}Window::on_my_widget_something));
    //
    // You may also the signals to non-class member functions by typing, for
    // example:
    // my_widget->signal_something().connect(sigc::ptr_fun(&on_object_something));
}

{{Prefix}}Window *{{Prefix}}Window::create() {
    // Create a builder instance from our UI file.
    auto builder = Gtk::Builder::create_from_resource("{{appid_path}}/{{ui_file}}");

    // Derive the UI to the {{Prefix}}Window instance to be returned to make it
    // can be instanced. You could add additional parameter to 'get_widget_derived'
    // function call if your class provides a constructor with compatible parameter.
    {{Prefix}}Window *parent_instance;
    builder->get_widget_derived("{{PreFix}}Window", parent_instance);
    
    return parent_instance;
}
