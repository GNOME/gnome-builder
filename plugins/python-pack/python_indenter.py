# python_indenter.py
#
# Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

from collections import namedtuple
import gi
import unittest

gi.require_version('Gdk', '3.0')
gi.require_version('GObject', '2.0')
gi.require_version('Gtk', '3.0')
gi.require_version('GtkSource', '3.0')

from gi.repository import Gdk
from gi.repository import GObject
from gi.repository import Gtk
from gi.repository import GtkSource

_BACKTRACK_LINES = 40

class Rank:
    FUNCTION = 1
    CLASS    = 1 << 1
    TUPLE    = 1 << 2
    LIST     = 1 << 3
    DICT     = 1 << 4
    IF       = 1 << 5
    ELIF     = 1 << 6
    ELSE     = 1 << 7
    STRING   = 1 << 8
    COMMENT  = 1 << 9
    PASS     = 1 << 10
    RETURN   = 1 << 11
    BREAK    = 1 << 12
    CONTINUE = 1 << 13

_RANK_NAMES = {
    Rank.FUNCTION: 'FUNCTION',
    Rank.CLASS: 'CLASS',
    Rank.TUPLE: 'TUPLE',
    Rank.LIST: 'LIST',
    Rank.DICT: 'DICT',
    Rank.IF: 'IF',
    Rank.ELIF: 'ELIF',
    Rank.ELSE: 'ELSE',
    Rank.STRING: 'STRING',
    Rank.COMMENT: 'COMMENT',
    Rank.PASS: 'PASS',
    Rank.RETURN: 'RETURN',
    Rank.BREAK: 'BREAK',
    Rank.CONTINUE: 'CONTINUE',
}

class Discovery:
    __slots__ = ['offset', 'rank', 'line', 'column', 'prune']

    def __init__(self, offset, rank, line, column):
        self.offset = offset
        self.rank = rank
        self.line = line
        self.column = column
        self.prune = False

    def __repr__(self):
        if self.rank in _RANK_NAMES:
            name = _RANK_NAMES[self.rank]
        else:
            name = self.rank
        return 'Discovery(offset=%u[%u:%u], rank=%s)' % (
            self.offset, self.line, self.column, name)

class Discoveries:
    discoveries = None
    buffer = None
    offset = 0
    stop = 0
    has_run = False

    def __init__(self, buffer, location):
        self.buffer = buffer
        self.offset = location.get_offset()
        self.discoveries = []

        pos = location.copy()
        pos.set_line(max(0, pos.get_line() - _BACKTRACK_LINES))
        pos.set_line_offset(0)
        self.stop = pos.get_offset()

    def select(self, *ranks):
        self._run()
        result = []
        ranks = list(ranks)
        count = 0
        for discovery in reverse(self.discoveries):
            if discovery.rank == ranks[count]:
                result.append(discovery)
                count += 1
                if len(ranks) == count:
                    break
        if len(ranks) == count:
            return result
        return None

    @property
    def nearest(self):
        self._run()
        if self.discoveries:
            return self.discoveries[0]
        return None

    def nearest_of(self, mask):
        """
        Find the nearest discovery that matches the mask.
        The mask should be a bitwise or of the ranks you
        care to find.

        Returns a Discovery instance or None.
        """
        self._run()
        for discovery in self.discoveries:
            if discovery.rank & mask != 0:
                return discovery
        return None

    @property
    def all_mask(self):
        self._run()
        flags = 0
        for discovery in self.discoveries:
            flags |= discovery.rank
        return flags

    @property
    def in_function_params(self):
        """
        Checks if the discoveries indicate that we might be in the parameter
        list for a function call.
        """
        mask = Rank.FUNCTION | Rank.TUPLE
        if self.all_mask & mask == mask:
            tup = self.nearest_of(Rank.TUPLE)
            func = self.nearest_of(Rank.FUNCTION)
            return tup.line == func.line
        return False

    @property
    def in_function_call_params(self):
        """
        Checks if the discoveries indicate that we might be in the parameter
        list for a function call.
        """
        nearest = self.nearest
        if nearest is None or nearest.rank != Rank.TUPLE:
            return False
        if self.in_function_params:
            return False
        # XXX: really we should check for some text before
        return True

    def _add(self, rank, location):
        d = Discovery(location.get_offset(), rank, location.get_line(),
                      location.get_line_offset())
        self.discoveries.append(d)

    def _run(self):
        if self.has_run:
            return

        self.has_run = True

        iter = self.buffer.get_iter_at_offset(self.offset)
        stop = self.buffer.get_iter_at_offset(self.stop)

        self._discover_break(iter, stop)
        self._discover_continue(iter, stop)
        self._discover_class(iter, stop)
        self._discover_comment(iter, stop)
        self._discover_dict(iter, stop)
        self._discover_elif(iter, stop)
        self._discover_else(iter, stop)
        self._discover_function(iter, stop)
        self._discover_if(iter, stop)
        self._discover_list(iter, stop)
        self._discover_pass(iter, stop)
        self._discover_return(iter, stop)
        self._discover_string(iter, stop)
        self._discover_tuple(iter, stop)
        self.discoveries.sort(key=lambda x: -x.offset)
        self._eliminate()

    def _discover_context_class(self, iter, stop, word, rank):
        iter = iter.copy()
        if not iter.starts_line():
            iter.backward_char()
        if self.buffer.iter_has_context_class(iter, word):
            self._add(rank, iter)

    def _discover_string(self, iter, stop, *, word='string', rank=Rank.STRING):
        self._discover_context_class(iter, stop, word, rank)

    def _discover_comment(self, iter, stop, *, word='comment', rank=Rank.COMMENT):
        self._discover_context_class(iter, stop, word, rank)

    def _discover_simple(self, iter, stop, word, rank):
        ret = iter.backward_search(word, Gtk.TextSearchFlags.TEXT_ONLY, stop)
        if ret is None:
            return
        begin, end = ret
        if self._is_special(begin):
            return
        if begin.starts_word():
            self._add(rank, begin)

    def _discover_function(self, iter, stop, *, word="def ", rank=Rank.FUNCTION):
        self._discover_simple(iter, stop, word, rank)

    def _discover_pass(self, iter, stop, *, word="pass", rank=Rank.PASS):
        self._discover_simple(iter, stop, word, rank)

    def _discover_break(self, iter, stop, *, word="break", rank=Rank.PASS):
        self._discover_simple(iter, stop, word, rank)

    def _discover_continue(self, iter, stop, *, word="continue", rank=Rank.PASS):
        self._discover_simple(iter, stop, word, rank)

    def _discover_return(self, iter, stop, *, word="return", rank=Rank.RETURN):
        self._discover_simple(iter, stop, word, rank)

    def _discover_class(self, iter, stop, *, word="class ", rank=Rank.CLASS):
        self._discover_simple(iter, stop, word, rank)

    def _discover_if(self, iter, stop, *, word="if ", rank=Rank.IF):
        self._discover_simple(iter, stop, word, rank)

    def _discover_elif(self, iter, stop, *, word="elif ", rank=Rank.ELIF):
        self._discover_simple(iter, stop, word, rank)

    def _discover_else(self, iter, stop, *, word="else:", rank=Rank.ELSE):
        self._discover_simple(iter, stop, word, rank)

    def _discover_tuple(self, iter, stop, *, char='(', opposite=')', rank=Rank.TUPLE):
        iter = iter.copy()
        if self._previous_unmatched(iter, char, opposite, stop):
            self._add(rank, iter)

    def _discover_list(self, iter, stop, *, char='[', opposite=']', rank=Rank.LIST):
        iter = iter.copy()
        if self._previous_unmatched(iter, char, opposite, stop):
            self._add(rank, iter)

    def _discover_dict(self, iter, stop, *, char='{', opposite='}', rank=Rank.DICT):
        iter = iter.copy()
        if self._previous_unmatched(iter, char, opposite, stop):
            self._add(rank, iter)

    def _is_special(self, iter):
        return (self.buffer.iter_has_context_class(iter, 'string') or
                self.buffer.iter_has_context_class(iter, 'comment'))

    def _line_starts_with(self, iter, word):
        begin = iter.copy()
        begin.set_line_offset(0)
        end = begin.copy()
        if not end.ends_line():
            end.forward_to_line_end()
        return begin.get_slice(end).strip().startswith(word.strip())

    def _previous_unmatched(self, iter, char, opposite, stop=None):
        if stop is None:
            stop = self.buffer.get_start_iter()
        count = 1
        while iter.compare(stop) > 0:
            iter.backward_char()
            ch = iter.get_char()
            if (ch == char) and not self._is_special(iter):
                count -= 1
                if count == 0:
                    return True
            if (ch == opposite) and not self._is_special(iter):
                count += 1
        return False

    def _mark_ranks_with_mask(self, mask):
        for discovery in self.discoveries:
            if discovery.rank & mask != 0:
                discovery.prune = True

    def _eliminate(self):
        """
        Walk through our rankings and see if we found anything that
        can be eliminated based on combinations.
        """
        self._eliminate_if_elif_else()
        self._eliminate_breakouts()

        survived = []
        for discovery in self.discoveries:
            if not discovery.prune:
                survived.append(discovery)
            else:
                #print("Pruning", discovery)
                pass
        self.discoveries = survived

    def _eliminate_if_elif_else(self):
        nearest = self.nearest_of(Rank.IF | Rank.ELIF | Rank.ELSE)
        if nearest is None:
            return

        # If we have any line after our nearest block that does
        # not match the indentation of this block, we can turn
        # off all the if/elif/else ranks.
        iter = self.buffer.get_iter_at_offset(nearest.offset)
        stop = self.buffer.get_iter_at_offset(self.offset)
        column = iter.get_line_offset()

        iter.set_line_offset(0)
        while iter.compare(stop) < 0:
            if not iter.forward_line():
                break
            forward_to_nonspace(iter)
            if iter.get_line_offset() < column:
                self._mark_ranks_with_mask(Rank.IF | Rank.ELIF | Rank.ELSE)
                return
            iter.set_line_offset(0)

    def _eliminate_breakouts(self):
        # The goal here is to eliminate the parent of anything
        # containing a return that would cause us to not need
        # to see it in our selection chain.
        discoveries = list(self.discoveries)
        for discovery in discoveries:
            if discovery.rank in (Rank.RETURN, Rank.PASS, Rank.BREAK, Rank.CONTINUE):
                self._cascade_parent(discovery)

    def _cascade_parent(self, node):
        parent = self._find_parent(node)
        if parent is not None:
            children = self._find_children(parent)
            for child in children:
                child.prune = True
            parent.prune = True

    def _find_parent(self, node):
        # very inefficient
        index = self.discoveries.index(node)
        for discovery in self.discoveries[index+1:]:
            if discovery.column < node.column:
                return discovery

    def _find_children(self, node):
        # very inefficient
        children = []
        index = self.discoveries.index(node)
        for child in reversed(self.discoveries[:index]):
            # Eeek, O(n^2)
            if node == self._find_parent(child):
                children.append(child)
        return children

class PythonSettings:
    indent_width = 4
    insert_spaces = True

class PythonIndenter(GObject.Object): #, Ide.Indenter):
    settings = None

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.settings = PythonSettings()

    def do_is_trigger(self, event):
        if event.keyval in (Gdk.KEY_Return, Gdk.KEY_KP_Enter):
            return True
        return False

    def do_format(self, view, begin, end, event):
        if event.keyval in (Gdk.KEY_Return, Gdk.KEY_KP_Enter):
            return self.format_enter(view, begin, end, event)
        if event.keyval in (Gdk.KEY_colon,):
            return self.format_colon(view, begin, end, event)
        return '', 0

    def copy_indent(self, view, iter, prefix='\n', suffix='', extra=0):
        begin = iter.copy()
        begin.set_line_offset(0)
        end = begin.copy()
        forward_to_nonspace(end)
        extra_str = (' ' * self.settings.indent_width) * extra
        text = prefix + begin.get_slice(end) + extra_str + suffix
        return text, 0

    def format_enter(self, view, begin, end, event):
        iter = begin.copy()

        # Discover our various rankings
        discoveries = Discoveries(view.get_buffer(), iter)
        if not discoveries.nearest:
            return self.copy_indent(view, iter)

        nearest = discoveries.nearest

        if nearest.rank == Rank.COMMENT:
            return self.copy_indent(view, iter, suffix='# ')

        iter = begin.copy()
        iter.set_line(nearest.line)
        return self.copy_indent(view, iter, extra=1)

    def format_colon(self, view, begin, end, event):
        return '', 0

def forward_to_nonspace(iter):
    """
    Moves forward but stays on the same line.
    Returns True if found, otherwise False.
    If False, iter will be at the line end.
    """
    while not iter.ends_line():
        if not iter.get_char().isspace():
            return True
        iter.forward_char()
    return False


class TestLocation(unittest.TestCase):
    TEST_DATA = """#!/usr/bin/env python

class MyClass(GObject.Object, Ide.Indenter):

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

    def test_func(self):
        foo = "hi there"
        # and a comment
        if foo:
            a = {'asdf': 1234}
        elif foo2.do_the_humpty_dance([l for l in items()]):
            z = {
                key: value,
                "foo": "bar,
            }
        else:
            return foo()

# basic comment

class Class2:
    def function(self):
        class InnerClass:
            def inner(self):
                pass
"""
    _buffer = None

    def get_buffer(self):
        if not self._buffer:
            text_buffer = GtkSource.Buffer()
            text_buffer.set_text(self.TEST_DATA)
            manager = GtkSource.LanguageManager.get_default()
            language = manager.get_language('python')
            text_buffer.set_language(language)
            # Give the text buffer a chance to scan things.
            while Gtk.events_pending():
                Gtk.main_iteration()
            self._buffer = text_buffer
        return self._buffer

    def get_iter(self, line, line_offset):
        buffer = self.get_buffer()
        iter = buffer.get_iter_at_line(line)
        while line_offset > 0 and not iter.ends_line():
            line_offset -= 1
            if not iter.forward_char():
                break
        return iter

    def assertRankings(self, discoveries, *ranks):
        if not discoveries.has_run:
            discoveries._run()
        self.assertEqual(len(discoveries.discoveries), len(ranks))
        i = 0
        for rank in reversed(ranks):
            self.assertEqual(discoveries.discoveries[i].rank, rank)
            i += 1

    def test_in_class_and_function(self):
        text_buffer = self.get_buffer()

        iter = self.get_iter(5, 7)
        discovery = Discoveries(text_buffer, iter).nearest
        self.assertEqual(discovery.rank, Rank.FUNCTION)

    def test_in_string(self):
        text_buffer = self.get_buffer()

        iter = self.get_iter(8, 17)
        discovery = Discoveries(text_buffer, iter).nearest
        self.assertEqual(discovery.rank, Rank.STRING)

    def test_in_comment(self):
        text_buffer = self.get_buffer()

        iter = self.get_iter(9, 12)
        discovery = Discoveries(text_buffer, iter).nearest
        self.assertEqual(discovery.rank, Rank.COMMENT)

        iter = self.get_iter(20, 4)
        discoveries = Discoveries(text_buffer, iter)
        self.assertEqual(discoveries.nearest.rank, Rank.COMMENT)

    def test_in_func(self):
        text_buffer = self.get_buffer()

        iter = self.get_iter(4, 40)
        discoveries = Discoveries(text_buffer, iter)
        self.assertEqual(discoveries.nearest.rank, Rank.FUNCTION)
        self.assertRankings(discoveries, Rank.CLASS, Rank.FUNCTION)

        iter = self.get_iter(23, 23)
        discoveries = Discoveries(text_buffer, iter)
        self.assertEqual(discoveries.nearest.rank, Rank.FUNCTION)
        self.assertRankings(discoveries, Rank.CLASS, Rank.FUNCTION)

    def test_in_if(self):
        text_buffer = self.get_buffer()

        iter = self.get_iter(11, 12)
        discoveries = Discoveries(text_buffer, iter)
        self.assertEqual(discoveries.nearest.rank, Rank.IF)

    def test_in_dict(self):
        text_buffer = self.get_buffer()

        iter = self.get_iter(11, 21)
        discoveries = Discoveries(text_buffer, iter)
        self.assertEqual(discoveries.nearest.rank, Rank.STRING)
        self.assertEqual(discoveries.all_mask, (Rank.STRING |
                                                Rank.DICT |
                                                Rank.FUNCTION |
                                                Rank.IF |
                                                Rank.CLASS))

    def test_in_list(self):
        text_buffer = self.get_buffer()

        iter = self.get_iter(12, 40)
        discoveries = Discoveries(text_buffer, iter)
        self.assertEqual(discoveries.nearest.rank, Rank.LIST)
        self.assertRankings(discoveries,
                            Rank.CLASS,
                            Rank.FUNCTION,
                            Rank.ELIF,
                            Rank.TUPLE,
                            Rank.LIST)

    def test_in_call_params(self):
        text_buffer = self.get_buffer()

        iter = self.get_iter(5, 32)
        discoveries = Discoveries(text_buffer, iter)
        self.assertEqual(discoveries.all_mask, (Rank.TUPLE | Rank.CLASS | Rank.FUNCTION))
        self.assertEqual(discoveries.nearest.rank, Rank.TUPLE)
        self.assertFalse(discoveries.in_function_params)
        self.assertTrue(discoveries.in_function_call_params)

    def test_in_function_params(self):
        text_buffer = self.get_buffer()

        iter = self.get_iter(4, 24)
        discoveries = Discoveries(text_buffer, iter)
        self.assertEqual(discoveries.all_mask, (Rank.TUPLE | Rank.CLASS | Rank.FUNCTION))
        self.assertEqual(discoveries.nearest.rank, Rank.TUPLE)
        self.assertTrue(discoveries.in_function_params)
        self.assertFalse(discoveries.in_function_call_params)

    def test_in_inner_class_func(self):
        text_buffer = self.get_buffer()

        iter = self.get_iter(26, 22)
        discoveries = Discoveries(text_buffer, iter)
        self.assertEqual(discoveries.all_mask, Rank.CLASS)
        self.assertEqual(discoveries.nearest.rank, Rank.CLASS)

def view_test():
    win = Gtk.Window()
    scroller = Gtk.ScrolledWindow()
    view = GtkSource.View()
    win.add(scroller)
    scroller.add(view)
    lang = GtkSource.LanguageManager.get_default().get_language('python')
    view.get_buffer().set_language(lang)
    view.get_buffer().set_text("#!/usr/bin/env python")
    view.set_monospace(True)
    view.set_show_line_numbers(True)
    view.set_insert_spaces_instead_of_tabs(True)
    view.set_tab_width(4)
    view.set_indent_width(4)
    view.set_auto_indent(False)
    ident = PythonIndenter()
    def on_key_press_event(view, event):
        if ident.do_is_trigger(event):
            buffer = view.get_buffer()
            insert = buffer.get_insert()
            begin = buffer.get_iter_at_mark(insert)
            end = begin.copy()
            ret,off = ident.do_format(view, begin, end, event)
            if ret is None:
                ret = ''
            buffer.delete(begin,end)
            buffer.insert(begin, ret, -1)
            begin.backward_chars(off)
            buffer.select_range(begin, begin)
            return True
        return False
    win.set_default_size(640, 480)
    win.connect('delete-event', lambda *_: Gtk.main_quit())
    view.connect('key-press-event', on_key_press_event)
    win.show_all()
    Gtk.main()

if __name__ == '__main__':
    unittest.main()
    #view_test()
