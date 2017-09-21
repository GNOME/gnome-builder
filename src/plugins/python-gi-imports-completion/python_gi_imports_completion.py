#!/usr/bin/env python3

#
# python_gi_imports_completion.py
#
# Copyright (C) 2015 Christian Hergert <chris@dronelabs.com>
# Copyright (C) 2015 Elad Alfassa <elad@fedoraproject.org>
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

gi.require_version('Gtk', '3.0')
gi.require_version('GtkSource', '3.0')
gi.require_version('GIRepository', '2.0')
gi.require_version('Ide', '1.0')

from gi.repository import GIRepository
from gi.repository import Gio
from gi.repository import GLib
from gi.repository import GObject
from gi.repository import Gtk
from gi.repository import GtkSource
from gi.repository import Ide

# 2 minutes
CACHE_EXPIRE_USEC = 2 * 60 * 1000 * 1000

_NamespaceIcon = Gio.ThemedIcon.new('lang-namespace-symbolic')

class CompletionProvider(Ide.Object,
                         GtkSource.CompletionProvider,
                         Ide.CompletionProvider):
    _libraries = None
    _libraries_expire_at = 0

    def do_get_name(self):
        return 'Python GObject Introspection Imports Provider'

    def do_get_icon(self):
        return None

    def do_populate(self, context):

        _, iter = context.get_iter()
        buffer = iter.get_buffer()

        copy = iter.copy()
        copy.set_line_offset(0)
        text = buffer.get_text(copy, iter, True)

        if not text.startswith('from gi.repository import'):
            context.add_proposals(self, [], True)
            return

        line = iter.get_line() + 1
        column = iter.get_line_offset()

        text = text.replace('from gi.repository import', '').strip().lower()

        proposals = []
        for library_name in self.get_libraries():
            if library_name.lower().startswith(text):
                proposal = CompletionProposal(self, context, library_name, text)
                proposals.append(proposal)
        context.add_proposals(self, proposals, True)

    def do_match(self, context):
        return True

    def do_get_info_widget(self, proposal):
        return None

    def do_update_info(self, proposal, info):
        pass

    def do_get_start_iter(self, context, proposal):
        _, iter = context.get_iter()
        return True, iter

    def do_activate_proposal(self, provider, proposal):
        return False, None

    def do_get_interactive_delay(self):
        return -1

    def do_get_priority(self):
        return 201

    def get_libraries(self):
        now = GLib.get_monotonic_time()
        if now < self._libraries_expire_at:
            return self._libraries

        self._libraries = []
        self._libraries_expire_at = now + CACHE_EXPIRE_USEC

        for directory in GIRepository.Repository.get_search_path():
            if os.path.exists(directory):
                for filename in os.listdir(directory):
                    name = filename.split('-')[0]
                    if name not in self._libraries:
                        self._libraries.append(name)

        self._libraries.sort()

        return self._libraries

class CompletionProposal(GObject.Object, GtkSource.CompletionProposal):
    def __init__(self, provider, context, completion, start_text, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.provider = provider
        self.context = context
        self.completion = completion
        self.complete = completion[len(start_text):]

    def do_get_label(self):
        return self.completion

    def do_get_markup(self):
        return self.completion

    def do_get_text(self):
        return self.complete

    def do_get_gicon(self):
        return _NamespaceIcon

    def do_hash(self):
        return hash(self.completion)

    def do_equal(self, other):
        return False

    def do_changed(self):
        pass
