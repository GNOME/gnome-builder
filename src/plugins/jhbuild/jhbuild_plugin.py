#!/usr/bin/env python3

# jhbuild_plugin.py
#
# Copyright 2016 Patrick Griffis <tingping@tingping.se>
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
# SPDX-License-Identifier: GPL-3.0-or-later

import os

from gi.repository import GLib
from gi.repository import GObject
from gi.repository import Gio
from gi.repository import Ide

_ = Ide.gettext

class JhbuildRuntime(Ide.Runtime):
    __gtype_name__ = 'JhbuildRuntime'

    def __init__(self, *args, **kwargs):
        self.jhbuild_path = kwargs.get('executable_path', None)
        if self.jhbuild_path is None:
            raise TypeError('Missing keyword argument: executable_path')
        del kwargs['executable_path']

        super().__init__(*args, **kwargs)

    def do_create_runner(self, build_target):
        runner = Ide.Runtime.do_create_runner(self, build_target)
        runner.set_run_on_host(True)
        return runner

    def do_create_launcher(self):
        launcher = Ide.Runtime.do_create_launcher(self)

        launcher.push_argv(self.jhbuild_path)
        launcher.push_argv('run')

        launcher.set_run_on_host(True)
        launcher.set_clear_env(False)

        return launcher

    def do_prepare_configuration(self, config):
        launcher = self.create_launcher()
        launcher.push_argv('sh')
        launcher.push_argv('-c')
        launcher.push_argv('echo $JHBUILD_PREFIX')
        launcher.set_flags(Gio.SubprocessFlags.STDOUT_PIPE)

        prefix = None
        try:
            # FIXME: Async
            subprocess = launcher.spawn(None)
            success, output, err_output = subprocess.communicate_utf8(None, None)
            if success:
                prefix = output.strip()
        except GLib.Error:
            pass

        if not prefix:
            prefix = os.path.join(GLib.get_home_dir(), 'jhbuild', 'install')

        config.set_prefix(prefix)
        config.set_prefix_set(False)

    def do_contains_program_in_path(self, program, cancellable):
        launcher = self.create_launcher()
        launcher.push_argv('which')
        launcher.push_argv(program)

        try:
            subprocess = launcher.spawn(cancellable)
            return subprocess.wait_check(cancellable)
        except GLib.Error:
            return False

class JhbuildRuntimeProvider(Ide.Object, Ide.RuntimeProvider):
    __gtype_name__ = 'JhbuildRuntimeProvider'

    def __init__(self, *args, **kwargs):
        super().__init__(self, *args, **kwargs)
        self.runtimes = []

    @staticmethod
    def _get_jhbuild_path():
        for jhbuild_bin in ['jhbuild', os.path.expanduser('~/.local/bin/jhbuild')]:
            try:
                launcher = Ide.SubprocessLauncher.new(Gio.SubprocessFlags.STDOUT_SILENCE |
                                                      Gio.SubprocessFlags.STDERR_SILENCE)
                launcher.push_argv('which')
                launcher.push_argv(jhbuild_bin)

                launcher.set_run_on_host(True)
                launcher.set_clear_env(False)

                subprocess = launcher.spawn(None)
                if subprocess.wait_check(None) is True:
                    return jhbuild_bin
            except GLib.Error:
                pass

        return None

    def do_load(self, manager):
        jhbuild_path = self._get_jhbuild_path()
        if jhbuild_path is not None:
            runtime = JhbuildRuntime(id='jhbuild',
                                     category=_('Host System'),
                                     display_name='JHBuild',
                                     executable_path=jhbuild_path)
            self.append(runtime)
            manager.add(runtime)
            self.runtimes.append(runtime)

    def do_unload(self, manager):
        for runtime in self.runtimes:
            manager.remove(runtime)
            runtime.destroy()
        self.runtimes = []
