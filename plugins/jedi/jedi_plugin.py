#!/usr/bin/env python3

#
# jedi_plugin.py
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

from gi.importer import DynamicImporter
from gi.module import IntrospectionModule
from gi.repository import GLib
from gi.repository import GObject
from gi.repository import Gtk
from gi.repository import GtkSource
from gi.repository import Ide
gi_importer = DynamicImporter('gi.repository')

try:
    import jedi
    from jedi.evaluate.compiled import CompiledObject
    from jedi.evaluate.imports import Importer

    class PatchedJediCompiledObject(CompiledObject):
        "A modified version of Jedi CompiledObject to work with GObject Introspection modules"
        def _cls(self):
            if self.obj.__class__ == IntrospectionModule:
                return self
            else:
                return super()._cls()

    class PatchedJediImporter(Importer):
        "A modified version of Jedi Importer to work with GObject Introspection modules"
        def follow(self):
            module_list = super().follow()
            if module_list == []:
                import_path = '.'.join([str(i) for i in self.import_path])
                if import_path.startswith('gi.repository'):
                    try:
                        module = gi_importer.load_module(import_path)
                        module_list = [PatchedJediCompiledObject(module)]
                    except ImportError:
                        pass
            return module_list

    original_jedi_get_module = jedi.evaluate.compiled.fake.get_module

    def patched_jedi_get_module(obj):
        "Work around a weird bug in jedi"
        try:
            return original_jedi_get_module(obj)
        except ImportError as e:
            if e.msg == "No module named 'gi._gobject._gobject'":
                return original_jedi_get_module('gi._gobject')

    jedi.evaluate.compiled.fake.get_module = patched_jedi_get_module

    jedi.evaluate.imports.Importer = PatchedJediImporter
    HAS_JEDI = True
except ImportError:
    HAS_JEDI = False

# FIXME: Should we be using multiprocessing or something?
#        Alternatively, this can go in gnome-code-assistance
#        once we have an API that can transfer completions
#        relatively fast enough for interactivity.
import threading


class CompletionThread(threading.Thread):
    def __init__(self, provider, context, text, line, column, filename):
        super().__init__()
        self._provider = provider
        self._context = context
        self._text = text
        self._line = line
        self._column = column
        self._filename = filename
        self._completions = []

    def run(self):
        try:
            script = jedi.Script(self._text, self._line, self._column, self._filename)
            self._completions = [JediCompletionProposal(self._provider, self._context, info)
                                 for info in script.completions()]
        finally:
            self.complete_in_idle()

    def _complete(self):
        self._context.add_proposals(self._provider, self._completions, True)

    def complete_in_idle(self):
        GLib.timeout_add(0, self._complete)


class JediCompletionProvider(Ide.Object,
                             GtkSource.CompletionProvider,
                             Ide.CompletionProvider):
    def do_get_name(self):
        return 'Jedi Provider'

    def do_get_icon(self):
        return None

    def do_populate(self, context):
        if not HAS_JEDI:
            context.add_proposals(self, [], True)
            return

        _, iter = context.get_iter()
        buffer = iter.get_buffer()

        # ignore completions if we are following whitespace.
        copy = iter.copy()
        copy.set_line_offset(0)
        text = buffer.get_text(copy, iter, True)
        if not text or text[-1].isspace():
            context.add_proposals(self, [], True)
            return

        begin, end = buffer.get_bounds()

        filename = (iter.get_buffer()
                        .get_file()
                        .get_file()
                        .get_basename())

        text = buffer.get_text(begin, end, True)
        line = iter.get_line() + 1
        column = iter.get_line_offset()

        CompletionThread(self, context, text, line, column, filename).start()

    def do_get_activiation(self):
        return GtkSource.CompletionActivation.INTERACTIVE

    def do_match(self, context):
        if not HAS_JEDI:
            return False
        _, iter = context.get_iter()
        iter.backward_char()
        buffer = iter.get_buffer()
        classes = buffer.get_context_classes_at_iter(iter)
        if 'string' in classes:
            return False
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
        return 200


class JediCompletionProposal(GObject.Object, GtkSource.CompletionProposal):
    def __init__(self, provider, context, completion, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.provider = provider
        self.context = context
        self.completion = completion

    def do_get_label(self):
        return self.completion.name

    def do_get_markup(self):
        return self.completion.name

    def do_get_text(self):
        return self.completion.complete

    def do_get_icon(self):
        if self.completion.type == 'class':
            return load_icon(self.context, 'lang-class-symbolic')
        elif self.completion.type == 'instance':
            return load_icon(self.context, 'lang-variable-symbolic')
        elif self.completion.type in ('import', 'module'):
            # FIXME: Would be nice to do something better here.
            return load_icon(self.context, 'lang-include-symbolic')
        elif self.completion.type == 'function':
            return load_icon(self.context, 'lang-function-symbolic')
        elif self.completion.type == 'keyword':
            # FIXME: And here
            return None
        return None

    def do_hash(self):
        return hash(self.completion.full_name)

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
