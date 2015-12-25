#!/usr/bin/env python3

#
# todo.py
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
import gi

gi.require_version('Ide', '1.0')

from gi.repository import Ide
from gi.repository import Gio
from gi.repository import GLib
from gi.repository import GObject
from gi.repository import Gtk

from gettext import gettext as _
import re
import subprocess
import threading

LINE1 = re.compile('(.*):(\d+):(.*)')
LINE2 = re.compile('(.*)-(\d+)-(.*)')
KEYWORDS = ['FIXME:', 'XXX:', 'TODO:']

class TodoWorkbenchAddin(GObject.Object, Ide.WorkbenchAddin):
    workbench = None
    panel = None

    def do_load(self, workbench):
        self.workbench = workbench

        # Watch the buffer manager for file changes (to update)
        context = workbench.get_context()
        bufmgr = context.get_buffer_manager()
        bufmgr.connect('buffer-saved', self.on_buffer_saved)

        # Get the working directory of the project
        vcs = context.get_vcs()
        workdir = vcs.get_working_directory()

        # Create our panel to display results
        self.panel = TodoPanel(workdir, visible=True)
        editor = workbench.get_perspective_by_name('editor')
        pane = editor.get_bottom_pane()
        pane.add_page(self.panel, _("Todo"), None)

        # Mine the directory in a background thread
        self.mine(workdir)

    def do_unload(self, workbench):
        self.panel.destroy()
        self.panel = None

        self.workbench = None

    def on_buffer_saved(self, bufmgr, buf):
        # Get the underline GFile
        file = buf.get_file().get_file()

        # XXX: Clear existing items from this file

        # Mine the file for todo items
        self.mine(file)

    def post(self, items):
        context = self.workbench.get_context()
        vcs = context.get_vcs()

        for item in items:
            if vcs.is_ignored(item.props.file):
                continue
            self.panel.add_item(item)

    def mine(self, file):
        """
        Mine a file or directory.

        We use a simple grep command to do the work for us rather than
        trying to write anything too complex that would just approximate
        the same thing anyway.
        """
        args = ['grep', '-A', '5', '-I', '-H', '-n', '-r']
        for keyword in KEYWORDS:
            args.append('-e')
            args.append(keyword)
        args.append(file.get_path())
        p = subprocess.Popen(args, stdout=subprocess.PIPE)

        def communicate(proc):
            stdout, _ = proc.communicate()
            lines = stdout.decode('utf-8').splitlines()
            stdout = None

            items = []
            item = TodoItem()

            for line in lines:
                # Skip long lines, like from SVG files
                if not line.strip() or len(line) > 1024:
                    continue

                if line.startswith('--'):
                    if item.props.file:
                        items.append(item)
                    item = TodoItem()
                    continue

                # If there is no file, then we haven't reached the x:x: line
                regex = LINE1 if not item.props.file else LINE2
                try:
                    (filename, line, message) = regex.match(line).groups()
                except Exception as ex:
                    continue

                if not item.props.file:
                    item.props.file = Gio.File.new_for_path(filename)
                    item.props.line = int(line)

                # XXX: not efficient use of roundtrips to/from pygobject
                if item.props.message:
                    item.props.message += '\n' + message
                else:
                    item.props.message = message

            if item.props.file:
                items.append(item)

            GLib.timeout_add(0, lambda: self.post(items) and GLib.SOURCE_REMOVE)

        threading.Thread(target=communicate, args=[p], name='todo-thread').start()

class TodoItem(GObject.Object):
    message = GObject.Property(type=str)
    line = GObject.Property(type=int)
    file = GObject.Property(type=Gio.File)

    def __repr__(self):
        return u'<TodoItem(%s:%d)>' % (self.props.file.get_path(), self.props.line)

    @property
    def shortdesc(self):
        msg = self.props.message
        if '\n' in msg:
            return msg[:msg.index('\n')].strip()
        return msg.strip()

class TodoPanel(Gtk.Bin):
    def __init__(self, basedir, *args, **kwargs):
        super().__init__(*args, **kwargs)

        self.basedir = basedir
        self.model = Gtk.ListStore(TodoItem)

        scroller = Gtk.ScrolledWindow(visible=True)
        self.add(scroller)

        treeview = Gtk.TreeView(visible=True, model=self.model, has_tooltip=True)
        treeview.connect('query-tooltip', self.on_query_tooltip)
        treeview.connect('row-activated', self.on_row_activated)
        scroller.add(treeview)

        column1 = Gtk.TreeViewColumn(title="File")
        treeview.append_column(column1)

        cell = Gtk.CellRendererText(xalign=0.0)
        column1.pack_start(cell, True)
        column1.set_cell_data_func(cell, self._file_data_func)

        column2 = Gtk.TreeViewColumn(title="Message")
        treeview.append_column(column2)

        cell = Gtk.CellRendererText(xalign=0.0)
        column2.pack_start(cell, True)
        column2.set_cell_data_func(cell, self._message_data_func)

    def _file_data_func(self, column, cell, model, iter, data):
        item, = model.get(iter, 0)
        relpath = self.basedir.get_relative_path(item.props.file)
        if not relpath:
            relpath = item.props.file.get_path()
        cell.props.text = '%s:%u' % (relpath, item.props.line)

    def _message_data_func(self, column, cell, model, iter, data):
        item, = model.get(iter, 0)
        cell.props.text = item.shortdesc

    def add_item(self, item):
        iter = self.model.append()
        self.model.set_value(iter, 0, item)

    def on_query_tooltip(self, treeview, x, y, keyboard, tooltip):
        x, y = treeview.convert_widget_to_bin_window_coords(x, y)
        try:
            path, column, cell_x, cell_y = treeview.get_path_at_pos(x, y)
            iter = self.model.get_iter(path)
            item, = self.model.get(iter, 0)
            tooltip.set_markup('<tt>' + GLib.markup_escape_text(item.props.message) + '</tt>')
            return True
        except:
            return False

    def on_row_activated(self, treeview, path, column):
        iter = self.model.get_iter(path)
        item, = self.model.get(iter, 0)
        uri = Ide.Uri.new_from_file(item.props.file)
        uri.set_fragment('L%u' % item.props.line)

        workbench = self.get_ancestor(Ide.Workbench)
        workbench.open_uri_async(uri, 'editor', None, None, None)
