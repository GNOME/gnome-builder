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

from gi.repository import GIRepository
from gi.repository import GLib
from gi.repository import GObject
from gi.repository import Gtk
from gi.repository import GtkSource
from gi.repository import Ide

import os


class CompletionProvider(Ide.Object,
                         GtkSource.CompletionProvider,
                         Ide.CompletionProvider):
    def do_get_name(self):
        return 'Python GObject Introspection Imports Provider'

    def do_get_icon(self):
        return None

    def do_populate(self, context):

        _, iter = context.get_iter()
        buffer = iter.get_buffer()

        # ignore completions if we are following whitespace.
        copy = iter.copy()
        copy.set_line_offset(0)
        text = buffer.get_text(copy, iter, True)
        if not text or text[-1].isspace():
            context.add_proposals(self, [], True)
            return
        if not text.startswith('from gi.repository import'):
            context.add_proposals(self, [], True)
            return

        line = iter.get_line() + 1
        column = iter.get_line_offset()

        text = text.replace('from gi.repository import', '').strip().lower()

        # TODO: Cache directory contents? watch for changes?
        proposals = []
        for directory in GIRepository.Repository.get_search_path():
            for filename in os.listdir(directory):
                library_name = filename.split('-')[0]
                if library_name.lower().startswith(text):
                    proposals.append(CompletionProposal(self, context,
                                                        library_name, text))
        context.add_proposals(self, proposals, True)

    def do_get_activiation(self):
        return GtkSource.CompletionActivation.INTERACTIVE

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

    def do_get_icon(self):
        return load_icon(self.context, 'lang-include-symbolic')

    def do_hash(self):
        return hash(self.completion)

    def do_equal(self, other):
        return False

    def do_changed(self):
        pass

_icon_cache = {}


def purge_cache():
    _icon_cache.clear()

settings = Gtk.Settings.get_default()
settings.connect('notify::gtk-theme-name', lambda *_: purge_cache())
settings.connect('notify::gtk-application-prefer-dark-theme', lambda *_: purge_cache())


def load_icon(context, name):
    if name in _icon_cache:
        return _icon_cache[name]

    window = context.props.completion.get_info_window()
    size = 16
    style_context = window.get_style_context()
    icon_theme = Gtk.IconTheme.get_default()
    icon_info = icon_theme.lookup_icon(name, size, 0)
    if not icon_info:
        icon = None
    else:
        icon = icon_info.load_symbolic_for_context(style_context)

    _icon_cache[name] = icon

    return icon
