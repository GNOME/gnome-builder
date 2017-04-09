#!/usr/bin/env python3

#
# gdb_plugin.py
#
# Copyright (C) 2017 Christian Hergert <chris@dronelabs.com>
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
import os

gi.require_version('Mi2', '1.0')

from gi.repository import GLib
from gi.repository import GObject
from gi.repository import Gio
from gi.repository import Ide
from gi.repository import Mi2

class GdbDebugger(Ide.Object, Ide.Debugger):
    can_step_in = GObject.Property('can-step-in', type=bool, default=False)
    can_step_over = GObject.Property('can-step-over', type=bool, default=False)
    can_continue = GObject.Property('can-continue', type=bool, default=False)

    # If we've stolen a PTY from the runner to be used later,
    # it will be here. This should be close()d if we do not
    # get a chance to assitn it using gdb.
    inferior_pty = None

    # Our Mi2.Client is what we use to communicate with the controlling gdb
    # process over stdin/stdout.
    client = None

    def do_get_name(self):
        return 'GNU Debugger'

    def do_supports_runner(self, runner):
        if runner.get_runtime().contains_program_in_path('gdb'):
            return (True, GLib.MAXINT)
        else:
            return (False, 0)

    def do_prepare(self, runner):
        # Run the program under gdb so that we can control the debugger
        # with stdin/stdout.
        for arg in reversed(['gdb', '--interpreter', 'mi2', '--args']):
            runner.prepend_argv(arg)

        # Connect to signals so we can setup/cleanup properly
        runner.connect('spawned', self.on_runner_spawned)
        runner.connect('exited', self.on_runner_exited)

        # We need to steal the TTY from the runner (if there is one) so
        # that we can tell GDB to use it. But we need our own access to
        # gdb over a regular stdin/stdout pipe. We remap the TTY in as
        # a different FD so that we can use it later from gdb.
        self.inferior_pty = runner.take_fd(runner.steal_tty(), -1)

        # We need access to stdin/stdout so that we can communicate with
        # the gdb process.
        runner.set_flags(Gio.SubprocessFlags.STDIN_PIPE | Gio.SubprocessFlags.STDOUT_PIPE)

    def on_runner_spawned(self, runner, identifier):
        # Create our client to communicate with gdb
        io_stream = Gio.SimpleIOStream.new(runner.get_stdout(), runner.get_stdin())
        self.client = Mi2.Client.new(io_stream)

        # Connect to all the signals we need from the client before we ask it to start
        # processing incoming messages. We don't want to lose anything.
        self.client.connect('breakpoint-inserted', self.on_client_breakpoint_inserted)
        self.client.connect('breakpoint-removed', self.on_client_breakpoint_removed)
        self.client.connect('event', self.on_client_event)
        self.client.connect('stopped', self.on_client_stopped)
        self.client.connect('log', self.on_client_log)

        # Start the async read loop for the client to process replies.
        self.client.listen_async(None, self.on_listen_cb)

        # We stole the pty from the runner so that we could pass it to gdb
        # instead. This will ensure that gdb re-opens that pty. Since gdb is
        # in the same sandbox as the application, this should Just Work™.
        command = '-gdb-set inferior-tty /proc/self/fd/{}'.format(self.inferior_pty)
        self.client.exec_async(command, None, self.on_client_exec_cb, command)

        # Add a breakpoint at main()
        #breakpoint = Mi2.Breakpoint(function='main')
        #self.client.insert_breakpoint_async(breakpoint, None)

        # Now ask gdb to start the inferior
        self.client.run_async(None, self.on_client_run_cb)

    def on_client_run_cb(self, client, result):
        try:
            ret = client.run_finish(result)
        except Exception as ex:
            print(repr(ex))

    def on_client_exec_cb(self, client, result, command=None):
        try:
            ret = client.exec_finish(result)
            if command:
                print('{} completed'.format(command))
        except Exception as ex:
            print(repr(ex))

    def on_listen_cb(self, client, result):
        try:
            client.listen_finish(result)
        except Exception as ex:
            print(repr(ex))

    def on_runner_exited(self, runner):
        self.client.shutdown_async()

    def on_client_log(self, client, message):
        print('>>>', message[:-1])

    def on_client_event(self, client, message):
        print('Event: {}'.format(message.get_name()))

    def on_client_breakpoint_inserted(self, client, breakpoint):
        print('Breakpoint inserted {}'.format(breakpoint))

    def on_client_breakpoint_removed(self, client, breakpoint_id):
        print('Breakpoint removed{}'.format(breakpoint_id))

    def on_client_stopped(self, client, reason, message):
        print('::stopped {}'.format(reason))
