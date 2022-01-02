#!/usr/bin/env python3
#
# clangd_plugin.py
#
# Copyright 2022 Christian Hergert <chergert@redhat.com>
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

from gi.repository import Ide

# Clangd creates a .cache directory within the project
Ide.g_file_add_ignored_pattern('.cache')

class ClangdService(Ide.LspService):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.set_program('clangd')

    def do_configure_client(self, client):
        client.add_language('c')
        client.add_language('cpp')
        client.add_language('objective-c')
        client.add_language('objective-cpp')

class ClangdDiagnosticProvider(Ide.LspDiagnosticProvider, Ide.DiagnosticProvider):
    def do_load(self):
        ClangdService.bind_client(self)

class ClangdCompletionProvider(Ide.LspCompletionProvider, Ide.CompletionProvider):
    def do_load(self, context):
        ClangdService.bind_client(self)

    def do_get_priority(self, context):
        return -1000

class ClangdSymbolResolver(Ide.LspSymbolResolver, Ide.SymbolResolver):
    def do_load(self):
        ClangdService.bind_client(self)

class ClangdHighlighter(Ide.LspHighlighter, Ide.Highlighter):
    def do_load(self):
        ClangdService.bind_client(self)

class ClangdFormatter(Ide.LspFormatter, Ide.Formatter):
    def do_load(self):
        ClangdService.bind_client(self)

class ClangdHoverProvider(Ide.LspHoverProvider, Ide.HoverProvider):
    def do_prepare(self):
        self.props.category = 'Clangd'
        self.props.priority = 200
        ClangdService.bind_client(self)

class ClangdRenameProvider(Ide.LspRenameProvider, Ide.RenameProvider):
    def do_load(self):
        ClangdService.bind_client(self)

class ClangdCodeActionProvider(Ide.LspCodeActionProvider, Ide.CodeActionProvider):
    def do_load(self):
        ClangdService.bind_client(self)

