#!/usr/bin/env python3

# java_langserv_plugin.py
#
# Copyright 2019 Alberto Fanjul <albertofanjul@gmail.com>
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
This plugin provides integration with the Java Language Server.
It builds off the generic language service components in libide
by bridging them to our supervised Java Language Server.
"""

import os
import gi

from gi.repository import GLib
from gi.repository import Gio
from gi.repository import GObject
from gi.repository import Ide

DEV_MODE = os.getenv('DEV_MODE') and True or False

class JavaService(Ide.Object):
    _client = None
    _has_started = False
    _supervisor = None

    @classmethod
    def from_context(klass, context):
        return context.ensure_child_typed(JavaService)

    @GObject.Property(type=Ide.LspClient)
    def client(self):
        return self._client

    @client.setter
    def client(self, value):
        self._client = value
        self.notify('client')

    def do_stop(self):
        if self._supervisor:
            supervisor, self._supervisor = self._supervisor, None
            supervisor.stop()

    def _which_java_lanserver(self):
        path = os.getenv('JAVALS_CMD')
        if path and os.path.exists(os.path.expanduser(path)):
            return path
        return "javals"

    def _ensure_started(self):
        # To avoid starting the process unconditionally at startup, lazily
        # start it when the first provider tries to bind a client to its
        # :client property.
        if not self._has_started:
            self._has_started = True

            launcher = self._create_launcher()
            launcher.set_clear_env(False)

            # Locate the directory of the project and run java-langserver from there
            workdir = self.get_context().ref_workdir()
            launcher.set_cwd(workdir.get_path())


            # Bash will load the host $PATH for us.
            # This does mean there will be a possible .bashrc vs .bash_profile
            # discrepancy. Possibly there is a better native way to make sure that
            # builder running in flatpak can run processes in the host context with
            # the host's $PATH.
            launcher.push_argv("/bin/bash")
            launcher.push_argv("--login")
            launcher.push_argv("-c")
            launcher.push_argv('exec %s %s' % (self._which_java_lanserver(),
                "--quiet" if not DEV_MODE else ""))

            # Spawn our peer process and monitor it for
            # crashes. We may need to restart it occasionally.
            self._supervisor = Ide.SubprocessSupervisor()
            self._supervisor.connect('spawned', self._ls_spawned)
            self._supervisor.set_launcher(launcher)
            self._supervisor.start()

    def _ls_spawned(self, supervisor, subprocess):
        stdin = subprocess.get_stdin_pipe()
        stdout = subprocess.get_stdout_pipe()
        io_stream = Gio.SimpleIOStream.new(stdout, stdin)

        if self._client:
            self._client.stop()
            self._client.destroy()

        self._client = Ide.LspClient.new(io_stream)
        self.append(self._client)
        self._client.add_language('java')
        self._client.start()
        self.notify('client')

    def _create_launcher(self):
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
        context = provider.get_context()
        self = JavaService.from_context(context)
        self._ensure_started()
        self.bind_property('client', provider, 'client', GObject.BindingFlags.SYNC_CREATE)

class JavaDiagnosticProvider(Ide.LspDiagnosticProvider):
    def do_load(self):
        JavaService.bind_client(self)

class JavaRenameProvider(Ide.LspRenameProvider):
    def do_load(self):
        JavaService.bind_client(self)

class JavaSymbolResolver(Ide.LspSymbolResolver, Ide.SymbolResolver):
    def do_load(self):
        JavaService.bind_client(self)

class JavaHighlighter(Ide.LspHighlighter):
    def do_load(self):
        JavaService.bind_client(self)

class JavaCompletionProvider(Ide.LspCompletionProvider, Ide.CompletionProvider):
    def do_load(self, context):
        JavaService.bind_client(self)

    def do_get_priority(self, context):
        # This provider only activates when it is very likely that we
        # want the results. So use high priority (negative is better).
        return -1000

class JavaFormatter(Ide.LspFormatter, Ide.Formatter):
    def do_load(self):
        JavaService.bind_client(self)

class JavaHoverProvider(Ide.LspHoverProvider):
    def do_prepare(self):
        self.props.category = 'Java'
        self.props.priority = 200
        JavaService.bind_client(self)

