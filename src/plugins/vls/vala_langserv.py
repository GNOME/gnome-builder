
#!/usr/bin/env python

#   vala_langserv.py
#
# Copyright 2016 Christian Hergert <chergert@redhat.com>
# Copyright 2020 Princeton Ferro <princetonferro@gmail.com>
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

from gi.repository import GLib
from gi.repository import Gio
from gi.repository import GObject
from gi.repository import Ide

DEV_MODE = os.getenv('DEV_MODE') and True or False

class VlsService(Ide.LspService):
    def __init__(self, *args, **kwargs):
        super().__init__(self, *args, **kwargs)
        self.set_program('vala-language-server')
        self.set_inherit_stderr(DEV_MODE)

    def do_configure_client(self, client):
        client.add_language('vala')

class VlsDiagnosticProvider(Ide.LspDiagnosticProvider):
    def do_load(self):
        VlsService.bind_client(self)

class VlsCompletionProvider(Ide.LspCompletionProvider):
    def do_load(self, context):
        VlsService.bind_client(self)

    def do_get_priority(self, context):
        # This provider only activates when it is very likely that we
        # want the results. So use high priority (negative is better).
        return -1000

class VlsRenameProvider(Ide.LspRenameProvider):
    def do_load(self):
        VlsService.bind_client(self)

class VlsSymbolResolver(Ide.LspSymbolResolver):
    def do_load(self):
        VlsService.bind_client(self)

class VlsHighlighter(Ide.LspHighlighter):
    def do_load(self):
        VlsService.bind_client(self)

class VlsFormatter(Ide.LspFormatter):
    def do_load(self):
        VlsService.bind_client(self)

class VlsHoverProvider(Ide.LspHoverProvider):
    def do_prepare(self):
        self.props.category = 'Vala'
        self.props.priority = 100
        VlsService.bind_client(self)

class VlsSearchProvider(Ide.LspSearchProvider):
    def do_load(self, context):
        if not context.has_project():
            return

        build_system = Ide.BuildSystem.from_context(context)
        if not build_system.supports_language('vala'):
            return

        VlsService.bind_client_lazy(self)
