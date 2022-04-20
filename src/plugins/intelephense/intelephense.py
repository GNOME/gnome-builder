#!/usr/bin/env python3

import os
import gi

from gi.repository import GLib
from gi.repository import Gio
from gi.repository import GObject
from gi.repository import Ide

DEV_MODE = os.getenv('DEV_MODE') and True or False

class PhpService(Ide.LspService):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.set_program('intelephense')
        self.set_inherit_stderr(DEV_MODE)

    def do_configure_client(self, client):
        client.add_language('php')
        client.connect('load-configuration', self._on_load_configuration)
        client.connect('notification', self._on_notification)

    def do_configure_launcher(self, pipeline, launcher):
        launcher.push_argv('--stdio')

    def _on_load_configuration(self, data):
        try:
            d = GLib.Variant('a{sv}', {
                "intelephense": GLib.Variant('a{sv}', {
                    "files": GLib.Variant('a{sv}', {
                        "associations": GLib.Variant('as', ["*.php", "*.phtml"]),
                        "exclude": GLib.Variant('as', [])
                    }),
                    "completion": GLib.Variant('a{sv}', {
                        "insertUseDeclaration": GLib.Variant('b', True),
                        "fullyQualifyGlobalConstantsAndFunctions": GLib.Variant('b', False),
                        "triggerParameterHints": GLib.Variant('b', True),
                        "maxItems": GLib.Variant('i', 100)
                    }),
                    "format": GLib.Variant('a{sv}', {
                        "enable": GLib.Variant('b', True)
                    })
                })
            })

            return d
        except Error as e:
            Ide.critical ('On Load Configuration Error: {}'.format(e.message))
            return GLib.Variant ('a{sv}', {})

    def _on_notification(self, client, name, data):
        if name == "indexingStarted":
            if self.notif is not None:
                self.notif.withdraw()

            self.notif = Ide.Notification(
                id='org.gnome.builder.intelephense.indexing',
                title="Intelephense",
                body=_('Indexing php code…'),
                has_progress=True,
                progress_is_imprecise=True,
                progress=0.0)
            self.notif.attach(client.get_context())
        elif name == "indexingEnded":
            if self.notif is not None:
                self.notif.withdraw()

class PhpLspSymbolResolver(Ide.LspSymbolResolver):
    def do_load(self):
        PhpService.bind_client(self)

class PhpLspHoverProvider(Ide.LspHoverProvider):
    def do_prepare(self):
        self.props.category = 'PHP'
        self.props.priority = 300
        PhpService.bind_client(self)

class PhpLspFormatter(Ide.LspFormatter):
    def do_load(self):
        PhpService.bind_client(self)

class PhpLspDiagnosticProvider(Ide.LspDiagnosticProvider):
    def do_load(self):
        PhpService.bind_client(self)

class PhpLspCompletionProvider(Ide.LspCompletionProvider):
    def do_load(self):
        PhpService.bind_client(self)

class PhpLspHighlighter(Ide.LspHighlighter):
    def do_load(self):
        PhpService.bind_client(self)

class PhpLspRenameProvider(Ide.LspRenameProvider):
    def do_load(self):
        PhpService.bind_client(self)

class PhpLspCodeActionProvider(Ide.LspCodeActionProvider):
    def do_load(self):
        PhpService.bind_client(self)
