#!/usr/bin/env python

# vala_langserv_plugin.py
#
# Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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
This plugin provides integration with the Vala Language Server.
It builds off the generic language service components in libide
by bridging them to our supervised Vala Language Server.
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

class ValaService(Ide.Object, Ide.Service):
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
        Stops the Vala Language Server upon request to shutdown the
        ValaService.
        """
        if self._supervisor:
            supervisor, self._supervisor = self._supervisor, None
            supervisor.stop()

    def _ensure_started(self):
        """
        Start the vala service which provides communication with the
        Vala Language Server. We supervise our own instance of the
        language server and restart it as necessary using the
        Ide.SubprocessSupervisor.

        Various extension points (diagnostics, symbol providers, etc) use
        the ValaService to access the vala components they need.
        """
        # To avoid starting the `rls` process unconditionally at startup,
        # we lazily start it when the first provider tries to bind a client
        # to its :client property.
        if not self._has_started:
            self._has_started = True

            # Setup a launcher to spawn the vala language server
            launcher = self._create_launcher()
            launcher.set_clear_env(False)
            if DEV_MODE:
                launcher.setenv('JSONRPC_DEBUG', '1', True)

            # Locate the directory of the project and run rls from there.
            workdir = self.get_context().get_vcs().get_working_directory()
            launcher.set_cwd(workdir.get_path())

            path_to_vls = "/home/ben/dev/vala-language-server/build/vala-language-server"

            # Setup our Argv. We want to communicate over STDIN/STDOUT,
            # so it does not require any command line options.
            launcher.push_argv(path_to_vls)

            # Spawn our peer process and monitor it for
            # crashes. We may need to restart it occasionally.
            self._supervisor = Ide.SubprocessSupervisor()
            self._supervisor.connect('spawned', self._vls_spawned)
            self._supervisor.set_launcher(launcher)
            self._supervisor.start()

    def _vls_spawned(self, supervisor, subprocess):
        """
        This callback is executed when the `vls` process is spawned.
        We can use the stdin/stdout to create a channel for our
        LangservClient.
        """
        stdin = subprocess.get_stdin_pipe()
        stdout = subprocess.get_stdout_pipe()
        io_stream = Gio.SimpleIOStream.new(stdout, stdin)

        if self._client:
            self._client.stop()

        self._client = Ide.LangservClient.new(self.get_context(), io_stream)
        self._client.add_language('vala')
        self._client.start()
        self.notify('client')

    def _create_launcher(self):
        """
        Creates a launcher to be used by the vala service. This needs
        to be run on the host because we do not currently bundle vala
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
        launcher.set_cwd(GLib.get_home_dir())
        launcher.set_run_on_host(True)
        return launcher

    @classmethod
    def bind_client(klass, provider):
        """
        This helper tracks changes to our client as it might happen when
        our `rls` process has crashed.
        """
        context = provider.get_context()
        self = context.get_service_typed(ValaService)
        self._ensure_started()
        self.bind_property('client', provider, 'client', GObject.BindingFlags.SYNC_CREATE)

class ValaDiagnosticProvider(Ide.LangservDiagnosticProvider):
    def do_load(self):
        ValaService.bind_client(self)

class ValaCompletionProvider(Ide.LangservCompletionProvider):
    def do_load(self, context):
        ValaService.bind_client(self)

class ValaRenameProvider(Ide.LangservRenameProvider):
    def do_load(self):
        ValaService.bind_client(self)

class ValaSymbolResolver(Ide.LangservSymbolResolver):
    def do_load(self):
        ValaService.bind_client(self)

class ValaHighlighter(Ide.LangservHighlighter):
    def do_load(self):
        ValaService.bind_client(self)

class ValaFormatter(Ide.LangservFormatter):
    def do_load(self):
        ValaService.bind_client(self)
