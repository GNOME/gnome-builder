{{include "license.cpp"}}

#pragma once

#include <gtkmm/builder.h>
#include <gtkmm/headerbar.h>
#include <gtkmm/label.h>
#include <gtkmm/applicationwindow.h>

class {{Prefix}}Window : public Gtk::ApplicationWindow
{
public:
	// Due to convention of using Gtk::Builder::get_widget_derived()
	// constructor of the class should look like this. You can read
	// more about it in the reference.
	{{Prefix}}Window(BaseObjectType* cobject,
			 const Glib::RefPtr<Gtk::Builder>& refBuilder);

	static std::unique_ptr<{{Prefix}}Window> create();
private:
	Glib::RefPtr<Gtk::Builder> m_refBuilder;
	Gtk::Label* m_pLabel;
};
