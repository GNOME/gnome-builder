{{include "license.cpp"}}

#include "{{prefix}}-window.h"

{{Prefix}}Window::{{Prefix}}Window(BaseObjectType* cobject,
		const Glib::RefPtr<Gtk::Builder>& refBuilder)
	: Gtk::ApplicationWindow(cobject)
	, m_refBuilder(refBuilder)
	, m_pLabel(nullptr)
{
	// Just an example of getting widgets in derived ApplicationWindow constructor.
	// You have to declare pointer in header file of needed type and initialize a
	// member pointer to nullptr in constructor initializer list. But you have to
	// do it only for widgets that you want to access or change from code.
	//
	// Here we just tilt the label by 30 degrees.
	m_refBuilder->get_widget("label", m_pLabel);
	m_pLabel->set_angle(30);
}

std::unique_ptr<{{Prefix}}Window> {{Prefix}}Window::create()
{
	// Parse a resource file containing a GtkBuilder UI definition.
	auto builder = Gtk::Builder::create_from_resource("{{appid_path}}/{{ui_file}}");

	// Get ApplicationWindow that is specified in the UI file but
	// implemented in our code. So our ApplicationWindow is derived.
	{{Prefix}}Window* window = nullptr;
	builder->get_widget_derived("window", window);

	if (!window)
		throw std::runtime_error("No \"window\" object {{ui_file}}");

	return std::unique_ptr<{{Prefix}}Window>(window);
}
