# __init__.py
#
# Copyright (C) 2016 Patrick Griffis <tingping@tingping.se>
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

from os import path
import subprocess
import threading
import shutil
import json
import gi

gi.require_version('Ide', '1.0')

from gi.repository import (
    GLib,
    GObject,
    Gio,
    Ide
)

_ = Ide.gettext

ninja = None


class MesonBuildSystem(Ide.Object, Ide.BuildSystem, Gio.AsyncInitable):
    project_file = GObject.Property(type=Gio.File)

    def do_init_async(self, priority, cancel, callback, data=None):
        task = Gio.Task.new(self, cancel, callback)
        task.set_priority(priority)

        # TODO: Be async here also
        project_file = self.get_context().get_project_file()
        if project_file.get_basename() == 'meson.build':
            task.return_boolean(True)
        else:
            child = project_file.get_child('meson.build')
            exists = child.query_exists(cancel)
            if exists:
                self.props.project_file = child
            task.return_boolean(exists)

    def do_init_finish(self, result):
        return result.propagate_boolean()

    def do_get_priority(self):
        return -200 # Lower priority than Autotools for now

    def do_get_builder(self, config):
        return MesonBuilder(context=self.get_context(), configuration=config)


class MesonBuilder(Ide.Builder):
    configuration = GObject.Property(type=Ide.Configuration)

    def __init__(self, **kwargs):
        super().__init__(**kwargs)

    def _get_build_dir(self) -> Gio.File:
        context = self.get_context()

        # This matches the Autotools layout
        project_id = context.get_project().get_id()
        buildroot = context.get_root_build_dir()
        device = self.props.configuration.get_device()
        device_id = device.get_id()
        system_type = device.get_system_type()

        return Gio.File.new_for_path(path.join(buildroot, project_id, device_id, system_type))

    def _get_source_dir(self) -> Gio.File:
        context = self.get_context()
        return context.get_vcs().get_working_directory()

    @staticmethod
    def _task_set_error(task, err):
        task.build_result.set_mode(_('Failed'))
        task.build_result.set_failed(True)
        task.return_error(err)
        task.build_result.set_running(False)

    @staticmethod
    def _task_set_success(task):
        task.build_result.set_mode(_('Successful'))
        task.build_result.set_failed(False)
        task.return_boolean(True)
        task.build_result.set_running(False)

    def do_build_async(self, flags, cancellable, callback, data=None):
        task = Gio.Task.new(self, cancellable, callback)
        task.build_result = MesonBuildResult(self.configuration,
                                             self._get_build_dir(),
                                             self._get_source_dir(),
                                             cancellable,
                                             flags=flags)

        def wrap_build(task):
            task.build_result.set_running(True)
            try:
                task.build_result.build()
            except GLib.Error as err:
                self._task_set_error(task, err)
            else:
                def postbuild_finish(err):
                    if err:
                        self._task_set_error(task, err)
                    else:
                        self._task_set_success(task)
                task.build_result._postbuild_async(postbuild_finish)

        def _on_prebuild_finish(task, err):
            if err:
                self._task_set_error(task, err)
            else:
                thread = threading.Thread(target=wrap_build, args=(task,))
                thread.start()
        task.build_result._prebuild_async(task, _on_prebuild_finish)

        return task.build_result

    def do_build_finish(self, result) -> Ide.BuildResult:
        if result.propagate_boolean():
            return result.build_result

    def do_install_async(self, cancellable, callback, data=None):
        task = Gio.Task.new(self, cancellable, callback)
        task.build_result = MesonBuildResult(self.configuration,
                                             self._get_build_dir(),
                                             self._get_source_dir(),
                                             cancellable)

        def wrap_install(task):
            task.build_result.set_running(True)
            try:
                task.build_result.install()
            except GLib.Error as err:
                self._task_set_error(task, err)
            else:
                def postinstall_finish(err):
                    if err:
                        self._task_set_error(task, err)
                    else:
                        self._task_set_success(task)
                task.build_result._postinstall_async(postinstall_finish)

        thread = threading.Thread(target=wrap_install, args=(task,))
        thread.start()

        return task.build_result

    def do_install_finish(self, result) -> Ide.BuildResult:
        if result.propagate_boolean():
            return result.build_result

    def do_get_build_flags_async(self, ifile, cancellable, callback, data=None):
        task = Gio.Task.new(self, cancellable, callback)
        task.build_flags = []

        def extract_flags(command: str):
            flags = GLib.shell_parse_argv(command)[1] # Raises on failure
            return [flag for flag in flags if flag.startswith(('-I', '-isystem', '-W', '-D'))]

        def build_flags_thread():
            commands_file = path.join(self._get_build_dir().get_path(), 'compile_commands.json')
            try:
                with open(commands_file) as f:
                    commands = json.loads(f.read(), encoding='utf-8')
            except (json.JSONDecodeError, FileNotFoundError, UnicodeDecodeError) as e:
                task.return_error(GLib.Error('Failed to decode meson json: {}'.format(e)))
                return

            infile = ifile.get_path()
            for c in commands:
                filepath = path.normpath(path.join(c['directory'], c['file']))
                if filepath == infile:
                    try:
                        task.build_flags = extract_flags(c['command'])
                    except GLib.Error as e:
                        task.return_error(e)
                        return
                    break
            else:
                print('Meson: Warning: No flags found')

            task.return_boolean(True)

        thread = threading.Thread(target=build_flags_thread)
        thread.start()

    def do_get_build_flags_finish(self, result):
        if result.propagate_boolean():
            return result.build_flags

    def do_get_build_targets_async(self, cancellable, callback, data=None):
        task = Gio.Task.new(self, cancellable, callback)
        task.build_targets = []

        config = self.get_configuration()

        def build_targets_thread():
            # TODO: Ide.Subprocess.communicate_utf8(None, cancellable) doesn't work?
            try:
                ret = subprocess.check_output(['mesonintrospect', '--targets',
                                               self._get_build_dir().get_path()])
            except (subprocess.CalledProcessError, FileNotFoundError) as e:
                task.return_error(GLib.Error('Failed to run mesonintrospect: {}'.format(e)))
                return

            targets = []
            try:
                meson_targets = json.loads(ret.decode('utf-8'))
            except (json.JSONDecodeError, UnicodeDecodeError) as e:
                task.return_error(GLib.Error('Failed to decode mesonintrospect json: {}'.format(e)))
                return

            bindir = path.join(config.get_prefix(), 'bin')
            for t in meson_targets:
                # TODO: Ideally BuildTargets understand filename != name
                name = t['filename']
                if isinstance(name, list):
                    name = name[0]
                name = path.basename(name)

                install_dir = path.dirname(t.get('install_filename', ''))
                installed = t['installed']

                ide_target = MesonBuildTarget(install_dir, name=name)
                # Try to be smart and sort these because Builder runs the
                # first one. Ideally it allows the user to select the run targets.
                if t['type'] == 'executable' and t['installed'] and \
                    install_dir.startswith(bindir) and not t['filename'].endswith('-cli'):
                    targets.insert(0, ide_target)
                else:
                    targets.append(ide_target)

            task.build_targets = targets
            task.return_boolean(True)

        thread = threading.Thread(target=build_targets_thread)
        thread.start()

    def do_get_build_targets_finish(self, result):
        if result.propagate_boolean():
            return result.build_targets


class MesonBuildResult(Ide.BuildResult):

    def __init__(self, config, blddir, srcdir, cancel, flags=0, **kwargs):
        super().__init__(**kwargs)
        self.config = config
        self.cancel = cancel
        self.flags = flags
        self.runtime = config.get_runtime()
        self.blddir = blddir
        self.srcdir = srcdir

    def _new_launcher(self, cwd=None):
        if self.runtime:
            launcher = self.runtime.create_launcher()
        else:
            launcher = Ide.SubprocessLauncher.new(Gio.SubprocessFlags.NONE)
            launcher.set_run_on_host(True)
            launcher.set_clear_env(False)
        if cwd:
            launcher.set_cwd(cwd.get_path())
        return launcher

    def _get_ninja(self):
        global ninja
        if not ninja:
            if GLib.find_program_in_path('ninja-build'):
                ninja = 'ninja-build' # Fedora...
            else:
                ninja = 'ninja'
        return ninja

    def _run_subprocess(self, launcher):
        self.log_stdout_literal('Running: {}…'.format(' '.join(launcher.get_argv())))
        proc = launcher.spawn()
        self.log_subprocess(proc)
        proc.wait_check(self.cancel)

    def _ensure_configured(self):
        bootstrap = bool(self.flags & Ide.BuilderBuildFlags.FORCE_BOOTSTRAP)

        if bootstrap or self.config.get_dirty():
            try:
                shutil.rmtree(self.blddir.get_path())
                self.log_stdout_literal('Deleting build directory…')
            except FileNotFoundError:
                pass
            self.config.set_dirty(False)

        if not self.blddir.query_exists():
            self.log_stdout_literal('Creating build directory…')
            self.blddir.make_directory_with_parents(self.cancel)

        # TODO: For dirty config we could use `mesonconf` but it does not
        # handle removing defines which might be unclear

        if not self.blddir.get_child('build.ninja').query_exists():
            config_opts = self.config.get_config_opts()
            extra_opts = config_opts.split() if config_opts else []
            extra_opts.append('--prefix=' + self.config.get_prefix())
            launcher = self._new_launcher(self.srcdir)
            launcher.push_args(['meson', self.blddir.get_path()] + extra_opts)

            self.set_mode(_('Configuring…'))
            self._run_subprocess(launcher)

    def install(self):
        self._ensure_configured()

        launcher = self._new_launcher(cwd=self.blddir)
        launcher.push_args([self._get_ninja(), 'install'])
        self._run_subprocess(launcher)

    def _prebuild_async(self, task, callback):
        # Unlike the rest of this class this is ran on the main thread
        # using async apis so try to catch the error and propogate it
        # where others are handled
        def prebuild_command_finish(command, result):
            try:
                command.execute_finish(result)
            except GLib.Error as e:
                callback(task, e)
            else:
                callback(task, None)

        def prebuild_finish(runtime, result):
            try:
                runtime.prebuild_finish(result)
                prebuild_command = self.config.get_prebuild()
                prebuild_command.execute_async(runtime, self.config.get_environment(),
                                             self, self.cancel, prebuild_command_finish)
            except GLib.Error as e:
                callback(task, e)

        if self.runtime:
            self.set_mode(_('Running prebuild…'))
            self.runtime.prebuild_async(self, self.cancel, prebuild_finish)

    def _postaction_async(self, callback, install=False):
        if not self.runtime:
            return

        def postaction_finish(runtime, result):
            try:
                if install:
                    runtime.postinstall_finish(result)
                else:
                    runtime.postbuild_finish(result)
            except GLib.Error as e:
                callback(e)
            else:
                callback(None)

        if install:
            self.set_mode(_('Running post-install…'))
            self.runtime.postinstall_async(self, self.cancel, postaction_finish)
        else:
            self.set_mode(_('Running post-build…'))
            self.runtime.postbuild_async(self, self.cancel, postaction_finish)

    def _postbuild_async(self, callback):
        self._postaction_async(callback)

    def _postinstall_async(self, callback):
        self._ppostaction_async(callback, install=True)

    def build(self):
        # NOTE: These are ran in a thread and it raising GLib.Error is handled a layer up.
        self._ensure_configured()

        clean = bool(self.flags & Ide.BuilderBuildFlags.FORCE_CLEAN)
        build = not self.flags & Ide.BuilderBuildFlags.NO_BUILD

        launcher = self._new_launcher(self.blddir)
        launcher.push_args([self._get_ninja()])
        if clean:
            self.log_stdout_literal('Cleaning…')
            self.set_mode(_('Cleaning…'))
            launcher.push_args(['clean'])
            self._run_subprocess(launcher)
        if build:
            if clean: # Build after cleaning
                launcher.pop_argv()
            self.log_stdout_literal('Building…')
            self.set_mode(_('Building…'))
            self._run_subprocess(launcher)

class MesonBuildTarget(Ide.Object, Ide.BuildTarget):
    # TODO: These should be part of the BuildTarget interface
    name = GObject.Property(type=str)
    install_directory = GObject.Property(type=Gio.File)

    def __init__(self, install_dir, **kwargs):
        super().__init__(**kwargs)
        self.props.install_directory = Gio.File.new_for_path(install_dir)

    def do_get_install_directory(self):
        return self.props.install_directory

