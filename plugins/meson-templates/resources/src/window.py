{{include "license.py"}}

from gi.repository import Gtk
from .gi_composites import GtkTemplate

@GtkTemplate(ui='{{appid_path}}/{{ui_file}}')
class {{PreFix}}Window(Gtk.ApplicationWindow):
    __gtype_name__ = '{{PreFix}}Window'

    label = GtkTemplate.Child()

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.init_template()

