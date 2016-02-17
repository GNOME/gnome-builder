#!/usr/bin/env python3

# jhbuild_plugin.py
#
# Copyright (C) 2016 Patrick Griffis <tingping@tingping.se>
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

import os

import gi
gi.require_version('Ide', '1.0')

from gi.repository import (
    GLib,
    GObject,
    Gio,
    Ide,
)

class JhbuildRuntime(Ide.Runtime):

    def __init__(self, *args, **kwargs):
        Ide.Runtime.__init__(self, *args, **kwargs)

    def do_create_launcher(self):
        try:
            launcher = Ide.Runtime.do_create_launcher(self)
            launcher.push_argv('jhbuild')
            launcher.push_argv('run')
            return launcher
        except GLib.Error:
            return None

    def do_prepare_configuration(self, configuration):
        launcher = self.create_launcher()
        launcher.push_argv('sh')
        launcher.push_argv('-c')
        launcher.push_argv('echo $JHBUILD_PREFIX')
        launcher.set_flags(Gio.SubprocessFlags.STDOUT_PIPE)

        prefix = None
        try:
            # FIXME: Async
            subprocess = launcher.spawn_sync (None)
            success, output, err_output = subprocess.communicate_utf8 (None, None)
            if success:
                prefix = output.strip()
        except GLib.Error:
            pass

        if not prefix:
            prefix = os.path.join(GLib.get_home_dir(), 'jhbuild', 'install')

        configuration.set_prefix(prefix)

    def do_contains_program_in_path(self, program, cancellable):
        launcher = self.create_launcher()
        launcher.push_argv('which')
        launcher.push_argv(program)

        try:
            subprocess = launcher.spawn_sync (cancellable)
            return subprocess.wait_check (cancellable)
        except GLib.Error:
            return False

class JhbuildRuntimeProvider(GObject.Object, Ide.RuntimeProvider):

    def __init__(self, *args, **kwargs):
        super().__init__(self, *args, **kwargs)
        self.runtimes = []

    @staticmethod
    def _is_jhbuild_installed():
        # This could be installed into another path, but their build system
        # defaults to ~/.local
        jhbuild_path = os.path.join(GLib.get_home_dir(), '.local', 'bin', 'jhbuild')
        return os.path.isfile(jhbuild_path)

    def do_load(self, manager):
        if self._is_jhbuild_installed():
            runtime = JhbuildRuntime(id="jhbuild", display_name="JHBuild")
            manager.add(runtime)
            self.runtimes.append(runtime)

    def do_unload(self, manager):
        for runtime in self.runtimes:
            manager.remove(runtime)
        self.runtimes = []
