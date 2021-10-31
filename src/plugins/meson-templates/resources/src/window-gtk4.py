{{include "license.py"}}

from gi.repository import Gtk


@Gtk.Template(resource_path='{{appid_path}}/{{ui_file}}')
class {{PreFix}}Window(Gtk.ApplicationWindow):
    __gtype_name__ = '{{PreFix}}Window'

    label = Gtk.Template.Child()

    def __init__(self, **kwargs):
        super().__init__(**kwargs)


class AboutDialog(Gtk.AboutDialog):

    def __init__(self, parent):
        Gtk.AboutDialog.__init__(self)
        self.props.program_name = '{{name}}'
        self.props.version = "0.1.0"
        self.props.authors = ['{{author}}']
        self.props.license_type = Gtk.License.GPL_3_0
        self.props.copyright = '(C) 2021 {{author}}'
        self.props.website = 'https://www.gnome.org/'
        self.props.logo_icon_name = '{{appid}}'
        self.set_transient_for(parent)
