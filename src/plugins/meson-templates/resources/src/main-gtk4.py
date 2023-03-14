{{include "license.py"}}

import sys
import gi

gi.require_version('Gtk', '4.0')
{{if is_adwaita}}
gi.require_version('Adw', '1')
{{end}}

from gi.repository import Gtk, Gio{{if is_adwaita}}, Adw{{end}}

from .window import {{PreFix}}Window


class {{PreFix}}Application({{if is_adwaita}}Adw{{else}}Gtk{{end}}.Application):
    """The main application singleton class."""

    def __init__(self):
        super().__init__(application_id='{{appid}}',
                         flags=Gio.ApplicationFlags.DEFAULT_FLAGS)
        self.create_action('quit', lambda *_: self.quit(), ['<primary>q'])
        self.create_action('about', self.on_about_action)
        self.create_action('preferences', self.on_preferences_action)

    def do_activate(self):
        """Called when the application is activated.

        We raise the application's main window, creating it if
        necessary.
        """
        win = self.props.active_window
        if not win:
            win = {{PreFix}}Window(application=self)
        win.present()

    def on_about_action(self, widget, _):
        """Callback for the app.about action."""
{{if is_adwaita}}
        about = Adw.AboutWindow(transient_for=self.props.active_window,
                                application_name='{{name}}',
                                application_icon='{{appid}}',
                                developer_name='{{author}}',
                                version='{{project_version}}',
                                developers=['{{author}}'],
                                copyright='© {{year}} {{author}}')
{{else}}
        about = Gtk.AboutDialog(transient_for=self.props.active_window,
                                modal=True,
                                program_name='{{name}}',
                                logo_icon_name='{{appid}}',
                                version='{{project_version}}',
                                authors=['{{author}}'],
                                copyright='© {{year}} {{author}}')
{{end}}
        about.present()

    def on_preferences_action(self, widget, _):
        """Callback for the app.preferences action."""
        print('app.preferences action activated')

    def create_action(self, name, callback, shortcuts=None):
        """Add an application action.

        Args:
            name: the name of the action
            callback: the function to be called when the action is
              activated
            shortcuts: an optional list of accelerators
        """
        action = Gio.SimpleAction.new(name, None)
        action.connect("activate", callback)
        self.add_action(action)
        if shortcuts:
            self.set_accels_for_action(f"app.{name}", shortcuts)


def main(version):
    """The application's entry point."""
    app = {{PreFix}}Application()
    return app.run(sys.argv)
