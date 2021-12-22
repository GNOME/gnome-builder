#!/usr/bin/env python

# rust_langserv_plugin.py
#
# Copyright 2016 Christian Hergert <chergert@redhat.com>
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

from gi.repository import GLib
from gi.repository import Gio
from gi.repository import GObject
from gi.repository import Ide

DEV_MODE = os.getenv('DEV_MODE') and True or False

class RlsService(Ide.LspService):
    def do_parent_set(self, parent):
        self.set_program('rls')
        self.set_inherit_stderr(DEV_MODE)
        self.set_search_path([os.path.expanduser("~/.cargo/bin")])

        # Monitor Cargo.toml for changes so that we can reload the
        # rls binary as necessary. This requires Cargo.toml to be
        # in the root of the git however.
        context = self.get_context()
        cargo_toml = context.get_workdir().get_child('Cargo.toml')
        if cargo_toml.query_exists():
            try:
                self._monitor = cargo_toml.monitor(0, None)
                self._monitor.set_rate_limit(5 * 1000) # 5 Seconds
                self._monitor.connect('changed', self._monitor_changed_cb)
            except Exception as ex:
                Ide.debug('Failed to monitor Cargo.toml for changes:', repr(ex))

    def _monitor_changed_cb(self, *args):
        self.restart()

    def do_configure_launcher(self, pipeline, launcher):
        if DEV_MODE:
            launcher.setenv('RUST_LOG', 'debug', True)

    def do_configure_client(self, client):
        client.add_language('rust')

class RlsDiagnosticProvider(Ide.LspDiagnosticProvider, Ide.DiagnosticProvider):
    def do_load(self):
        RlsService.bind_client(self)

class RlsCompletionProvider(Ide.LspCompletionProvider, Ide.CompletionProvider):
    def do_load(self, context):
        RlsService.bind_client(self)

    def do_get_priority(self, context):
        # This provider only activates when it is very likely that we
        # want the results. So use high priority (negative is better).
        return -1000

class RlsRenameProvider(Ide.LspRenameProvider, Ide.RenameProvider):
    def do_load(self):
        RlsService.bind_client(self)

class RlsSymbolResolver(Ide.LspSymbolResolver, Ide.SymbolResolver):
    def do_load(self):
        RlsService.bind_client(self)

class RlsHighlighter(Ide.LspHighlighter, Ide.Highlighter):
    def do_load(self):
        RlsService.bind_client(self)

class RlsFormatter(Ide.LspFormatter, Ide.Formatter):
    def do_load(self):
        RlsService.bind_client(self)

class RlsHoverProvider(Ide.LspHoverProvider, Ide.HoverProvider):
    def do_prepare(self):
        self.props.category = 'Rust'
        self.props.priority = 200
        RlsService.bind_client(self)
