#!/usr/bin/env python3

#
# helper.py
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

from getopt import getopt, GetoptError
import shutil
import sys

from gi.repository import GObject
from gi.repository import Gio
from gi.repository import Ide

_ = Ide.gettext

class PyApplicationTool(GObject.Object, Ide.ApplicationTool):
    # Leftover args during parsing
    args = None

    # Options extracted during parsing
    options = None

    # "ide"
    prgname = None

    # "build"
    command = None

    # getopt long and short options
    long_opts = []
    short_opts = ''

    def parse(self, args):
        args = list(args)
        self.prgname = args.pop(0)
        self.command = args.pop(0)
        self.args = args

        if 'h' not in self.short_opts:
            self.short_opts += 'h'

        if 'help' not in self.long_opts:
            self.long_opts.append('help')

        try:
            self.options, self.args = getopt(args, self.short_opts, self.long_opts)

            for o,a in self.options:
                if o in ('-h', '--help'):
                    self.usage(stream=sys.stdout)
                    self.exit(0)
                    return

        except GetoptError:
            self.usage(sys.stderr)

    def usage(self, stream=sys.stdout):
        stream.write(_("""Usage:
  %(prgname) %(command) OPTIONS
""" % {'prgname': self.prgname, 'command': self.command}))

    def do_run_async(self, args, cancellable, callback, user_data):
        task = Gio.Task(self, cancellable, callback)
        task.return_int(0)

    def do_run_finish(self, result):
        return result.propagate_int()

    def exit(self, exit_code):
        sys.exit(exit_code)

    def printerr(self, *args):
        for arg in args:
            sys.stderr.write(str(arg))
            sys.stderr.write('\n')

    def write_line(self, line):
        sys.stdout.write(line)
        sys.stdout.write('\n')

    def clear_line(self):
        sys.stdout.write("\033[2K")
        sys.stdout.write("\r")

    def write_progress(self, fraction, prefix="", suffix=""):
        width = shutil.get_terminal_size().columns

        sys.stdout.write("    ")
        width -= 4

        if prefix:
            sys.stdout.write(prefix)
            width -= len(prefix)

            sys.stdout.write(" ")
            width -= 1

        if suffix:
            width -= len(suffix)
            width -= 1

        sys.stdout.write(self._mkprog(fraction, width))

        if suffix:
            sys.stdout.write(" ")
            sys.stdout.write(suffix)

        sys.stdout.flush()

    def _mkprog(self, fraction, width):
        width -= 2 # [ and ]
        fraction = min(1.0, max(0.0, fraction))
        if fraction < 1.0:
            width = max(4, width)
            prog = '['
            prog += '=' * (max(0, int(fraction * width) - 1))
            prog += '>'
            prog += max(0, (width - 2 - len(prog))) * ' '
            prog += ']'
            return prog
        else:
            return '[' + (width * '=') + ']'
