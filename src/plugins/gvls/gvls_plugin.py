#!/usr/bin/env python3

# gvls_plugin.py
#
# Copyright 2016 Christian Hergert <chergert@redhat.com>
# Copyright 2019-2022 Daniel Espinosa <esodan@gmail.com>
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

from gi.repository import GLib
from gi.repository import Gio
from gi.repository import GObject
from gi.repository import Ide

DEV_MODE = True

class GVlsService(Ide.LspService):
    def __init__(self, *args, **kwargs):
        super().__init__(self, *args, **kwargs)
        self.set_program('org.gnome.gvls.stdio.Server')
        self.set_inherit_stderr(DEV_MODE)

    def do_configure_client(self, client):
        client.add_language('vala')

class GVlsDiagnosticProvider(Ide.LspDiagnosticProvider, Ide.DiagnosticProvider):
    def do_load(self):
        GVlsService.bind_client(self)

class GVlsCompletionProvider(Ide.LspCompletionProvider, Ide.CompletionProvider):
    def do_load(self, context):
        GVlsService.bind_client(self)

    def do_get_priority(self, context):
        # This provider only activates when it is very likely that we
        # want the results. So use high priority (negative is better).
        return -900

class GVlsHighlighter(Ide.LspHighlighter, Ide.Highlighter):
    def do_load(self):
        GVlsService.bind_client(self)

class GVlsSymbolResolver(Ide.LspSymbolResolver, Ide.SymbolResolver):
    def do_load(self):
        GVlsService.bind_client(self)

class GVlsHoverProvider(Ide.LspHoverProvider, Ide.HoverProvider):
    def do_prepare(self):
        self.props.category = 'Vala'
        self.props.priority = 200
        GVlsService.bind_client(self)

class GVlsFormatter(Ide.LspFormatter, Ide.Formatter):
    def do_load(self):
        GVlsService.bind_client(self)

