#!/usr/bin/env python

# rust_langserv_plugin.py
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
This plugin provides integration with the Rust Language Server.
It builds off the generic language service components in libide
by bridging them to our supervised Rust Language Server.
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

class RustService(Ide.Object, Ide.Service):
    client = None

    __gsignals__ = {
        # This signal is emitted whenever the `rls` has restarted. This happens
        # at the start of the process as well as when the rls process crashes
        # and we need to reload things.
        'client-changed': (GObject.SIGNAL_RUN_LAST, GObject.TYPE_NONE, (Ide.LangservClient,)),
    }

    def do_start(self):
        """
        Start the rust service which provides communication with the
        Rust Language Server. We supervise our own instance of the
        language server and restart it as necessary using the
        Ide.SubprocessSupervisor.

        Various extension points (diagnostics, symbol providers, etc) use
        the RustService to access the rust components they need.
        """
        # Setup a launcher to spawn the rust language server
        launcher = self._create_launcher()
        launcher.set_clear_env(False)
        launcher.setenv("SYS_ROOT", self._discover_sysroot(), True)

        # If rls was installed with Cargo, try to discover that
        # to save the user having to update PATH.
        path_to_rls = os.path.expanduser("~/.cargo/bin/rls")
        if not os.path.exists(path_to_rls):
            path_to_rls = "rls"

        # Setup our Argv. We want to communicate over STDIN/STDOUT,
        # so it does not require any command line options.
        launcher.push_argv(path_to_rls)

        # Spawn our peer process and monitor it for
        # crashes. We may need to restart it occasionally.
        supervisor = Ide.SubprocessSupervisor()
        supervisor.connect('spawned', self._rls_spawned)
        supervisor.set_launcher(launcher)
        supervisor.start()

    def _rls_spawned(self, supervisor, subprocess):
        """
        This callback is executed when the `rls` process is spawned.
        We can use the stdin/stdout to create a channel for our
        LangservClient.
        """
        stdin = subprocess.get_stdin_pipe()
        stdout = subprocess.get_stdout_pipe()
        io_stream = Gio.SimpleIOStream.new(stdout, stdin)

        if self.client:
            self.client.stop()

        self.client = Ide.LangservClient.new(self.get_context(), io_stream)
        self.client.start()

        self.emit('client-changed', self.client)

    def _create_launcher(self):
        """
        Creates a launcher to be used by the rust service. This needs
        to be run on the host because we do not currently bundle rust
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

    def _discover_sysroot(self):
        """
        The Rust Language Server needs to know where the sysroot is of
        the Rust installation we are using. This is simple enough to
        get, by using `rust --print sysroot` as the rust-language-server
        documentation suggests.
        """
        launcher = self._create_launcher()
        launcher.push_args(['rustc', '--print', 'sysroot'])
        subprocess = launcher.spawn_sync()
        _, stdout, _ = subprocess.communicate_utf8()
        return stdout.strip()

    @classmethod
    def bind_client(klass, provider):
        """
        This helper tracks changes to our client as it might happen when
        our `rls` process has crashed.
        """
        context = provider.get_context()
        self = context.get_service_typed(RustService)
        self.connect('client-changed', lambda _,client: provider.set_client(client))
        if self.client is not None:
            provider.set_client(self.client)

class RustDiagnosticProvider(Ide.LangservDiagnosticProvider):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.connect('notify::context', lambda *_: RustService.bind_client(self))

class RustCompletionProvider(Ide.LangservCompletionProvider):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.connect('notify::context', lambda *_: RustService.bind_client(self))

class RustSymbolResolver(Ide.LangservSymbolResolver):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.connect('notify::context', lambda *_: RustService.bind_client(self))

"""
class RustHighlighter(Ide.LangservHighlighter):
    def do_set_context(self, context):
        RustService.bind_client(self);
"""
