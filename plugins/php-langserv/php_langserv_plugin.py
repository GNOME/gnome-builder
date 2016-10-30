#!/usr/bin/env python3

#
# php_langserv_plugin.py
#
# Copyright (C) 2016 Christian Hergert <chris@dronelabs.com>
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
import os

gi.require_version('Ide', '1.0')
gi.require_version('GtkSource', '3.0')

from gi.repository import GLib
from gi.repository import Gio
from gi.repository import GObject
from gi.repository import GtkSource
from gi.repository import Ide

class PhpService(Ide.Object, Ide.Service):
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
        Stops the PHP Language Server upon request to shutdown the
        PhpService.
        """
        if self._supervisor:
            supervisor, self._supervisor = self._supervisor, None
            supervisor.stop()

    def _ensure_started(self):
        """
        Start the php service which provides communication with the
        Php Language Server. We supervise our own instance of the
        language server and restart it as necessary using the
        Ide.SubprocessSupervisor.

        Various extension points (diagnostics, symbol providers, etc) use
        the PhpService to access the php components they need.
        """
        # To avoid starting the `php` process unconditionally at startup,
        # we lazily start it when the first provider tries to bind a client
        # to it's :client property.
        if not self._has_started:
            self._has_started = True

            # Setup a launcher to spawn the php language server
            launcher = self._create_launcher()
            launcher.set_clear_env(False)

            # TODO: Discover path to php-language-server.php
            php_language_server = os.path.expanduser("~/Projects/php-language-server/bin/php-language-server.php")

            # Setup our Argv. We want to communicate over STDIN/STDOUT,
            # so it does not require any command line options.
            launcher.push_args(["php", php_language_server])

            # Spawn our peer process and monitor it for
            # crashes. We may need to restart it occasionally.
            self._supervisor = Ide.SubprocessSupervisor()
            self._supervisor.connect('spawned', self._php_spawned)
            self._supervisor.set_launcher(launcher)
            self._supervisor.start()

    def _php_spawned(self, supervisor, subprocess):
        """
        This callback is executed when the `php` process is spawned.
        We can use the stdin/stdout to create a channel for our
        LangservClient.
        """
        stdin = subprocess.get_stdin_pipe()
        stdout = subprocess.get_stdout_pipe()
        io_stream = Gio.SimpleIOStream.new(stdout, stdin)

        if self._client:
            self._client.stop()

        self._client = Ide.LangservClient.new(self.get_context(), io_stream)
        self._client.add_language('php')
        self._client.start()
        self.notify('client')

    def _create_launcher(self):
        """
        Creates a launcher to be used by the php service. This needs
        to be run on the host because we do not currently bundle php
        inside our flatpak.

        In the future, we might be able to rely on the runtime for
        the tooling. Maybe even the program if flatpak-builder has
        prebuilt our dependencies.
        """
        launcher = Ide.SubprocessLauncher()
        launcher.set_flags(Gio.SubprocessFlags.STDIN_PIPE |
                           Gio.SubprocessFlags.STDOUT_PIPE |
                           Gio.SubprocessFlags.STDERR_SILENCE)
        launcher.set_cwd(GLib.get_home_dir())
        launcher.set_run_on_host(True)
        return launcher

    @classmethod
    def bind_client(klass, provider):
        """
        This helper tracks changes to our client as it might happen when
        our `php` process has crashed.
        """
        context = provider.get_context()
        self = context.get_service_typed(PhpService)
        self._ensure_started()
        self.bind_property('client', provider, 'client', GObject.BindingFlags.SYNC_CREATE)

class PhpDiagnosticProvider(Ide.LangservDiagnosticProvider):
    def do_load(self):
        PhpService.bind_client(self)

class PhpSymbolResolver(Ide.LangservSymbolResolver):
    def do_load(self):
        PhpService.bind_client(self)
