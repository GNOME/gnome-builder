#!/usr/bin/env python3

#
# npm_plugin.py
#
# Copyright © 2016 Christian Hergert <chris@dronelabs.com>
#               2017 Giovanni Campagna <gcampagn@cs.stanford.edu>
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
import threading
import os

gi.require_version('Ide', '1.0')

from gi.repository import Gio
from gi.repository import GLib
from gi.repository import GObject
from gi.repository import Ide

class NPMBuildSystem(Ide.Object, Ide.BuildSystem, Gio.AsyncInitable):
    project_file = GObject.Property(type=Gio.File)

    def do_get_id(self):
        return 'npm'

    def do_get_display_name(self):
        return 'NPM (node.js)'

    def do_init_async(self, io_priority, cancellable, callback, data):
        task = Gio.Task.new(self, cancellable, callback)

        # This is all done synchronously, doing it in a thread would probably
        # be somewhat ideal although unnecessary at this time.

        try:
            # Maybe this is a package.json
            if self.props.project_file.get_basename() in ('package.json',):
                task.return_boolean(True)
                return

            # Maybe this is a directory with a package.json
            if self.props.project_file.query_file_type(0) == Gio.FileType.DIRECTORY:
                child = self.props.project_file.get_child('package.json')
                if child.query_exists(None):
                    self.props.project_file = child
                    task.return_boolean(True)
                    return
        except Exception as ex:
            task.return_error(ex)

        task.return_error(Ide.NotSupportedError())

    def do_init_finish(self, task):
        return task.propagate_boolean()

    def do_get_priority(self):
        return 300

class NPMPipelineAddin(Ide.Object, Ide.BuildPipelineAddin):
    """
    The NPMPipelineAddin is responsible for creating the necessary build
    stages and attaching them to phases of the build pipeline.
    """

    def do_load(self, pipeline):
        context = self.get_context()
        build_system = context.get_build_system()

        # Ignore pipeline unless this is a npm/nodejs project
        if type(build_system) != NPMBuildSystem:
            return

        package_json = build_system.props.project_file
        config = pipeline.get_configuration()
        system_type = config.get_device().get_system_type()
        builddir = pipeline.get_builddir()
        runtime = config.get_runtime()

        npm = 'npm'
        if config.getenv('NPM'):
            npm = config.getenv('NPM')
        elif not runtime.contains_program_in_path('npm'):
            raise OSError('The runtime must contain nodejs/npm to build npm modules')

        # Fetch dependencies so that we no longer need network access
        fetch_launcher = pipeline.create_launcher()
        fetch_launcher.set_cwd(package_json.get_parent().get_path())
        fetch_launcher.push_argv(npm)
        if Ide.get_system_type() != system_type:
            fetch_launcher.push_argv('--arch')
            fetch_launcher.push_argv(system_type)
        fetch_launcher.push_argv('install')
        self.track(pipeline.connect_launcher(Ide.BuildPhase.DOWNLOADS, 0, fetch_launcher))

