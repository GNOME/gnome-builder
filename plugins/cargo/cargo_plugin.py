#!/usr/bin/env python3

#
# cargo_plugin.py
#
# Copyright (C) 2016 Christian Hergert <chris@dronelabs.com>
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

gi.require_version('Ide', '1.0')

from gi.repository import Gio
from gi.repository import GLib
from gi.repository import GObject
from gi.repository import Ide

_ = Ide.gettext

class CargoBuildSystem(Ide.Object, Ide.BuildSystem, Gio.AsyncInitable):
    project_file = GObject.Property(type=Gio.File)

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

        raise NotImplemented

    def do_init_finish(self, task):
        return task.propagate_boolean()

    def do_get_priority(self):
        # Priority is used to determine the order of discovery
        return 2000

    def do_get_build_flags_async(self, ifile, cancellable, callback, data):
        # GTask sort of is painful from Python.
        # We can use it to attach some data to return from the finish
        # function though.
        task = Gio.Task.new(self, cancellable, callback)
        task.build_flags = []
        task.return_boolean(True)

    def do_get_build_flags_finish(self, result):
        if task.propagate_boolean():
            return result.build_flags

    def do_get_builder(self, config):
        return CargoBuilder(config, context=self.get_context())

    def do_get_build_targets_async(self, cancellable, callback, data):
        # TODO: We need a way to figure out what "cargo run" will do so that
        #       we can synthesize that as a build result.
        task = Gio.Task.new(self, cancellable, callback)
        task.build_targets = []
        task.return_boolean(True)

    def do_get_build_targets_finish(self, task):
        if task.propagate_boolean():
            return task.build_targets

class CargoBuilder(Ide.Builder):
    config = GObject.Property(type=Ide.Configuration)

    def __init__(self, config, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.config = config

    def do_build_async(self, flags, cancellable, callback, data):
        task = Gio.Task.new(self, cancellable, callback)
        task.build_result = CargoBuildResult(self.config, flags, context=self.get_context())

        def wrap_execute():
            try:
                task.build_result.build()
                task.build_result.set_mode(_('Successful'))
                task.build_result.set_failed(False)
                task.build_result.set_running(False)
                task.return_boolean(True)
            except Exception as ex:
                task.build_result.set_mode(_("Failed"))
                task.build_result.set_failed(True)
                task.build_result.set_running(False)
                task.return_error(ex)

        thread = threading.Thread(target=wrap_execute)
        thread.start()

        return task.build_result

    def do_build_finish(self, task):
        if task.propagate_boolean():
            return task.build_result

    def do_install_async(self, cancellable, callback, data):
        task = Gio.Task.new(self, cancellable, callback)
        task.build_result = CargoBuildResult(self.config, 0, context=self.get_context())

        def wrap_execute():
            try:
                task.build_result.install()
                self = task.get_source_object()
                task.build_result.set_mode(_('Successful'))
                task.build_result.set_failed(False)
                task.build_result.set_running(False)
                task.return_boolean(True)
            except Exception as ex:
                task.build_result.set_mode(_("Failed"))
                task.build_result.set_failed(True)
                task.build_result.set_running(False)
                task.return_error(ex)

        thread = threading.Thread(target=wrap_execute)
        thread.start()

        return task.build_result

    def do_install_finish(self, task):
        if task.propagate_boolean():
            return task.build_result

class CargoBuildResult(Ide.BuildResult):
    runtime = GObject.Property(type=Ide.Runtime)

    def __init__(self, config, flags, *args, **kwargs):
        super().__init__(*args, **kwargs)

        context = self.get_context()
        vcs = context.get_vcs()

        self.flags = flags
        self.runtime = config.get_runtime()
        self.directory = vcs.get_working_directory().get_path()

        self.set_running(True)

    def _run(self, mode, *args):
        self.set_mode(mode)

        if self.runtime is not None:
            launcher = self.runtime.create_launcher()
        else:
            launcher = Ide.SubprocessLauncher()
            launcher.set_run_on_host(True)
            launcher.set_clear_environment(False)

        launcher.set_cwd(self.directory)
        launcher.push_argv('cargo')
        for arg in args:
            launcher.push_argv(arg)

        subprocess = launcher.spawn_sync()

        self.log_subprocess(subprocess)

        subprocess.wait_check()

        return True

    def build(self):
        # Do a clean first if requested.
        if self.flags & Ide.BuilderBuildFlags.FORCE_CLEAN != 0:
            self._run(_("Cleaning…"), 'clean')

        if self.flags & Ide.BuilderBuildFlags.NO_BUILD == 0:
            self._run(_("Building…"), 'build', '--verbose')

        self.set_mode(_('Successful'))
        self.set_failed(False)
        self.set_running(False)

        return True

    def install(self):
        self.build()
        self._run(_('Installing…'), 'install')

        self.set_mode(_('Successful'))
        self.set_failed(False)
        self.set_running(False)
