{{include "license.cpp"}}

#pragma once

#include <gtkmm.h>
#include <glibmm/i18n.h>

class {{Prefix}}Window: public Gtk::ApplicationWindow {
public:
  // constructors
  {{Prefix}}Window(BaseObjectType*, const Glib::RefPtr<Gtk::Builder>&);
	static {{Prefix}}Window *create();

protected:
  // you can add some widgets here
  Gtk::Label *label;
  
private:
  // put your functions here.
  // you can connect widgets' signal by put the belowing in {{Prefix}}Window(...) constructor:
  // my_widget->signal_something().connect(sigc::mem_fun(*this, &{{Prefix}}Window::on_my_widget_something));
};
