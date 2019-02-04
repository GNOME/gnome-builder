{{include "license.cpp"}}

#pragma once

#include <gtkmm.h>
#include <glibmm/i18n.h>

class {{Prefix}}Window: public Gtk::ApplicationWindow {
public:
  // Constructors, are the functions for initializing this class instance. You
  // may create constructors with difference additional parameter and delegate
  // the constructor to another.
  {{Prefix}}Window(BaseObjectType*, const Glib::RefPtr<Gtk::Builder>&);
	static {{Prefix}}Window *create();

protected:
  // You may put widgets in public, protected, or private block. But it's
  // recommended to put them here.
  Gtk::Label *label;
  
private:
  // You may put functions connected with widgets' signal in any block depending
  // on your application needs. But it's recommended to put them here.
};
