#!/usr/bin/env python3

import os
import json
import gi

from gi.repository import GLib
from gi.repository import Gio
from gi.repository import GObject
from gi.repository import Ide

class BlueprintService(Ide.LspService):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.set_program('blueprint-compiler')

    def do_configure_client(self, client):
        client.add_language("blueprint")

    def do_configure_launcher(self, pipeline, launcher):
        launcher.push_argv('lsp')

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
