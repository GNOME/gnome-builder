#!/usr/bin/env python3

import os
import gi

from gi.repository import GLib
from gi.repository import Gio
from gi.repository import GObject
from gi.repository import Ide

DEV_MODE = os.getenv('DEV_MODE') and True or False

class PhpService(Ide.Object):
    _client = None
    _has_started = False
    _supervisor = None
    _context = None
    notif = None

    @classmethod
    def from_context(klass, context):
        return context.ensure_child_typed(PhpService)

    @GObject.Property(type=Ide.LspClient)
    def client(self):
        return self._client

    @client.setter
    def client(self, value):
        self._client = value
        self.notify('client')

    @staticmethod
    def on_destroy(self):
        if self._supervisor:
            supervisor, self._supervisor = self._supervisor, None
            supervisor.stop()

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
            self.notif.attach(self._context)
        elif name == "indexingEnded":
            if self.notif is not None:
                self.notif.withdraw()

    def _get_runtime(self):
        config_manager = Ide.ConfigManager.from_context(self._context)
        config = config_manager.get_current()
        return config.get_runtime()

    def _ensure_started(self):
        # To avoid starting the process unconditionally at startup, lazily
        # start it when the first provider tries to bind a client to its
        # :client property.
        if not self._has_started:
            self._has_started = True

            launcher = self._create_launcher()
            launcher.set_clear_env(False)

            # Locate the directory of the project and run intelephense from there
            workdir = self.get_context().ref_workdir()
            launcher.set_cwd(workdir.get_path())

            launcher.push_argv("intelephense")
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
        self._client.connect('load-configuration', self._on_load_configuration)
        self._client.connect('notification', self._on_notification)
        self.append(self._client)
        self._client.add_language('php')
        self._client.start()
        self.notify('client')

    def _create_launcher(self):
        flags = Gio.SubprocessFlags.STDIN_PIPE | Gio.SubprocessFlags.STDOUT_PIPE
        if not DEV_MODE:
            flags |= Gio.SubprocessFlags.STDERR_SILENCE
        launcher = Ide.SubprocessLauncher()
        launcher.set_flags(flags)
        return launcher

    @classmethod
    def bind_client(klass, provider):
        context = provider.get_context()
        self = PhpService.from_context(context)
        self._context = context
        self._ensure_started()
        self.connect('destroy', PhpService.on_destroy)
        self.bind_property('client', provider, 'client', GObject.BindingFlags.SYNC_CREATE)

class PhpLspSymbolResolver(Ide.LspSymbolResolver, Ide.SymbolResolver):
    def do_load(self):
        PhpService.bind_client(self)

class PhpLspHoverProvider(Ide.LspHoverProvider):
    def do_prepare(self):
        self.props.category = 'PHP'
        self.props.priority = 300
        PhpService.bind_client(self)

class PhpLspFormatter(Ide.LspFormatter, Ide.Formatter):
    def do_load(self):
        PhpService.bind_client(self)

class PhpLspDiagnosticProvider(Ide.LspDiagnosticProvider, Ide.DiagnosticProvider):
   def do_load(self):
       PhpService.bind_client(self)

class PhpLspCompletionProvider(Ide.LspCompletionProvider, Ide.CompletionProvider):
    def do_load(self, context):
        PhpService.bind_client(self)

class PhpLspHighlighter(Ide.LspHighlighter, Ide.Highlighter):
    def do_load(self):
        PhpService.bind_client(self)

class PhpLspRenameProvider(Ide.LspRenameProvider, Ide.RenameProvider):
    def do_load(self):
        PhpService.bind_client(self)

class PhpLspCodeActionProvider(Ide.LspCodeActionProvider, Ide.CodeActionProvider):
    def do_load(self):
        PhpService.bind_client(self)
