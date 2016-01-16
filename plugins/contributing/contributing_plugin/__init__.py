#!/usr/bin/env python3

#
# __init__.py
#
# Copyright (C) 2015 Christian Hergert <chris@dronelabs.com>
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
from getopt import getopt, GetoptError
import getpass
from gettext import gettext as _
import os
import sys
import time

gi.require_version('Ggit', '1.0')

from contributing_plugin import helper

from gi.repository import GLib
from gi.repository import GObject
from gi.repository import Gio
from gi.repository import Ggit
from gi.repository import Ide

class ContributeTool(helper.PyApplicationTool):
    location = None

    def do_run_async(self, args, cancellable, callback, user_data):
        task = Gio.Task.new(self, cancellable, callback)

        try:
            self.parse(args)
        except Exception as ex:
            self.printerr(repr(ex))
            task.return_int(1)
            return

        if not self.args:
            self.printerr(_("Missing project name"))
            self.printerr("")
            self.usage(stream=sys.stderr)
            task.return_int(1)
            return

        project_name = self.args[0]

        self.clone(task, project_name)

    def do_run_finish(self, result):
        return result.propagate_int()

    def usage(self, stream=sys.stdout):
        stream.write(_("""Usage:
  ide contribute PROJECT_NAME

  This command will bootstrap your system to begin contributing to the project
  denoted by PROJECT_NAME. This includes fetching the sources, ensuring that
  you have the required dependencies to build, and bootstraps the first build
  of the project.

Examples:
  ide contribute gnome-builder
  ide contribute gnome-maps

"""))

    def clone(self, task, project_name):
        callbacks = RemoteCallbacks(self)

        fetch_options = Ggit.FetchOptions()
        fetch_options.set_remote_callbacks(callbacks)

        clone_options = Ggit.CloneOptions()
        clone_options.set_is_bare(False)
        clone_options.set_checkout_branch("master")
        clone_options.set_fetch_options(fetch_options)

        uri = self.get_project_uri(project_name)
        self.location = self.get_project_dir(project_name)

        self.write_line("Cloning %s into %s ..." % (project_name, self.location.get_path()))

        try:
            repository = Ggit.Repository.clone(uri, self.location, clone_options)
        except Exception as ex:
            self.printerr(ex.message)
            task.return_int(1)
            return

        # TODO: Find project dependencies, install
        #       that could be xdg-app runtime, jhbuild, etc
        #self.write_line("Locating project dependencies")

        # Load the project and try to build it.
        Ide.Context.new_async(self.location,
                              task.get_cancellable(),
                              self.get_context_cb,
                              task)

    def get_project_uri(self, project_name):
        # TODO: check jhbuildrc or ssh_config for username
        return 'git://git.gnome.org/'+project_name+'.git'

    def get_project_dir(self, project_name):
        # TODO: configurable project dirs
        parent_dir = os.path.join(GLib.get_home_dir(), 'Projects')

        try:
            os.makedirs(parent_dir)
        except:
            pass

        return Gio.File.new_for_path(os.path.join(parent_dir, project_name))

    def get_context_cb(self, object, result, task):
        try:
            context = Ide.Context.new_finish(result)
        except Exception as ex:
            self.printerr(ex.message)
            task.return_int(1)
            return

        # Build the project
        device_manager = context.get_device_manager()
        device = device_manager.get_device('local')
        build_system = context.get_build_system()
        builder = build_system.get_builder(None, device)
        result = builder.build_async(Ide.BuilderBuildFlags.NONE,
                                     task.get_cancellable(),
                                     self.build_cb, task)
        result.connect('log', self.log_message)

    def build_cb(self, builder, result, task):
        try:
            builder.build_finish(result)
        except Exception as ex:
            self.printerr(repr(ex))

        contributing_md = self.location.get_child('CONTRIBUTING.md')
        if contributing_md.query_exists():
            (_, data, _) = contributing_md.load_contents()
            print("\n\n\n")
            print(data.decode('utf-8'))

        task.return_int(0)

    def log_message(self, result, stream, message):
        if stream == Ide.BuildResultLog.STDOUT:
            sys.stdout.write(message)
        else:
            sys.stderr.write(message)

class RemoteCallbacks(Ggit.RemoteCallbacks):
    done = False
    started_at = 0

    def __init__(self, tool, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.tool = tool

    def do_transfer_progress(self, stats):
        # Ignore callbacks after we finish progress
        if self.done:
            return

        # Start tracking after first call to transfer_progress
        if not self.started_at:
            self.started_at = time.time()

        fraction = stats.get_received_objects() / float(max(1, stats.get_total_objects()))

        duration = time.time() - self.started_at
        hours = duration / (60 * 60)
        minutes = (duration % (60 * 60)) / 60
        seconds = duration % 60
        prefix = "%02u:%02u:%02u" % (hours, minutes, seconds)

        rate = stats.get_received_bytes() / max(1, duration)
        ratestr = GLib.format_size(rate)
        if len(ratestr) < 8:
            ratestr = (' ' * (8 - len(ratestr))) + ratestr

        if fraction == 1.0:
            transfered = GLib.format_size(stats.get_received_bytes())
            self.tool.clear_line()
            self.tool.write_progress(fraction, prefix, transfered)
            self.tool.write_line("")
            self.done = True
        else:
            self.tool.clear_line()
            self.tool.write_progress(fraction, prefix, ratestr)

    def do_credentials(self, url, username, allowed_creds):
        if (allowed_creds & Ggit.Credtype.SSH_KEY) != 0:
            username = self.get_username(username)
            return Ggit.CredSshKeyFromAgent.new(username)

        if (allowed_creds & Ggit.Credtype.SSH_INTERACTIVE) != 0:
            username = self.get_username(username)
            return Ggit.CredSshInteractive.new(username)

        if (allowed_creds & Ggit.Credtype.USERPASS_PLAINTEXT) != 0:
            username = self.get_username(username)
            password = getpass.getpass()
            return Ggit.CredPlaintext.new(username, password)

        return None

    def get_username(self, username):
        if not username:
            username = input('%s [%s]: ' % (_("Username"), getpass.getuser().strip()))
            if not username:
                username = getpass.getuser()
        return username
