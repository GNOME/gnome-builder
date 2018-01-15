{{include "license.cpp"}}

#pragma once

#include <gtkmm/builder.h>
#include <gtkmm/headerbar.h>
#include <gtkmm/label.h>
#include <gtkmm/window.h>

class {{Prefix}}Window : public Gtk::Window
{
public:
	{{Prefix}}Window();

private:
	Gtk::HeaderBar *headerbar;
	Gtk::Label *label;
	Glib::RefPtr<Gtk::Builder> builder;
};
