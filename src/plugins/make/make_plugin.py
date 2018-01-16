# __init__.py
#
# Copyright © 2017 Matthew Leeds <mleeds@redhat.com>
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

gi.require_version('Ide', '1.0')

from gi.repository import GObject
from gi.repository import Gio
from gi.repository import GLib
from gi.repository import Ide

_ = Ide.gettext

class MakeBuildSystem(Ide.Object, Ide.BuildSystem, Gio.AsyncInitable):
    project_file = GObject.Property(type=Gio.File)
    make_dir = GObject.Property(type=Gio.File)
    run_args = None

    def do_get_id(self):
        return 'make'

    def do_get_display_name(self):
        return 'Make'

    def do_init_async(self, priority, cancel, callback, data=None):
        task = Gio.Task.new(self, cancel, callback)
        task.set_priority(priority)

        # TODO: Be async here also
        project_file = self.get_context().get_project_file()
        if project_file.get_basename() == 'Makefile':
            self.props.make_dir = project_file.get_parent()
            task.return_boolean(True)
        else:
            child = project_file.get_child('Makefile')
            exists = child.query_exists(cancel)
            if exists:
                self.props.make_dir = project_file
                self.props.project_file = child
            task.return_boolean(exists)

    def do_init_finish(self, result):
        return result.propagate_boolean()

    def do_get_priority(self):
        return -400 # Lower priority than Autotools and Meson

    def do_get_builddir(self, config):
        context = self.get_context()
        return context.get_vcs().get_working_directory().get_path()

    def do_get_build_flags_async(self, ifile, cancellable, callback, data=None):
        task = Gio.Task.new(self, cancellable, callback)
        task.ifile = ifile
        task.build_flags = []
        task.return_boolean(True)

    def do_get_build_flags_finish(self, result):
        if result.propagate_boolean():
            return result.build_flags

    def get_make_dir(self):
        return self.props.make_dir

class MakePipelineAddin(Ide.Object, Ide.BuildPipelineAddin):
    """
    The MakePipelineAddin registers stages to be executed when various
    phases of the build pipeline are requested.
    """

    def do_load(self, pipeline):
        context = pipeline.get_context()
        build_system = context.get_build_system()

        # Only register stages if we are a makefile project
        if type(build_system) != MakeBuildSystem:
            return

        config = pipeline.get_configuration()
        runtime = config.get_runtime()

        # If the configuration has set $MAKE, then use it.
        make = config.getenv('MAKE') or "make"

        srcdir = context.get_vcs().get_working_directory().get_path()
        builddir = build_system.get_builddir(config)

        # Register the build launcher which will perform the incremental
        # build of the project when the Ide.BuildPhase.BUILD phase is
        # requested of the pipeline.
        build_launcher = pipeline.create_launcher()
        build_launcher.set_cwd(build_system.get_make_dir().get_path())
        build_launcher.push_argv(make)
        if config.props.parallelism > 0:
            build_launcher.push_argv('-j{}'.format(config.props.parallelism))

        clean_launcher = pipeline.create_launcher()
        clean_launcher.set_cwd(build_system.get_make_dir().get_path())
        clean_launcher.push_argv(make)
        clean_launcher.push_argv('clean')

        build_stage = Ide.BuildStageLauncher.new(context, build_launcher)
        build_stage.set_name(_("Build project"))
        build_stage.set_clean_launcher(clean_launcher)
        build_stage.connect('query', self._query)
        self.track(pipeline.connect(Ide.BuildPhase.BUILD, 0, build_stage))

        # Register the install launcher which will perform our
        # "make install" when the Ide.BuildPhase.INSTALL phase
        # is requested of the pipeline.
        install_launcher = pipeline.create_launcher()
        install_launcher.set_cwd(build_system.get_make_dir().get_path())
        install_launcher.push_argv(make)
        install_launcher.push_argv('install')

        install_stage = Ide.BuildStageLauncher.new(context, install_launcher)
        install_stage.set_name(_("Install project"))
        self.track(pipeline.connect(Ide.BuildPhase.INSTALL, 0, install_stage))

        # Determine what it will take to "make run" for this pipeline
        # and stash it on the build_system for use by the build target.
        # This allows us to run Make projects as long as the makefile
        # has a "run" target.
        build_system.run_args = [make, '-C', build_system.get_make_dir().get_path(), 'run']

    def _query(self, stage, pipeline, cancellable):
        stage.set_completed(False)

class MakeBuildTarget(Ide.Object, Ide.BuildTarget):

    def do_get_install_directory(self):
        return None

    def do_get_name(self):
        return 'make-run'

    def do_get_language(self):
        # Not meaningful, since we have an indirect process.
        return 'make'

    def do_get_argv(self):
        context = self.get_context()
        build_system = context.get_build_system()
        assert type(build_system) == MakeBuildSystem
        return build_system.run_args

    def do_get_priority(self):
        return 0

class MakeBuildTargetProvider(Ide.Object, Ide.BuildTargetProvider):
    """
    The MakeBuildTargetProvider just wraps a "make run" command. If the
    Makefile doesn't have a run target, we'll just fail to execute and the user
    should get the warning in their application output (and can update their
    Makefile appropriately).
    """

    def do_get_targets_async(self, cancellable, callback, data):
        task = Gio.Task.new(self, cancellable, callback)
        task.set_priority(GLib.PRIORITY_LOW)

        context = self.get_context()
        build_system = context.get_build_system()

        if type(build_system) != MakeBuildSystem:
            task.return_error(GLib.Error('Not a make build system',
                                         domain=GLib.quark_to_string(Gio.io_error_quark()),
                                         code=Gio.IOErrorEnum.NOT_SUPPORTED))
            return

        task.targets = [MakeBuildTarget(context=self.get_context())]
        task.return_boolean(True)

    def do_get_targets_finish(self, result):
        if result.propagate_boolean():
            return result.targets
