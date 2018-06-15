#!/usr/bin/env python

# d_langserver_plugin.py
#
# Copyright 2016 Clipsey <clipseypone@gmail.com>
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
This plugin provides integration with the D Language Server (serve-d).
It builds off the generic language service components in libide
by bridging them to our supervised D Language Server.
"""

import gi
import os
import glob

gi.require_version('Ide', '1.0')

from gi.repository import GLib
from gi.repository import Gio
from gi.repository import GObject
from gi.repository import Ide

DEV_MODE = True

class DService(Ide.Object, Ide.Service):
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
        Stops the D Language Server upon request to shutdown the
        DService.
        """
        if self._supervisor:
            supervisor, self._supervisor = self._supervisor, None
            supervisor.stop()

    def _ensure_started(self):
        """
        Start the D service which provides communication with the
        D Language Server. We supervise our own instance of the
        language server and restart it as necessary using the
        Ide.SubprocessSupervisor.

        Various extension points (diagnostics, symbol providers, etc) use
        the DService to access the D components they need.
        """
        # To avoid starting the `serve-d` process unconditionally at startup,
        # we lazily start it when the first provider tries to bind a client
        # to its :client property.
        if not self._has_started:
            self._has_started = True

            # Setup a launcher to spawn the D language server
            launcher = self._create_launcher()
            launcher.set_clear_env(False)

            # Locate the directory of the project and run serve-d from there.
            workdir = self.get_context().get_vcs().get_working_directory()
            launcher.set_cwd(workdir.get_path())

            # run serve-d
            pname = os.path.expanduser("~/.dub/packages/")
            # Find current version of serve-d
            pname = glob.glob(pname+"serve-d-*/")[0] + "serve-d/serve-d"
            launcher.push_argv(pname)

            # require DLang support
            launcher.push_argv("--skip-configuration")
            launcher.push_argv("--require")
            launcher.push_argv("D")

            # Spawn our peer process and monitor it for
            # crashes. We may need to restart it occasionally.
            self._supervisor = Ide.SubprocessSupervisor()
            self._supervisor.connect('spawned', self._served_spawned)
            self._supervisor.set_launcher(launcher)
            self._supervisor.start()

    def _served_spawned(self, supervisor, subprocess):
        """
        This callback is executed when the `serve-d` process is spawned.
        We can use the stdin/stdout to create a channel for our
        LangservClient.
        """
        stdin = subprocess.get_stdin_pipe()
        stdout = subprocess.get_stdout_pipe()
        io_stream = Gio.SimpleIOStream.new(stdout, stdin)

        if self._client:
            self._client.stop()

        self._client = Ide.LangservClient.new(self.get_context(), io_stream)
        self._client.add_language('d')
        self._client.start()
        self.notify('client')

    def _create_launcher(self):
        """
        Creates a launcher to be used by the D service. This needs
        to be run on the host because we do not currently bundle D
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
        our `serve-d` process has crashed.
        """
        context = provider.get_context()
        self = context.get_service_typed(DService)
        self._ensure_started()
        self.bind_property('client', provider, 'client', GObject.BindingFlags.SYNC_CREATE)

class DDiagnosticProvider(Ide.LangservDiagnosticProvider):
    def do_load(self):
        DService.bind_client(self)

class DCompletionProvider(Ide.LangservCompletionProvider):
    def do_load(self, context):
        DService.bind_client(self)

class DRenameProvider(Ide.LangservRenameProvider):
    def do_load(self):
        DService.bind_client(self)

class DSymbolResolver(Ide.LangservSymbolResolver):
    def do_load(self):
        DService.bind_client(self)

class DFormatter(Ide.LangservFormatter):
    def do_load(self):
        DService.bind_client(self)
