# __init__.py
#
# Copyright © 2016 Patrick Griffis <tingping@tingping.se>
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
import threading
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

_NINJA_NAMES = ['ninja-build', 'ninja']


def execInRuntime(runtime, *args, **kwargs):
    directory = kwargs.get('directory', None)
    launcher = runtime.create_launcher()
    launcher.push_args(args)
    if directory is not None:
        launcher.set_cwd(directory)
    proc = launcher.spawn(None)
    _, stdout, stderr = proc.communicate_utf8(None, None)
    return stdout


def extract_flags(command: str, builddir: str):
    flags = GLib.shell_parse_argv(command)[1] # Raises on failure
    wanted_flags = []
    for i, flag in enumerate(flags):
        if flag.startswith('-I'):
            # All paths are relative to build
            abspath = path.normpath(path.join(builddir, flag[2:]))
            wanted_flags.append('-I' + abspath)
        elif flag.startswith(('-isystem', '-W', '-D', '-std')):
            wanted_flags.append(flag)
        elif flag == '-include':
            wanted_flags += [flag, flags[i + 1]]
    return wanted_flags


class MesonBuildSystem(Ide.Object, Ide.BuildSystem, Gio.AsyncInitable):
    project_file = GObject.Property(type=Gio.File)

    def do_get_id(self):
        return 'meson'

    def do_get_display_name(self):
        return 'Meson'

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
        return 100

    def do_get_build_flags_async(self, ifile, cancellable, callback, data=None):
        task = Gio.Task.new(self, cancellable, callback)
        task.ifile = ifile
        task.build_flags = []

        context = self.get_context()
        build_manager = context.get_build_manager()

        # First we need to ensure that the build pipeline has progressed
        # to complete the CONFIGURE stage which is where our mesonintrospect
        # command caches the results.
        build_manager.execute_async(Ide.BuildPhase.CONFIGURE,
                                    cancellable,
                                    self._get_build_flags_cb,
                                    task)

    def do_get_build_flags_finish(self, result):
        if result.propagate_boolean():
            return result.build_flags

    def _get_build_flags_cb(self, build_manager, result, task):
        config = build_manager.get_pipeline().get_configuration()
        builddir = build_manager.get_pipeline().get_builddir()
        commands_file = path.join(self.get_builddir(config), 'compile_commands.json')
        runtime = config.get_runtime()

        def build_flags_thread():
            try:
                with open(commands_file) as f:
                    commands = json.loads(f.read(), encoding='utf-8')
            except (json.JSONDecodeError, FileNotFoundError, UnicodeDecodeError) as e:
                task.return_error(GLib.Error('Failed to decode meson json: {}'.format(e)))
                return

            infile = task.ifile.get_path()
            # If this is a header file we want the flags for a C/C++/Objc file.
            # (Extensions Match GtkSourceViews list)
            is_header = infile.endswith(('.h', '.hpp', '.hh', '.h++', '.hp'))
            if is_header:
                # So just try to find a compilable file with the same prefix as
                # that is *probably* correct.
                infile = infile.rpartition('.')[0] + '.'
            for c in commands:
                filepath = path.normpath(path.join(c['directory'], c['file']))
                if (is_header is False and filepath == infile) or \
                   (is_header is True and filepath.startswith(infile)):
                    try:
                        task.build_flags = extract_flags(c['command'], builddir)
                    except GLib.Error as e:
                        task.return_error(e)
                        return
                    break

            if infile.endswith('.vala'):
                # We didn't find anything in the compile_commands.json, so now try to use
                # the compdb from ninja and see if it has anything useful for us.
                ninja = None
                for name in _NINJA_NAMES:
                    if runtime.contains_program_in_path(name):
                        ninja = name
                        break
                if ninja:
                    ret = execInRuntime(runtime, ninja, '-t', 'compdb', 'vala_COMPILER', directory=builddir)
                    try:
                        commands = json.loads(ret, encoding='utf-8')
                    except Exception as e:
                        task.return_error(GLib.Error('Failed to decode ninja json: {}'.format(e)))
                        return

                    for c in commands:
                        try:
                            _, argv = GLib.shell_parse_argv(c['command'])
                            # TODO: It would be nice to filter these arguments a bit,
                            #       but the vala plugin should handle that fine.
                            task.build_flags = argv
                            task.return_boolean(True)
                            return
                        except:
                            pass

            Ide.debug('No flags found for file', infile)

            task.return_boolean(True)

        try:
            build_manager.execute_finish(result)
            thread = threading.Thread(target=build_flags_thread)
            thread.start()
        except Exception as err:
            task.return_error(err)

    def do_get_build_targets_async(self, cancellable, callback, data=None):
        task = Gio.Task.new(self, cancellable, callback)
        task.build_targets = []

        build_manager = self.get_context().get_build_manager()
        build_manager.execute_async(Ide.BuildPhase.CONFIGURE,
                                    cancellable,
                                    self._get_build_targets_cb,
                                    task)

    def do_get_build_targets_finish(self, result):
        if result.propagate_boolean():
            return result.build_targets

    def _get_build_targets_cb(self, build_manager, result, task):
        context = build_manager.get_context()
        config = build_manager.get_pipeline().get_configuration()
        runtime = config.get_runtime()
        builddir = self.get_builddir(config)
        prefix = config.get_prefix()
        bindir = path.join(prefix, 'bin')

        def build_targets_thread():
            try:
                ret = execInRuntime(runtime, 'mesonintrospect', '--targets', builddir)
            except Exception as e:
                task.return_error(GLib.Error('Failed to run mesonintrospect: {}'.format(e)))
                return

            targets = []

            try:
                meson_targets = json.loads(ret)
            except (json.JSONDecodeError, UnicodeDecodeError) as e:
                task.return_error(GLib.Error('Failed to decode mesonintrospect json: {}'.format(e)))
                return

            for t in meson_targets:
                # TODO: Ideally BuildTargets understand filename != name
                name = t['filename']
                if isinstance(name, list):
                    name = name[0]
                name = path.basename(name)

                install_dir = path.dirname(t.get('install_filename', ''))
                installed = t['installed']

                ide_target = MesonBuildTarget(install_dir, name=name, context=context)
                # Try to be smart and sort these because Builder runs the
                # first one. Ideally it allows the user to select the run targets.
                if t['type'] == 'executable' and t['installed'] and \
                    install_dir.startswith(bindir) and not t['filename'].endswith('-cli'):
                    targets.insert(0, ide_target)
                else:
                    targets.append(ide_target)

            # It is possible the program installs a script not a binary
            if not targets or targets[0].install_directory.get_path() != bindir:
                try:
                    # This is a new feature in Meson 0.37.0
                    ret = execInRuntime(runtime, 'mesonintrospect', '--installed', builddir)
                    installed = json.loads(ret)
                    for f in installed.values():
                        install_dir = path.dirname(f)
                        if install_dir == bindir:
                            # FIXME: This isn't a real target but builder doesn't
                            # actually use it as such anyway.
                            ide_target = MesonBuildTarget(install_dir, name=path.basename(f))
                            targets.insert(0, ide_target)
                            break # Only need one
                except Exception as e:
                    pass

            task.build_targets = targets
            task.return_boolean(True)

        try:
            build_manager.execute_finish(result)
            thread = threading.Thread(target=build_targets_thread)
            thread.start()
        except Exception as err:
            task.return_error(err)


class MesonPipelineAddin(Ide.Object, Ide.BuildPipelineAddin):
    """
    The MesonPipelineAddin registers stages to be executed when various
    phases of the build pipeline are requested.

    The configuration cannot change during the lifetime of the pipeline,
    so it is safe to setup everything up-front.
    """

    def do_load(self, pipeline):
        context = pipeline.get_context()
        build_system = context.get_build_system()

        # Only register stages if we are a meson project
        if type(build_system) != MesonBuildSystem:
            return

        config = pipeline.get_configuration()
        runtime = config.get_runtime()

        srcdir = context.get_vcs().get_working_directory().get_path()
        builddir = build_system.get_builddir(config)

        # Discover ninja in the runtime/SDK
        ninja = None
        for name in _NINJA_NAMES:
            if runtime.contains_program_in_path(name):
                ninja = name
                break
        if ninja is None:
            print("Failed to locate ninja. Meson Building is disabled.")
            return

        # Register the configuration launcher which will perform our
        # "meson --prefix=..." configuration command.
        config_launcher = pipeline.create_launcher()
        config_launcher.push_argv('meson')
        config_launcher.push_argv(srcdir)
        # We will be launched from the builddir, so . is fine (as the directory
        # may be mapped somewhere else in the build runtime).
        config_launcher.push_argv('.')
        config_launcher.push_argv('--prefix={}'.format(config.props.prefix))
        config_opts = config.get_config_opts()
        if config_opts:
            _, config_opts = GLib.shell_parse_argv(config_opts)
            config_launcher.push_args(config_opts)

        config_stage = Ide.BuildStageLauncher.new(context, config_launcher)
        config_stage.set_completed(path.exists(path.join(builddir, 'build.ninja')))
        self.track(pipeline.connect(Ide.BuildPhase.CONFIGURE, 0, config_stage))

        # Register the build launcher which will perform the incremental
        # build of the project when the Ide.BuildPhase.BUILD phase is
        # requested of the pipeline.
        build_launcher = pipeline.create_launcher()
        build_launcher.push_argv(ninja)
        if config.props.parallelism > 0:
            build_launcher.push_argv('-j{}'.format(config.props.parallelism))

        clean_launcher = pipeline.create_launcher()
        clean_launcher.push_argv(ninja)
        clean_launcher.push_argv('clean')
        if config.props.parallelism > 0:
            clean_launcher.push_argv('-j{}'.format(config.props.parallelism))

        build_stage = Ide.BuildStageLauncher.new(context, build_launcher)
        build_stage.set_clean_launcher(clean_launcher)
        build_stage.set_check_stdout(True)
        build_stage.connect('query', self._query)
        self.track(pipeline.connect(Ide.BuildPhase.BUILD, 0, build_stage))

        # Register the install launcher which will perform our
        # "ninja install" when the Ide.BuildPhase.INSTALL phase
        # is requested of the pipeline.
        install_launcher = pipeline.create_launcher()
        install_launcher.push_argv(ninja)
        install_launcher.push_argv('install')

        install_stage = Ide.BuildStageLauncher.new(context, install_launcher)
        install_stage.connect('query', self._query)
        self.track(pipeline.connect(Ide.BuildPhase.INSTALL, 0, install_stage))

    def _query(self, stage, pipeline, cancellable):
        stage.set_completed(False)


class MesonBuildTarget(Ide.Object, Ide.BuildTarget):
    # TODO: These should be part of the BuildTarget interface
    name = GObject.Property(type=str)
    install_directory = GObject.Property(type=Gio.File)

    def __init__(self, install_dir, **kwargs):
        super().__init__(**kwargs)
        self.props.install_directory = Gio.File.new_for_path(install_dir)

    def do_get_install_directory(self):
        return self.props.install_directory

