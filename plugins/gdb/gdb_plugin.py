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
    def do_get_name(self):
        return 'GNU Debugger'

    def do_supports_runner(self, runner):
        """
        Checks to see if we support running this program.

        TODO: We should check if it is an ELF binary.

        For now, we just always return True, but with a priority that
        allows other debuggers to take priority.
        """
        return (True, GLib.MAXINT)
