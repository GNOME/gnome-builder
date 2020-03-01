#!/usr/bin/env python3

#
# cargo_plugin.py
#
# Copyright 2016 Christian Hergert <chris@dronelabs.com>
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

import threading
import os

from gi.repository import Gio
from gi.repository import GLib
from gi.repository import GObject
from gi.repository import Ide

_ = Ide.gettext

_CARGO = 'cargo'
_ERROR_FORMAT_REGEX = ("(?<filename>[a-zA-Z0-9\\+\\-\\.\\/_]+):"
                       "(?<line>\\d+):"
                       "(?<column>\\d+): "
                       "(?<level>[\\w\\[a-zA-Z0-9\\]\\s]+): "
                       "(?<message>.*)")

class CargoBuildSystemDiscovery(Ide.SimpleBuildSystemDiscovery):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.props.glob = 'Cargo.toml'
        self.props.hint = 'cargo_plugin'
        self.props.priority = -200

class CargoBuildSystem(Ide.Object, Ide.BuildSystem):
    project_file = GObject.Property(type=Gio.File)

    def do_get_id(self):
        return 'cargo'

    def do_get_display_name(self):
        return 'Cargo'

    def do_get_priority(self):
        return -200

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

class CargoPipelineAddin(Ide.Object, Ide.PipelineAddin):
    """
    The CargoPipelineAddin is responsible for creating the necessary build
    stages and attaching them to phases of the build pipeline.
    """

    def do_load(self, pipeline):
        context = self.get_context()
        build_system = Ide.BuildSystem.from_context(context)

        # Ignore pipeline unless this is a cargo project
        if type(build_system) != CargoBuildSystem:
            return

        project_file = build_system.props.project_file
        if project_file.get_basename() != 'Cargo.toml':
            project_file = project_file.get_child('Cargo.toml')

        project_dir = os.path.dirname(project_file.get_path())
        config = pipeline.get_config()
        builddir = pipeline.get_builddir()
        runtime = config.get_runtime()
        config_opts = config.get_config_opts()

        # We might need to use cargo from ~/.cargo/bin
        cargo = locate_cargo_from_config(config)

        # Fetch dependencies so that we no longer need network access
        fetch_launcher = pipeline.create_launcher()
        fetch_launcher.setenv('CARGO_TARGET_DIR', builddir, True)
        fetch_launcher.set_cwd(project_dir)
        fetch_launcher.push_argv(cargo)
        fetch_launcher.push_argv('fetch')
        self.track(pipeline.attach_launcher(Ide.PipelinePhase.DOWNLOADS, 0, fetch_launcher))

        # Now create our launcher to build the project
        build_launcher = pipeline.create_launcher()
        build_launcher.setenv('CARGO_TARGET_DIR', builddir, True)
        build_launcher.set_cwd(project_dir)
        build_launcher.push_argv(cargo)
        build_launcher.push_argv('build')
        build_launcher.push_argv('--message-format')
        build_launcher.push_argv('human')

        if not pipeline.is_native():
            build_launcher.push_argv('--target')
            build_launcher.push_argv(pipeline.get_host_triplet().get_full_name())

        if config.props.parallelism > 0:
            build_launcher.push_argv('-j{}'.format(config.props.parallelism))

        if not config.props.debug:
            build_launcher.push_argv('--release')

        # Configure Options get passed to "cargo build" because there is no
        # equivalent "configure stage" for cargo.
        if config_opts:
            try:
                ret, argv = GLib.shell_parse_argv(config_opts)
                build_launcher.push_args(argv)
            except Exception as ex:
                print(repr(ex))

        clean_launcher = pipeline.create_launcher()
        clean_launcher.setenv('CARGO_TARGET_DIR', builddir, True)
        clean_launcher.set_cwd(project_dir)
        clean_launcher.push_argv(cargo)
        clean_launcher.push_argv('clean')

        build_stage = Ide.PipelineStageLauncher.new(context, build_launcher)
        build_stage.set_name(_("Building project"))
        build_stage.set_clean_launcher(clean_launcher)
        build_stage.connect('query', self._query)
        self.track(pipeline.attach(Ide.PipelinePhase.BUILD, 0, build_stage))

    def _query(self, stage, pipeline, targets, cancellable):
        # Always defer to cargo to check if build is needed
        stage.set_completed(False)

class CargoBuildTarget(Ide.Object, Ide.BuildTarget):

    def do_get_install_directory(self):
        return None

    def do_get_display_name(self):
        return 'cargo run'

    def do_get_name(self):
        return 'cargo-run'

    def do_get_language(self):
        return 'rust'

    def do_get_argv(self):
        context = self.ref_context()
        config_manager = Ide.ConfigManager.from_context(context)
        config = config_manager.get_current()
        cargo = locate_cargo_from_config(config)

        # Pass the Cargo.toml path so that we don't
        # need to run from the project directory.
        cargo_toml = context.ref_workdir().get_child('Cargo.toml').get_path()

        return [cargo, 'run', '--manifest-path', cargo_toml]

    def do_get_priority(self):
        return 0

class CargoBuildTargetProvider(Ide.Object, Ide.BuildTargetProvider):

    def do_get_targets_async(self, cancellable, callback, data):
        task = Gio.Task.new(self, cancellable, callback)
        task.set_priority(GLib.PRIORITY_LOW)

        context = self.get_context()
        build_system = Ide.BuildSystem.from_context(context)

        if type(build_system) != CargoBuildSystem:
            task.return_error(GLib.Error('Not cargo build system',
                                         domain=GLib.quark_to_string(Gio.io_error_quark()),
                                         code=Gio.IOErrorEnum.NOT_SUPPORTED))
            return

        task.targets = [CargoBuildTarget()]
        task.return_boolean(True)

    def do_get_targets_finish(self, result):
        if result.propagate_boolean():
            return result.targets

class CargoDependencyUpdater(Ide.Object, Ide.DependencyUpdater):

    def do_update_async(self, cancellable, callback, data):
        task = Gio.Task.new(self, cancellable, callback)
        task.set_priority(GLib.PRIORITY_LOW)

        context = self.get_context()
        build_system = Ide.BuildSystem.from_context(context)

        # Short circuit if not using cargo
        if type(build_system) != CargoBuildSystem:
            task.return_boolean(True)
            return

        build_manager = Ide.BuildManager.from_context(context)
        pipeline = build_manager.get_pipeline()
        if not pipeline:
            task.return_error(GLib.Error('Cannot update dependencies without build pipeline',
                                         domain=GLib.quark_to_string(Gio.io_error_quark()),
                                         code=Gio.IOErrorEnum.FAILED))
            return

        config_manager = Ide.ConfigManager.from_context(context)
        config = config_manager.get_current()
        cargo = locate_cargo_from_config(config)

        project_file = build_system.props.project_file
        if project_file.get_basename() != 'Cargo.toml':
            project_file = project_file.get_child('Cargo.toml')
        cargo_toml = project_file.get_path()

        launcher = pipeline.create_launcher()
        launcher.setenv('CARGO_TARGET_DIR', pipeline.get_builddir(), True)
        launcher.push_argv(cargo)
        launcher.push_argv('update')
        launcher.push_argv('--manifest-path')
        launcher.push_argv(cargo_toml)

        try:
            subprocess = launcher.spawn()
            subprocess.wait_check_async(None, self.wait_check_cb, task)
        except Exception as ex:
            task.return_error(ex)

    def do_update_finish(self, result):
        return result.propagate_boolean()

    def wait_check_cb(self, subprocess, result, task):
        try:
            subprocess.wait_check_finish(result)
            task.return_boolean(True)
        except Exception as ex:
            task.return_error(ex)
