#!/usr/bin/env python3
#
# vala_pack_plugin.py
#
# Copyright 2020 Christian Hergert <christian@hergert.me>
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
from gi.repository import Gdk
from gi.repository import Ide

_ERROR_REGEX = (
    "(?<filename>[a-zA-Z0-9\\-\\.\\/_]+.vala):" +
    "(?<line>\\d+).(?<column>\\d+)-(?<line2>\\d+).(?<column2>\\d+): " +
    "(?<level>[\\w\\s]+): " +
    "(?<message>.*)"
)

class ValaPipelineAddin(Ide.Object, Ide.PipelineAddin):
    error_format = 0

    def do_load(self, pipeline):
        self.error_format = pipeline.add_error_format(_ERROR_REGEX, GLib.RegexCompileFlags.OPTIMIZE)

    def do_unload(self, pipeline):
        pipeline.remove_error_format(self.error_format)

def copy_indent(iter):
    begin = iter.copy()
    begin.set_line_offset(0)
    end = begin.copy()
    while not end.ends_line() and end.get_char().isspace() and end.forward_char():
        # Do nothing
        pass
    return begin.get_slice(end)

def get_line_text(iter):
    begin = iter.copy()
    end = iter.copy()
    begin.set_line_offset(0)
    if not end.ends_line():
        end.forward_to_line_end()
    return begin.get_slice(end)

def is_newline_in_braces(iter):
    prev = iter.copy()
    next = iter.copy()
    prev.backward_char()
    next.forward_char()
    return prev.get_char() == '{' and iter.get_char() == '\n' and next.get_char() == '}'

def is_newline_keyval(keyval):
    return keyval in (Gdk.KEY_Return, Gdk.KEY_KP_Enter)

def in_comment(iter):
    buffer = iter.get_buffer()
    copy = iter.copy()
    copy.backward_char()
    return buffer.iter_has_context_class(copy, 'comment')

def indent_comment(iter):
    line = get_line_text(iter).strip()

    # Continue with another single line comment
    if line.startswith('//'):
        return copy_indent(iter) + '// '

    # Comment is closed, copy indent, possibly trimming extra space
    if line.endswith('*/'):
        if line.startswith('*'):
            str = copy_indent(iter)
            if str.endswith(' '):
                str = str[:-1]
            return str

    if line.startswith('/*') and not line.endswith('*/'):
        return copy_indent(iter) + ' * '
    elif line.startswith('*'):
        return copy_indent(iter) + '* '

    return copy_indent(iter)

class ValaIndenter(Ide.Object, Ide.Indenter):

    def do_is_trigger(self, key):
        return key.keyval in (Gdk.KEY_Return, Gdk.KEY_KP_Enter, Gdk.KEY_slash)

    def do_format(self, text_view, begin, end, key):
        was_newline = is_newline_keyval(key.keyval)
        copy = end.copy()

        # Move us back to the jsut inserted character
        copy.backward_char()

        # If we are in a comment, continue the indentation
        if in_comment(copy):
            # Maybe close a multi-line comment
            if copy.get_char() == '/':
                close = copy.copy()

                if close.backward_char() and close.get_char() == ' ' and close.backward_char() and close.get_char() == '*' :
                    begin.backward_char()
                    begin.backward_char()
                    return ('/', 0)

            if was_newline:
                return (indent_comment(copy), 0)

        if is_newline_in_braces(copy):
            prefix = copy_indent(copy)

            if text_view.get_insert_spaces_instead_of_tabs():
                indent = "    "
            else:
                indent = '\t'

            return (prefix + indent + "\n" + prefix, -(len(prefix) + 1))

        if was_newline:
            return (copy_indent(copy), 0)

        return (None, 0)

