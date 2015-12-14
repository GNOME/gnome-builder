#!/usr/bin/env python3

#
# fpaste_plugin.py
#
# Copyright (C) 2015 Christian Hergert <chris@dronelabs.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

from gettext import gettext as _

from gi.repository import Gio
from gi.repository import GLib
from gi.repository import GObject
from gi.repository import Gdk
from gi.repository import Gtk
from gi.repository import Ide

import json
import threading
import urllib.parse
import urllib.request

USER_AGENT = 'gnome-builder-fpaste/0.1'
BASE_URL = 'https://paste.fedoraproject.org'

class FpasteEditorViewAddin(GObject.Object, Ide.EditorViewAddin):
    def do_load(self, view):
        self.view = view
        action = Gio.SimpleAction(name='send-to-fpaste')
        action.connect('activate', self.send_to_faste)
        self.view.get_action_group('view').add_action(action)

    def do_unload(self, view):
        self.view.get_action_group('view').remove_action('send-to-fpaste')
        self.view = None

    def get_text(self):
        buf = self.view.get_document()
        if buf.get_has_selection():
            # XXX: .get_selection_bounds() looks broken
            begin = buf.get_iter_at_mark(buf.get_insert())
            end = buf.get_iter_at_mark(buf.get_selection_bound())
        else:
            begin = buf.get_start_iter()
            end = buf.get_end_iter()
        return begin.get_slice(end)

    def send_to_faste(self, action, param):
        text = self.get_text()
        self._send_to_fpaste(text)

    def _send_to_fpaste(self, text, options=None):
        if options is None:
            options = {}

        text = self.get_text()

        params = urllib.parse.urlencode({
            'paste_lang': options.get('language', ''),
            'paste_data': text,
            'api_submit': True,
            'mode': 'json'
        })

        req = urllib.request.Request(BASE_URL,
                                     data=params.encode(),
                                     headers={'User-Agent': USER_AGENT})

        Uploader(req).start()

class Uploader(threading.Thread):
    def __init__(self, request):
        super().__init__()
        self.request = request

    def run(self):
        f = urllib.request.urlopen(self.request)
        GLib.timeout_add(0, self.complete, f)
        print("Got "+repr(f))

    def complete(self, stream):
        try:
            response = json.loads(stream.read().decode())
            result = response.get('result', {})
            message = str(result)
            dialog = Gtk.MessageDialog(buttons=Gtk.ButtonsType.CLOSE, text=message)
            if 'id' in result:
                uri = BASE_URL + '/' + str(result['id'] + '/')
                if result.get('hash', None):
                    uri += str(result['hash']) + '/'
                dialog.props.text = _("The following URL has been copied to the clipboard")
                label = Gtk.LinkButton(visible=True, label=uri, uri=uri, margin=12)
                Gtk.Clipboard.get(Gdk.SELECTION_CLIPBOARD).set_text(uri, len(uri))
                dialog.get_message_area().add(label)
            dialog.run()
            dialog.destroy()
        except Exception as ex:
            print(ex)
        finally:
            return False
