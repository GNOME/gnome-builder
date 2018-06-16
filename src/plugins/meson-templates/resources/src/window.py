{{include "license.py"}}

from gi.repository import Gtk


@Gtk.Template(resource_path='{{appid_path}}/{{ui_file}}')
class {{PreFix}}Window(Gtk.ApplicationWindow):
    __gtype_name__ = '{{PreFix}}Window'

    label = Gtk.Template.Child()

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
