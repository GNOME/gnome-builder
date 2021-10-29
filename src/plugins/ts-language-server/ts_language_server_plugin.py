#!/usr/bin/env python3

# ts_language_server_plugin.py
#
# Copyright 2021 Georg Vienna <georg.vienna@himbarsoft.com>
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

import gi

from gi.repository import GLib
from gi.repository import Gio
from gi.repository import GObject
from gi.repository import Ide

DEV_MODE = True

class TypescriptService(Ide.Object):
    _client = None
    _has_started = False
    _supervisor = None

    @classmethod
    def from_context(klass, context):
        return context.ensure_child_typed(TypescriptService)

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

    def _ensure_started(self):
        # To avoid starting the process unconditionally at startup, lazily
        # start it when the first provider tries to bind a client to its
        # :client property.
        if not self._has_started:
            self._has_started = True

            launcher = self._create_launcher()
            launcher.set_clear_env(False)

            # Locate the directory of the project and run typescript-language-server from there
            workdir = self.get_context().ref_workdir()
            launcher.set_cwd(workdir.get_path())

            # this needs https://github.com/typescript-language-server/typescript-language-server installed on the host
            launcher.push_argv("typescript-language-server")
            launcher.push_argv("--stdio")

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
        self._client.add_language('javascript')
        self._client.add_language('typescript')
        self._client.start()
        self.notify('client')

    def _create_launcher(self):
        flags = Gio.SubprocessFlags.STDIN_PIPE | Gio.SubprocessFlags.STDOUT_PIPE
        if not DEV_MODE:
            flags |= Gio.SubprocessFlags.STDERR_SILENCE
        launcher = Ide.SubprocessLauncher()
        launcher.set_flags(flags)
        launcher.set_run_on_host(True)
        return launcher

    @classmethod
    def bind_client(klass, provider):
        context = provider.get_context()
        self = TypescriptService.from_context(context)
        self._ensure_started()
        self.bind_property('client', provider, 'client', GObject.BindingFlags.SYNC_CREATE)

class TypescriptDiagnosticProvider(Ide.LspDiagnosticProvider, Ide.DiagnosticProvider):
    def do_load(self):
        TypescriptService.bind_client(self)

class TypescriptCompletionProvider(Ide.LspCompletionProvider, Ide.CompletionProvider):
    def do_load(self, context):
        TypescriptService.bind_client(self)

    def do_get_priority(self, context):
        # This provider only activates when it is very likely that we
        # want the results. So use high priority (negative is better).
        return -1000

class TypescriptSymbolResolver(Ide.LspSymbolResolver, Ide.SymbolResolver):
    def do_load(self):
        TypescriptService.bind_client(self)

class TypescriptHighlighter(Ide.LspHighlighter, Ide.Highlighter):
    def do_load(self):
        TypescriptService.bind_client(self)

class TypescriptFormatter(Ide.LspFormatter, Ide.Formatter):
    def do_load(self):
        TypescriptService.bind_client(self)

class TypescriptHoverProvider(Ide.LspHoverProvider, Ide.HoverProvider):
    def do_prepare(self):
        self.props.category = 'Typescript'
        self.props.priority = 200
        TypescriptService.bind_client(self)

class TypescriptRenameProvider(Ide.LspRenameProvider, Ide.RenameProvider):
    def do_load(self):
        TypescriptService.bind_client(self)

