#!/usr/bin/env python3

#
# cargo_plugin.py
#
# Copyright © 2016 Christian Hergert <chris@dronelabs.com>
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

_CARGO = 'cargo'

class CargoBuildSystem(Ide.Object, Ide.BuildSystem, Gio.AsyncInitable):
    project_file = GObject.Property(type=Gio.File)

    def do_get_id(self):
        return 'cargo'

    def do_get_display_name(self):
        return 'Cargo'

    def do_init_async(self, io_priority, cancellable, callback, data):
        task = Gio.Task.new(self, cancellable, callback)

        # This is all done synchronously, doing it in a thread would probably
        # be somewhat ideal although unnecessary at this time.

        try:
            # Maybe this is a Cargo.toml
            if self.props.project_file.get_basename() in ('Cargo.toml',):
                task.return_boolean(True)
                return

            # Maybe this is a directory with a Cargo.toml
            if self.props.project_file.query_file_type(0) == Gio.FileType.DIRECTORY:
                child = self.props.project_file.get_child('Cargo.toml')
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

def locate_cargo_from_config(config):
    cargo = _CARGO

    if config:
        runtime = config.get_runtime()
        if config.getenv('CARGO'):
            cargo = config.getenv('CARGO')
        elif not runtime or not runtime.contains_program_in_path(_CARGO):
            cargo_in_home = os.path.expanduser('~/.cargo/bin/cargo')
            if os.path.exists(cargo_in_home):
                cargo = cargo_in_home

    return cargo

class CargoPipelineAddin(Ide.Object, Ide.BuildPipelineAddin):
    """
    The CargoPipelineAddin is responsible for creating the necessary build
    stages and attaching them to phases of the build pipeline.
    """

    def do_load(self, pipeline):
        context = self.get_context()
        build_system = context.get_build_system()

        # Ignore pipeline unless this is a cargo project
        if type(build_system) != CargoBuildSystem:
            return

        cargo_toml = build_system.props.project_file.get_path()
        config = pipeline.get_configuration()
        system_type = config.get_device().get_system_type()
        builddir = pipeline.get_builddir()
        runtime = config.get_runtime()

        # We might need to use cargo from ~/.cargo/bin
        cargo = locate_cargo_from_config(config)

        # Fetch dependencies so that we no longer need network access
        fetch_launcher = pipeline.create_launcher()
        fetch_launcher.setenv('CARGO_TARGET_DIR', builddir, True)
        fetch_launcher.push_argv(cargo)
        fetch_launcher.push_argv('fetch')
        fetch_launcher.push_argv('--manifest-path')
        fetch_launcher.push_argv(cargo_toml)
        self.track(pipeline.connect_launcher(Ide.BuildPhase.DOWNLOADS, 0, fetch_launcher))

        # Fetch dependencies so that we no longer need network access
        build_launcher = pipeline.create_launcher()
        build_launcher.setenv('CARGO_TARGET_DIR', builddir, True)
        build_launcher.push_argv(cargo)
        build_launcher.push_argv('build')
        build_launcher.push_argv('--verbose')
        build_launcher.push_argv('--manifest-path')
        build_launcher.push_argv(cargo_toml)
        build_launcher.push_argv('--message-format')
        build_launcher.push_argv('human')

        if Ide.get_system_type() != system_type:
            build_launcher.push_argv('--target')
            build_launcher.push_argv(system_type)

        if config.props.parallelism > 0:
            build_launcher.push_argv('-j{}'.format(config.props.parallelism))

        if not config.props.debug:
            build_launcher.push_argv('--release')

        clean_launcher = pipeline.create_launcher()
        clean_launcher.setenv('CARGO_TARGET_DIR', builddir, True)
        clean_launcher.push_argv(cargo)
        clean_launcher.push_argv('clean')
        clean_launcher.push_argv('--manifest-path')
        clean_launcher.push_argv(cargo_toml)

        build_stage = Ide.BuildStageLauncher.new(context, build_launcher)
        build_stage.set_clean_launcher(clean_launcher)
        self.track(pipeline.connect(Ide.BuildPhase.BUILD, 0, build_stage))

class CargoBuildTarget(Ide.Object, Ide.BuildTarget):

    def do_get_install_directory(self):
        return None

    def do_get_name(self):
        return 'cargo-run'

    def do_get_language(self):
        return 'rust'

    def do_get_argv(self):
        context = self.get_context()
        config_manager = context.get_configuration_manager()
        config = config_manager.get_current()
        cargo = locate_cargo_from_config(config)

        # Pass the Cargo.toml path so that we don't
        # need to run from the project directory.
        project_file = context.get_project_file()
        if project_file.get_basename() == 'Cargo.toml':
            cargo_toml = project_file.get_path()
        else:
            cargo_toml = project_file.get_child('Cargo.toml')

        return [cargo, 'run', '--manifest-path', cargo_toml]

    def do_get_priority(self):
        return 0

class CargoBuildTargetProvider(Ide.Object, Ide.BuildTargetProvider):

    def do_get_targets_async(self, cancellable, callback, data):
        task = Gio.Task.new(self, cancellable, callback)
        task.set_priority(GLib.PRIORITY_LOW)

        context = self.get_context()
        build_system = context.get_build_system()

        if type(build_system) != CargoBuildSystem:
            task.return_error(GLib.Error('Not cargo build system',
                                         domain=GLib.quark_to_string(Gio.io_error_quark()),
                                         code=Gio.IOErrorEnum.NOT_SUPPORTED))
            return

        task.targets = [CargoBuildTarget(context=self.get_context())]
        task.return_boolean(True)

    def do_get_targets_finish(self, result):
        if result.propagate_boolean():
            return result.targets
