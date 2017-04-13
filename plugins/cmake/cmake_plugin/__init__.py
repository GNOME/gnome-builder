# __init__.py
#
# Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

import gi
from os import path

gi.require_version('Ide', '1.0')

from gi.repository import GLib
from gi.repository import GObject
from gi.repository import Gio
from gi.repository import Ide

_CMAKE = "cmake"
_NINJA_NAMES = [ 'ninja-build', 'ninja' ]

class CMakeBuildSystem(Ide.Object, Ide.BuildSystem, Gio.AsyncInitable):
    project_file = GObject.Property(type=Gio.File)

    def do_get_id(self):
        return 'cmake'

    def do_get_display_name(self):
        return 'Cmake'

    def do_init_async(self, priority, cancel, callback, data=None):
        task = Gio.Task.new(self, cancel, callback)
        task.set_priority(priority)

        # TODO: Be async here also
        project_file = self.get_context().get_project_file()
        if project_file.get_basename() == 'CMakeLists.txt':
            task.return_boolean(True)
        else:
            child = project_file.get_child('CMakeLists.txt')
            exists = child.query_exists(cancel)
            if exists:
                self.props.project_file = child
            task.return_boolean(exists)

    def do_init_finish(self, result):
        return result.propagate_boolean()

    def do_get_priority(self):
        return 200


class CMakePipelineAddin(Ide.Object, Ide.BuildPipelineAddin):
    """
    The CMakePipelineAddin registers stages to be executed when various
    phases of the build pipeline are requested.

    The configuration cannot change during the lifetime of the pipeline,
    so it is safe to setup everything up-front.
    """

    def do_load(self, pipeline):
        context = pipeline.get_context()
        build_system = context.get_build_system()

        # Only register stages if we are a cmake project
        if type(build_system) != CMakeBuildSystem:
            return

        config = pipeline.get_configuration()
        runtime = config.get_runtime()

        srcdir = context.get_vcs().get_working_directory().get_path()
        builddir = build_system.get_builddir(config)

        # Discover ninja in the runtime/SDK
        if not runtime.contains_program_in_path(_CMAKE):
            print("Failed to locate “cmake”. Building is disabled.")
            return

        # Discover ninja in the runtime/SDK
        ninja = None
        for name in _NINJA_NAMES:
            if runtime.contains_program_in_path(name):
                ninja = name
                break
        if ninja is None:
            print("Failed to locate ninja. CMake building is disabled.")
            return

        # Register the configuration launcher which will perform our
        # "cmake -DCMAKE_INSTALL_PREFIX=..." configuration command.
        config_launcher = pipeline.create_launcher()
        config_launcher.push_argv(_CMAKE)
        # We need the parent directory of CMakeLists.txt, not the CMakeLists.txt
        # itself (or cmake will do in-tree configuration)
        config_launcher.push_argv(build_system.project_file.get_parent().get_path())
        config_launcher.push_argv('-G')
        config_launcher.push_argv('Ninja')
        config_launcher.push_argv('-DCMAKE_INSTALL_PREFIX={}'.format(config.props.prefix))
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
        self.track(pipeline.connect(Ide.BuildPhase.INSTALL, 0, install_stage))

    def _query(self, stage, pipeline, cancellable):
        stage.set_completed(False)

