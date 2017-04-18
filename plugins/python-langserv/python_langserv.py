#!/usr/bin/env python3

# python_langserv.py
#
# Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
# Copyright (C) 2017 Patrick Griffis <tingping@tingping.se>
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

"""
This plugin provides integration with the Python Language Server.
It builds off the generic language service components in libide
by bridging them to our supervised Python Language Server.
"""

import gi
import os

gi.require_version('Ide', '1.0')
gi.require_version('GtkSource', '3.0')

from gi.repository import GLib
from gi.repository import Gio
from gi.repository import GObject
from gi.repository import GtkSource
from gi.repository import Ide

DEV_MODE = True

class PythonService(Ide.Object, Ide.Service):
    _client = None
    _has_started = False
    _supervisor = None

    @GObject.Property(type=Ide.LangservClient)
    def client(self):
        return self._client

    @client.setter
    def client(self, value):
        self._client = value
        self.notify('client')

    def do_stop(self):
        """
        Stops the Python Language Server upon request to shutdown the
        PythonService.
        """
        if self._supervisor:
            supervisor, self._supervisor = self._supervisor, None
            supervisor.stop()

    def _ensure_started(self):
        """
        Start the python service which provides communication with the
        Python Language Server. We supervise our own instance of the
        language server and restart it as necessary using the
        Ide.SubprocessSupervisor.

        Various extension points (diagnostics, symbol providers, etc) use
        the PythonService to access the python components they need.
        """
        # To avoid starting the `pyls` process unconditionally at startup,
        # we lazily start it when the first provider tries to bind a client
        # to its :client property.
        if not self._has_started:
            self._has_started = True

            # Setup a launcher to spawn the python language server
            launcher = self._create_launcher()
            launcher.set_clear_env(False)

            # Locate the directory of the project and run pyls from there.
            workdir = self.get_context().get_vcs().get_working_directory()
            launcher.set_cwd(workdir.get_path())

            launcher.push_argv('pyls')

            # Spawn our peer process and monitor it for
            # crashes. We may need to restart it occasionally.
            self._supervisor = Ide.SubprocessSupervisor()
            self._supervisor.connect('spawned', self._pyls_spawned)
            self._supervisor.set_launcher(launcher)
            self._supervisor.start()

    def _pyls_spawned(self, supervisor, subprocess):
        """
        This callback is executed when the `pyls` process is spawned.
        We can use the stdin/stdout to create a channel for our
        LangservClient.
        """
        stdin = subprocess.get_stdin_pipe()
        stdout = subprocess.get_stdout_pipe()
        io_stream = Gio.SimpleIOStream.new(stdout, stdin)

        if self._client:
            self._client.stop()

        self._client = Ide.LangservClient.new(self.get_context(), io_stream)
        self._client.add_language('python3')
        self._client.add_language('python')
        self._client.start()
        self.notify('client')

    def _create_launcher(self):
        """
        Creates a launcher to be used by the python service. This needs
        to be run on the host because we do not currently bundle python
        inside our flatpak.

        In the future, we might be able to rely on the runtime for
        the tooling. Maybe even the program if flatpak-builder has
        prebuilt our dependencies.
        """
        flags = Gio.SubprocessFlags.STDIN_PIPE | Gio.SubprocessFlags.STDOUT_PIPE
        if not DEV_MODE:
            flags |= Gio.SubprocessFlags.STDERR_SILENCE
        launcher = Ide.SubprocessLauncher()
        launcher.set_flags(flags)
        launcher.set_run_on_host(True)
        return launcher

    @classmethod
    def bind_client(klass, provider):
        """
        This helper tracks changes to our client as it might happen when
        our `pyls` process has crashed.
        """
        context = provider.get_context()
        self = context.get_service_typed(PythonService)
        self._ensure_started()
        self.bind_property('client', provider, 'client', GObject.BindingFlags.SYNC_CREATE)

class PythonDiagnosticProvider(Ide.LangservDiagnosticProvider):
    def do_load(self):
        PythonService.bind_client(self)

class PythonCompletionProvider(Ide.LangservCompletionProvider):
    def do_load(self, context):
        PythonService.bind_client(self)

class PythonSymbolResolver(Ide.LangservSymbolResolver):
    def do_load(self):
        PythonService.bind_client(self)

class PythonFormatter(Ide.LangservFormatter):
    def do_load(self):
        PythonService.bind_client(self)
