#!/usr/bin/env python3

#
# gdb_plugin.py
#
# Copyright (C) 2017 Christian Hergert <chris@dronelabs.com>
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
from gi.repository import Ide

class GdbDebugger(Ide.Object, Ide.Debugger):
    can_step_in = GObject.Property('can-step-in', type=bool, default=False)
    can_step_over = GObject.Property('can-step-over', type=bool, default=False)
    can_continue = GObject.Property('can-continue', type=bool, default=False)

    def do_get_name(self):
        return 'GNU Debugger'

    def do_supports_runner(self, runner):
        if runner.get_runtime().contains_program_in_path('gdb'):
            return (True, GLib.MAXINT)
        else:
            return (False, 0)

    def do_prepare(self, runner):
        gdb_arguments = ['gdb', '-ex', 'run', '--args']

        for arg in reversed(gdb_arguments):
            runner.prepend_argv(arg)

