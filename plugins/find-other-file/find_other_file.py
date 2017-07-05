#!/usr/bin/env python3

#
# find_other_file.py
#
# Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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
    workbench = None
    window = None
    model = None

    def do_load(self, workbench):
        self.workbench = workbench

        action = Gio.SimpleAction.new('find-other-file', None)
        action.connect('activate', self.on_activate)
        self.workbench.add_action(action)

        self.window = Gtk.Window(resizable=False,
                                 width_request=400,
                                 transient_for=self.workbench,
                                 title=_("Find other file"),
                                 window_position=Gtk.WindowPosition.CENTER_ON_PARENT)
        self.window.connect('key-press-event', self.on_key_press)
        self.window.connect('delete-event', self.on_delete_event)

        scroller = Gtk.ScrolledWindow(visible=True,
                                      propagate_natural_width=True,
                                      propagate_natural_height=True,
                                      hscrollbar_policy=Gtk.PolicyType.NEVER)
        self.window.add(scroller)

        self.model = Gtk.ListStore.new([OtherFile])
        treeview = Gtk.TreeView(visible=True, model=self.model, headers_visible=False)
        treeview.connect('row-activated', self.on_row_activated)
        scroller.add(treeview)

        column = Gtk.TreeViewColumn()
        treeview.append_column(column)

        cell = Gtk.CellRendererPixbuf(xpad=6, ypad=6)
        column.pack_start(cell, False)
        column.set_cell_data_func(cell, self.icon_cell_func)

        cell = Gtk.CellRendererText(ypad=6)
        column.pack_start(cell, True)
        column.set_cell_data_func(cell, self.text_cell_func)

    def do_unload(self, workbench):
        self.window.destroy()
        self.window = None

        self.model.clear()
        self.model = None

        self.workbench = None

    def on_activate(self, *args):
        editor = self.workbench.get_perspective_by_name('editor')
        view = editor.get_active_view()
        if type(view) is not Ide.EditorView:
            return

        buffer = view.get_buffer()
        file = buffer.get_file().get_file()
        parent = file.get_parent()

        basename = file.get_basename()
        if '.' in basename:
            basename = basename[:basename.rindex('.')+1]

        parent.enumerate_children_async(_ATTRIBUTES,
                                        Gio.FileQueryInfoFlags.NONE,
                                        GLib.PRIORITY_LOW,
                                        None,
                                        self.on_enumerator_loaded,
                                        basename)

    def on_enumerator_loaded(self, parent, result, prefix):
        try:
            files = []

            enumerator = parent.enumerate_children_finish(result)
            info = enumerator.next_file(None)

            while info is not None:
                name = info.get_name()

                if name.startswith(prefix):
                    content_type = info.get_attribute_string(Gio.FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE)
                    display_name = info.get_attribute_string(Gio.FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME)
                    icon = info.get_attribute_object(Gio.FILE_ATTRIBUTE_STANDARD_SYMBOLIC_ICON)

                    file = OtherFile(parent.get_child(name), display_name, icon)
                    files.append(file)

                info = enumerator.next_file(None)

            enumerator.close()

            if files:
                self.present_results(files)

        except Exception as ex:
            Ide.warning(repr(ex))
            return

    def on_delete_event(self, window, event):
        window.hide()
        return True

    def on_key_press(self, window, event):
        if event.keyval == Gdk.KEY_Escape:
            window.hide()
            self.workbench.present()
            self.workbench.grab_focus()
            return True
        return False

    def on_row_activated(self, treeview, path, column):
        model = treeview.get_model()
        iter = model.get_iter(path)
        file, = model.get(iter, 0)

        self.window.hide()

        self.workbench.open_files_async([file.file], 'editor', 0, None, None)

    def icon_cell_func(self, layout, cell, model, iter, data):
        file, = model.get(iter, 0)
        cell.props.gicon = file.icon

    def text_cell_func(self, layout, cell, model, iter, data):
        file, = model.get(iter, 0)
        cell.props.text = file.display_name

    def populate(self, results):
        self.model.clear()
        for row in results:
            iter = self.model.append()
            self.model.set(iter, {0: row})

    def present_results(self, results):
        self.populate(results)
        self.window.present()
        self.window.grab_focus()

class OtherFile(GObject.Object):
    icon = GObject.Property(type=Gio.Icon)
    display_name = GObject.Property(type=str)
    file = GObject.Property(type=Gio.File)

    def __init__(self, file, display_name, icon):
        super().__init__()
        self.file = file
        self.display_name = display_name
        self.icon = icon

