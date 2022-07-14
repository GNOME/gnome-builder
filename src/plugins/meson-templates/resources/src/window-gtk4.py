{{include "license.py"}}

{{if is_adwaita}}
from gi.repository import Adw
{{end}}
from gi.repository import Gtk

@Gtk.Template(resource_path='{{appid_path}}/{{ui_file}}')
class {{PreFix}}Window({{if is_adwaita}}Adw{{else}}Gtk{{end}}.ApplicationWindow):
    __gtype_name__ = '{{PreFix}}Window'

    label = Gtk.Template.Child()

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
