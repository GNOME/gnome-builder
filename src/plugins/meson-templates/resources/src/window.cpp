{{include "license.cpp"}}

#include "{{prefix}}-window.h"

{{Prefix}}Window::{{Prefix}}Window()
	: Glib::ObjectBase("{{Prefix}}Window")
	, Gtk::Window()
	, headerbar(nullptr)
	, label(nullptr)
{
	builder = Gtk::Builder::create_from_resource("{{appid_path}}/{{ui_file}}");
	builder->get_widget("headerbar", headerbar);
	builder->get_widget("label", label);
	add(*label);
	label->show();
	set_titlebar(*headerbar);
	headerbar->show();
}
