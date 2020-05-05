#!/usr/bin/env python

# gvls_plugin.py
#
# Copyright 2016 Christian Hergert <chergert@redhat.com>
# Copyright 2019 Daniel Espinosa <esodan@gmail.com>
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

class GVlsService(Ide.Object):
    _client = None
    _has_started = False
    _supervisor = None
    _monitor = None
    meson_build_system = True
    initialized = True
    default_namespaces = True
    default_vapi_dirs = True
    scan_work_space = True
    add_using_namespaces = True
    library_vapidir = ""
    system_vapidir = ""
    files = []
    packages = []
    vala_args = {}
    options = []
    vala_api_version = ""
    build_system = None
    pipeline = None
    build_args = None
    ide_config = None
    source_file = None
    build_monitor = None
    meson_compile_commands = ""

    @classmethod
    def from_context(klass, context):
        return context.ensure_child_typed(GVlsService)

    @GObject.Property(type=Ide.LspClient)
    def client(self):
        return self._client

    @client.setter
    def client(self, value):
        self._client = value
        self.notify('client')

    def do_parent_set(self, parent):
        """
        No useful for VLS
        """
        if parent is None:
            return

    def do_stop(self):
        """
        Stops the Vala Language Server upon request to shutdown the
        GVlsService.
        """
        if self._client is not None:
            Ide.warning ("Shutting down server")
            self._client.stop()
            self._client.destroy()

        if self._supervisor is not None:
            supervisor, self._supervisor = self._supervisor, None
            supervisor.stop()

    def _ensure_started(self):
        """
        Start the Vala service which provides communication with the
        Vala Language Server. We supervise our own instance of the
        language server and restart it as necessary using the
        Ide.SubprocessSupervisor.

        Various extension points (diagnostics, symbol providers, etc) use
        the GVlsService to access the rust components they need.
        """
        # To avoid starting the `gvls` process unconditionally at startup,
        # we lazily start it when the first provider tries to bind a client
        # to its :client property.
        if not self._has_started:
            self._has_started = True

            # Setup a launcher to spawn the rust language server
            launcher = self._create_launcher()
            launcher.set_clear_env(False)
            # Locate the directory of the project and run gvls from there.
            workdir = self.get_context().ref_workdir()
            launcher.set_cwd(workdir.get_path())

            # If org.gnome.gvls.stdio.Server is installed by GVls
            path = 'org.gnome.gvls.stdio.Server'

            # Setup our Argv. We want to communicate over STDIN/STDOUT,
            # so it does not require any command line options.
            launcher.push_argv(path)

            # Spawn our peer process and monitor it for
            # crashes. We may need to restart it occasionally.
            self._supervisor = Ide.SubprocessSupervisor()
            self._supervisor.connect('spawned', self._gvls_spawned)
            self._supervisor.set_launcher(launcher)
            self._supervisor.start()

    def list_to_variant(self, l):
        b = GLib.VariantBuilder(GLib.VariantType.new('av'))
        for s in l:
            v = GLib.Variant.new_string (s)
            b.add_value(GLib.Variant.new_variant(v))
        return b.end()

    def dict_to_array_variant(self, d):
        b = GLib.VariantBuilder(GLib.VariantType.new('av'))
        for k in d.keys():
            a = d[k]
            b2 = GLib.VariantBuilder(GLib.VariantType.new('a{sv}'))
            ndi = GLib.Variant.new_string ('name')
            vndi = GLib.Variant.new_variant(GLib.Variant.new_string(k))
            di = GLib.Variant.new_dict_entry(ndi, vndi)
            vdi = GLib.Variant.new_variant(di)
            b2.add_value(di)
            vdi = GLib.Variant.new_string ('value')
            vvdi = GLib.Variant.new_variant(GLib.Variant.new_string(a))
            div = GLib.Variant.new_dict_entry(vdi, vvdi)
            vdi = GLib.Variant.new_variant(div)
            b2.add_value(div)
            b.add_value(GLib.Variant.new_variant(b2.end()))
        return b.end()
    
    def create_dict_entry_boolean(self, key, val):
        vk = GLib.Variant.new_string (key)
        vv = GLib.Variant.new_variant(GLib.Variant.new_boolean(val))
        return GLib.Variant.new_dict_entry(vk, vv)

    def create_dict_entry_string(self, key, val):
        vk = GLib.Variant.new_string (key)
        vv = GLib.Variant.new_variant(GLib.Variant.new_string(val))
        return GLib.Variant.new_dict_entry(vk, vv)

    def create_configuration_variant(self):
        try:
            b = GLib.VariantBuilder(GLib.VariantType.new('a{sv}'))
            b.add_value(self.create_dict_entry_boolean('initialized', self.initialized))
            b.add_value(self.create_dict_entry_boolean('defaultNamespaces', self.default_namespaces))
            b.add_value(self.create_dict_entry_boolean('defaultVapiDirs', self.default_vapi_dirs))
            b.add_value(self.create_dict_entry_boolean('scanWorkspace', self.scan_work_space))
            b.add_value(self.create_dict_entry_boolean('addUsingNamespaces', self.add_using_namespaces))
            b.add_value(self.create_dict_entry_boolean('mesonBuildSystem', self.meson_build_system))
            b.add_value(self.create_dict_entry_string('libraryVapi', self.library_vapidir))
            b.add_value(self.create_dict_entry_string('systemVapi', self.system_vapidir))
            b.add_value(self.create_dict_entry_string('valaApiVersion', self.vala_api_version))
            b.add_value(self.create_dict_entry_string('mesonCompileCommands', self.meson_compile_commands))
            ad = GLib.Variant.new_string ('valaArgs')
            vadi = self.dict_to_array_variant(self.vala_args)
            adi = GLib.Variant.new_dict_entry(ad, GLib.Variant.new_variant (vadi))
            b.add_value(adi)
            od = GLib.Variant.new_string ('options')
            vodi = self.list_to_variant(self.options)
            odi = GLib.Variant.new_dict_entry(od, GLib.Variant.new_variant (vodi))
            b.add_value(odi)
            pd = GLib.Variant.new_string ('packages')
            vpdi = self.list_to_variant(self.packages)
            pdi = GLib.Variant.new_dict_entry(pd, GLib.Variant.new_variant (vpdi))
            b.add_value(pdi)
            fd = GLib.Variant.new_string ('files')
            vfdi = self.list_to_variant(self.files)
            fdi = GLib.Variant.new_dict_entry(fd, GLib.Variant.new_variant (vfdi))
            b.add_value(fdi)
            return GLib.Variant.new_variant (b.end())
        except Error as e:
            Ide.debug ('On Load Configuration Error: {}'.format(e.message))
            return GLib.Variant ('a{sv}', {})
    
    def _build_config_changed(self, obj, mfile, ofile, event_type):
        if event_type == Gio.FileMonitorEvent.CHANGED or event_type == Gio.FileMonitorEvent.CREATED:
            self._parse_build_commands()
            self._notify_change_configuration()
        if event_type == Gio.FileMonitorEvent.DELETED:
            self.build_monitor = None

    def read_meson_compile_commands(self, file):
        ostream = Gio.MemoryOutputStream.new_resizable()
        ostream.splice(file.read(), Gio.OutputStreamSpliceFlags.CLOSE_SOURCE, None)
        ostream.close()
        b = ostream.steal_as_bytes()
        self.meson_compile_commands = str(b.get_data(),encoding='utf8')
    
    def _parse_build_commands(self):
        try:
            self.build_args = []
            ctx = self._client.ref_context()
            buildmgr = Ide.BuildManager.from_context (ctx)
            self.pipeline = buildmgr.get_pipeline ()
            if self.pipeline != None:
                if self.pipeline.has_configured():
                    bcdir = Gio.File.new_for_path(self.pipeline.get_builddir())
                    if self.meson_build_system:
                        bcf = Gio.File.new_for_uri(bcdir.get_uri()+'/compile_commands.json')
                        if not bcf.query_exists(None):
                            return
                        self.read_meson_compile_commands(bcf)
                        if self.build_monitor == None:
                            self.build_monitor = bcf.monitor(Gio.FileMonitorFlags.NONE, None)
                            self.build_monitor.connect('changed', self._build_config_changed)
                        cc = Ide.CompileCommands.new()
                        cc.load (bcf)
                        commands = cc.lookup (self.source_file, '')
                        if commands != None:
                            self.build_args = commands[0]
            
            self.files = []
            self.packages = []
            self.vala_args = {}
            self.options = []
            found_package = False
            found_arg = False
            arg_name = ''
            for s in self.build_args:
                if found_package:
                    self.packages += [s]
                    found_package = False
                    continue
                if found_arg:
                    self.vala_args[arg_name] = s
                    found_arg = False
                    continue
                if s == '--pkg' or s == 'pkg':
                    found_package = True
                if s.startswith('-') and not (s == '--pkg' or s == 'pkg'):
                    if s.startswith('--version'):
                        continue
                    if s.startswith('--api-version'):
                        continue
                    if s == '-C' or s == '--ccode':
                        self.vala_args[s] = ''
                        continue
                    if s == '--use-header':
                        self.vala_args[s] = ''
                        continue
                    if s == '--fast-vapi':
                        self.vala_args[s] = ''
                        continue
                    if s == '--use-fast-vapi':
                        self.vala_args[s] = ''
                        continue
                    if s == '--vapi-comments':
                        self.vala_args[s] = ''
                        continue
                    if s == '--deps':
                        self.vala_args[s] = ''
                        continue
                    if s == '-c' or s == '--compile':
                        self.vala_args[s] = ''
                        continue
                    if s == '-g' or s == '--debug':
                        self.vala_args[s] = ''
                        continue
                    if s == '--enable-mem-profiler':
                        self.vala_args[s] = ''
                        continue
                    if s == '--nostdpkg':
                        self.vala_args[s] = ''
                        continue
                    if s == '--disable-assert':
                        self.vala_args[s] = ''
                        continue
                    if s == '--enable-checking':
                        self.vala_args[s] = ''
                        continue
                    if s == '--enable-deprecated':
                        self.vala_args[s] = ''
                        continue
                    if s == '--hide-internal':
                        self.vala_args[s] = ''
                        continue
                    if s == '--enable-experimental':
                        self.vala_args[s] = ''
                        continue
                    if s == '--disable-warnings':
                        self.vala_args[s] = ''
                        continue
                    if s == '--fatal-warnings':
                        self.vala_args[s] = ''
                        continue
                    if s == '--disable-since-check':
                        self.vala_args[s] = ''
                        continue
                    if s == '--enable-experimental-non-null':
                        self.vala_args[s] = ''
                        continue
                    if s == '--enable-gobject-tracing':
                        self.vala_args[s] = ''
                        continue
                    if s == '--save-temps':
                        self.vala_args[s] = ''
                        continue
                    if s == '-q' or s == '--quiet':
                        self.vala_args[s] = ''
                        continue
                    if s == '-v' or s == '--verbose':
                        self.vala_args[s] = ''
                        continue
                    if s == '--no-color':
                        self.vala_args[s] = ''
                        continue
                    if s == '--enable-version-header':
                        self.vala_args[s] = ''
                        continue
                    if s == '--disable-version-header':
                        self.vala_args[s] = ''
                        continue
                    if s == '--run-args':
                        self.vala_args[s] = ''
                        continue
                    if s == '--abi-stability':
                        self.vala_args[s] = ''
                        continue
                    if '=' in s:
                        ps = s.split('=')
                        if len(ps) == 2:
                            self.vala_args[ps[0]] = ps[1]
                        continue
                    found_arg = True
                    arg_name = s
                    continue
        except BaseException as exc:
            Ide.debug('Parse Build Commands Error: {}'.format(exc.args))

    def _did_change_configuration(self, source_object, result, user_data):
        try:
            self._client.send_notification_finish(result)
        except BaseException as exc:
            Ide.debug('Change Configuration Notification error: {}'.format(exc.args))
    
    def _notify_change_configuration(self):
        try:
            vconf = self.create_configuration_variant(None)
            cancellable = self._client.ref_cancellable()
            b = GLib.VariantBuilder(GLib.VariantType.new('a{sv}'))
            vk = GLib.Variant.new_string ('settings')
            vv = GLib.Variant.new_variant(GLib.Variant.new_variant(vconf))
            de = GLib.Variant.new_dict_entry(vk, vv)
            b.add_value(de)
            vnotify = GLib.Variant.new_variant(b.end())
            self._client.send_notification_async("workspace/didChangeConfiguration", vnotify, cancellable, self._did_change_configuration, None)
        except BaseException as exc:
            Ide.debug('Notify change configuration error: {}'.format(exc.args))
    
    def _on_load_configuration(self, data):
        ctx = self._client.get_context()
        bufm = Ide.BufferManager.from_context(ctx)
        for i in range(bufm.get_n_items()):
            buf = bufm.get_item(i)
            f = buf.get_file()
            if f.get_path().endswith('.vala'):
                self.source_file = f
                self._parse_build_commands()
                break
        return self.create_configuration_variant()
    
    def _on_pipeline_diagnostic(self, obj, diagnostic):
        try:
            self.source_file = diagnostic.get_file()
        except BaseException as exc:
            Ide.debug('On Pipeline Loaded start get build flags error: {}'.format(exc.args))

    def on_get_vala_data_dir(self, vdp, cancellable, data):
        try:
            if self.pipeline == None:
                return
            rt = self.pipeline.get_runtime()
            if rt == None:
                return
            vdpio = vdp.get_stdout_pipe()
            ddpp = Gio.DataInputStream.new(vdpio)
            ldp = ddpp.read_line()
            fgdvapi = Gio.File.new_for_path(str(ldp[0],encoding='utf8'))
            rtfgdvapi = rt.translate_file(fgdvapi)
            self.system_vapidir = rtfgdvapi.get_uri() + "/vala/vapi"
        except BaseException as exc:
            Ide.debug('On get Vala DATA VAPI DIR: {}'.format(str(exc)))
    
    def on_get_vapidir(self, vpkgp, cancellable, data):
        try:
            if self.pipeline == None:
                return
            rt = self.pipeline.get_runtime()
            if rt == None:
                return
            vpstdio = vpkgp.get_stdout_pipe()
            dpp = Gio.DataInputStream.new(vpstdio)
            lp = dpp.read_line()
            flvapi = Gio.File.new_for_path(str(lp[0],encoding='utf8'))
            rtfvapi = rt.translate_file(flvapi)
            self.library_vapidir = rtfvapi.get_uri()
            flags = Gio.SubprocessFlags.STDOUT_PIPE
            launcher = self.pipeline.create_launcher()
            if launcher == None:
                return
            launcher.set_cwd(GLib.get_home_dir())
            launcher.set_flags(flags)
            launcher.push_args (['pkg-config','--variable','datadir','libvala-'+self.vala_api_version])
            vdp = launcher.spawn(None)
            vdp.wait_async(None, self.on_get_vala_data_dir, None)
        except BaseException as exc:
            Ide.debug('On get Vala VAPI DIR: {}'.format(str(exc)))
    
    def on_get_vala_api_version(self, valacp, cancellable, data):
        try:
            vstdio = valacp.get_stdout_pipe()
            dp = Gio.DataInputStream.new(vstdio)
            l = dp.read_line()
            self.vala_api_version = str(l[0],encoding='utf8')
            if self.pipeline == None:
                return
            rt = self.pipeline.get_runtime()
            if rt == None:
                return
            flags = Gio.SubprocessFlags.STDOUT_PIPE
            launcher = self.pipeline.create_launcher()
            if launcher == None:
                return
            launcher.set_cwd(GLib.get_home_dir())
            launcher.set_flags(flags)
            launcher.push_args (['pkg-config','--variable','vapidir','libvala-'+self.vala_api_version])
            vpkgp = launcher.spawn(None)
            vpkgp.wait_async(None, self.on_get_vapidir, None)
        except BaseException as exc:
            Ide.debug('On get Vala API VERSION Runtime Configuration: {}'.format(str(exc)))
    
    def _update_config_from_runtime(self):
        try:
            if self.pipeline == None:
                return
            rt = self.pipeline.get_runtime()
            if rt == None:
                return
            flags = Gio.SubprocessFlags.STDOUT_PIPE
            launcher = self.pipeline.create_launcher()
            if launcher == None:
                return
            launcher.set_cwd(GLib.get_home_dir())
            launcher.set_flags(flags)
            launcher.push_args (['valac','--api-version'])
            valacp = launcher.spawn(None)
            valacp.wait_async(None, self.on_get_vala_api_version, None)
        except BaseException as exc:
            Ide.debug('On Update Runtime Configuration: {}'.format(str(exc)))
    
    def on_config_changed_cb(self, data):
        try:
            ctx = self._client.ref_context()
            buildmgr = Ide.BuildManager.from_context (ctx)
            self.pipeline = buildmgr.get_pipeline ()
            self.pipeline.connect('diagnostic', self._on_pipeline_diagnostic)
        except BaseException as exc:
            Ide.debug('On Config Changed CB: {}'.format(str(exc)))

    def _gvls_spawned(self, supervisor, subprocess):
        """
        This callback is executed when the `org.gnome.gvls.stdio.Server` process is spawned.
        We can use the stdin/stdout to create a channel for our
        LspClient.
        """
        stdin = subprocess.get_stdin_pipe()
        stdout = subprocess.get_stdout_pipe()
        io_stream = Gio.SimpleIOStream.new(stdout, stdin)

        if self._client:
            self._client.stop()
            self._client.destroy()

        self._client = Ide.LspClient.new(io_stream)
        self._client.connect('load-configuration', self._on_load_configuration)
        self.append(self._client)
        self._client.add_language('vala')
        self._client.start()
        self.notify('client')
        try:
            ctx = self._client.ref_context()
            self.build_system = Ide.BuildSystem.from_context (ctx)
            if self.build_system.get_id () == 'meson':
                self.meson_build_system = True
            else:
                self.meson_build_system = False
            buildmgr = Ide.BuildManager.from_context (ctx)
            self.pipeline = buildmgr.get_pipeline ()
            self.pipeline.connect('diagnostic', self._on_pipeline_diagnostic)
            cfgmgr = Ide.ConfigManager.from_context(ctx)
            cfgmgr.connect('notify::current', self.on_config_changed_cb)
            self._update_config_from_runtime()
        except BaseException as exc:
            Ide.debug('Exception Arguments: {}'.format(exc.args))

    def _create_launcher(self):
        """
        Creates a launcher to be used by the vala service.

        In the future, we might be able to rely on the runtime for
        the tooling. Maybe even the program if flatpak-builder has
        prebuilt our dependencies.
        """
        flags = Gio.SubprocessFlags.STDIN_PIPE | Gio.SubprocessFlags.STDOUT_PIPE
        if not DEV_MODE:
            flags |= Gio.SubprocessFlags.STDERR_SILENCE
        launcher = Ide.SubprocessLauncher()
        launcher.set_flags(flags)
        launcher.set_cwd(GLib.get_home_dir())
        return launcher

    @classmethod
    def bind_client(klass, provider):
        """
        This helper tracks changes to our client as it might happen when
        our `org.gnome.gvls.Server` process has crashed.
        """
        context = provider.get_context()
        self = GVlsService.from_context(context)
        self._ensure_started()
        self.bind_property('client', provider, 'client', GObject.BindingFlags.SYNC_CREATE)

class GVlsDiagnosticProvider(Ide.LspDiagnosticProvider, Ide.DiagnosticProvider):
    def do_load(self):
        GVlsService.bind_client(self)

class GVlsCompletionProvider(Ide.LspCompletionProvider, Ide.CompletionProvider):
    def do_load(self, context):
        GVlsService.bind_client(self)

    def do_get_priority(self, context):
        # This provider only activates when it is very likely that we
        # want the results. So use high priority (negative is better).
        return -1000

class GVlsHighlighter(Ide.LspHighlighter, Ide.Highlighter):
    def do_load(self):
        GVlsService.bind_client(self)

class GVlsSymbolResolver(Ide.LspSymbolResolver, Ide.SymbolResolver):
    def do_load(self):
        GVlsService.bind_client(self)

