#!/usr/bin/env python3

import os
import json
import gi

from gi.repository import GLib
from gi.repository import Gio
from gi.repository import GObject
from gi.repository import Ide

DEV_MODE = os.getenv('DEV_MODE') and True or False

class GoService(Ide.LspService):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.set_program('gopls')
        self.set_inherit_stderr(DEV_MODE)
        self.set_search_path([os.path.expanduser("~/go/bin")])

    def do_configure_launcher(self, pipeline, launcher):
        if DEV_MODE:
            launcher.push_argv('-debug')
        launcher.push_argv('serve')

        # Bash will load the host $PATH and $GOPATH (and optionally $GOROOT)
        # for us.  This does mean there will be a possible .bashrc vs
        # .bash_profile discrepancy. Possibly there is a better native way to
        # make sure that builder running in flatpak can run processes in the
        # host context with the host's $PATH.
        argv = launcher.get_argv()
        quoted = ' '.join([GLib.shell_quote(arg) for arg in argv])
        launcher.set_argv(["/bin/bash", "--login", "-c", quoted])

    def do_configure_client(self, client):
        client.add_language('go')

class GoSymbolResolver(Ide.LspSymbolResolver, Ide.SymbolResolver):
    def do_load(self):
        GoService.bind_client(self)

class GoHoverProvider(Ide.LspHoverProvider):
    def do_prepare(self):
        self.props.category = 'Go'
        self.props.priority = 100
        GoService.bind_client(self)

class GoCompletionProvider(Ide.LspCompletionProvider, Ide.CompletionProvider):
    def do_load(self, context):
        GoService.bind_client(self)
