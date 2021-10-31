{{include "license.py"}}

import sys
import gi

gi.require_version('Gtk', '4.0')

from gi.repository import Gtk, Gio

from .window import {{PreFix}}Window, AboutDialog


class Application(Gtk.Application):
    def __init__(self):
        super().__init__(application_id='{{appid}}',
                         flags=Gio.ApplicationFlags.FLAGS_NONE)

    def do_activate(self):
        win = self.props.active_window
        if not win:
            win = {{PreFix}}Window(application=self)
        self.create_action('about', self.on_about_action)
        self.create_action('preferences', self.on_preferences_action)
        win.present()

    def on_about_action(self, widget, _):
        about = AboutDialog(self.props.active_window)
        about.present()

    def on_preferences_action(self, widget, _):
        print('app.preferences action activated')

    def create_action(self, name, callback):
        """ Add an Action and connect to a callback """
        action = Gio.SimpleAction.new(name, None)
        action.connect("activate", callback)
        self.add_action(action)


def main(version):
    app = Application()
    return app.run(sys.argv)
