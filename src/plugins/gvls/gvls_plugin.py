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

import gi
import os

from gi.repository import GLib
from gi.repository import Gio
from gi.repository import GObject
from gi.repository import Ide
from gi.repository import Gdk
from gi.repository import Gtk

DEV_MODE = True

class GVlsService(Ide.Object):
    _client = None
    _has_started = False
    _supervisor = None
    _monitor = None

    @classmethod
    def from_context(klass, context):
        return context.ensure_child_typed(GVlsService)

    @GObject.Property(type=Ide.LspClient)
    def client(self):
        return self._client

    @client.setter
    def client(self, value):
        self._client = value
        ## Construct GVls Configuration response at `initialize`
        self._client.connect('load-configuration', self._on_load_configuration)
        self.notify('client')

    def do_parent_set(self, parent):
        """
        No useful for VLS
        """
        if parent is None:
            return

        context = self.get_context()
        workdir = context.ref_workdir()


    def do_stop(self):
        """
        Stops the Vala Language Server upon request to shutdown the
        GVlsService.
        """
        if self._client is not None:
            print ("Shutting down server")
            _client.stop()
            _client.destroy()

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

    def _on_load_configuration(self):
        if DEV_MODE:
            print ("Creating Vala Language Server Configuration response")

        conf = GLib.Variant('a{sv}', {
            'initialized': GLib.Variant.new_boolean(True),
            'defaultNamespaces': GLib.Variant.new_boolean(True),
            'defaultVapiDirs': GLib.Variant.new_boolean(True),
            'scanWorkspace': GLib.Variant.new_boolean(True),
            'addUsingNamespaces': GLib.Variant.new_boolean(True),
        })


        # pkgs = GLib.VariantBuilder (GLib.Variant ('av'))
        # Get from build environment
        # pkgs.add ('v', GLib.Variant.string ())

        # vargs = GLib.VariantBuilder (GLib.Variant ('av'))
        # Get from build environment
        # vargs.add ('v', GLib.Variant.string ())

        # opts = GLib.VariantBuilder (GLib.Variant ('av'))
        # Get from build environment
        # vargs.add ('v', GLib.Variant.string ())

        # conf.add ('{sv}', 'packages', pkgs)
        # conf.add ('{sv}', 'valaArgs', vargs)
        # conf.add ('{sv}', 'options', opts)

        if DEV_MODE:
            print ("Configuration response: "+conf.print(True))

        return conf

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
        self.append(self._client)
        self._client.add_language('vala')
        self._client.start()
        self.notify('client')

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
        launcher.set_run_on_host(True)
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

#class GVlsDiagnosticProvider(Ide.LspDiagnosticProvider):
#    def do_load(self):
#        GVlsService.bind_client(self)

class GVlsCompletionProvider(Ide.LspCompletionProvider):
    def do_load(self, context):
        GVlsService.bind_client(self)

    def do_get_priority(self, context):
        # This provider only activates when it is very likely that we
        # want the results. So use high priority (negative is better).
        return -1000

class GVlsHighlighter(Ide.LspHighlighter):
    def do_load(self):
        GVlsService.bind_client(self)

class GVlsSymbolResolver(Ide.LspSymbolResolver):
    def do_load(self):
        GVlsService.bind_client(self)


class GVlsDiagnosticProvider(Ide.LspDiagnosticProvider):
    def do_load(self):
        GVlsService.bind_client(self)

#
# Stolen from src/plugins/vala-pack/ide-vala-identer.vala
# Copyright 2015 Christian Hergert <christian@hergert.me>
#
class GVlsIdenter (Ide.Object, Ide.Indenter):
    def is_trigger (self, evkey):
        val = evkey.get_keyval ()
        if val == Gdk.KEY_Return or val == Gdk.KEY_KP_Enter:
            return True
        if val == Gdk.KDEY_slash:
            return True
        return False
    def format (self, textView, begin, end, evkey):
        source_view = text_view
        was_newline = self.is_newline_keyval (evkey.keyval)
        copy = end
        cursor_offset = 0
        # Move us back to the just inserted character
        copy.backward_char ()
        # If we are in a comment, continue the indentation
        if (in_comment (text_view, copy)):
            # maybe close a multiline comment
            if (copy.get_char () == '/'):
                close = copy
            if (close.backward_char () and close.get_char () == ' ' and close.backward_char () and close.get_char () == '*'):
                begin.backward_char ();
                begin.backward_char ();
                return ("/", cursor_offset);
        if (was_newline):
            return indent_comment (text_view, copy);
        if (is_newline_in_braces (copy)):
            prefix = copy_indent (text_view, copy);
            if (source_view.insert_spaces_instead_of_tabs):
                indent = "    "
            else:
                indent = "\t"
            cursor_offset = -prefix.length - 1
            return (prefix + indent + "\n" + prefix, cursor_offset)
        if (was_newline):
            return (copy_indent (text_view, copy), cursor_offset)
        return null
    def copy_indent (self, text_view, iter):
        begin = iter
        begin.set_line_offset (0)
        end = begin
        while (not end.ends_line () and end.get_char ().isspace () and end.forward_char ()):
            # Do nothing
            pass
        return begin.get_slice (end)
    def get_line_text (self, iter):
        begin = iter
        end = iter
        begin.set_line_offset (0)
        if (not end.ends_line ()):
            end.forward_to_line_end ()
        return begin.get_slice (end)
    def indent_comment (self, text_view, iter):
        line = get_line_text (iter).strip ();
        # continue with another single line comment
        if line.has_prefix ("//"):
            return copy_indent (text_view, iter) + "// "
        # comment is closed, copy indent, possibly trimming extra space
        if (line.has_suffix ("*/")):
            if (line.has_prefix ("*")):
                s = GLib.String.new (copy_indent (text_view, iter))
                if (str(s).endswith (" ")):
                    s.truncate (len (s) - 1)
                return str(s);
        if (line.endswith ("/*") and not line.endswith ("*/")):
            return copy_indent (text_view, iter) + " * "
        elif (line.has_prefix ("*")):
            return copy_indent (text_view, iter) + "* "
        return copy_indent (text_view, iter)
    def in_comment (self, text_view, iter):
        buffer = text_view.buffer
        copy = iter
        copy.backward_char ()
        return buffer.iter_has_context_class (copy, "comment")
    def is_newline_keyval (self, keyval):
        if keyval == Gdk.KEY_Return or keyval == Gdk.KEY_KP_Enter:
            return true
        return false
    def is_newline_in_braces (self, iter):
        prev = iter
        next = iter
        prev.backward_char ()
        next.forward_char ()
        ret = (prev.get_char () == '{') and (iter.get_char () == '\n')
        return ret and (next.get_char () == '}')

