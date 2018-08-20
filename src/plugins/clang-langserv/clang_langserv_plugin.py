#!/usr/bin/env python

# clang_langserv_plugin.py
#
# Copyright © 2016 Christian Hergert <chergert@redhat.com>
# Copyright © 2018 Nathaniel McCallum <npmccallum@redhat.com>
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
This plugin provides integration with the Clang Language Server.
It builds off the generic language service components in libide
by bridging them to our supervised Clang Language Server.
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

DEV_MODE = False

class ClangService(Ide.Object, Ide.Service):
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
        Stops the Clang Language Server upon request to shutdown the
        ClangService.
        """
        if self._supervisor:
            supervisor, self._supervisor = self._supervisor, None
            supervisor.stop()

    def _ensure_started(self):
        """
        Start the clangd service which provides communication with the
        Clang Language Server. We supervise our own instance of the
        language server and restart it as necessary using the
        Ide.SubprocessSupervisor.

        Various extension points (diagnostics, symbol providers, etc) use
        the ClangService to access the Clang components they need.
        """
        # To avoid starting the `clangd` process unconditionally at startup,
        # we lazily start it when the first provider tries to bind a client
        # to its :client property.
        if not self._has_started:
            self._has_started = True

            # Setup a launcher to spawn the clang language server
            launcher = self._create_launcher()
            launcher.set_clear_env(False)

            # Locate the directory of the project and run clangd from there.
            workdir = self.get_context().get_vcs().get_working_directory()
            launcher.set_cwd(workdir.get_path())

            # Setup our Argv. We want to communicate over STDIN/STDOUT,
            # so it does not require any command line options.
            launcher.push_argv("clangd")

            # Spawn our peer process and monitor it for
            # crashes. We may need to restart it occasionally.
            self._supervisor = Ide.SubprocessSupervisor()
            self._supervisor.connect('spawned', self._clangd_spawned)
            self._supervisor.set_launcher(launcher)
            self._supervisor.start()

    def _clangd_spawned(self, supervisor, subprocess):
        """
        This callback is executed when the `clangd` process is spawned.
        We can use the stdin/stdout to create a channel for our
        LangservClient.
        """
        stdin = subprocess.get_stdin_pipe()
        stdout = subprocess.get_stdout_pipe()
        io_stream = Gio.SimpleIOStream.new(stdout, stdin)

        if self._client:
            self._client.stop()

        self._client = Ide.LangservClient.new(self.get_context(), io_stream)
        self._client.add_language('c')
        self._client.add_language('chdr')
        self._client.add_language('cpp')
        self._client.add_language('cpphdr')
        self._client.start()
        self.notify('client')

    def _create_launcher(self):
        """
        Creates a launcher to be used by the clangd service. This needs
        to be run on the host because we do not currently bundle clangd
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
        our `clangd` process has crashed.
        """
        context = provider.get_context()
        self = context.get_service_typed(ClangService)
        self._ensure_started()
        self.bind_property('client', provider, 'client', GObject.BindingFlags.SYNC_CREATE)

class ClangDiagnosticProvider(Ide.LangservDiagnosticProvider):
    def do_load(self):
        ClangService.bind_client(self)

class ClangCompletionProvider(Ide.LangservCompletionProvider):
    def do_load(self, context):
        ClangService.bind_client(self)

class ClangRenameProvider(Ide.LangservRenameProvider):
    def do_load(self):
        ClangService.bind_client(self)

class ClangSymbolResolver(Ide.LangservSymbolResolver):
    def do_load(self):
        ClangService.bind_client(self)

class ClangHighlighter(Ide.LangservHighlighter):
    def do_load(self):
        ClangService.bind_client(self)

class ClangFormatter(Ide.LangservFormatter):
    def do_load(self):
        ClangService.bind_client(self)
