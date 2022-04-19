#!/usr/bin/env python3

#
# python_gi_imports_completion.py
#
# Copyright 2015 Christian Hergert <chris@dronelabs.com>
# Copyright 2015 Elad Alfassa <elad@fedoraproject.org>
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
import os

from gi.repository import GIRepository
from gi.repository import Gio
from gi.repository import GLib
from gi.repository import GObject
from gi.repository import Gtk
from gi.repository import GtkSource
from gi.repository import Ide

# 2 minutes
CACHE_EXPIRE_USEC = 2 * 60 * 1000 * 1000

class CompletionProvider(Ide.Object, GtkSource.CompletionProvider):
    _libraries = None
    _libraries_expire_at = 0

    def do_get_title(self):
        return 'Python G-I Imports'

    def do_populate_async(self, context, cancellable, callback, data):
        task = Ide.Task.new(self, cancellable, callback)
        task.set_name('python gi imports')

        _, begin, end = context.get_bounds()
        begin.set_line_offset(0)
        text = begin.get_slice(end)

        if not text.startswith('from gi.repository import'):
            task.return_error(Ide.NotSupportedError())
            return

        text = text.replace('from gi.repository import', '').strip().lower()

        proposals = Gio.ListStore.new(GtkSource.CompletionProposal)
        for library in self.get_libraries():
            if library.matches(text):
                proposals.append(library)

        task.return_object(proposals)

    def do_populate_finish(self, task):
        return task.propagate_object()

    def do_display(self, context, proposal, cell):
        column = cell.get_column()

        if column == GtkSource.CompletionColumn.ICON:
            cell.set_icon_name('lang-namespace-symbolic')
        elif column == GtkSource.CompletionColumn.TYPED_TEXT:
            cell.set_text(proposal.completion)
        else:
            cell.set_text(None)

    def do_activate(self, context, proposal):
        _, begin, end = context.get_bounds()
        buffer = context.get_buffer()

        buffer.begin_user_action()
        buffer.delete(begin, end)
        buffer.insert(begin, proposal.completion, -1)
        buffer.end_user_action()

    def do_get_priority(self, context):
        # This provider only activates when it is very likely that we
        # want the results. So use high priority (negative is better).
        return -1000

    def do_refilter(self, context, proposals):
        text = context.get_word().lower()
        proposals.remove_all()
        for library in self.get_libraries():
            if library.matches(text):
                proposals.append(library)

    def get_libraries(self):
        now = GLib.get_monotonic_time()
        if now < self._libraries_expire_at:
            return self._libraries

        self._libraries = []
        self._libraries_expire_at = now + CACHE_EXPIRE_USEC
        found = {}

        for directory in GIRepository.Repository.get_search_path():
            if os.path.exists(directory):
                for filename in os.listdir(directory):
                    name = filename.split('-')[0]
                    if name not in found:
                        self._libraries.append(CompletionProposal(name))
                        found[name] = None

        self._libraries.sort(key=lambda x: x.completion)

        return self._libraries

class CompletionProposal(GObject.Object, GtkSource.CompletionProposal):
    def __init__(self, completion, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.completion = completion
        self.lower = completion.lower()

    def matches(self, text):
        return self.lower.startswith(text)
