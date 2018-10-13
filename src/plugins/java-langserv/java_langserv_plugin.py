#!/usr/bin/env python

# java_langserv_plugin.py
#
# Copyright 2018 Daniel Buch Hansen <boogiewasthere@gmail.com>
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
This plugin provides integration with the Java Language Server.
It builds off the generic language service components in libide
by bridging them to our supervised Java Language Server.
"""

import gi
import os
import subprocess
import glob
import sys

gi.require_version('Ide', '1.0')

from gi.repository import GLib
from gi.repository import Gio
from gi.repository import GObject
from gi.repository import Ide

RUN_ON_HOST = True
SERVER_ROOT = '~/.jdt-language-server/'

DEV_MODE = True
"""
 Note: `tail -f SERVER_ROOT/.metadata/.log` where SERVER_ROOT is as above AND
       DEV_MODE = True, we will follow the server communication log
"""

class JavaService(Ide.Object, Ide.Service):
    _client = None
    _has_started = False
    _supervisor = None

    @GObject.Property(type=Ide.LangservClient)
    def client(self):
        return self._client

    @client.setter
    def client(self, value):
        self._client = value
        self.notify('client')

    def do_stop(self):
        """
        Stops the Java Language Server upon request to shutdown the
        JavaService.
        """
        if self._supervisor:
            supervisor, self._supervisor = self._supervisor, None
            supervisor.stop()

    """

    * We should probably introduce a log for language servers or something.
    * We could offload these messages to where `JavaStatusMessage()` is to be delivered.
      Above is a jdt language server extension (https://github.com/eclipse/eclipse.jdt.ls/wiki/Language-Server-Protocol-Extensions)
    """
    def log(self, message):
        if DEV_MODE:
            self.get_context().emit_log(GLib.LogLevelFlags.LEVEL_INFO, "[{service}] {msg}".format(msg=message, service=self.get_name()), -1)

    """

    * Maybe we want to chain async operations like communicate_utf8_async()
      and in the end start the server when/if all requirements are meet.
    * Also we probably need more error checking here, perhaps we could push this logig
      into ide_subprocess.{c, h} to hava a handy tool for checking oneline outputs of
      processes.

    """
    def check_output(self, args):
        launcher = Ide.SubprocessLauncher()
        launcher.set_run_on_host(RUN_ON_HOST)
        launcher.set_clear_env(False)
        launcher.set_flags(Gio.SubprocessFlags.STDOUT_PIPE | Gio.SubprocessFlags.STDERR_PIPE)
        launcher.push_args(args)
        process = launcher.spawn(None)
        return process.communicate_utf8(None, None).stdout_buf

    def locate_java(self):
        result = self.check_output(['which', 'java'])
        path = result.rstrip()
        self.log("Found OpenJdk at: {path}".format(path=path))
        return path

    def get_java_version(self, java_path):
        output = self.check_output([java_path, '--version'])
        version_string = output.split(" ")[1]
        version = int(version_string.split('.')[0])
        self.log("OpenJdk version: {version}".format(version=version))
        return int(version)

    def find_launcher(self, server_path):
        launcher_path = os.path.join(server_path, 'plugins')
        launchers = glob.glob(os.path.join(launcher_path,"org.eclipse.equinox.launcher_*.jar"))
        latest_launcher = max(launchers)
        path = os.path.join(launcher_path, latest_launcher)
        self.log("Found JDT launcher: {path}".format(path=path))
        return path

    def _ensure_started(self):
        """
        Start the java service which provides communication with the
        Java Language Server. We supervise our own instance of the
        language server and restart it as necessary using the
        Ide.SubprocessSupervisor.

        Various extension points (diagnostics, symbol providers, etc) use
        the JavaService to access the java components they need.
        """
        # To avoid starting the `jdtls` process unconditionally at startup,
        # we lazily start it when the first provider tries to bind a client
        # to its :client property.
        if not self._has_started:
            self._has_started = True

            # Setup a launcher to spawn the java language server
            flags = (Gio.SubprocessFlags.STDIN_PIPE |
                     Gio.SubprocessFlags.STDOUT_PIPE)

            if not DEV_MODE:
                flags |= Gio.SubprocessFlags.STDERR_SILENCE

            launcher = Ide.SubprocessLauncher()
            launcher.set_flags(flags)
            launcher.set_run_on_host(RUN_ON_HOST)
            launcher.set_clear_env(False)

            # Locate the directory of the project and run jdtls from there.
            workdir = self.get_context().get_vcs().get_working_directory()

            java_path = self.locate_java()
            launcher.push_argv(java_path)

            java_version = self.get_java_version(java_path)
            if java_version > 8:
                launcher.push_args([
                    '--add-modules=ALL-SYSTEM',
                    '--add-opens', 'java.base/java.util=ALL-UNNAMED',
                    '--add-opens', 'java.base/java.lang=ALL-UNNAMED'
                ])

            server_root = os.path.expanduser(SERVER_ROOT)
            launcher.set_cwd(server_root)
            launcher_path = self.find_launcher(server_root)

            launcher.push_args([
                '-Declipse.application=org.eclipse.jdt.ls.core.id1',
                '-Dosgi.bundles.defaultStartLevel=4',
                '-Declipse.product=org.eclipse.jdt.ls.core.product',
                "-Dlog.level={level}".format(level='ALL' if DEV_MODE else 'NONE'),
                "-Dlog.protocol={enabled}".format(enabled='true' if DEV_MODE else 'false'),
                '-noverify',
                '-Xmx1G',
                '-jar', launcher_path,
                '-configuration', os.path.join(server_root, 'config_linux'),
                '-data',server_root,
            ])

            # Spawn our peer process and monitor it for
            # crashes. We may need to restart it occasionally.
            self._supervisor = Ide.SubprocessSupervisor()
            self._supervisor.connect('spawned', self._jdtls_spawned)
            self._supervisor.set_launcher(launcher)
            self._supervisor.start()

    def _jdtls_spawned(self, supervisor, subprocess):
        """
        This callback is executed when the `jdtls` process is spawned.
        We can use the stdin/stdout to create a channel for our
        LangservClient.
        """
        stdin = subprocess.get_stdin_pipe()
        stdout = subprocess.get_stdout_pipe()
        io_stream = Gio.SimpleIOStream.new(stdout, stdin)

        if self._client:
            self._client.stop()
        self._client = Ide.LangservClient.new(self.get_context(), io_stream)
        self._client.add_language('java')
        self._client.start()
        self.notify('client')
        self.log("Jdt has spawned")

    @classmethod
    def bind_client(klass, provider):
        """
        This helper tracks changes to our client as it might happen when
        our `jdtls` process has crashed.
        """
        context = provider.get_context()
        self = context.get_service_typed(JavaService)
        self._ensure_started()
        self.bind_property('client', provider, 'client', GObject.BindingFlags.SYNC_CREATE)

class JavaDiagnosticProvider(Ide.LangservDiagnosticProvider):
    def do_load(self):
        JavaService.bind_client(self)

class JavaHighlighter(Ide.LangservHighlighter):
    def do_load(self):
        JavaService.bind_client(self)

class JavaFormatter(Ide.LangservFormatter):
    def do_load(self):
        JavaService.bind_client(self)

class JavaSymbolResolver(Ide.LangservSymbolResolver):
    def do_load(self):
        JavaService.bind_client(self)

class JavaCompletionProvider(Ide.LangservCompletionProvider):
    def do_load(self, context):
        JavaService.bind_client(self)

class JavaHoverProvider(Ide.LangservHoverProvider):
    def do_prepare(self):
        self.props.category = 'Java'
        self.props.priority = 200
        JavaService.bind_client(self)

    def do_get_priority(self, context):
        # This provider only activates when it is very likely that we
        # want the results. So use high priority (negative is better).
        return -1000

class JavaRenameProvider(Ide.LangservRenameProvider):
    def do_load(self):
        JavaService.bind_client(self)

"""
TODO: Implement language server extension whos job is to deliver
      `language/status` messages to be shown in ui

class JavaStatusMessage(Ide.LangservStatusMessage):
    def do_load(self):
        JavaService.bind_client(self)
"""
