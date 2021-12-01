#!/usr/bin/env python3

import os
import json
import gi

from gi.repository import GLib
from gi.repository import Gio
from gi.repository import GObject
from gi.repository import Ide

class BlueprintService(Ide.LspService):
    def do_constructed(self):
        self.set_inherit_stderr(True)

    def do_configure_client(self, client):
        client.add_language("blueprint")

    def do_configure_launcher(self, launcher):
        launcher.set_argv(["blueprint-compiler", "lsp"])


class BlueprintDiagnosticProvider(Ide.LspDiagnosticProvider, Ide.DiagnosticProvider):
    def do_load(self):
        BlueprintService.bind_client(self)

class BlueprintCompletionProvider(Ide.LspCompletionProvider, Ide.CompletionProvider):
    def do_load(self, context):
        BlueprintService.bind_client(self)

class BlueprintHoverProvider(Ide.LspHoverProvider):
    def do_prepare(self):
        self.props.priority = 100
        BlueprintService.bind_client(self)

class BlueprintCodeActionProvider(Ide.LspCodeActionProvider, Ide.CodeActionProvider):
    def do_load(self):
        BlueprintService.bind_client(self)
