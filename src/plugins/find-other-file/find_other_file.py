#!/usr/bin/env python3

#
# find_other_file.py
#
# Copyright © 2017 Christian Hergert <chergert@redhat.com>
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

from gi.repository import Dazzle
from gi.repository import GLib
from gi.repository import GObject
from gi.repository import Gio
from gi.repository import Gdk
from gi.repository import Gtk
from gi.repository import Ide

_ATTRIBUTES = ",".join([
    Gio.FILE_ATTRIBUTE_STANDARD_NAME,
    Gio.FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
    Gio.FILE_ATTRIBUTE_STANDARD_SYMBOLIC_ICON,
])

class FindOtherFile(GObject.Object, Ide.WorkbenchAddin):
    context = None
    workbench = None

    def do_load(self, workbench):
        self.workbench = workbench
        self.context = workbench.get_context()

        action = Gio.SimpleAction.new('find-other-file', None)
        action.connect('activate', self.on_activate)
        self.workbench.add_action(action)

    def do_unload(self, workbench):
        self.workbench = None
        self.context = None

    def on_activate(self, *args):
        editor = self.workbench.get_perspective_by_name('editor')
        view = editor.get_active_view()
        if type(view) is not Ide.EditorView:
            return

        buffer = view.get_buffer()
        file = buffer.get_file().get_file()
        parent = file.get_parent()

        basename = file.get_basename()
        parent.enumerate_children_async(_ATTRIBUTES,
                                        Gio.FileQueryInfoFlags.NONE,
                                        GLib.PRIORITY_LOW,
                                        None,
                                        self.on_enumerator_loaded,
                                        basename)

    def on_enumerator_loaded(self, parent, result, basename):
        try:
            files = Gio.ListStore.new(Ide.SearchResult)

            enumerator = parent.enumerate_children_finish(result)
            info = enumerator.next_file(None)
            if '.' in basename:
                prefix = basename[:basename.rindex('.')+1]
            else:
                prefix = basename

            while info is not None:
                name = info.get_name()
                if name != basename and name.startswith(prefix):
                    content_type = info.get_attribute_string(Gio.FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE)
                    display_name = info.get_attribute_string(Gio.FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME)
                    icon = info.get_attribute_object(Gio.FILE_ATTRIBUTE_STANDARD_SYMBOLIC_ICON)
                    icon_name = icon.to_string() if icon else None
                    gfile = parent.get_child(name)
                    ifile = Ide.File.new(self.context, gfile)
                    result = OtherFileSearchResult(file=ifile, icon_name=icon_name, title=display_name)
                    files.append(result)

                info = enumerator.next_file(None)

            enumerator.close()

            if files.get_n_items() == 1:
                file = files.get_item(0).file.get_file()
                self.workbench.open_files_async([file], 'editor', 0, None, None)
            elif files:
                self.present_results(files, basename)

        except Exception as ex:
            Ide.warning(repr(ex))
            return

    def present_results(self, results, name):
        headerbar = self.workbench.get_headerbar()
        search = Dazzle.gtk_widget_find_child_typed(headerbar, Ide.SearchEntry)
        search.set_text('')
        search.set_model(results)
        search.grab_focus()
        search.emit('show-suggestions')


class OtherFileSearchResult(Ide.SearchResult):
    file = GObject.Property(type=Ide.File)

    def do_get_source_location(self):
        return Ide.SourceLocation.new(self.file, 0, 0, 0)
